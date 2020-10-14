#include "directory_list.hh"
#include <malloc.h>
#include <string.h>
#include "threads/synch.hh"
#include "threads/system.hh"

DNode::DNode(int id, OpenFile* dir){
    ASSERT(dir!=nullptr);
    // threads=new List<int*>;
    // threads->Append(&id);
    threads = (int *) malloc(sizeof(int));
    threads[0] = id;
    size = 1;
    directoryFile=dir;
    dirlock = new Lock(dir->fileName);
    next=nullptr;
}

DNode::~DNode(){
    delete directoryFile;
    delete dirlock;
    free(threads);
}

bool 
DNode::Has(int id) {
    for(int i = 0; i < size; i++)
        if(threads[i] == id) return true;
    return false;
}

void
DNode::Append(int id) {
    threads = (int *) realloc(threads, ++size * sizeof(int));
    threads[size - 1] = id;
}

void
DNode::Remove(int id) {
    int * newThreads = (int *) malloc((size - 1) * sizeof(int));
    for (int i = 0, j = 0; i < size; i++) {
        if (threads[i] == id) continue;
        newThreads[j] = threads[i];
        j++;
    }
    free(threads);
    threads = newThreads;
    size--;
}

bool
DNode::IsEmpty() {
    return size == 0;
}

void
DNode::Print() {
    printf("Directorio: %s\n Threads: ", directoryFile->fileName);
    for(int i=0;i<size;i++) printf("%d ",threads[i]);
    printf("\n");
}

DirectoryList::DirectoryList(){
    first=nullptr;
    listlock = new Lock("directorios");
}


DirectoryList::~DirectoryList(){
    for (DNode *ptr = first, *i = first; ptr != nullptr; ptr = i) {
        i = i->next;
        delete ptr;
    }
    delete listlock;
 }

OpenFile* DirectoryList::Get(int id){
    // printf("Get: currentThread %d \n", currentThread->myId);
    //listlock->Acquire();
    DNode* ptr=first;
    for(;ptr!=nullptr;ptr=ptr->next){
        if(ptr->Has(id)) {
            //listlock->Release();
            return ptr->directoryFile; //revisar si podemos trabajar con punteros, si el puntero del currentThread->myId cambia.
        }
    }
    //listlock->Release();
    return nullptr;
}

void DirectoryList::Add(int id, unsigned sector, const char *name) {
    // printf("Add: currentThread %d \n", currentThread->myId);
    listlock->Acquire();
    if(first==nullptr){
        OpenFile* dir = new OpenFile(sector,name);
        DNode* nuevoNodo = new DNode(id, dir);
        first=nuevoNodo;
        listlock->Release();
        return;
    }
    DNode* ptr=first;
    DNode* prv_ptr=nullptr;
    for(;ptr!=nullptr;prv_ptr=ptr ,ptr=ptr->next){
        ASSERT(!ptr->Has(id));
        // printf("%s==%s? -> %d\n",ptr->directoryFile->fileName, name, strcmp(ptr->directoryFile->fileName,name));
        if(strcmp(ptr->directoryFile->fileName,name)==0){
            ptr->Append(id);
            listlock->Release();
            return;
        }
    }
    //Si llego el directorio no esta abierto entonces
    OpenFile* dir= new OpenFile(sector,name);
    DNode* nuevoNodo= new DNode(id, dir);
    prv_ptr->next=nuevoNodo;
    listlock->Release();
}

void DirectoryList::Remove(int id){
    //printf("Remove: currentThread %d \n", currentThread->myId);
    listlock->Acquire();
    DNode *ptr = first;
    DNode *prev_ptr = nullptr;
    for (; ptr != nullptr; prev_ptr = ptr, ptr = ptr->next){
        if (ptr->Has(id)){
            ptr->Remove(id);
            if(ptr->IsEmpty()){
                if (prev_ptr) {
                    prev_ptr->next = ptr->next;
                }
                if (first == ptr) {
                    first = ptr->next;
                }
                delete ptr;
            }
            listlock->Release();
            return;
        }
    }
    ASSERT(false);
}

Lock* DirectoryList::GetLockFromDir(char* name){
    //listlock->Acquire();
    if(first==nullptr){
        return nullptr;
    }
    DNode* ptr=first;
    for(;ptr->next!=nullptr;ptr=ptr->next){
        if(strcmp(ptr->directoryFile->fileName,name)==0){
            //listlock->Release();
            return ptr->dirlock;
        }
    }
    //listlock->Release();
    return nullptr;
}

/*
void DirectoryList::Replace(int id, unsigned sector, const char *name){

    listlock->Acquire();
    DNode *ptr = first;
    DNode *prev_ptr = nullptr;
    for (; ptr != nullptr; prev_ptr = ptr, ptr = ptr->next){
        if (ptr->Has(id)){
            ptr->Remove(id);

            if(ptr->IsEmpty()){//No queda thread para guardarle el lock
                if (prev_ptr) {
                    prev_ptr->next = ptr->next;
                }
                if (first == ptr) {
                    first = ptr->next;
                }
                Lock *ltemp=ptr->dirlock;
                ptr->dirlock=nullptr;
                delete ptr;

                writeback dir
                release
                remove
                add

                listlock->Release();
                return;
            }

            if(ptr->IsEmpty()){
                if (prev_ptr) {
                    prev_ptr->next = ptr->next;
                }
                if (first == ptr) {
                    first = ptr->next;
                }
                delete ptr;
                listlock->Release();
                return;
            }
        }
    }

}*/

void DirectoryList::Print(){
    // listlock->Acquire();
    for(DNode* ptr=first;ptr!=nullptr;ptr=ptr->next){
        ptr->Print();//Puede que se rompa
    }
    // listlock->Release();
}

void DirectoryList::CheckDirectoryUse(int id){
    //printf("CheckDirectoryUse: currentThread %d \n", currentThread->myId);
    listlock->Acquire();
    DNode* ptr=first;
    //DNode* prv_ptr=nullptr;
    //for(;ptr!=nullptr;prv_ptr=ptr, ptr=ptr->next)
    for(;ptr!=nullptr;ptr=ptr->next)
        if(ptr->Has(id)) {
            listlock->Release();
            return;
        }
    listlock->Release();
    //OpenFile* directoryFile = new OpenFile(1,"/"); // 1 = DIRECTORY_SECTOR
    //DNode* nuevoNodo=new DNode(id, directoryFile);
    //prv_ptr->next= nuevoNodo;
    Add(id, 1, "/");// 1 = DIRECTORY_SECTOR
    listOpenFiles->Add("/");
    
}

Lock* DirectoryList::GetLock(int id){
    //listlock->Acquire();
    DNode* ptr=first;
    for(;ptr!=nullptr;ptr=ptr->next){
        if(ptr->Has(id)) {
            // listlock->Release();
            return ptr->dirlock; //revisar si podemos trabajar con punteros, si el puntero del currentThread->myId cambia.
        }
    }
    //listlock->Release();
    return nullptr;
}
