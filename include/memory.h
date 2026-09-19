#ifndef clox_memory_h
#define clox_memory_h


#include "common.h"
#include "vm.h"
#include "object.h"


#define GROW_ARRAY(type, pointer, oldCount, newCount) \
    reallocate(pointer, sizeof(type) * (oldCount), sizeof(type) * (newCount))

#define FREE_ARRAY(type, pointer, oldCount) \
    reallocate(pointer, sizeof(type) * (oldCount), 0)

#define ALLOCATE(type, count) \
    reallocate(nullptr, 0, sizeof(type) * (count))

#define FREE(type, pointer) \
    reallocate(pointer, sizeof(type), 0)


static inline uint32_t growCapacity(uint32_t capacity) {
    return capacity < 8 ? 8 : 2 * capacity;
}


[[nodiscard("pointer to allocated memory dropped")]]
void* reallocate(void* pointer, size_t oldSize, size_t newSize);

void freeObjects(VM* restrict vm);

#endif
