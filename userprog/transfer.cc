/// Copyright (c) 2019-2020 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


#include "transfer.hh"
#include "lib/utility.hh"
#include "threads/system.hh"


void ReadBufferFromUser(int userAddress, char *outBuffer,
                        unsigned byteCount)
{  
    ASSERT(userAddress != 0);
    ASSERT(outBuffer != nullptr);
    ASSERT(byteCount != 0);

    unsigned count = 0;
    do {
        int temp;
        count++;

        //ASSERT(machine->ReadMem(userAddress++, 1, &temp));
        int j=0;
        for(;j<5;j++){
            if(machine->ReadMem(userAddress, 1, &temp)) break;
        }
        ASSERT(j<5);//terminaron las iteraciones y no ejecuto bien la funcion
        userAddress++;

        *outBuffer = (unsigned char) temp;
    } while (count < byteCount);
}

bool ReadStringFromUser(int userAddress, char *outString, // desde memoria virtual del usuario a la maquina
                        unsigned maxByteCount)
{
    ASSERT(userAddress != 0);
    ASSERT(outString != nullptr);
    ASSERT(maxByteCount != 0);

    unsigned count = 0;
    do {
        int temp;
        count++;

        //ASSERT(machine->ReadMem(userAddress++, 1, &temp));
        int i=0;
        for(;i<5;i++){
            if(machine->ReadMem(userAddress, 1, &temp)) break;
        }
        ASSERT(i<5);//terminaron las iteraciones y no ejecuto bien la funcion
        userAddress++;

        *outString = (unsigned char) temp;
    } while (*outString++ != '\0' && count < maxByteCount);

    return *(outString - 1) == '\0';
}

void WriteBufferToUser(const char *buffer, int userAddress, // desde la maquina a memoria virtual de usuario
                       unsigned byteCount)
{
    //ASSERT(userAddress != 0);//???
    ASSERT(buffer != nullptr);
    ASSERT(byteCount != 0);

    unsigned count = 0;
    do {
        int temp;
        temp = (int)*buffer++;//(unsigned char) *outBuffer;
        count++;

        //ASSERT(machine->WriteMem(userAddress++, 1, temp));
        int i=0;
        for(;i<5;i++){
            if(machine->WriteMem(userAddress, 1, temp)) break;
        }
        ASSERT(i<5);//terminaron las iteraciones y no ejecuto bien la funcion
        userAddress++;

    } while (count < byteCount);
}

void WriteStringToUser(const char *string, int userAddress)
{
    //ASSERT(userAddress != 0);//???
    ASSERT(string != nullptr);

    unsigned count = 0;
    do {
        int temp;
        temp = (int)*string;//(unsigned char) *outBuffer;
        count++;

        //ASSERT(machine->WriteMem(userAddress++, 1, temp));
        int j=0;
        for(;j<5;j++){
            if(machine->WriteMem(userAddress, 1, temp)) break;
        }
        ASSERT(j<5);//terminaron las iteraciones y no ejecuto bien la funcion
        userAddress++;
    } while (*string++ != '\0');
}
