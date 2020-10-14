#include "file_list.hh"


FData::FData(const char *name_){
    name = new char[strlen(name_) + 1];
    name = strncpy(name,name_,strlen(name_) + 1);
    ASSERT(0 == strcmp(name,name_));
    condLock = new Lock(name);
    canWrite = new Condition(name, condLock);
    canRead = new Condition(name, condLock);
    inst = 1;
    writings = 0;
    readings = 0;
    waitingWriters = 0;
    waitingReaders = 0;
    deleted= false;
    next=nullptr;
}

FData::~FData(){
    delete [] name;
    delete condLock;
    delete canWrite;
    delete canRead;
}

void
FData::Print() {
    printf("name: %s, inst: %d, writings: %d, readings: %d\n", name, inst, writings, readings);
}

// FileList

FileList::FileList()
{
    first = last = nullptr;
}

FileList::~FileList()
{
    for (FData *ptr = first, *i = first; ptr != nullptr; ptr = i){
        i=i->next;
        delete ptr;
    }
}

int
FileList::IsEmpty()
{
    return first == nullptr;
}

FData*
FileList::Find(const char*name)
{
    FData* fd = first;
    for (; fd != nullptr && strncmp(name, fd->name, FILE_NAME_MAX_LEN + 1) != 0; fd = fd->next);
    return fd;
}

void
FileList::Add(const char* name)
{
    if (IsEmpty())
        first = last = new FData(name);
    else {
       FData* fd = Find(name);
       if (fd != nullptr)
           fd->inst++;
       else {
           last->next = new FData(name);
           last = last->next;
       }
    }

    /*int i=0;
    printf("Estoy en Add con nombre=%s\n",name);
    for(FData *ptraux=first;ptraux!=nullptr && i<10;ptraux=ptraux->next,i++) printf("puntero actual: %p -(next)-> %p\n",ptraux,ptraux->next);
    printf("Murio con %d iteraciones\n",i);*/
}


void 
FileList::DeleteFData(const char* name)
{
    DEBUG('F', "borrando FData de %s\n", name);
    FData *ptr = first;
    FData *prev_ptr = nullptr;
    for (; ptr != nullptr;
        prev_ptr = ptr, ptr = ptr->next) {
        if (0==strncmp(name,ptr->name, FILE_NAME_MAX_LEN + 1)) {
            if (prev_ptr) {
                prev_ptr->next = ptr->next;
            }
            if (first == ptr) {
                first = ptr->next;
            }
            if (last == ptr) {
                last = prev_ptr;
            }
            break;
        }
    }
    //Como el archivo esta 
    if(ptr->deleted){
        delete ptr;
        fileSystem->Remove(name);
    }
    else delete ptr;

    /*int i=0;
    printf("Estoy en DeleteFData con nombre=%s\n",name);
    for(FData *ptraux=first;ptraux!=nullptr && i<10;ptraux=ptraux->next,i++) printf("puntero actual: %p -(next)-> %p\n",ptraux,ptraux->next);
    printf("Murio con %d iteraciones\n",i);*/
}


void
FileList::Remove(const char* name)
{
    FData* fd = Find(name);
    if (fd != nullptr) { 
        if (fd->inst > 1)
            fd->inst--;
        else DeleteFData(name);   
    }
}

void
FileList::Print()
{
    for (FData *ptr = first; ptr != nullptr; ptr = ptr->next)
        ptr->Print();
}
