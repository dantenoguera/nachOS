#ifndef NACHOS_FILESYS_DIRECTORY_LIST__HH 
#define NACHOS_FILESYS_DIRECTORY_LIST__HH

#include "open_file.hh"


class Lock;
class DNode{
public:
    DNode(int id, OpenFile* dir);
    ~DNode();
    bool Has(int id);
    void Append(int id);
    bool IsEmpty();
    void Remove(int id);
    void Print();
    OpenFile* directoryFile;
    DNode *next;
    int *threads;//para igualar con el myId del proceso
    int size;
    Lock *dirlock;
};


class DirectoryList {
public:
    DirectoryList();
    ~DirectoryList();
    OpenFile* Get(int id);
    Lock* GetLock(int id);
    Lock* GetLockFromDir(char* name);
    void Add(int id, unsigned sector, const char *name);
    void Remove(int id);
    void Print();
    //void Replace(int id, unsigned sector, const char *name);
    void CheckDirectoryUse(int id);
    Lock *listlock;
private:
    DNode *first;
};

#endif