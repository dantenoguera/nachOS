/// Routines to manage address spaces (memory for executing user programs).
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2020 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


#include "address_space.hh"
#include "threads/system.hh"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define MIN(x, y) (((x) < (y)) ? (x) : (y))
/// First, set up the translation from program memory to physical memory.
/// For now, this is really simple (1:1), since we are only uniprogramming,
/// and we have a single unsegmented page table.

void
AddressSpace::Print() const
{
    for (unsigned i = 0; i < numPages; i++) {
        printf("pageTable[%d] = { virtualPage = %d, physicalPage = %d, valid = %d, use = %d, dirty = %d, readOnly = %d }\n",
                i,
                pageTable[i].virtualPage,
                pageTable[i].physicalPage,
                pageTable[i].valid,
                pageTable[i].use,
                pageTable[i].dirty,
                pageTable[i].readOnly);
    }
    printf("\n");
}
AddressSpace::AddressSpace(OpenFile *executable_file, int id)
{
    ASSERT(executable_file != nullptr);

    Executable exe (executable_file);
    ASSERT(exe.CheckMagic());


    unsigned size = exe.GetSize() + USER_STACK_SIZE;
    numPages = DivRoundUp(size, PAGE_SIZE);
    DEBUG('e', "numPages = %d, NUM_PHYS_PAGES = %d\n", numPages, NUM_PHYS_PAGES);
    size = numPages * PAGE_SIZE;

#ifdef DEMAND_LOADING
    exeFile = executable_file;
    asid = id;

    snprintf(swap, 16, "SWAP.%d", asid);
    fileSystem->Create(swap, numPages * PAGE_SIZE);
    //fileSystem->Create(nombre, numPages * PAGE_SIZE);
    // swap = fileSystem->Open(nombre);
#endif

#ifndef DEMAND_LOADING
    ASSERT(numPages <= bmp->CountClear());
#endif

    // Check we are not trying to run anything too big -- at least until we
    // have virtual memory.
    DEBUG('a', "Initializing address space, num pages %u, size %u\n",
            numPages, size);

    // First, set up the translation.

    pageTable = new TranslationEntry[numPages];
    for (unsigned i = 0; i < numPages; i++) {
        pageTable[i].virtualPage  = i;
#ifdef DEMAND_LOADING
        pageTable[i].physicalPage = -1; // -1;
#else
        pageTable[i].physicalPage = bmp->Find();
#endif
        pageTable[i].valid        = true;
        pageTable[i].use          = false;
        pageTable[i].dirty        = false;
        pageTable[i].readOnly     = false;
        // If the code segment was entirely on a separate page, we could
        // set its pages to be read-only.
    }

#ifndef DEMAND_LOADING
    char *mainMemory = machine->GetMMU()->mainMemory;

    // Zero out the entire address space, to zero the unitialized data
    // segment and the stack segment.
    // void * memset ( void * ptr, int value, size_t num );

    //Initialization
    for(unsigned i=0; i < numPages; i++) memset(mainMemory+pageTable[i].physicalPage*PAGE_SIZE, 0, PAGE_SIZE);

    // Then, copy in the code and data segments into memory.
    uint32_t codeSize = exe.GetCodeSize();
    if (codeSize > 0) {
        uint32_t virtualAddr = exe.GetCodeAddr();
        DEBUG('z', "Initializing code segment, at 0x%X, size %u\n", virtualAddr, codeSize);
        uint32_t virtualPage = virtualAddr/PAGE_SIZE,
                 offset = virtualAddr%PAGE_SIZE,
                 codeLeftToRead = codeSize,
                 sizeToRead = (PAGE_SIZE-offset>codeLeftToRead) ? codeLeftToRead : PAGE_SIZE-offset;

        exe.ReadCodeBlock(&mainMemory[(pageTable[virtualPage].physicalPage*PAGE_SIZE)+offset], sizeToRead, 0);
        virtualPage++;
        codeLeftToRead = codeLeftToRead-sizeToRead;
        for(;codeLeftToRead > 0; virtualPage++){
            sizeToRead = (PAGE_SIZE>codeLeftToRead) ? codeLeftToRead : PAGE_SIZE;
            exe.ReadCodeBlock(&mainMemory[pageTable[virtualPage].physicalPage*PAGE_SIZE], sizeToRead, codeSize-codeLeftToRead);
            codeLeftToRead = codeLeftToRead-sizeToRead;
        }
    }

    uint32_t initDataSize = exe.GetInitDataSize();
    if (initDataSize > 0) {
        uint32_t virtualAddr = exe.GetInitDataAddr();
        DEBUG('z', "Initializing data segment, at 0x%X, size %u\n", virtualAddr, initDataSize);

        uint32_t virtualPage = virtualAddr/PAGE_SIZE,
                 offset = virtualAddr%PAGE_SIZE,
                 currentIDataSize = initDataSize,
                 sizeToRead = (PAGE_SIZE-offset>currentIDataSize) ? currentIDataSize : PAGE_SIZE-offset;

        exe.ReadDataBlock(&mainMemory[(pageTable[virtualPage].physicalPage*PAGE_SIZE)+offset], sizeToRead, 0);
        virtualPage++;
        currentIDataSize = currentIDataSize-sizeToRead;
        for(;currentIDataSize > 0; virtualPage++){
            sizeToRead = (PAGE_SIZE>currentIDataSize) ? currentIDataSize : PAGE_SIZE;
            exe.ReadDataBlock(&mainMemory[pageTable[virtualPage].physicalPage*PAGE_SIZE], sizeToRead, initDataSize-currentIDataSize);
            currentIDataSize = currentIDataSize-sizeToRead;
        }
    }
#endif
}

