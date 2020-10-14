/// Routines to manage the overall operation of the file system.  Implements
/// routines to map from textual file names to files.
///
/// Each file in the file system has:
/// * a file header, stored in a sector on disk (the size of the file header
///   data structure is arranged to be precisely the size of 1 disk sector);
/// * a number of data blocks;
/// * an entry in the file system directory.
///
/// The file system consists of several data structures:
/// * A bitmap of free disk sectors (cf. `bitmap.h`).
/// * A directory of file names and file headers.
///
/// Both the bitmap and the directory are represented as normal files.  Their
/// file headers are located in specific sectors (sector 0 and sector 1), so
/// that the file system can find them on bootup.
///
/// The file system assumes that the bitmap and directory files are kept
/// “open” continuously while Nachos is running.
///
/// For those operations (such as `Create`, `Remove`) that modify the
/// directory and/or bitmap, if the operation succeeds, the changes are
/// written immediately back to disk (the two files are kept open during all
/// this time).  If the operation fails, and we have modified part of the
/// directory and/or bitmap, we simply discard the changed version, without
/// writing it back to disk.
///
/// Our implementation at this point has the following restrictions:
///
/// * there is no synchronization for concurrent accesses;
/// * files have a fixed size, set when the file is created;
/// * files cannot be bigger than about 3KB in size;
/// * there is no hierarchical directory structure, and only a limited number
///   of files can be added to the system;
/// * there is no attempt to make the system robust to failures (if Nachos
///   exits in the middle of an operation that modifies the file system, it
///   may corrupt the disk).
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2020 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


#include "file_system.hh"
#include "directory.hh"
#include "directory_entry.hh"
#include "file_header.hh"
#include "lib/bitmap.hh"
#include "machine/disk.hh"
#include "threads/system.hh"

#include <stdio.h>
#include <string.h>


/// Sectors containing the file headers for the bitmap of free sectors, and
/// the directory of files.  These file headers are placed in well-known
/// sectors, so that they can be located on boot-up.
static const unsigned FREE_MAP_SECTOR = 0;
static const unsigned DIRECTORY_SECTOR = 1;

/// Initial file sizes for the bitmap and directory; until the file system
/// supports extensible files, the directory size sets the maximum number of
/// files that can be loaded onto the disk.
static const unsigned FREE_MAP_FILE_SIZE = NUM_SECTORS / BITS_IN_BYTE;
static const unsigned NUM_DIR_ENTRIES = 10;
static const unsigned DIRECTORY_FILE_SIZE = sizeof (DirectoryEntry)
                                            * NUM_DIR_ENTRIES + 1;

/// Initialize the file system.  If `format == true`, the disk has nothing on
/// it, and we need to initialize the disk to contain an empty directory, and
/// a bitmap of free sectors (with almost but not all of the sectors marked
/// as free).
///
/// If `format == false`, we just have to open the files representing the
/// bitmap and the directory.
///
/// * `format` -- should we initialize the disk?

char*
FileSystem::FillPath(const char* path){
    char* path2;
    if (path[0]=='/'){//si es absoluto
        path2 = new char[strlen(path) + 1];
        strcpy(path2, path);
        return path2;
    }
    else {//si es relativo
        path2 = new char[strlen(directoryList->Get(currentThread->myId)->fileName) + strlen(path) + 2];
        strcpy(path2,directoryList->Get(currentThread->myId)->fileName);
        if(strcmp(directoryList->Get(currentThread->myId)->fileName,"/")!=0) strcat(path2,"/");
        strcat(path2,path);
        return path2;
    }
}

