#ifndef NACHOS_FILESYS_FILE_LIST__HH 
#define NACHOS_FILESYS_FILE_LIST__HH

#include "threads/synch.hh"
#include "directory_entry.hh"
#include "threads/system.hh"
#include <string.h>

class FData {
public:

    FData(const char *name_);

    ~FData();

    void Print();

    char *name;

    int inst;

    Lock *condLock;

    Condition *canWrite;  

    Condition *canRead;  

    int writings;
    int readings;
    int waitingWriters;
    int waitingReaders;

    bool deleted;


    FData* next;
};

class FileList {
public:

    FileList();

    ~FileList();

    int IsEmpty();

    FData* Find(const char *name);

    // si el archivo  ya esta incrementa inst correspondiente
    void Add(const char *name);

    void DeleteFData(const char* name);
    
    // si quedan instancias abiertas decrementa inst correspondiente
    void Remove(const char *name);

    void Print();

    FData *first, *last;
};

#endif