/// Deallocate an address space.
///
/// Nothing for now!
AddressSpace::~AddressSpace()
{
    for (unsigned i = 0; i < numPages; i++)
        if (pageTable[i].physicalPage!= (unsigned) -1) bmp->Clear(pageTable[i].physicalPage);//Chequear esto
    delete [] pageTable;

#ifdef DEMAND_LOADING
#endif
}

/// Set the initial values for the user-level register set.
///
/// We write these directly into the “machine” registers, so that we can
/// immediately jump to user code.  Note that these will be saved/restored
/// into the `currentThread->userRegisters` when this thread is context
/// switched out.
void
AddressSpace::InitRegisters()
{
    for (unsigned i = 0; i < NUM_TOTAL_REGS; i++)
        machine->WriteRegister(i, 0);

    // Initial program counter -- must be location of `Start`.
    machine->WriteRegister(PC_REG, 0);

    // Need to also tell MIPS where next instruction is, because of branch
    // delay possibility.
    machine->WriteRegister(NEXT_PC_REG, 4);

    // Set the stack register to the end of the address space, where we
    // allocated the stack; but subtract off a bit, to make sure we do not
    // accidentally reference off the end!
    machine->WriteRegister(STACK_REG, numPages * PAGE_SIZE - 16);
    DEBUG('a', "Initializing stack register to %u\n",
          numPages * PAGE_SIZE - 16);
}

/// On a context switch, save any machine state, specific to this address
/// space, that needs saving.
///
/// For now, nothing!
void
AddressSpace::SaveState()
{
#ifdef DEMAND_LOADING
    for (unsigned i = 0; i < TLB_SIZE; i++) {
        TranslationEntry e = machine->GetMMU()->tlb[i];
        if (e.valid) {
            pageTable[e.virtualPage].use = e.use;
            pageTable[e.virtualPage].dirty = e.dirty;
        }
    }
#endif
}

/// On a context switch, restore the machine state so that this address space
/// can run.
///
/// For now, tell the machine where to find the page table.
void
AddressSpace::RestoreState()
{
#ifndef USE_TLB
    machine->GetMMU()->pageTable     = pageTable;
    machine->GetMMU()->pageTableSize = numPages;
#else
    for(unsigned i = 0; i < TLB_SIZE; i++)
        machine->GetMMU()->tlb[i].valid = false;
#endif
}

#ifdef DEMAND_LOADING
uint32_t
LoadFromCode(Executable exe, uint32_t pageAddr, uint32_t frameAddr) {
    uint32_t codeSize = exe.GetCodeSize();
    uint32_t codeStart = exe.GetCodeAddr();
    uint32_t codeEnd = codeStart + codeSize;
    ASSERT(codeSize != 0);
    if (pageAddr >= codeEnd)
        return 0;

    uint32_t sizeToRead = MIN(PAGE_SIZE, codeEnd - pageAddr);
    char *mainMemory = machine->GetMMU()->mainMemory;

    exe.ReadCodeBlock(&mainMemory[frameAddr], sizeToRead, pageAddr - codeStart);

    return sizeToRead;
}

uint32_t
LoadFromInitData(Executable exe, uint32_t pageAddr, uint32_t frameAddr, uint32_t alreadyRead) {
    uint32_t initDataSize = exe.GetInitDataSize();
    uint32_t initDataStart = exe.GetInitDataAddr();
    uint32_t initDataEnd =  initDataStart + initDataSize; 

    if (alreadyRead == PAGE_SIZE || initDataSize == 0 || pageAddr + alreadyRead >= initDataEnd)// primera condicion para ver si leiste toda la pagina y no queda nada por leer
        return alreadyRead;

    uint32_t sizeToRead; 
    char *mainMemory = machine->GetMMU()->mainMemory;

    if (alreadyRead == 0) {
        sizeToRead = MIN(PAGE_SIZE, initDataEnd - pageAddr);
        exe.ReadDataBlock(&mainMemory[frameAddr], sizeToRead, pageAddr - initDataStart);
    }
    else {
        sizeToRead = MIN(PAGE_SIZE - alreadyRead, initDataSize);
        exe.ReadDataBlock(&mainMemory[frameAddr + alreadyRead], sizeToRead, 0);
    }

    return alreadyRead + sizeToRead;
}

uint32_t
LoadRest(Executable exe, uint32_t frameAddr, uint32_t alreadyRead) {

    if (alreadyRead == PAGE_SIZE)
        return alreadyRead;

    char *mainMemory = machine->GetMMU()->mainMemory;
    uint32_t sizeToWrite = PAGE_SIZE - alreadyRead;
    memset(&mainMemory[frameAddr + alreadyRead], 0, sizeToWrite);

    return alreadyRead + sizeToWrite;
}