FileSystem::FileSystem(bool format)
{
    DEBUG('f', "Initializing the file system.\n");
    if (format) {
        Bitmap     *freeMap = new Bitmap(NUM_SECTORS);
        Directory  *dir     = new Directory(NUM_DIR_ENTRIES);
        FileHeader *mapH    = new FileHeader;
        FileHeader *dirH    = new FileHeader;

        DEBUG('f', "Formatting the file system.\n");

        // First, allocate space for FileHeaders for the directory and bitmap
        // (make sure no one else grabs these!)
        freeMap->Mark(FREE_MAP_SECTOR);
        freeMap->Mark(DIRECTORY_SECTOR);

        // Second, allocate space for the data blocks containing the contents
        // of the directory and bitmap files.  There better be enough space!

        ASSERT(mapH->Allocate(freeMap, FREE_MAP_FILE_SIZE));
        ASSERT(dirH->Allocate(freeMap, DIRECTORY_FILE_SIZE));

        // Flush the bitmap and directory `FileHeader`s back to disk.
        // We need to do this before we can `Open` the file, since open reads
        // the file header off of disk (and currently the disk has garbage on
        // it!).

        DEBUG('f', "Writing headers back to disk.\n");
        mapH->WriteBack(FREE_MAP_SECTOR);
        dirH->WriteBack(DIRECTORY_SECTOR);


        
        // OK to open the bitmap and directory files now.
        // The file system operations assume these two files are left open
        // while Nachos is running.
        #ifdef FILESYS_STUB
        freeMapFile   = new OpenFile(FREE_MAP_SECTOR);
        directoryFile = new OpenFile(DIRECTORY_SECTOR);
        #else
        freeMapFile   = new OpenFile(FREE_MAP_SECTOR, "FREE_MAP_SECTOR");
        listOpenFiles->Add("FREE_MAP_SECTOR");
        // OpenFile* directoryFile = new OpenFile(DIRECTORY_SECTOR, "/");
        directoryList = new DirectoryList();
        directoryList->Add(currentThread->myId,DIRECTORY_SECTOR, "/");
        listOpenFiles->Add("/");

        freeMapLock = new Lock("freeMapLock");
        #endif

        // Once we have the files “open”, we can write the initial version of
        // each file back to disk.  The directory at this point is completely
        // empty; but the bitmap has been changed to reflect the fact that
        // sectors on the disk have been allocated for the file headers and
        // to hold the file data for the directory and bitmap.

        DEBUG('f', "Writing bitmap and directory back to disk.\n");
        freeMap->WriteBack(freeMapFile);     // flush changes to disk
        // dir->WriteBack(directoryFile);
        dir->WriteBack(directoryList->Get(currentThread->myId));

        if (debug.IsEnabled('f')) {
            freeMap->Print();
            dir->Print();

            delete freeMap;
            delete dir;
            delete mapH;
            delete dirH;
        }
    } else {
        // If we are not formatting the disk, just open the files
        // representing the bitmap and directory; these are left open while
        // Nachos is running.
        #ifdef FILESYS_STUB
        freeMapFile   = new OpenFile(FREE_MAP_SECTOR);
        directoryFile = new OpenFile(DIRECTORY_SECTOR);
        #else
        freeMapFile   = new OpenFile(FREE_MAP_SECTOR, "FREE_MAP_SECTOR");
        listOpenFiles->Add("FREE_MAP_SECTOR");
        // OpenFile* directoryFile = new OpenFile(DIRECTORY_SECTOR, "/");
        directoryList=new DirectoryList();
        directoryList->Add(currentThread->myId,DIRECTORY_SECTOR, "/");
        listOpenFiles->Add("/");
        freeMapLock = new Lock("freeMapLock");
        #endif
    }
}

FileSystem::~FileSystem()
{
    delete freeMapFile;
    delete directoryList;
    delete freeMapLock;
}

/// Create a file in the Nachos file system (similar to UNIX `create`).
/// Since we cannot increase the size of files dynamically, we have to give
/// Create the initial size of the file.
///
/// The steps to create a file are:
/// 1. Make sure the file does not already exist.
/// 2. Allocate a sector for the file header.
/// 3. Allocate space on disk for the data blocks for the file.
/// 4. Add the name to the directory.
/// 5. Store the new file header on disk.
/// 6. Flush the changes to the bitmap and the directory back to disk.
///
/// Return true if everything goes ok, otherwise, return false.
///
/// Create fails if:
/// * file is already in directory;
/// * no free space for file header;
/// * no free entry for file in directory;
/// * no free space for data blocks for the file.
///
/// Note that this implementation assumes there is no concurrent access to
/// the file system!
///
/// * `name` is the name of file to be created.
/// * `initialSize` is the size of file to be created.

