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
                                                       *   track of.
                                                       */

    size_t   bytesAllocated;                          /**< Number of bytes currently used. */
    size_t   nextGC;                                  /**< Threshold, at which a garbage collection cycle
                                                       *   will be triggered. Default is 1 MB. Adjusted at each cycle.
                                                       */

    uint32_t grayCount;                               /**< Number of object references in `GC::grayStack`. */
    uint32_t grayCapacity;                            /**< Maximum number of object references that can be kept
                                                       * in `GC::grayStack`.
                                                       */
    Obj**    grayStack;                               /**< List of reachable but not yet fully processed objects.
                                                       *   Is `NULL` between cycles.
                                                       */

    void     (*markRoots)(struct GC* gc, void* arg);  /**< Pointer to the function that marks all the objects
                                                       *   accessible by the VM.
                                                       */
    void*    markArg;                                 /**< Pointer to the argument for `GC::markRoots`. */
    void     (*cleanupWeakRefs)(void* arg);           /**<  Pointer to the function that deals with weak references
                                                       *    (only string objects in this implementation).
                                                       */
    void*    weakArg;                                 /**< Pointer to the argument fot `GC::cleanupWeakRefs`. */
} GC;


/**
 * \brief Initializes a garbage collector.
 *
 * Takes a newly-created `GC` and fills in its fields, putting the structure
 * in a well-defined state. All the pointer fields are `NULL` by default.
 *
 * \param[in] gc Non-`NULL` pointer to the garbage collector to be initialized.
 */
void initGC(GC gc[restrict static 1]);

/**
 * \brief Frees a garbage collector.
 *
 * Takes a `GC` and frees the memory occupied by it.
 *
 * \param[in] gc Non-`NULL` pointer to the garbage collector to be freed.
 */
void freeGC(GC gc[restrict static 1]);


#endif
