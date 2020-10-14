/// Simple test routines for the file system.
///
/// We implement:
///
/// Copy
///     Copy a file from UNIX to Nachos.
/// Print
///     Cat the contents of a Nachos file.
/// Perftest
///     A stress test for the Nachos file system read and write a really
///     really large file in tiny chunks (will not work on baseline system!)
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2020 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


#include "file_system.hh"
#include "lib/utility.hh"
#include "machine/disk.hh"
#include "machine/statistics.hh"
#include "threads/thread.hh"
#include "threads/system.hh"

#include <stdio.h>
#include <string.h>


static const unsigned TRANSFER_SIZE = 10;  // Make it small, just to be
                                           // difficult.

/// Copy the contents of the UNIX file `from` to the Nachos file `to`.
void
Copy(const char *from, const char *to)
{
    ASSERT(from != nullptr);
    ASSERT(to != nullptr);

    // Open UNIX file.
    FILE *fp = fopen(from, "r");
    if (fp == nullptr) {
        printf("Copy: could not open input file %s\n", from);
        return;
    }

    // Figure out length of UNIX file.
    fseek(fp, 0, 2);
    int fileLength = ftell(fp);
    fseek(fp, 0, 0);

    DEBUG('f', "Copying file %s, size %u, to file %s\n",
          from, fileLength, to);

    // Create a Nachos file of the same length.
    if (!fileSystem->Create(to, fileLength)) {  // Create Nachos file.
        printf("Copy: could not create output file %s\n", to);
        fclose(fp);
        return;
    }

    OpenFile *openFile = fileSystem->Open(to);
    ASSERT(openFile != nullptr);

    // Copy the data in `TRANSFER_SIZE` chunks.
    char *buffer = new char [TRANSFER_SIZE];
    int amountRead;
    while ((amountRead = fread(buffer, sizeof(char),
                               TRANSFER_SIZE, fp)) > 0)
        openFile->Write(buffer, amountRead);
    delete [] buffer;

    // Close the UNIX and the Nachos files.
    delete openFile;
    fclose(fp);
}

/// Print the contents of the Nachos file `name`.
void
Print(const char *name)
{
    ASSERT(name != nullptr);

    OpenFile *openFile = fileSystem->Open(name);
    if (openFile == nullptr) {
        fprintf(stderr, "Print: unable to open file %s\n", name);
        return;
    }

    char *buffer = new char [TRANSFER_SIZE];
    int amountRead;
    while ((amountRead = openFile->Read(buffer, TRANSFER_SIZE)) > 0)
        for (unsigned i = 0; i < (unsigned) amountRead; i++)
            printf("%c", buffer[i]);

    delete [] buffer;
    delete openFile;  // close the Nachos file
}


/// Performance test
///
/// Stress the Nachos file system by creating a large file, writing it out a
/// bit at a time, reading it back a bit at a time, and then deleting the
/// file.
///
/// Implemented as three separate routines:
/// * `FileWrite` -- write the file.
/// * `FileRead` -- read the file.
/// * `PerformanceTest` -- overall control, and print out performance #'s.

static const char FILE_NAME[] = "TestFile";
static const char CONTENTS[] = "1234567890";
static const unsigned CONTENT_SIZE = sizeof CONTENTS - 1;
static const unsigned FILE_SIZE = CONTENT_SIZE * 5000;

static void
FileWrite()
{
    printf("Sequential write of %u byte file, in %u byte chunks\n",
           FILE_SIZE, CONTENT_SIZE);

    if (!fileSystem->Create(FILE_NAME, 0)) {
        fprintf(stderr, "Perf test: cannot create %s\n", FILE_NAME);
        return;
    }

    OpenFile *openFile = fileSystem->Open(FILE_NAME);
    if (openFile == nullptr) {
        fprintf(stderr, "Perf test: unable to open %s\n", FILE_NAME);
        return;
    }

    for (unsigned i = 0; i < FILE_SIZE; i += CONTENT_SIZE) {
        int numBytes = openFile->Write(CONTENTS, CONTENT_SIZE);
        if (numBytes < 10) {
            fprintf(stderr, "Perf test: unable to write %s\n", FILE_NAME);
            break;
        }
        fprintf(stderr, "Se pudo escribir bien: numBytes=%d\n",numBytes);
    }

    delete openFile;
}

static void
FileRead()
{
    printf("Sequential read of %u byte file, in %u byte chunks\n",
           FILE_SIZE, CONTENT_SIZE);

    OpenFile *openFile = fileSystem->Open(FILE_NAME);
    if (openFile == nullptr) {
        fprintf(stderr, "Perf test: unable to open file %s\n", FILE_NAME);
        return;
    }

    char *buffer = new char [CONTENT_SIZE];
    for (unsigned i = 0; i < FILE_SIZE; i += CONTENT_SIZE) {
        int numBytes = openFile->Read(buffer, CONTENT_SIZE);
        if (numBytes < 10 || strncmp(buffer, CONTENTS, CONTENT_SIZE)) {
            printf("Perf test: unable to read %s numBytes = %d\n", FILE_NAME, numBytes);
            break;
        }
        fprintf(stderr, "Se pudo leer bien: numBytes=%d\n",numBytes);
    }

    delete [] buffer;
    delete openFile;
}

