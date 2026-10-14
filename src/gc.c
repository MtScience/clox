#include "gc.h"
#include "memory.h"


void initGC(GC gc[restrict static 1]) {
    *gc = (GC){
        .objects = nullptr,
        .grayCount = 0,
        .grayCapacity = 0,
        .grayStack = nullptr,
        .bytesAllocated = 0,
        .nextGC = 1024 * 1024,
        .markRoots = nullptr,
        .markArg = nullptr,
        .cleanupWeakRefs = nullptr,
        .weakArg = nullptr,
    };
}


void freeGC(GC gc[restrict static 1]) {
    freeObjects(gc);
    gc->objects = nullptr;
}
