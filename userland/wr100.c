
#include "syscall.h"

int
main()
{
    OpenFileId o = Open("test.txt");
    for (int i = 0; i < 100; i++)
        Write("w aux\n", 6,o);
    Close(o);
}
