#include "syscall.h"

int
main(void)
{
    char c;
    Read(&c, 1, CONSOLE_INPUT);
}
