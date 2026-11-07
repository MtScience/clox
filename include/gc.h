#ifndef clox_gc_h
#define clox_gc_h


#include "common.h"


/**
 * \brief Forward declares the `Obj` structure.
 */
typedef struct Obj Obj;


/**
 * \struct GC gc.h
 * \brief The garbage collector.
 *
 * A `struct` that represents the garbage collector. This implementation of Lox
 * uses a mark-and-sweep garbage collector. This structure is intended to be included
 * in a `VM` structure.
 */
typedef struct GC {
    Obj*     objects;                                 /**< Linked list of objects the garbage collector keeps
                                                       * track of.
                                                       */

    size_t   bytesAllocated;                          /**< Number of bytes currently used. */
    size_t   nextGC;                                  /**< Threshold, at which a garbage collection cycle
                                                       *   will be triggered. Adjusted at each cycle.
                                                       */

    uint32_t grayCount;                               /**<  */
    uint32_t grayCapacity;                            /**<  */
    Obj**    grayStack;                               /**<  */

    void     (*markRoots)(struct GC* gc, void* arg);  /**< Pointer to the function that marks all the objects
                                                       * accessible by the VM.
                                                       */
    void*    markArg;                                 /**< Pointer to the argument for `markRoots`. */
    void     (*cleanupWeakRefs)(void* arg);           /**<  */
    void*    weakArg;                                 /**< Pointer to the argument fot `cleanupWeakRefs`. */
} GC;


void initGC(GC gc[restrict static 1]);
void freeGC(GC gc[restrict static 1]);


#endif
