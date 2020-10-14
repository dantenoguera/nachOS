#include <stdio.h>
#include <string.h>
#include <unistd.h> // para sleep(segundos)
#include "system.hh"
#include <semaphore.h>
#include "lib/debug.hh"
#include "synch.hh"
#include "tests.hh"


// SimpleThread
Semaphore* semaphore = new Semaphore("simpleThread", 3);
/// Loop 10 times, yielding the CPU to another ready thread each iteration.
///
/// * `name` points to a string with a thread name, just for debugging
///   purposes.
void
SimpleThread(void *name_)
{
    // Reinterpret arg `name` as a string.
    char *name = (char *) name_;

    // If the lines dealing with interrupts are commented, the code will
    // behave incorrectly, because printf execution may cause race
    // conditions.
    #ifdef SEMAPHORE_TEST
    semaphore->P();
    DEBUG('s', "Thread `%s` reduce el valor del semaforo \n", name);

    for (unsigned num = 0; num < 10; num++) {
        printf("* Thread `%s` is running with %d priority: iteration %u\n", name,currentThread->GetPriority(), num);
        currentThread->Yield();
    }

    semaphore->V();
    DEBUG('s', "Thread `%s` aumenta el valor del semaforo \n", name);
    #else
    for (unsigned num = 0; num < 10; num++) {
        printf("* Thread `%s` is running: iteration %u\n", name, num);     
        currentThread->Yield();
    }
    #endif
    printf("!!! Thread `%s` has finished\n", name);
}


// Jardin ornamental
#define N_VISITANTES 10
int visitantes = 0; 
Lock *lock = new Lock("molinete"); 

void
Molinete(void *name_)
{
    char *name = (char *) name_;
    for (int i = 0; i < N_VISITANTES; i++){
        lock->Acquire();
        DEBUG('s', "Thread `%s` toma el lock \n", name);
        visitantes++;
        lock->Release();
        DEBUG('s', "Thread `%s` libera el lock \n", name);
    }
    printf("Thread %s visitantes %d \n", name, visitantes);
}


// Productor y consumidor
#define N 10 
int buffer[N]={0}; 
int in=0, out=0, ctos=0; 
Lock *sem = new Lock("p/c"); 
Condition* vacio = new Condition("c", sem); 
Condition* lleno = new Condition("p", sem);

void
Producer(void *name_)
{
    char *name = (char *) name_;
    while(true) {
        sleep(1);
        sem->Acquire();
        while(ctos >= N)
            lleno->Wait();
        buffer[in] = 42;
        printf("Thread `%s` buffer[%d] = %d \n", name, in, buffer[in]);
        in++;
        if (in >= N)
            in = 0;
        ctos++;
        vacio->Signal();
        sem->Release();
        
    }
}

void
Consumer(void *name_)
{
    char *name = (char *) name_;
    while(true) {
        sleep(1);
        sem->Acquire();
        while(ctos <= 0)
            vacio->Wait();
		buffer[out] = 0;
        printf("Thread `%s` buffer[%d] = %d \n", name, out, buffer[out]);
        out++;
        if (out >= N)
            out = 0;
        ctos--;
        lleno->Signal();
        sem->Release();
    }
}


// Envio de mensajes entre threads
Channel *ch = new Channel("s/r");

void
Sender(void *name_)
{
    char *name = (char *) name_;
    printf("Thread `%s` envia %d\n", name, 42);
    ch->Send(42);
    printf("Thread `%s` envia %d\n", name, 43);
    ch->Send(43);
    printf("Thread `%s` envia %d\n", name, 44);
    ch->Send(44);
}

void
Receiver(void *name_)
{
    char *name = (char *) name_;
    int d;
    ch->Receive(&d);
    printf("Thread `%s` recibe %d\n", name, d);
    ch->Receive(&d);
    printf("Thread `%s` recibe %d\n", name, d);
    ch->Receive(&d);
    printf("Thread `%s` recibe %d\n", name, d);
}

Lock *l = new Lock("R");

void high(void *name_)
{
    char *name = (char *) name_;
    l->Acquire();
    l->Release();
    printf("Thread `%s` tarea de alta prioridad realizada.\n", name);
}

void medium(void *name_)
{
    char *name = (char *) name_;
    printf("Thread `%s` realizando bucle infinito de media prioridad...\n", name);
    while(1) { sleep(5); currentThread->Yield(); }
}

void low(void *name_)
{
    char *name = (char *) name_;
    l->Acquire();
    for(int i=0;i<10;i++) { sleep(5), currentThread->Yield();}
    l->Release();
    printf("Thread `%s` tarea de baja prioridad realizada.\n", name);
}
