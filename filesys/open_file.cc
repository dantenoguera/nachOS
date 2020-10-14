/// Routines to manage an open Nachos file.  As in UNIX, a file must be open
/// before we can read or write to it.  Once we are all done, we can close it
/// (in Nachos, by deleting the `OpenFile` data structure).
///
/// Also as in UNIX, for convenience, we keep the file header in memory while
/// the file is open.
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2020 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


#include "open_file.hh"
#include "file_header.hh"
#include "threads/system.hh"
#include "file_list.hh"
#include <string.h>


/// Open a Nachos file for reading and writing.  Bring the file header into
/// memory while the file is open.
///
/// * `sector` is the location on disk of the file header for this file.
OpenFile::OpenFile(int sector, const char *name)
{
    DEBUG('F', "creando OpenFile para %s\n", name);

    hdr = new FileHeader;
    hdr->FetchFrom(sector);
    hdr->sector = sector;
    seekPosition = 0;

    fileName=new char[strlen(name) + 1];
    fileName=strncpy(fileName,name,strlen(name) + 1);
    ASSERT(0==strcmp(fileName,name));
}

/// Close a Nachos file, de-allocating any in-memory data structures.
// FREE_MAP_SECTOR y DIRECTORY_SECTOR no son agregados a listOpenFiles
// pero si invocan al destructor 
OpenFile::~OpenFile()
{
    DEBUG('F', "borrando OpenFile de %s\n", fileName);
    listOpenFiles->Remove(fileName);

    delete [] fileName;
    delete hdr;
}

/// Change the current location within the open file -- the point at which
/// the next `Read` or `Write` will start from.
///
/// * `position` is the location within the file for the next `Read`/`Write`.
void
OpenFile::Seek(unsigned position)
{
    seekPosition = position;
}

/// OpenFile::Read/Write
///
/// Read/write a portion of a file, starting from `seekPosition`.  Return the
/// number of bytes actually written or read, and as a side effect, increment
/// the current position within the file.
///
/// Implemented using the more primitive `ReadAt`/`WriteAt`.
///
/// * `into` is the buffer to contain the data to be read from disk.
/// * `from` is the buffer containing the data to be written to disk.
/// * `numBytes` is the number of bytes to transfer.

int
OpenFile::Read(char *into, unsigned numBytes)
{
    ASSERT(into != nullptr);
    ASSERT(numBytes > 0);

    int result = ReadAt(into, numBytes, seekPosition);

    seekPosition += result;
    return result;
}

int
OpenFile::Write(const char *into, unsigned numBytes)
{

    ASSERT(into != nullptr);
    ASSERT(numBytes > 0);

    // FData* fd = listOpenFiles->Find(fileName);

    int result = WriteAt(into, numBytes, seekPosition);
    seekPosition += result;
    return result;
}

/// OpenFile::ReadAt/WriteAt
///
/// Read/write a portion of a file, starting at `position`.  Return the
/// number of bytes actually written or read, but has no side effects (except
/// that `Write` modifies the file, of course).
///
/// There is no guarantee the request starts or ends on an even disk sector
/// boundary; however the disk only knows how to read/write a whole disk
/// sector at a time.  Thus:
///
/// For ReadAt:
///     We read in all of the full or partial sectors that are part of the
///     request, but we only copy the part we are interested in.
/// For WriteAt:
///     We must first read in any sectors that will be partially written, so
///     that we do not overwrite the unmodified portion.  We then copy in the
///     data that will be modified, and write back all the full or partial
///     sectors that are part of the request.
///
/// * `into` is the buffer to contain the data to be read from disk.
/// * `from` is the buffer containing the data to be written to disk.
/// * `numBytes` is the number of bytes to transfer.
/// * `position` is the offset within the file of the first byte to be
///   read/written.

int
OpenFile::ReadAt(char *into, unsigned numBytes, unsigned position)
{
    DEBUG('F', "thread %d, ReadAt: %s \n", currentThread->myId, fileName);
    ASSERT(into != nullptr);
    ASSERT(numBytes > 0);

    //-----Sincronizacion
    FData* fd = listOpenFiles->Find(fileName);
    ASSERT(fd != nullptr);
    
    if(not fd->condLock -> IsHeldByCurrentThread()){
        fd->condLock-> Acquire();
        fd->readings++;
        fd->condLock-> Release();
    }
    //--------------------
    
    unsigned fileLength = hdr->FileLength();
    unsigned firstSector, lastSector, numSectors;
    char *buf;

    if (position >= fileLength){
        //-----Sincronizacion
        if (not fd->condLock -> IsHeldByCurrentThread()) {
            fd->condLock -> Acquire();
            fd->readings--;
        if(fd->readings == 0)
            fd->canRead -> Broadcast();
        fd->condLock -> Release();
        }
        //--------------------
        return 0;  // Check request.
    }
    if (position + numBytes > fileLength)
        numBytes = fileLength - position;
    DEBUG('f', "Reading %u bytes at %u, from file of length %u.\n",
          numBytes, position, fileLength);

    firstSector = DivRoundDown(position, SECTOR_SIZE);
    lastSector = DivRoundDown(position + numBytes - 1, SECTOR_SIZE);
    numSectors = 1 + lastSector - firstSector;

    // Read in all the full and partial sectors that we need.
    buf = new char [numSectors * SECTOR_SIZE];
    for (unsigned i = firstSector; i <= lastSector; i++)
        synchDisk->ReadSector(hdr->ByteToSector(i * SECTOR_SIZE),
                              &buf[(i - firstSector) * SECTOR_SIZE]);

    // Copy the part we want.
    memcpy(into, &buf[position - firstSector * SECTOR_SIZE], numBytes);
    delete [] buf;

    //-----Sincronizacion
    if(not fd->condLock -> IsHeldByCurrentThread()){
        fd->condLock -> Acquire();
        fd->readings--;
        if(fd->readings == 0)
            fd->canRead -> Broadcast();
        fd->condLock -> Release();
    }
    //--------------------


    return numBytes;
}

