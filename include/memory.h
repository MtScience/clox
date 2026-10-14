#ifndef clox_memory_h
#define clox_memory_h


#include "common.h"
#include "vm.h"
#include "object.h"


#define GROW_ARRAY(vm_ptr, type, pointer, oldCount, newCount) \
    reallocate(vm_ptr, pointer, sizeof(type) * (oldCount), sizeof(type) * (newCount))

#define FREE_ARRAY(vm_ptr, type, pointer, oldCount) \
    reallocate(vm_ptr, pointer, sizeof(type) * (oldCount), 0)

#define ALLOCATE(vm_ptr, type, count) \
    reallocate(vm_ptr, nullptr, 0, sizeof(type) * (count))

#define FREE(vm_ptr, type, pointer) \
    reallocate(vm_ptr, pointer, sizeof(type), 0)


static inline uint32_t growCapacity(uint32_t capacity) {
    return capacity < 8 ? 8 : 2 * capacity;
}


[[nodiscard("pointer to allocated memory dropped")]]
void* reallocate(VM* vm, void* pointer, size_t oldSize, size_t newSize);

void freeObjects(VM* vm);

void markObject(VM* vm, Obj* object);
void markValue(VM* vm, Value value);
void collectGarbage(VM* vm);

#endif
