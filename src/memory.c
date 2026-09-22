#include <stdlib.h>  /* For realloc, free, exit, EXIT_FAILURE */
#include <assert.h>  /* For assert */

#include "memory.h"


/* Local functions */

static void freeObject(Obj* object) {
    switch (object->type) {
        case OBJ_FUNCTION: {
                ObjFunction* function = (ObjFunction*)object;
                freeChunk(&function->chunk);
                assert(FREE(ObjFunction, object) == nullptr);
                break;
            }
        case OBJ_NATIVE: {
                assert(FREE(ObjNative, object) == nullptr);
                break;
            }
        case OBJ_CLOSURE: {
                assert(FREE(ObjClosure, object) == nullptr);
                break;
            }
        case OBJ_STRING: {
                ObjString* string = (ObjString*)object;
                assert(FREE_ARRAY(char, string->chars, string->length + 1) == nullptr);
                assert(FREE(ObjString, object) == nullptr);
                break;
            }
    }
}


/* Public functions */

void* reallocate(void* pointer, [[maybe_unused]] size_t oldSize, size_t newSize) {
    if (newSize == 0) {
        free(pointer);
        return nullptr;
    }

    void* result = realloc(pointer, newSize);
    if (!result)
        exit(EXIT_FAILURE);

    return result;
}


void freeObjects(VM* restrict vm) {
    Obj* object = vm->objects;
    while (object) {
        Obj* next = object->next;
        freeObject(object);
        object = next;
    }
}
