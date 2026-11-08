#ifndef clox_memory_h
#define clox_memory_h


#include "common.h"
#include "object.h"


/**
 * \brief Resizes an array of the given type.
 *
 * This is a convenience macro. Contrary to its name, it can also be used for shrinking.
 *
 * \param[in] gc_ptr   Non-`NULL` pointer to the garbage collector that manages the memory block.
 * \param[in] pointer  The pointer to the block of memory, holding the array to be resized.
 * \param[in] type     Type of the values stored in the array.
 * \param[in] oldCount Current capacity of the array.
 * \param[in] newCount Desired capacity of the array.
 *
 * \returns The pointer to the resized memory block.
 *
 * \sa `FREE_ARRAY()`, `ALLOCATE()`, `FREE()`, `reallocate()`
 */
#define GROW_ARRAY(gc_ptr, type, pointer, oldCount, newCount) \
    reallocate(gc_ptr, pointer, sizeof(type) * (oldCount), sizeof(type) * (newCount))

/**
 * \brief Frees a heap-allocated array of the given type.
 *
 * This is a convenience macro. Unlike the `free()` function, it does not bypass the garbage
 * collector.
 *
 * \param[in] gc_ptr   Non-`NULL` pointer to the garbage collector that manages the memory block.
 * \param[in] pointer  The pointer to the block of memory, holding the array to be freed.
 * \param[in] type     Type of the values stored in the array.
 * \param[in] oldCount Current capacity of the array.
 *
 * \returns `NULL`.
 *
 * \sa `GROW_ARRAY()`, `ALLOCATE()`, `FREE()`, `reallocate()`
 */
#define FREE_ARRAY(gc_ptr, type, pointer, oldCount) \
    reallocate(gc_ptr, pointer, sizeof(type) * (oldCount), 0)

/**
 * \brief Allocates an array of the given type.
 *
 * This is a convenience macro.
 *
 * \param[in] gc_ptr Non-`NULL` pointer to the garbage collector that manages the memory block.
 * \param[in] type   Type of the values stored in the array.
 * \param[in] count  Desired capacity of the array.
 *
 * \returns The pointer to the allocated memory block.
 *
 * \sa `GROW_ARRAY()`, `FREE_ARRAY()`, `FREE()`, `reallocate()`
 */
#define ALLOCATE(gc_ptr, type, count) \
    reallocate(gc_ptr, nullptr, 0, sizeof(type) * (count))

/**
 * \brief Frees a block of memory.
 *
 * This is a convenience macro. Unlike the `free()` function, it does not bypass the garbage
 * collector.
 *
 * \param[in] gc_ptr Non-`NULL` pointer to the garbage collector that manages the memory block.
 * \param[in] type   Type of the values stored in the array.
 * \param[in] count  Desired capacity of the array.
 *
 * \returns `NULL`.
 *
 * \sa `GROW_ARRAY()`, `FREE_ARRAY()`, `ALLOCATE()`, `reallocate()`
 */
#define FREE(gc_ptr, type, pointer) \
    reallocate(gc_ptr, pointer, sizeof(type), 0)


/**
 * \brief Computes new size for an array.
 *
 * Inteded to be used when enlarging heap-allocated arrays. For arrays with less than
 * 8 slots the function returns 8, otherwise it doubles the capacity.
 *
 * \param[in] capacity The current capacity of the array to be grown.
 *
 * \returns Updated array capacity.
 */
static inline uint32_t growCapacity(uint32_t capacity) {
    return capacity < 8 ? 8 : 2 * capacity;
}


/**
 * \brief Handles allocation and freeing of memory.
 *
 * This is the main memory management function in this project. If the block of memory
 * is to be enlarged, it can trigger a garbage collection cycle. If the new size is 0,
 * the function frees the block.
 *
 * `reallocate()` is implemented in terms of `free()` and `realloc()`, with all the side effects
 * that result from this.
 *
 * \param[in]     gc      Non-`NULL` pointer to the garbage collector, that manages the memory block.
 * \param[in,out] pointer The pointer to the block of memory to be resized. If `NULL`, new memory
 *                        is allocated.
 * \param[in]     oldSize Current size of the block. Ignored if `pointer == NULL`.
 * \param[in]     newSize Desired size of the block.
 *
 * \returns The pointer to resized block or `NULL` (if the block was freed).
 */
void* reallocate(GC gc[static 1], void* pointer, size_t oldSize, size_t newSize);

/**
 * \brief Frees all objects.
 *
 * `freeObjects()` free all the objects managed in a `GC` (references to which are kep in `GC::objects`).
 * If there are several garbage collectors (\e e.g., when using several `VM`'s in separate threads),
 * this function will have to be called for each of them.
 *
 * \param[in] gc Non-`NULL` pointer to the garbage collector, whose managed objects are to be freed.
 */
void freeObjects(GC gc[static 1]);


/**
 * \brief Marks an object as reachable.
 *
 * Takes a pointer to the object to be marked, sets its `Obj::isMarked` to `true` and adds it to
 * the garbage collector's `GC::grayStack`.
 *
 * \param[in] gc     Non-`NULL` pointer to the garbage collector that manages the object.
 * \param[in] object The pointer to the object to be marked. If `NULL`, the function return immediately.
 */
void markObject(GC gc[static 1], Obj* object);

/**
 * \brief Marks a value as reachable.
 *
 * If the value is a Lox object, then this function is equivalent to `markObject()`. Otherwise, it
 * does nothing.
 *
 * \param[in] gc    Non-`NULL` pointer to the garbage collector that manages the object.
 * \param[in] value The value to be marked.
 */
void markValue(GC gc[static 1], Value value);

/**
 * \brief Runs a garbage collection cycle.
 *
 * This function implements the main garbage collection logic. It marks all the reachable roots,
 * frees unreachable objects (including weak references) and updates the memory threshold (`GC::nextGC`).
 *
 * \param[in] gc Non-`NULL` pointer to the garbage collector.
 */
void collectGarbage(GC gc[static 1]);

#endif
