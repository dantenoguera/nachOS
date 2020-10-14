
#ifndef NACHOS_USERPROG_SYNCH_CONSOLE__HH
#define NACHOS_USERPROG_SYNCH_CONSOLE__HH

#include "machine/console.hh"
#include "threads/synch.hh"

class SynchConsole {
public:
    SynchConsole();

    ~SynchConsole();

    /// Write `ch` to the console display, and return immediately.
    /// `writeHandler` is called when the I/O completes.
    void PutChar(char ch);

    /// Poll the console input.  If a char is available, return it.
    /// Otherwise, return EOF.  `readHandler` is called whenever there is a
    /// char to be gotten.
    char GetChar();

    void ReadAvail();

    void WriteDone();

private:
    Console* console;
    Semaphore *readAvail;
    Semaphore *writeDone;
    Lock* write;
    Lock* read;
};

#endif
