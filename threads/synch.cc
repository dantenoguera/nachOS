/// Routines for synchronizing threads.
///
/// Three kinds of synchronization routines are defined here: semaphores,
/// locks and condition variables (the implementation of the last two are
/// left to the reader).
///
/// Any implementation of a synchronization routine needs some primitive
/// atomic operation.  We assume Nachos is running on a uniprocessor, and
/// thus atomicity can be provided by turning off interrupts.  While
/// interrupts are disabled, no context switch can occur, and thus the
/// current thread is guaranteed to hold the CPU throughout, until interrupts
/// are reenabled.
///
/// Because some of these routines might be called with interrupts already
/// disabled (`Semaphore::V` for one), instead of turning on interrupts at
/// the end of the atomic operation, we always simply re-set the interrupt
/// state back to its original value (whether that be disabled or enabled).
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2020 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


#include "synch.hh"
#include "system.hh"
#include "synch_list.hh"
#include <string.h>
#include <stdio.h>

/// Initialize a semaphore, so that it can be used for synchronization.
///
/// * `debugName` is an arbitrary name, useful for debugging.
/// * `initialValue` is the initial value of the semaphore.
Semaphore::Semaphore(const char *debugName, int initialValue)
{
    name  = debugName;
    value = initialValue;
    queue = new List<Thread *>;
}

/// De-allocate semaphore, when no longer needed.
///
/// Assume no one is still waiting on the semaphore!
Semaphore::~Semaphore()
{
    delete queue;
}

const char *
Semaphore::GetName() const
{
    return name;
}

/// Wait until semaphore `value > 0`, then decrement.
///
/// Checking the value and decrementing must be done atomically, so we need
/// to disable interrupts before checking the value.
///
/// Note that `Thread::Sleep` assumes that interrupts are disabled when it is
/// called.
void
Semaphore::P()
{
    IntStatus oldLevel = interrupt->SetLevel(INT_OFF);
      // Disable interrupts.

    while (value == 0) {  // Semaphore not available.
        queue->Append(currentThread);  // So go to sleep.
        currentThread->Sleep();
    }
    value--;  // Semaphore available, consume its value.

    interrupt->SetLevel(oldLevel);  // Re-enable interrupts.
}

/// Increment semaphore value, waking up a waiter if necessary.
///
/// As with `P`, this operation must be atomic, so we need to disable
/// interrupts.  `Scheduler::ReadyToRun` assumes that threads are disabled
/// when it is called.
void
Semaphore::V()
{
    IntStatus oldLevel = interrupt->SetLevel(INT_OFF);

    Thread *thread = queue->Pop();
    if (thread != nullptr)
        // Make thread ready, consuming the `V` immediately.
        scheduler->ReadyToRun(thread);
    value++;

    interrupt->SetLevel(oldLevel);
}

/// Dummy functions -- so we can compile our later assignments.
///
/// Note -- without a correct implementation of `Condition::Wait`, the test
/// case in the network assignment will not work!

Lock::Lock(const char *debugName)
{
    name=(char *) new char[strlen(debugName) + 1];
    name=strncpy(name,debugName,strlen(debugName) + 1);
    ASSERT(0==strcmp(name,debugName));
    owner = nullptr;
    slock = new Semaphore(name, 1);
    swapedPrio=nullptr;
}

Lock::~Lock()
{
    delete [] name;
    delete slock;
}

const char *
Lock::GetName() const
{
    return name;
}

void
Lock::Acquire()
{    
    if(owner != nullptr && currentThread->GetPriority() < owner->GetPriority()) {
        swapedPrio=currentThread;
        DEBUG('s', "Thread %s realizando acquire. \n", currentThread->GetName());
        int temp=owner->GetPriority();
        owner->EditPriority(currentThread->GetPriority());
        currentThread->EditPriority(temp);
        scheduler->ChangePriority(owner);
    } 
    slock->P();
    DEBUG('s', "Thread %s hace P. \n", currentThread->GetName());
    ASSERT(owner == nullptr);
    ASSERT(!IsHeldByCurrentThread());
    owner = currentThread;
}

void
Lock::Release()
{
    ASSERT(IsHeldByCurrentThread());
    if(swapedPrio!=nullptr){
        DEBUG('s', "Thread %s restituyendo prioridad. \n", currentThread->GetName());
        int temp=owner->GetPriority();
        owner->EditPriority(swapedPrio->GetPriority());
        swapedPrio->EditPriority(temp);
        scheduler->ChangePriority(swapedPrio);
        swapedPrio=nullptr;
    }
    owner = nullptr;
    DEBUG('s', "Thread %s hace V. \n", currentThread->GetName());
    slock->V();
}

bool
Lock::IsHeldByCurrentThread() const
{
    return owner == currentThread;
}



Condition::Condition(const char *debugName, Lock *conditionLock)
{
    name=(char *) new char[strlen(debugName) + 1];
    name=strncpy(name,debugName,strlen(debugName) + 1);
    ASSERT(0==strcmp(name,debugName));
    foreignlock=conditionLock;
    locallock = new Lock("LocalLock");
    queuer = new Semaphore("Queuer",0);
    h = new Semaphore("H",0);
    waiters=0;
}

Condition::~Condition()
{
    delete [] name;
    delete locallock;
    delete queuer;
    delete h;
}

const char *
Condition::GetName() const
{
    return name;
}

void
Condition::Wait()
{
    ASSERT(foreignlock->IsHeldByCurrentThread());
    locallock->Acquire();{
        waiters++;
    }locallock->Release();
    foreignlock->Release();

    queuer->P();
    h->V();
    foreignlock->Acquire();
}

void
Condition::Signal()
{
    ASSERT(foreignlock->IsHeldByCurrentThread());
    locallock->Acquire();{
    if (waiters > 0) { waiters--; queuer->V(); h->P(); }
    } locallock->Release(); 
}

void
Condition::Broadcast()
{
    ASSERT(foreignlock->IsHeldByCurrentThread());
    locallock->Acquire();{
        for (int i = 0; i < waiters; i++) queuer->V();
        while (waiters > 0) { waiters--; h->P();}
    }locallock->Release();
}

Channel::Channel(const char *debugName){
    name=(char *) new char[strlen(debugName) + 1];
    name=strncpy(name,debugName,strlen(debugName) + 1);
    ASSERT(0==strcmp(name,debugName));
    mailbox = new SynchList<int>();
	lock = new Lock(debugName);
	sended = new Condition(debugName, lock);
}

Channel::~Channel(){
    delete name;
    delete mailbox;
	delete lock;
	delete sended;
}

const char *
Channel::GetName() const
{
    return name;
}

void
Channel::Send(int message){
	lock->Acquire();
    mailbox->Append(message);
	sended->Wait();
	lock->Release();
}

void
Channel::Receive(int *message){
    *message = mailbox->Pop();
	lock->Acquire();
	sended->Signal();
	lock->Release();
}
