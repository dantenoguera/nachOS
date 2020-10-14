#include "syscall.h"

int
main(int argc, char* argv[]) 
{
    int o = Open(argv[1]);
    char buff[1];
    for(; Read(buff,1,o); Write(buff,1,CONSOLE_OUTPUT));
    Close(o);
}

