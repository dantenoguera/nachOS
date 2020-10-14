/// Entry points into the Nachos kernel from user programs.
///
/// There are two kinds of things that can cause control to transfer back to
/// here from user code:
///
/// * System calls: the user code explicitly requests to call a procedure in
///   the Nachos kernel.  Right now, the only function we support is `Halt`.
///
/// * Exceptions: the user code does something that the CPU cannot handle.
///   For instance, accessing memory that does not exist, arithmetic errors,
///   etc.
///
/// Interrupts (which can also cause control to transfer from user code into
/// the Nachos kernel) are handled elsewhere.
///
/// For now, this only handles the `Halt` system call.  Everything else core-
/// dumps.
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2020 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


#include "transfer.hh"
#include "syscall.h"
#include "filesys/directory_entry.hh"
#include "threads/system.hh"
#include "transfer.hh"
#include "args.hh"
#include <stdio.h>
#include "filesys/raw_file_header.hh"

void RunFile(void* args) {
    /// Initialize user-level CPU registers, before jumping to user code.
    //// void InitRegisters();

    /// Save/restore address space-specific info on a context switch.
    currentThread->space->InitRegisters();
    currentThread->space->RestoreState();
    char **argv = (char**) args;
    if (argv == nullptr) {
        machine->WriteRegister(4, 0);
    }
    else {
        int argc = WriteArgs(argv);
        machine->WriteRegister(4, argc);
    }
    machine->WriteRegister(5, machine->ReadRegister(STACK_REG) + 16);
    machine->Run();
}

static void
IncrementPC()
{
    unsigned pc;

    pc = machine->ReadRegister(PC_REG);
    machine->WriteRegister(PREV_PC_REG, pc);
    pc = machine->ReadRegister(NEXT_PC_REG);
    machine->WriteRegister(PC_REG, pc);
    pc += 4;
    machine->WriteRegister(NEXT_PC_REG, pc);
}

/// Do some default behavior for an unexpected exception.
///
/// NOTE: this function is meant specifically for unexpected exceptions.  If
/// you implement a new behavior for some exception, do not extend this
/// function: assign a new handler instead.
///
/// * `et` is the kind of exception.  The list of possible exceptions is in
///   `machine/exception_type.hh`.
static void
DefaultHandler(ExceptionType et)
{
    int exceptionArg = machine->ReadRegister(2);

    fprintf(stderr, "Unexpected user mode exception: %s, arg %d.\n",
            ExceptionTypeToString(et), exceptionArg);
    ASSERT(false);
}