void
PerformanceTest()
{
    printf("Starting file system performance test:\n");
    stats->Print();
    FileWrite();
    FileRead();
    if (!fileSystem->Remove(FILE_NAME)) {
        printf("Perf test: unable to remove %s\n", FILE_NAME);
        return;
    }
    stats->Print();
}


void 
P(void* name) {
    printf("Ejecutando proceso hijo\n");
    fileSystem->ChangeDir("dir2");  
    fileSystem->Print(); // si descomento esto funciona
    ASSERT(fileSystem->Create("hijo", 0)); // !!!!, imprimir el directorio y bitmap

    OpenFile *o = fileSystem->Open("hijo");
    ASSERT(o != nullptr);
    o->Write("Escribo desde hijo\n", 19);
    /*
    for(int i=0;i<30;i++){
        printf("(hijo) i: %d \n", i);
        listOpenFiles->Print();
        o->Write("Escribo desde hijo\n",20);
    }
    */
    delete o;
    printf("Terminando proceso hijo\n");
}


void
Tests()
{
    printf("Testeando file system de Nachos\n");
    fileSystem->CreateDir("dir1");
    fileSystem->CreateDir("dir2");

    //fileSystem->ChangeDir("dir2");  
    //ASSERT(fileSystem->Create("hijo", 0));

    //fileSystem->ChangeDir("/dir2"); 
    //ASSERT(fileSystem->Create("padre", 0));
    // fileSystem->List();

    Thread *newThread = new Thread("Hijo", true, 0);
    newThread->Fork(P, (void *) "Hijo");

    fileSystem->ChangeDir("dir2"); 
    ASSERT(fileSystem->Create("padre", 0));

    OpenFile *t = fileSystem->Open("padre");
    ASSERT(t != nullptr);
    t->Write("Escribo desde padre\n",20);
    //for(int i=0;i<30;i++){
    //    printf("(padre) i: %d \n", i);
    //    listOpenFiles->Print();
    //    o->Write("Escribo desde padre\n",20);
    //}
    
    newThread->Join();
    delete t;
    fileSystem->List();
}


/*
void
Tests()
{
    printf("Testeando file system de Nachos\n");
    ASSERT(fileSystem->CreateDir("dir1"));
    ASSERT(fileSystem->CreateDir("dir2"));
    
    ASSERT(fileSystem->ChangeDir("dir2"));

    ASSERT(fileSystem->Create("hijo", 0));
    ASSERT(fileSystem->Create("padre", 0));

    OpenFile *l = fileSystem->Open("hijo");
    ASSERT(l != nullptr);
    l->Write("Escribo hijo\n", 13);

    delete l;

    OpenFile *o = fileSystem->Open("padre");
    ASSERT(o != nullptr);
    o->Write("Escribo padre\n",14);
    

    delete o;
    fileSystem->List();
}
*/
/*
void
Tests()
{
    ASSERT(fileSystem->CreateDir("dir"));
    ASSERT(fileSystem->ChangeDir("dir"));
    ASSERT(fileSystem->Create("archuivo1",0));
    ASSERT(fileSystem->Create("archuivo2",0));
    ASSERT(fileSystem->Create("archuivo3",0));
    ASSERT(fileSystem->Create("archuivo4",0));
    ASSERT(fileSystem->Create("archuivo5",0));
    ASSERT(fileSystem->Create("archuivo6",0));
    ASSERT(fileSystem->Create("archuivo7",0));
    ASSERT(fileSystem->Create("archuivo8",0));
    ASSERT(fileSystem->Create("archuivo10",0));
    ASSERT(fileSystem->Create("archuivo11",0));
    ASSERT(fileSystem->Create("archuivo12",0));
    ASSERT(fileSystem->Create("archuivo13",0));
    ASSERT(fileSystem->Create("archuivo14",0));
    ASSERT(fileSystem->Create("archuivo15",0));
}*/

// Secuencial con 1 anda
// Con 3000 anda todo
// Concurrencia con 1 falla

/*
^Cdante@sara:~/garcia-navall-noguera/trunk/practica5/code/filesys$ ./nachos -pr archivo
contenido 2
contenido 2
contenido 2

^Cdante@sara:~/garcia-navall-noguera/trunk/practica5/code/filesys$ ./nachos -pr archivo
contenido 1
contenido 1

^Cdante@sara:~/garcia-navall-noguera/trunk/practica5/code/filesys$ ./nachos -pr archivo
contenido 2
contenido 2
contenido 2
contenido 2
*/
