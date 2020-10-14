/// Miscellaneous useful definitions, including debugging utilities.
///
/// See also `lib/debug.hh`.
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2020 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.

#ifndef NACHOS_LIB_UTILITY__HH
#define NACHOS_LIB_UTILITY__HH


#include "assert.hh"
#include "debug.hh"


/// Miscellaneous useful routines.

//#define min(a, b)  (((a) < (b)) ? (a) : (b))
//#define max(a, b)  (((a) > (b)) ? (a) : (b))

/// Typedef for host memory references, expressed in numerical (integer)
/// form.

#ifdef HOST_x86_64
typedef unsigned long HostMemoryAddress;
#else
typedef unsigned int HostMemoryAddress;
#endif

/// Divide and either round up or down.

template <typename T>
inline T
DivRoundDown(T n, T s)
{
    return n / s;
}

template <typename T>
inline T
DivRoundUp(T n, T s)
{
    return n / s + (n % s > 0 ? 1 : 0);
}

/// This declares the type `VoidFunctionPtr` to be a “pointer to a
/// function taking a pointer argument and returning nothing”.
///
/// With such a function pointer (say it is "func"), we can call it like
/// this:
///    func (arg);
/// or:
///    (*func) (arg);
///
/// This is used by `Thread::Fork` and for interrupt handlers, as well as a
/// couple of other places.
typedef void (*VoidFunctionPtr)(void *arg);

typedef void (*VoidNoArgFunctionPtr)();


// Include interface that isolates us from the host machine system library.
// Requires definition of `VoidFunctionPtr`.
#include "machine/system_dep.hh"


/// Global object for debug output.
extern Debug debug;

#define DEBUG       (debug.Print)
#define DEBUG_CONT  (debug.PrintCont)




template <class T1, class T2>
class Pair {
    public:
        Pair(T1 t1, T2 t2){
            fst=t1;
            snd=t2;
        }

        Pair(){
            fst=T1();
            snd=T2();
        }

        //~Pair(){};

        //T1 Fst(){ return a;}
        //T2 Snd(){ return b;}

        T1 fst;
        T2 snd;
};

template <class T1, class T2>
bool operator ==(const Pair<T1, T2>& p1, const Pair<T1, T2>& p2)
{
    return p1.fst == p2.fst && p1.snd == p2.snd;
}
#endif