bool
FileSystem::Create(const char *name, unsigned initialSize)
{
    directoryList->CheckDirectoryUse(currentThread->myId);
    directoryList->GetLock(currentThread->myId)->Acquire();

    ASSERT(name != nullptr);

    DEBUG('f', "Creating file %s, size %u\n", name, initialSize);

    Directory *dir = new Directory(NUM_DIR_ENTRIES);
    dir->FetchFrom(directoryList->Get(currentThread->myId));

    bool success;

    if (dir->Find(name) != -1) {
        DEBUG('f', "Creating file %s error: File is already in directory.\n", name);
        success = false;  // File is already in directory.
    }
    else {
        Bitmap *freeMap = new Bitmap(NUM_SECTORS);

        //freeMapLock->Acquire();
        freeMap->FetchFrom(freeMapFile);

        int sector = freeMap->Find();
          // Find a sector to hold the file header.
        if (sector == -1) {
            DEBUG('f', "Creating file %s error: No free block for file header.\n", name);
            success = false;  // No free block for file header.
        }
            
        else if (!dir->Add(name, sector, false)) { 
            DEBUG('f', "Creating file %s error: No space in directory.\n", name);
            success = false;  // No space in directory.
        }   
        else {
            FileHeader *h = new FileHeader;
            success = h->Allocate(freeMap, initialSize);
              // Fails if no space on disk for data.
            if (success) {
                // Everything worked, flush all changes back to disk.
                h->sector=sector;
                h->WriteBack(sector);
                freeMap->WriteBack(freeMapFile);

                dir->WriteBack(directoryList->Get(currentThread->myId));
            } else this->Remove(name);//Necesitamos eliminarlo por los sectores que se asignaron
            delete h;
        }
        //freeMapLock->Release();
        delete freeMap;
    }
    delete dir;
    directoryList->GetLock(currentThread->myId)->Release();
    return success;
}

bool
FileSystem::CreateDir(const char *name)
{
    directoryList->CheckDirectoryUse(currentThread->myId);
    directoryList->GetLock(currentThread->myId)->Acquire();

    ASSERT(name != nullptr);

    DEBUG('f', "Creating directory %s\n", name);

    Directory *dir = new Directory(NUM_DIR_ENTRIES);

    dir->FetchFrom(directoryList->Get(currentThread->myId));

    bool success;

    if (dir->Find(name) != -1) {
        DEBUG('f', "Creating directory %s error: Directory is already in directory.\n", name);
        success = false;  // Directory is already in directory.
    }
    else {
        Bitmap *freeMap = new Bitmap(NUM_SECTORS);

        //freeMapLock->Acquire();
        freeMap->FetchFrom(freeMapFile);

        int sector = freeMap->Find();
          // Find a sector to hold the file header.
        if (sector == -1) {
            DEBUG('f', "Creating directory %s error: No free block for file header.\n", name);
            success = false;  // No free block for file header.
        }    
        else if (!dir->Add(name, sector, true)) { //no deberia fallar nunca, extensible
            DEBUG('f', "Creating directory %s error: No space in directory.\n", name);
            success = false;  // No space in directory.
        }   
        else {
            FileHeader *h = new FileHeader;
            success = h->Allocate(freeMap, DIRECTORY_FILE_SIZE);
              // Fails if no space on disk for data.
            if (success) {
                // Everything worked, flush all changes back to disk.
                h->sector=sector;
                h->WriteBack(sector);
                Directory *newDir = new Directory(NUM_DIR_ENTRIES);
                OpenFile *newDirOf = new OpenFile(sector, name);
                listOpenFiles->Add(name);
                newDir->WriteBack(newDirOf);
                freeMap->WriteBack(freeMapFile);
                dir->WriteBack(directoryList->Get(currentThread->myId));
                delete newDir;
                delete newDirOf;
            } else this->Remove(name);//Necesitamos eliminarlo por los sectores que se asignaron
            delete h;
        }
        //freeMapLock->Release();
        delete freeMap;
    }
    delete dir;
    directoryList->GetLock(currentThread->myId)->Release();
    return success;
}


