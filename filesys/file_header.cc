/// Routines for managing the disk file header (in UNIX, this would be called
/// the i-node).
///
/// The file header is used to locate where on disk the file's data is
/// stored.  We implement this as a fixed size table of pointers -- each
/// entry in the table points to the disk sector containing that portion of
/// the file data (in other words, there are no indirect or doubly indirect
/// blocks). The table size is chosen so that the file header will be just
/// big enough to fit in one disk sector,
///
/// Unlike in a real system, we do not keep track of file permissions,
/// ownership, last modification date, etc., in the file header.
///
/// A file header can be initialized in two ways:
///
/// * for a new file, by modifying the in-memory data structure to point to
///   the newly allocated data blocks;
/// * for a file already on disk, by reading the file header from disk.
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2020 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


#include "file_header.hh"
#include "threads/system.hh"

#include <ctype.h>
#include <stdio.h>



/// Initialize a fresh file header for a newly created file.  Allocate data
/// blocks for the file out of the map of free disk blocks.  Return false if
/// there are not enough free blocks to accomodate the new file.
///
/// * `freeMap` is the bit map of free disk sectors.
/// * `fileSize` is the bit map of free disk sectors.
bool
FileHeader::Allocate(Bitmap *freeMap, unsigned fileSize)
{
    ASSERT(freeMap != nullptr);

    //const unsigned NUM_DIRECT  = (SECTOR_SIZE - 2 * sizeof (int)) / sizeof (int);

    unsigned sectoresTotales= DivRoundUp(fileSize, SECTOR_SIZE);
    raw.numSectors = sectoresTotales > NUM_DIRECT - 1 ? NUM_DIRECT - 1 : sectoresTotales;
    if (freeMap->CountClear() < sectoresTotales)
        return false;  // Not enough space.

    //if (fileSize > MAX_FILE_SIZE)
    //    return false;
    raw.numBytes = fileSize;//si fileSize es menor al tamano bien, sino se pisa con el valor maximo posible en el if
    int resto = DivRoundUp(fileSize,SECTOR_SIZE) - NUM_DIRECT - 1;

    raw.dataSectors[NUM_DIRECT-1]=0;//utilizamos 0 como siguiente sector nulo ya que ese sector siempre sera ocupado por FREE_MAP por lo cual nadie tendra un next apuntando a este sector

    if (resto > 0){
        raw.numBytes = raw.numSectors * SECTOR_SIZE;
        DEBUG('F', "Se crea un fileheader next, resto = %d\n", resto);
        next = new FileHeader;
        int sectornext= freeMap->Find();
        if (sectornext == -1) return false;
        next->sector=sectornext;
        next->Allocate(freeMap, resto*SECTOR_SIZE);
        raw.dataSectors[NUM_DIRECT - 1] = sectornext;
        // next->WriteBack(sectornext);
    } 

    // sector = -1; //No tiene sector asignado el header todavia

    for (unsigned i = 0; i < raw.numSectors; i++)
        raw.dataSectors[i] = freeMap->Find();
    return true;
}

bool
FileHeader::UpdateRaw(Bitmap *freeMap, unsigned size) {
    ASSERT(freeMap != nullptr);

    unsigned nuevoTamano= ((NUM_DIRECT-1)*SECTOR_SIZE < raw.numBytes + size) ? (NUM_DIRECT-1)*SECTOR_SIZE : raw.numBytes + size;
    unsigned nuevonumSectors=DivRoundUp(nuevoTamano,SECTOR_SIZE);
    if (nuevonumSectors==0) return true;
    DEBUG('F', "Incrementado de tamaño de archivo en %d.\n", size);
    if (freeMap->CountClear() < nuevonumSectors - raw.numSectors)
        return false;

    raw.numBytes = nuevoTamano;
    

    for (unsigned i = raw.numSectors; i < nuevonumSectors; i++){
        raw.dataSectors[i] = freeMap->Find();
    }
    
    raw.numSectors = nuevonumSectors;
    return true;
}

void 
FileHeader::AddIndir(unsigned sector_) {
    raw.dataSectors[NUM_DIRECT-1] = sector_;
}

