#ifndef clox_memory_h
#define clox_memory_h


#include "common.h"
#include "object.h"


#define GROW_ARRAY(gc_ptr, type, pointer, oldCount, newCount) \
    reallocate(gc_ptr, pointer, sizeof(type) * (oldCount), sizeof(type) * (newCount))

#define FREE_ARRAY(gc_ptr, type, pointer, oldCount) \
    reallocate(gc_ptr, pointer, sizeof(type) * (oldCount), 0)

#define ALLOCATE(gc_ptr, type, count) \
    reallocate(gc_ptr, nullptr, 0, sizeof(type) * (count))

#define FREE(gc_ptr, type, pointer) \
    reallocate(gc_ptr, pointer, sizeof(type), 0)


static inline uint32_t growCapacity(uint32_t capacity) {
    return capacity < 8 ? 8 : 2 * capacity;
}


void* reallocate(GC* gc, void* pointer, size_t oldSize, size_t newSize);

void freeObjects(GC* gc);

void markObject(GC* gc, Obj* object);
void markValue(GC* gc, Value value);
void collectGarbage(GC* gc);

#endif
