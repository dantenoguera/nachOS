#include "syscall.h"

int
main(void)
{
    int child = Exec("matmult", 0, 1);
    int child2 = Exec("matmult", 0, 1);
    Join(child);
    Join(child2);
    return 0;
}
