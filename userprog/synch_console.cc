#include "synch_console.hh"
#include <stdio.h>

static void
ReadAvailDummy(void *args) //Necesaria para utilizarse en el constructor de SynchConsole
{
    SynchConsole *c = (SynchConsole*) args;
    c->ReadAvail();
}

static void
WriteDoneDummy(void *args)//Necesaria para utilizarse en el constructor de SynchConsole
{
    SynchConsole *c = (SynchConsole*) args;
    c->WriteDone();
}

SynchConsole::SynchConsole()
{
    readAvail = new Semaphore("read avail", 0);
    writeDone = new Semaphore("write done", 0);
    write = new Lock("write");
    read = new Lock("read");
    console  = new Console(nullptr, nullptr, ReadAvailDummy, WriteDoneDummy, this);
}

SynchConsole::~SynchConsole()
{
    delete readAvail;
    delete writeDone;
    delete write;
    delete read;
    delete console;
}

void
SynchConsole::PutChar(char ch)
{
    write->Acquire(); 
    console->PutChar(ch);
    writeDone->P();
    write->Release();
}

char
SynchConsole::GetChar()
{
    read->Acquire(); 
    readAvail->P();
    char ch = console->GetChar();
    read->Release();
    return ch;
}

void
SynchConsole::ReadAvail()
{
    readAvail->V();
}

void
SynchConsole::WriteDone()
{
    writeDone->V();
}
