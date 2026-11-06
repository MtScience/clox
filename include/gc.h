#ifndef clox_gc_h
#define clox_gc_h


#include "common.h"


typedef struct Obj Obj;


typedef struct GC {
    Obj*     objects;

    size_t   bytesAllocated;
    size_t   nextGC;

    uint32_t grayCount;
    uint32_t grayCapacity;
    Obj**    grayStack;

    void     (*markRoots)(struct GC* gc, void* arg);
    void*    markArg;
    void     (*cleanupWeakRefs)(void* arg);
    void*    weakArg;
} GC;


void initGC(GC gc[restrict static 1]);
void freeGC(GC gc[restrict static 1]);


#endif
