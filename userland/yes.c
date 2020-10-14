#include "syscall.h"

int
main(void)
{
    while (1)
        Write("y\n", 2, CONSOLE_OUTPUT);
}