/// Handle a system call exception.
///
/// * `et` is the kind of exception.  The list of possible exceptions is in
///   `machine/exception_type.hh`.
///
/// The calling convention is the following:
///
/// * system call identifier in `r2`;
/// * 1st argument in `r4`;
/// * 2nd argument in `r5`;
/// * 3rd argument in `r6`;
/// * 4th argument in `r7`;
/// * the result of the system call, if any, must be put back into `r2`.
///
/// And do not forget to increment the program counter before returning. (Or
/// else you will loop making the same system call forever!)
static void
SyscallHandler(ExceptionType _et)
{
    int scid = machine->ReadRegister(2);

    switch (scid) {

        case SC_HALT:
            DEBUG('e', "Shutdown, initiated by user program.\n");
            interrupt->Halt();
            break;

        case SC_CREATE: {
            int filenameAddr = machine->ReadRegister(4);
            if (filenameAddr == 0){
                DEBUG('e', "Error: address to filename string is null.\n");
                machine->WriteRegister(2, -1);
                break;
            }

            char filename[FILE_NAME_MAX_LEN + 1];
            if (!ReadStringFromUser(filenameAddr, filename, sizeof filename)){
                DEBUG('e', "Error: filename string too long (maximum is %u bytes).\n",
                      FILE_NAME_MAX_LEN);
                machine->WriteRegister(2, -1);
                break;
            }

            DEBUG('e', "`Create` requested for file `%s`.\n", filename);
            int created = fileSystem->Create(filename, 0);
            if (!created) {
                machine->WriteRegister(2, -1);
                break;
            }
            machine->WriteRegister(2, 0);
            break;
        }

        case SC_OPEN: {
            int filenameAddr = machine->ReadRegister(4);
            if (filenameAddr == 0){
                DEBUG('e', "Error: address to filename string is null.\n");
                machine->WriteRegister(2, -1);
                break;
            }

            char filename[FILE_NAME_MAX_LEN + 1];
            if (!ReadStringFromUser(filenameAddr, filename, sizeof filename)){
                DEBUG('e', "Error: filename string too long (maximum is %u bytes).\n",
                      FILE_NAME_MAX_LEN);
                machine->WriteRegister(2, -1);
                break;
            }
            
            DEBUG('e', "Open requested for file '%s'.\n",filename);

            OpenFile* archivo = fileSystem->Open(filename);
            if (archivo==nullptr) {
                machine->WriteRegister(2, -1);
                DEBUG('e', "No se encontro el archivo.\n");
                break;
            }

            OpenFileId id = currentThread->openFiles->Add(archivo);
            if (id == -1) {
                machine->WriteRegister(2, -1);
                DEBUG('e', "Error al agregar archivo en openFiles, no hay espacio.\n");
                break;
            };

            machine->WriteRegister(2,id);
            break;
        }

        case SC_CLOSE: {
            int fid = machine->ReadRegister(4);
            DEBUG('e', "`Close` requested for id %u.\n", fid);
            if(!currentThread->openFiles->HasKey(fid)){
                DEBUG('e', "El archivo con el id=%d no esta en la lista de openFiles.\n",fid);
                machine->WriteRegister(2,-1);
                break;
            }
            delete currentThread->openFiles->Remove(fid);
            machine->WriteRegister(2, 0);
            break;
        }

        case SC_WRITE: {
            int strAddr = machine->ReadRegister(4);
            int size = machine->ReadRegister(5);
            int id = machine->ReadRegister(6);

            if(id<=0){
                DEBUG('e', "Error: el id %d es invalido.\n", id);
                machine->WriteRegister(2,-1);
                break;
            }
            
            DEBUG('e', "Write requested for fileId '%d'.\n", id);

            if (strAddr == 0) {
                DEBUG('e', "Error: address to buffer string is null.\n");
                machine->WriteRegister(2, -1);
                break;
            }

            if (size <= 0) {
                DEBUG('e', "Error: size debe ser entero positivo.\n");
                machine->WriteRegister(2, -1);
                break;
            }
                
            char str[size+1];
            ReadStringFromUser(strAddr, str, size);
            if(id==1){
                for (int i = 0; i < size; i++)
                    sconsole->PutChar(str[i]);
                machine->WriteRegister(2, 0);
                break;
            }

            if (!currentThread->openFiles->HasKey(id)){
                DEBUG('e', "El archivo con el id=%d no esta en la lista de openFiles.\n",id);
                machine->WriteRegister(2,-1);
                break;
            }

            OpenFile* archivo = currentThread->openFiles->Get(id);
            archivo->Write(str, size);
            
            machine->WriteRegister(2, 0);
            break;
        }

        case SC_READ: {
            int buffer = machine->ReadRegister(4);
            int size = machine->ReadRegister(5);
            int id = machine->ReadRegister(6);

            DEBUG('e', "Read requested for fileId '%d'.\n", id);
            
        
            if(id<=-1 || id==1){
                DEBUG('e', "Error: el id %d es invalido.\n", id);
                machine->WriteRegister(2,-1);
                break;
            }

            if (buffer == 0) {
                DEBUG('e', "Error: buffer is null.\n");
                machine->WriteRegister(2, -1);
                break;
            }

            if (size <= 0) {
                DEBUG('e', "Error: size debe ser entero positivo.\n");
                machine->WriteRegister(2, -1);
                break;
            }

            if (!currentThread->openFiles->HasKey(id)){
                DEBUG('e', "El archivo con el id=%d no esta en la lista de openFiles.\n",id);
                machine->WriteRegister(2,-1);
                break;
            }
            char str[size+1]; //1
  
            if (id == 0) {
                int i = 0;
                for (; i < size; i++)
                    str[i] = sconsole->GetChar();
                str[i] = '\0';
                WriteStringToUser(str, buffer);
                machine->WriteRegister(2, i);
                break;
            }

            OpenFile* archivo = currentThread->openFiles->Get(id);
            int returnvalue=archivo->Read(str, size);
            WriteStringToUser(str, buffer);
            machine->WriteRegister(2, returnvalue);
            break;
        }
        case SC_EXEC: {
            int filenameAddr = machine->ReadRegister(4);
            int argvAddr = machine->ReadRegister(5);
            int joinable = machine->ReadRegister(6);
            char **argv = nullptr;
            
            if(argvAddr != 0)
                argv = SaveArgs(argvAddr);
            
            if (filenameAddr == 0){
                DEBUG('e', "Error: address to executable string is null.\n");
                machine->WriteRegister(2, -1);
                break;
            }

            char filename[FILE_NAME_MAX_LEN + 1];
            if (!ReadStringFromUser(filenameAddr, filename, sizeof filename)){
                DEBUG('e', "Error: filename string too long (maximum is %u bytes).\n",
                      FILE_NAME_MAX_LEN);
                machine->WriteRegister(2, -1);
                break;
            }
            DEBUG('e', "Open requested for file '%s'.\n",filename);
            
            OpenFile *bin = fileSystem->Open(filename);
            if (bin == nullptr) {
                DEBUG('e', "No se encontro el archivo '%s'.\n", filename);
                machine->WriteRegister(2, -1);
                break;
            }
            Thread *child = new Thread(filename, joinable, currentThread->GetPriority());
            child->space = new AddressSpace(bin, child->myId);
            child->Fork(RunFile, (void*) argv);
            machine->WriteRegister(2, child->myId);
            break;
        }
        case SC_EXIT: {
            int status= machine->ReadRegister(4);
            DEBUG('e', "El thread de id %d sale con estado %d.\n", currentThread->myId, status);
            currentThread->Finish(status);
            break;
        }
        case SC_JOIN: {
            int id = machine->ReadRegister(4);
            if (!spaceIds->HasKey(id)){
                DEBUG('e', "No se encuentra el proceso.\n");
                machine->WriteRegister(2, -1);
                break;
            }
            Thread* t = spaceIds->Get(id);
            machine->WriteRegister(2, t->Join());
            break;
        }
        default:
            fprintf(stderr, "Unexpected system call: id %d.\n", scid);
            ASSERT(false);
    }
    IncrementPC();
}