bool
FileSystem::ChangeDir(const char *path) {

    directoryList->CheckDirectoryUse(currentThread->myId);

    if (strcmp(directoryList->Get(currentThread->myId)->fileName, path) == 0) return true;

    char *pathaux, *pathaux2;
    pathaux = pathaux2 = new char[strlen(path) + 2];
    pathaux[strlen(path) + 1] = '\0';
    strcpy(pathaux,path);

    Directory *dir = new Directory(NUM_DIR_ENTRIES);
    int nuevoDirSector = -1;

    OpenFile* root = nullptr;

    if (path[0] == '/') {
        nuevoDirSector = DIRECTORY_SECTOR;
        root = new OpenFile(DIRECTORY_SECTOR, "/");
        if (listOpenFiles->Find("/") == nullptr) listOpenFiles->Add("/"); // puede que ya este añadido, no lo quiero añadir 2 veces
        directoryList->GetLock(currentThread->myId)->Acquire();
        dir->FetchFrom(root);
        directoryList->GetLock(currentThread->myId)->Release();
        delete root;
        pathaux = &pathaux[1];
    }
    else {
        directoryList->GetLock(currentThread->myId)->Acquire();
        dir->FetchFrom(directoryList->Get(currentThread->myId));
        directoryList->GetLock(currentThread->myId)->Release(); // listlock -> Aquire();
    }
    while(strlen(pathaux) != 0) {
        int i = 0;
        for(; pathaux[i] != '/' && pathaux[i] != '\0'; i++); // recorrer hasta el proximo '/' y guardo en un i cuanto recorri, strncpy y hacer find
        char* pathsiguiente = new char[i+1];
        strncpy(pathsiguiente, pathaux, i);
        pathsiguiente[i] = '\0';
        pathaux = &pathaux[i+1];
        nuevoDirSector = dir->Find(pathsiguiente); 
        if (nuevoDirSector < 0 || (listOpenFiles->Find(pathsiguiente) != nullptr && listOpenFiles->Find(pathsiguiente)->deleted)) {
            delete dir;
            DEBUG('F', "No se encontro el directorio %s\n", pathsiguiente);
            delete [] pathaux2;
            delete [] pathsiguiente;
            //directoryList->GetLock(currentThread->myId)->Release();
            return false;
        }
        
        OpenFile *nuevoDir = new OpenFile(nuevoDirSector, pathsiguiente);
        listOpenFiles->Add(pathsiguiente); // aca
        DEBUG('F', "Nombre del directorio al que se esta entrando: %s\n",pathsiguiente);

        delete dir;
        dir = new Directory(NUM_DIR_ENTRIES);
        // directoryList->GetLock(currentThread->myId)->Acquire();

        Lock* ltemp= directoryList->GetLockFromDir(pathsiguiente);
        if(ltemp) ltemp->Acquire();
        else directoryList->listlock->Acquire();
        dir->FetchFrom(nuevoDir); //si tira error o algo delete y new denuevo
        if(ltemp) ltemp->Release();
        else directoryList->listlock->Release();

        // directoryList->GetLock(currentThread->myId)->Release();
        delete [] pathsiguiente;
        delete nuevoDir;
    }

    if (nuevoDirSector != -1){
        // delete dir; //se nos rompe con esto
        if(nuevoDirSector != DIRECTORY_SECTOR){
            char *absolutepath=FillPath(path);
            listOpenFiles->Add(absolutepath);
            directoryList->Remove(currentThread->myId);
            directoryList->Add(currentThread->myId,nuevoDirSector, absolutepath);
            delete absolutepath;
            
        } else {
            directoryList->Remove(currentThread->myId);
            listOpenFiles->Add("/");
            directoryList->Add(currentThread->myId,DIRECTORY_SECTOR, "/");   
        }
        delete [] pathaux2;
        //directoryList->GetLock(currentThread->myId)->Release();
        return true;
    }
    //directoryList->GetLock(currentThread->myId)->Release();
    return false;
}

bool 
FileSystem::RemoveDir(const char *name)
{
    directoryList->CheckDirectoryUse(currentThread->myId);
    directoryList->GetLock(currentThread->myId)->Acquire();

    Directory *dir = new Directory(NUM_DIR_ENTRIES);
    dir->FetchFrom(directoryList->Get(currentThread->myId));
    int sector = dir->Find(name);
    if (sector == -1) {
        DEBUG('F', "Directorio %s no encontrado.\n", name);
        delete dir;
        directoryList->GetLock(currentThread->myId)->Release();   
        return false;
    }
    //char *absolutepath=FillPath(name);
    OpenFile *targetOf = new OpenFile(sector, name);
    //delete absolutepath;
    listOpenFiles->Add(name);
    Directory *target = new Directory(NUM_DIR_ENTRIES);
    target->FetchFrom(targetOf);
    if (!target->IsEmpty()) {
        DEBUG('F', "Directorio %s no vacio.\n", name);
        delete dir;
        delete targetOf;
        delete target;
        directoryList->GetLock(currentThread->myId)->Release();
        return false;
    }
    delete target;
    delete targetOf;
    delete dir;
    directoryList->GetLock(currentThread->myId)->Release();
    return Remove(name);
}