int
OpenFile::WriteAt(const char *from, unsigned numBytes, unsigned position)
{ 
    DEBUG('F', "thread %d, WriteAt: %s \n", currentThread->myId, fileName);
    ASSERT(from != nullptr);
    ASSERT(numBytes > 0);

    //---------Sincronizacion
    FData* fd = listOpenFiles->Find(fileName);
    ASSERT(fd != nullptr);
    // listOpenFiles->Print();
    fd->condLock -> Acquire();

    while(fd->readings > 0)
        fd->canRead -> Wait();
    //-----------------------

    unsigned fileLength = hdr->FileLength();
    unsigned firstSector, lastSector, numSectors, freeSectors;
    bool firstAligned, lastAligned;
    char *buf;

    if (position > fileLength) {
        //------Sincronizacion
        fd->canRead -> Signal();
        fd->condLock -> Release();
        //--------------------
        return 0;  // Check request.
    }
    
    DEBUG('f', "Writing %s %u bytes at %u, from file of length %u.\n",
          fileName, numBytes, position, fileLength);

    firstSector = DivRoundDown(position, SECTOR_SIZE);
    lastSector  = DivRoundDown(position + numBytes - 1, SECTOR_SIZE);
    numSectors  = 1 + lastSector - firstSector;

    //-------Extender archivo
    if (position + numBytes > fileLength) {
        Bitmap *freeMap = new Bitmap(NUM_SECTORS);
        freeMap->FetchFrom(fileSystem->freeMapFile);
        
        FileHeader *ptr = hdr;
        for (; ptr->next != nullptr; ptr = ptr->next);
        freeSectors = NUM_DIRECT - 1 - ptr->GetRaw()->numSectors;
        // DEBUG('F', "freeSectors: %d, firstSector: %d, lastSector: %d, ptr->GetRaw()->numSectors: %d.\n", freeSectors, firstSector, lastSector, ptr->GetRaw()->numSectors);
        if (!ptr->UpdateRaw(freeMap, numBytes)) {
            delete freeMap;
            //------Sincronizacion
            fd->canRead -> Signal();
            fd->condLock -> Release();
            //--------------------
            return 0;
        }

        if ((NUM_DIRECT-1)*SECTOR_SIZE - ptr->GetRaw()->numBytes < numBytes) { //bytes libres < bytes que voy a guardar
            DEBUG('F', "Creando next.\n");
            int sector = freeMap->Find();
            if (sector == -1){
                delete freeMap;
                //------Sincronizacion
                fd->canRead -> Signal();
                fd->condLock -> Release();
                //--------------------
                return 0;
            }
            ptr->next = new FileHeader;
            ptr->AddIndir(sector);
            ptr->next->sector = sector;
            ptr->next->Allocate(freeMap, (numSectors - freeSectors) * SECTOR_SIZE);
        }
        ptr->WriteBack(ptr->sector);
        freeMap->WriteBack(fileSystem->freeMapFile);
        delete freeMap;
    }
    //--------------------

    buf = new char [numSectors * SECTOR_SIZE];

    firstAligned = position == firstSector * SECTOR_SIZE;
    lastAligned  = position + numBytes == (lastSector + 1) * SECTOR_SIZE;

    // Read in first and last sector, if they are to be partially modified.
    if (!firstAligned)
        ReadAt(buf, SECTOR_SIZE, firstSector * SECTOR_SIZE);
        
    if (!lastAligned && (firstSector != lastSector || firstAligned))
        ReadAt(&buf[(lastSector - firstSector) * SECTOR_SIZE],
               SECTOR_SIZE, lastSector * SECTOR_SIZE);
        

    // Copy in the bytes we want to change.
    memcpy(&buf[position - firstSector * SECTOR_SIZE], from, numBytes);

    // Write modified sectors back. IMPRIME BIEN
    for (unsigned i = firstSector; i <= lastSector; i++) {// 0 - 30
        // DEBUG('F', "i: %d, hdr->ByteToSector(%d): %d.\n", i, i * SECTOR_SIZE, hdr->ByteToSector(i * SECTOR_SIZE));
        synchDisk->WriteSector(hdr->ByteToSector(i * SECTOR_SIZE),//no necesito fijarme el fileheader especifico para cada sector por que ByteToSector lo hace automaticamente
                               &buf[(i - firstSector) * SECTOR_SIZE]);
    }
    delete [] buf;

    //------Sincronizacion
    fd->canRead -> Signal();
    fd->condLock -> Release();
    //--------------------

    return numBytes;
}

/// Return the number of bytes in the file.
unsigned
OpenFile::Length() const
{
    return hdr->FileLength();
}
