/// All global variables used in Nachos are defined here.
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2020 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.

#ifndef NACHOS_THREADS_SYSTEM__HH
#define NACHOS_THREADS_SYSTEM__HH


#include "thread.hh"
#include "scheduler.hh"
#include "lib/utility.hh"
#include "machine/interrupt.hh"
#include "machine/statistics.hh"
#include "machine/timer.hh"
#include "synch.hh"


/// Initialization and cleanup routines.

// Initialization, called before anything else.
extern void Initialize(int argc, char **argv);

// Cleanup, called when Nachos is done.
extern void Cleanup();


extern Thread *currentThread;        ///< The thread holding the CPU.
extern Thread *threadToBeDestroyed;  ///< The thread that just finished.
extern Scheduler *scheduler;         ///< The ready list.
extern Interrupt *interrupt;         ///< Interrupt status.
extern Statistics *stats;            ///< Performance metrics.
extern Timer *timer;                 ///< The hardware alarm clock.

#ifdef USER_PROGRAM
#include "machine/machine.hh"
#include "lib/bitmap.hh"
#include "userprog/synch_console.hh"
extern Machine *machine;  // User program memory and registers.
extern Bitmap* bmp;
extern Table<Thread*> *spaceIds;
extern SynchConsole *sconsole;
#endif

#ifdef FILESYS_NEEDED  // *FILESYS* or *FILESYS_STUB*.
#include "filesys/file_system.hh"
extern FileSystem *fileSystem;
#endif

#ifdef FILESYS
#include "filesys/synch_disk.hh"
extern SynchDisk *synchDisk;
//#include "lib/list.hh"
#include "filesys/file_list.hh"
class FileList;
extern FileList *listOpenFiles; ///< lista de archivos abiertos sincronizacion.
#endif

#ifdef NETWORK
#include "network/post.hh"
extern PostOffice *postOffice;
#endif

#ifdef DEMAND_LOADING
extern Pair <AddressSpace*, int> coremap[NUM_PHYS_PAGES];
extern unsigned victim;
#endif

#endif
