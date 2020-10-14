#include "syscall.h"

int
main(int argc, char* argv[])
{
    int o=Open(argv[1]);
    if(o==-1){
        Write("Error al abrir el archivo\n",26,CONSOLE_OUTPUT);
        return -1;
    }
    Create(argv[2]);
    int d=Open(argv[2]);
    for(char str[1];Read(str,1,o);Write(str,1,d));
    Close(o);
    Close(d);
    return 0;
}