void
UpdateTLB(unsigned fn)
{
    for (unsigned i = 0; i < TLB_SIZE; i++)
        if (machine->GetMMU()->tlb[i].valid && fn == machine->GetMMU()->tlb[i].physicalPage)
            machine->GetMMU()->tlb[i].valid = false;
}

void
AddressSpace::UpdatePageTable() {
    for (unsigned i = 0; i < TLB_SIZE; i++) {
        TranslationEntry e = machine->GetMMU()->tlb[i];
        if (e.valid) {
            pageTable[e.virtualPage].use = e.use;
            pageTable[e.virtualPage].dirty = e.dirty;
        }
    }
}

void
printCoremap()
{
    for (unsigned i = 0; i < NUM_PHYS_PAGES; i++)
        printf("coremap[%d] = (%d, %d)\n", i, coremap[i].fst->asid, coremap[i].snd);
}

int
AddressSpace::Fifo() {
    return victim++ % NUM_PHYS_PAGES;
}

#ifdef LRU

int AddressSpace::Lru() {
    int i = victim;
    do {
        if (!coremap[i].fst->pageTable[coremap[i].snd].use)
            break;
        i = (i + 1) % NUM_PHYS_PAGES;
    } while (i != victim);
    if (i == victim) i = rand() % NUM_PHYS_PAGES;
    victim = (i + 1) % NUM_PHYS_PAGES;
    return i;
}


/*
Segunda oportunidad mejorado (NO funciona)
int
AddressSpace::Lru() {
    // printCoremap();
    int i = victim; // interpretar victim como next victim
    Pair<AddressSpace*, int> par;
    do {
        par = coremap[i];
        if (!par.fst->pageTable[par.snd].use  && !par.fst->pageTable[par.snd].dirty) {
            printf("Rompe aca, i: %d, par.snd: %d\n", i, par.snd);
            break;
        }
        i = (i + 1) % NUM_PHYS_PAGES;
    } while (i != victim);
    if (i == victim) {
        do {
            par = coremap[i];
            par.fst->pageTable[par.snd].use = false;  // <--- Cambiado
            if (!par.fst->pageTable[par.snd].dirty)
                break;
            i = (i + 1) % NUM_PHYS_PAGES;
        } while (i != victim);
    }
    victim = (i + 1) % NUM_PHYS_PAGES;
    printf("i:%d, victim:%d \n", i, victim);
    return i;
}
*/
#endif

void
AddressSpace::LoadFromSwap(int vpn, int frameAddr) {
    OpenFile* f = fileSystem->Open(swap);
    char *mainMemory = machine->GetMMU()->mainMemory;
    f->ReadAt(&mainMemory[frameAddr], PAGE_SIZE, vpn * PAGE_SIZE);
    delete f;
}

void
AddressSpace::LoadFromExecutable(int vpn, int frameAddr) {
    Executable exe (exeFile);
    ASSERT(exe.CheckMagic());
    uint32_t alreadyRead = 0;
    uint32_t pageAddr = vpn * PAGE_SIZE;

    DEBUG('v', "codeAddr: %u, codeSize: %u.\ninitDataAddr: %u, initDataSize: %u.\nvpn: %d\n", exe.GetCodeAddr(),
            exe.GetCodeSize(), exe.GetInitDataAddr(), exe.GetInitDataSize(), vpn);

    alreadyRead = LoadFromCode(exe, pageAddr, frameAddr);
    alreadyRead = LoadFromInitData(exe, pageAddr, frameAddr, alreadyRead);
    alreadyRead = LoadRest(exe, frameAddr, alreadyRead);
    ASSERT(alreadyRead == PAGE_SIZE);
}

void
AddressSpace::ToSwap(int fn, unsigned oldVpn) {
    OpenFile* f = fileSystem->Open(swap);
    char *mainMemory = machine->GetMMU()->mainMemory;
    f->WriteAt(&mainMemory[fn * PAGE_SIZE], PAGE_SIZE, oldVpn * PAGE_SIZE);
    delete f;
}

void
AddressSpace::LoadPage(unsigned vpn) {
    int fn = bmp->Find();
    if (fn == -1) {
#ifdef LRU
        bmp->Clear(Lru());
#else
        bmp->Clear(Fifo());
#endif
        fn = bmp->Find();
        Pair<AddressSpace*, int> par = coremap[fn];
        if (par.fst->pageTable[par.snd].dirty)
            par.fst->ToSwap(fn, par.snd);

        par.fst->pageTable[par.snd].physicalPage = -1;
        UpdateTLB(fn);
    }

    if (pageTable[vpn].dirty)
        LoadFromSwap(vpn, fn * PAGE_SIZE);
    else
        LoadFromExecutable(vpn, fn * PAGE_SIZE);

    pageTable[vpn].physicalPage = fn;
    // Print();
    coremap[fn] = Pair<AddressSpace*,int>(this, vpn);
}

#endif