/// Open a file for reading and writing.
///
/// To open a file:
/// 1. Find the location of the file's header, using the directory.
/// 2. Bring the header into memory.
///
/// * `name` is the text name of the file to be opened.
OpenFile *
FileSystem::Open(const char *name)
{
    directoryList->CheckDirectoryUse(currentThread->myId);
    //directoryList->GetLock(currentThread->myId)->Acquire();

    ASSERT(name != nullptr);

    Directory *dir = new Directory(NUM_DIR_ENTRIES);
    OpenFile  *openFile = nullptr;

    DEBUG('f', "Opening file %s\n", name);

    // directoryList->GetLock(currentThread->myId)->Acquire();
    dir->FetchFrom(directoryList->Get(currentThread->myId));
    // directoryList->GetLock(currentThread->myId)->Release();

    int sector = dir->Find(name);

    char *absolutepath=FillPath(name);
    
    if (sector >= 0 && (listOpenFiles->Find(absolutepath) == nullptr || 
        !listOpenFiles->Find(absolutepath)->deleted)) {
            openFile = new OpenFile(sector, absolutepath);  // `name` was found in directory.
            listOpenFiles->Add(absolutepath);
        }
    delete dir;
    // listOpenFiles->Print();
    delete absolutepath;
    //directoryList->GetLock(currentThread->myId)->Release();
    return openFile;  // Return null if not found.
}

/// Delete a file from the file system.
///
/// This requires:
/// 1. Remove it from the directory.
/// 2. Delete the space for its header.
/// 3. Delete the space for its data blocks.
/// 4. Write changes to directory, bitmap back to disk.
///
/// Return true if the file was deleted, false if the file was not in the
/// file system.
///
/// * `name` is the text name of the file to be removed.
bool
FileSystem::Remove(const char *name)
{
    directoryList->CheckDirectoryUse(currentThread->myId);
    directoryList->GetLock(currentThread->myId)->Acquire();

    ASSERT(name != nullptr);
    
    Directory *dir = new Directory(NUM_DIR_ENTRIES);
    dir->FetchFrom(directoryList->Get(currentThread->myId));

    char *absolutepath=FillPath(name);

    int sector = dir->Find(name);
    if (sector == -1) {
       delete dir;
       directoryList->GetLock(currentThread->myId)->Release();
       return false;  // file not found
    }

    FData *fd = listOpenFiles->Find(absolutepath);

    if(fd != nullptr) {
        DEBUG('F', "marcando %s para removerse\n", absolutepath);
        fd->deleted = true;
        directoryList->GetLock(currentThread->myId)->Release();
        return false;
    }

    DEBUG('F', "removiendo archivo %s de directorio %s\n", absolutepath, directoryList->Get(currentThread->myId)->fileName);

    FileHeader *fileH = new FileHeader;
    fileH->FetchFrom(sector);

    Bitmap *freeMap = new Bitmap(NUM_SECTORS);
    //freeMapLock->Acquire();
    freeMap->FetchFrom(freeMapFile);

    fileH->Deallocate(freeMap);  // Remove data blocks. Desaloca los next tambien.
    freeMap->Clear(sector);      // Remove header block.

    dir->Remove(name);

    freeMap->WriteBack(freeMapFile);  // Flush to disk.
    //freeMapLock->Release();
    dir->WriteBack(directoryList->Get(currentThread->myId));    // Flush to disk.
    delete fileH;
    delete dir;
    delete freeMap;
    directoryList->GetLock(currentThread->myId)->Release();
    return true;
}

