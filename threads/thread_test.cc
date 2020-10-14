/// Simple test case for the threads assignment.
///
/// Create several threads, and have them context switch back and forth
/// between themselves by calling `Thread::Yield`, to illustrate the inner
/// workings of the thread system.
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2007-2009 Universidad de Las Palmas de Gran Canaria.
///               2016-2017 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.

#include "system.hh"
#include "lib/debug.hh"
#include "synch.hh"
#include "tests.hh"

void
ThreadTest()
{
    /*
    Para cambiar de test modifique la variable test:
    'a': Jardin ornamental (locks)
    'b': Poductor consumidor (variables de condicion)
    'c': Sender receiver (channel)
    'd': Join
    'e': Prioridades
    default: SimpleThread
    */
    char test = 'f';
    switch(test)
    {
        case 'a':
        {
            DEBUG('t', "Probando Jardin Ornamental \n");
            Thread *newThread = new Thread("2nd", false, 0);
            newThread->Fork(Molinete, (void *) "2nd");
            Molinete ((void *) "1st");
            break;
        }
        case 'b':
        {
            DEBUG('t',"Probando Productor/Consumidor \n");
            Thread *newThread = new Thread("Consumidor", false, 0);
            newThread->Fork(Consumer, (void *) "Consumidor");
            Producer((void *) "Productor"); 
            break;
        }
        case 'c':
        {
            DEBUG('t',"Probando Sender/Receiver \n");
            Thread *newThread = new Thread("Dante", false, 0);
            newThread->Fork(Receiver, (void *) "Dante");
            Sender((void *) "Franco");
            break;
        }
        case 'd':
        {
            DEBUG('t',"Probando Join \n");
            Thread *newThread = new Thread("Hijo", true, 0);
            newThread->Fork(SimpleThread, (void *) "Hijo");
            newThread->Join();
            SimpleThread((void *) "Padre");
            break;
        }
        case 'e':
        {
            DEBUG('t', "Probado prioridades \n");
            const char *names[4] = {"2nd", "3rd", "4th", "5th"};
            for(int i = 0; i < 4; i++) {
                Thread *newThread = new Thread(names[i], false, i+2);
	            newThread->Fork(SimpleThread, (void *) names[i]);
            }
            SimpleThread((void *) "1st");
            break;
        }
        case 'f':
        {
            DEBUG('t', "Probado inversion de prioridades \n");
            Thread *newThread = new Thread("L", false, 2); 
            newThread->Fork(low, (void *) "L");
            currentThread->Yield();
            Thread *newThread2 = new Thread("M1", false, 1); 
            newThread2->Fork(medium, (void *) "M1");
            Thread *newThread3 = new Thread("M2", false, 1); 
            newThread3->Fork(medium, (void *) "M2");
            Thread *newThread4 = new Thread("H", false, 0); 
            newThread4->Fork(high, (void *) "H");
            break;
        }
        default:
        {
            DEBUG('t', "Probado SimpleThread \n");
            const char *names[4] = {"2nd", "3rd", "4th", "5th"};
            for(int i = 0; i < 4; i++) {
                Thread *newThread = new Thread(names[i], false, 0);
	            newThread->Fork(SimpleThread, (void *) names[i]);
            }
            SimpleThread((void *) "1st");
        }
    }
}
