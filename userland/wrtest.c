/// Simple program to test whether running a user program works.
///
/// Just do a “syscall” that shuts down the OS.
///
/// NOTE: for some reason, user programs with global data structures
/// sometimes have not worked in the Nachos environment.  So be careful out
/// there!  One option is to allocate data structures as automatics within a
/// procedure, but if you do this, you have to be careful to allocate a big
/// enough stack to hold the automatics!


#include "syscall.h"

int
main()
{
    Create("test.txt");
    OpenFileId o = Open("test.txt");
    int pid = Exec("wr100", 0, 1);
    for (int i = 0; i < 10; i++)
        Write("w main\n", 7,o);
    Close(o);
    Join(pid);
    Halt();
}