/// List all the files in the file system directory.
void
FileSystem::List()
{
    directoryList->GetLock(currentThread->myId)->Acquire();

    Directory *dir = new Directory(NUM_DIR_ENTRIES);
    OpenFile* directoryFile = new OpenFile(DIRECTORY_SECTOR,"/");
    listOpenFiles->Add("/");
    dir->FetchFrom(directoryFile);
    dir->List(0);

    listOpenFiles->Remove("/");
    delete directoryFile;
    delete dir;
    directoryList->GetLock(currentThread->myId)->Release();
}

static bool
AddToShadowBitmap(unsigned sector, Bitmap *map)
{
    ASSERT(map != nullptr);

    if (map->Test(sector)) {
        DEBUG('f', "Sector %u was already marked.\n", sector);
        return false;
    }
    map->Mark(sector);
    DEBUG('f', "Marked sector %u.\n", sector);
    return true;
}

static bool
CheckForError(bool value, const char *message)
{
    if (!value)
        DEBUG('f', message);
    return !value;
}

static bool
CheckSector(unsigned sector, Bitmap *shadowMap)
{
    bool error = false;

    error |= CheckForError(sector < NUM_SECTORS, "Sector number too big.\n");
    error |= CheckForError(AddToShadowBitmap(sector, shadowMap),
                           "Sector number already used.\n");
    return error;
}

static bool
CheckFileHeader(const RawFileHeader *rh, unsigned num, Bitmap *shadowMap)
{
    ASSERT(rh != nullptr);

    bool error = false;

    DEBUG('f', "Checking file header %u.  File size: %u bytes, number of sectors: %u.\n",
          num, rh->numBytes, rh->numSectors);
    // error |= CheckForError(rh->numSectors >= DivRoundUp(rh->numBytes,
       //                                                 SECTOR_SIZE),
         //                  "Sector count not compatible with file size.\n");
    error |= CheckForError(rh->numSectors <= NUM_DIRECT, // cambiamos < a <=
		           "Too many blocks.\n");
    for (unsigned i = 0; i < rh->numSectors; i++) {
        unsigned s = rh->dataSectors[i];
        error |= CheckSector(s, shadowMap);
    }
    return error;
}

static bool
CheckBitmaps(const Bitmap *freeMap, const Bitmap *shadowMap)
{
    bool error = false;
    for (unsigned i = 0; i < NUM_SECTORS; i++) {
        DEBUG('f', "Checking sector %u. Original: %u, shadow: %u.\n",
              i, freeMap->Test(i), shadowMap->Test(i));
        error |= CheckForError(freeMap->Test(i) == shadowMap->Test(i),
                               "Inconsistent bitmap.\n");
    }
    return error;
}

static bool
CheckDirectory(const RawDirectory *rd, Bitmap *shadowMap)
{
    ASSERT(rd != nullptr);
    ASSERT(shadowMap != nullptr);

    bool error = false;
    unsigned nameCount = 0;
    const char *knownNames[NUM_DIR_ENTRIES];

    for (unsigned i = 0; i < NUM_DIR_ENTRIES; i++) {
        DEBUG('f', "Checking direntry: %u.\n", i);
        const DirectoryEntry *e = &rd->table[i];

        if (e->inUse) {
            if (strlen(e->name) > FILE_NAME_MAX_LEN) {
                DEBUG('f', "Filename too long.\n");
                error = true;
            }

            // Check for repeated filenames.
            DEBUG('f', "Checking for repeated names.  Name count: %u.\n",
                  nameCount);
            bool repeated = false;
            for (unsigned j = 0; j < nameCount; j++) {
                DEBUG('f', "Comparing \"%s\" and \"%s\".\n",
                      knownNames[j], e->name);
                if (strcmp(knownNames[j], e->name) == 0) {
                    DEBUG('f', "Repeated filename.\n");
                    repeated = true;
                    error = true;
                }
            }
            if (!repeated) {
                knownNames[nameCount] = e->name;
                DEBUG('f', "Added \"%s\" at %u.\n", e->name, nameCount);
                nameCount++;
            }

            // Check sector.
            error |= CheckSector(e->sector, shadowMap);

            // Check file header.
            FileHeader *h = new FileHeader;
            const RawFileHeader *rh = h->GetRaw();
            h->FetchFrom(e->sector);
            error |= CheckFileHeader(rh, e->sector, shadowMap);

            for(FileHeader* ptr = h->next; ptr != nullptr; ptr = ptr->next) { // Marcamos en el shadowmap los fileheader asociados con next
                error |= CheckSector(ptr->sector, shadowMap);
                const RawFileHeader *rhaux = ptr->GetRaw();
                ptr->FetchFrom(ptr->sector);
                error |= CheckFileHeader(rhaux, ptr->sector, shadowMap);
            }
            delete h;
        }
    }
    return error;
}

