#include "syscall.h"

void Sleep(int x) {
    for (int i = 0; i < 100000 * x; i++);
}

int main(int argc, char ** argv) {
    Create("archivo");
    OpenFileId id = Open("archivo");

    if (argc == 1) {
        Write("Ejecutando thread 1\n", 20, CONSOLE_OUTPUT);

        Exec(argv[0], 0, 0);

        for (int i = 0; i < 6; i++) {
            Sleep(4);
            Write("contenido 1\n", 13, id);
        }

        Write("Finalizando thread 1\n", 21, CONSOLE_OUTPUT);
    } else {
        Write("Ejecutando thread 2\n", 20, CONSOLE_OUTPUT);

        for (int i = 0; i < 6; i++) {
            Sleep(2);
            Write("contenido 2\n", 13, id);
            //Remove("archivo");
        }

        Write("Finalizando thread 2\n", 21, CONSOLE_OUTPUT);
    }

    Close(id);
    return 3;
}

/*
Directory entry:
    name: archivo
    sector: 27
File header:
    size: 78 bytes
    link count: 0
    raw.numSectors: 1
    block indexes: 29 
    contents of block 29:
contenido 1\A\0contenido 1\A\0contenido 1\A\0contenido 1\A\0contenido 1\A\0contenido 1\A\0
*/