#ifdef USE_TLB
void
printTLB() {
    static int iter = 0;
    printf("(%d) \n", iter);
    TranslationEntry *tlb = machine->GetMMU()->tlb;
    for (unsigned i = 0; i < TLB_SIZE; i++) {
        printf("tlb[%d] = { virtualPage = %d, physicalPage = %d, valid = %d, use = %d, dirty = %d, readOnly = %d }\n",
                i,
                tlb[i].virtualPage,
                tlb[i].physicalPage,
                tlb[i].valid,
                tlb[i].use,
                tlb[i].dirty,
                tlb[i].readOnly);
    }
    printf("\n");
    iter++;
}
#endif

static void
PageFaultHandler(ExceptionType et) {
#ifdef USE_TLB
    static int i = 0;
    unsigned vaddr = machine->ReadRegister(BAD_VADDR_REG);
    unsigned vpn = vaddr / PAGE_SIZE;
    ASSERT(vpn < currentThread->space->numPages);

    #ifdef DEMAND_LOADING
    currentThread->space->UpdatePageTable();
    if (currentThread->space->pageTable[vpn].physicalPage == (unsigned) -1)
        currentThread->space->LoadPage(vpn);
    #endif

    machine->GetMMU()->tlb[i] = currentThread->space->pageTable[vpn];
    i = (i + 1) % TLB_SIZE;
#endif
}


static void
ReadOnlyExceptionHandler(ExceptionType et)
{ 
    unsigned vaddr = machine->ReadRegister(BAD_VADDR_REG);
    DEBUG('v', "(ReadOnlyExceptionHandler) %s: %u.\n", ExceptionTypeToString(et), vaddr);
    currentThread->Finish(-1);
}


/// By default, only system calls have their own handler.  All other
/// exception types are assigned the default handler.
void
SetExceptionHandlers()
{
    machine->SetHandler(NO_EXCEPTION,            &DefaultHandler);
    machine->SetHandler(SYSCALL_EXCEPTION,       &SyscallHandler);
    machine->SetHandler(PAGE_FAULT_EXCEPTION,    &PageFaultHandler);
    machine->SetHandler(READ_ONLY_EXCEPTION,     &ReadOnlyExceptionHandler);
    machine->SetHandler(BUS_ERROR_EXCEPTION,     &DefaultHandler);
    machine->SetHandler(ADDRESS_ERROR_EXCEPTION, &DefaultHandler);
    machine->SetHandler(OVERFLOW_EXCEPTION,      &DefaultHandler);
    machine->SetHandler(ILLEGAL_INSTR_EXCEPTION, &DefaultHandler);
}