bool
FileSystem::Check()
{
    DEBUG('f', "Performing filesystem check\n");
    bool error = false;

    Bitmap *shadowMap = new Bitmap(NUM_SECTORS);
    shadowMap->Mark(FREE_MAP_SECTOR);
    shadowMap->Mark(DIRECTORY_SECTOR);

    DEBUG('f', "Checking bitmap's file header.\n");

    FileHeader *bitH = new FileHeader;
    const RawFileHeader *bitRH = bitH->GetRaw();
    bitH->FetchFrom(FREE_MAP_SECTOR);
    DEBUG('f', "  File size: %u bytes, expected %u bytes.\n"
               "  Number of sectors: %u, expected %u.\n",
          bitRH->numBytes, FREE_MAP_FILE_SIZE,
          bitRH->numSectors, FREE_MAP_FILE_SIZE / SECTOR_SIZE);
    error |= CheckForError(bitRH->numBytes == FREE_MAP_FILE_SIZE,
                           "Bad bitmap header: wrong file size.\n");
    error |= CheckForError(bitRH->numSectors == FREE_MAP_FILE_SIZE / SECTOR_SIZE,
                           "Bad bitmap header: wrong number of sectors.\n");
    error |= CheckFileHeader(bitRH, FREE_MAP_SECTOR, shadowMap);
    delete bitH;

    DEBUG('f', "Checking directory.\n");

    FileHeader *dirH = new FileHeader;
    const RawFileHeader *dirRH = dirH->GetRaw();
    dirH->FetchFrom(DIRECTORY_SECTOR);
    error |= CheckFileHeader(dirRH, DIRECTORY_SECTOR, shadowMap);
    delete dirH;

    Bitmap *freeMap = new Bitmap(NUM_SECTORS);
    freeMap->FetchFrom(freeMapFile);
    Directory *dir = new Directory(NUM_DIR_ENTRIES);
    const RawDirectory *rdir = dir->GetRaw();
    dir->FetchFrom(directoryList->Get(currentThread->myId));
    error |= CheckDirectory(rdir, shadowMap);
    delete dir;

    // The two bitmaps should match.
    DEBUG('f', "Checking bitmap consistency.\n");
    error |= CheckBitmaps(freeMap, shadowMap);
    delete shadowMap;
    delete freeMap;

    DEBUG('f', error ? "Filesystem check failed.\n"
                     : "Filesystem check succeeded.\n");

    return !error;
}

/// Print everything about the file system:
/// * the contents of the bitmap;
/// * the contents of the directory;
/// * for each file in the directory:
///   * the contents of the file header;
///   * the data in the file.
void
FileSystem::Print()
{
    FileHeader *bitH    = new FileHeader;
    FileHeader *dirH    = new FileHeader;
    Bitmap     *freeMap = new Bitmap(NUM_SECTORS);
    Directory  *dir     = new Directory(NUM_DIR_ENTRIES);

    printf("--------------------------------\n");
    bitH->FetchFrom(FREE_MAP_SECTOR);
    bitH->Print("Bitmap");

    printf("--------------------------------\n");
    dirH->FetchFrom(DIRECTORY_SECTOR);
    dirH->Print("Directory");

    printf("--------------------------------\n");
    
    //freeMapLock->Acquire();
    freeMap->FetchFrom(freeMapFile);
    //freeMapLock->Release();
    freeMap->Print();

    printf("--------------------------------\n");
    OpenFile* directoryFile= new OpenFile(DIRECTORY_SECTOR,"/");
    listOpenFiles->Add("/");

    directoryList->GetLock(currentThread->myId)->Acquire();
    dir->FetchFrom(directoryFile);
    directoryList->GetLock(currentThread->myId)->Release();

    dir->Print();
    delete directoryFile;
    printf("--------------------------------\n");

    delete bitH;
    delete dirH;
    delete freeMap;
    delete dir;
}