/// De-allocate all the space allocated for data blocks for this file.
///
/// * `freeMap` is the bit map of free disk sectors.
void
FileHeader::Deallocate(Bitmap *freeMap)
{
    ASSERT(freeMap != nullptr);

    for (unsigned i = 0; i < raw.numSectors; i++) {
        ASSERT(freeMap->Test(raw.dataSectors[i]));  // ought to be marked!
        freeMap->Clear(raw.dataSectors[i]);
    }

    if(next!=nullptr){
        freeMap->Clear(next->sector);//No hago clear del header principal por que no lo estoy eliminando, solo dealocando
        next->Deallocate(freeMap);
        delete next; //VER
    }
}

/// Fetch contents of file header from disk.
///
/// * `sector` is the disk sector containing the file header.
void
FileHeader::FetchFrom(unsigned sector_)
{
    synchDisk->ReadSector(sector_, (char *) &raw);
    if (raw.dataSectors[NUM_DIRECT-1]!=0) {
        next = new FileHeader;
        next->sector = raw.dataSectors[NUM_DIRECT-1];
        next->FetchFrom(next->sector);
    }
}

/// Write the modified contents of the file header back to disk.
///
/// * `sector` is the disk sector to contain the file header.
void
FileHeader::WriteBack(unsigned sector_)
{
    synchDisk->WriteSector(sector_, (char *) &raw);

/*
    if (raw.dataSectors[NUM_DIRECT]!=0){
        next = new FileHeader;
        next->sector = raw.dataSectors[NUM_DIRECT];
        next->WriteBack(next->sector);
    }
*/
    if (next != nullptr)
        next->WriteBack(next->sector);
}


/// Return which disk sector is storing a particular byte within the file.
/// This is essentially a translation from a virtual address (the offset in
/// the file) to a physical address (the sector where the data at the offset
/// is stored).
///
/// * `offset` is the location within the file of the byte in question.
unsigned
FileHeader::ByteToSector(unsigned offset) //offset es bytes
{
    /*
    FileHeader *ptr=this;
    for(int i=(offset / SECTOR_SIZE) / (NUM_DIRECT-1);i!=0;i--){
        ASSERT(ptr->next!=nullptr);//Si se rompe lo vemos, no deberia estar llamandose asi
        ptr=ptr->next;
    }
    return ptr->GetRaw()->dataSectors[offset/SECTOR_SIZE - ((offset/ SECTOR_SIZE)/NUM_DIRECT)*NUM_DIRECT]; //revisar la cuenta esta*/
    unsigned lastByte = (NUM_DIRECT - 1) * SECTOR_SIZE - 1;
    if (offset > lastByte) {
        ASSERT(next != nullptr);
        // DEBUG('F',"offset-lastByte= %d\n", offset - lastByte);
        return next->ByteToSector(offset - lastByte);
    }
    return raw.dataSectors[offset / SECTOR_SIZE];
}

/// Return the number of bytes in the file.
unsigned
FileHeader::FileLength() const
{
    if(next!=nullptr) return raw.numBytes + next->FileLength();
    return raw.numBytes;
}

int
FileHeader::LinkCount()
{
    int i=0;
    for(FileHeader *ptr=this;ptr->next!=nullptr;ptr=ptr->next) i++;
    return i;
}

/// Print the contents of the file header, and the contents of all the data
/// blocks pointed to by the file header.
void
FileHeader::Print(const char *title)
{
    char *data = new char [SECTOR_SIZE];

    if (title == nullptr)
        printf("File header:\n");
    else
        printf("%s file header:\n", title);

    printf("    size: %u bytes\n"
           "    link count: %u\n"
           "    raw.numSectors: %u\n"
           "    block indexes: ",
           raw.numBytes, LinkCount(), raw.numSectors);

    for (unsigned i = 0; i < raw.numSectors; i++)
        printf("%u ", raw.dataSectors[i]);
    printf("\n");

    for (unsigned i = 0, k = 0; i < raw.numSectors; i++) {
        printf("    contents of block %u:\n", raw.dataSectors[i]);
        synchDisk->ReadSector(raw.dataSectors[i], data);
        for (unsigned j = 0; j < SECTOR_SIZE && k < raw.numBytes; j++, k++) {
            if (isprint(data[j]))
                printf("%c", data[j]);
            else
                printf("\\%X", (unsigned char) data[j]);
        }
        printf("\n");
    }
    delete [] data;
    if(next!=nullptr) next->Print(title);
}

const RawFileHeader *
FileHeader::GetRaw() const
{
    return &raw;
}
