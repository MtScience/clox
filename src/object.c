#include <stdio.h>   /* For printf */
#include <string.h>  /* For memcpy */
#include <assert.h>  /* For assert */

#include "memory.h"
#include "value.h"
#include "object.h"
#include "table.h"


/* Local functions */

#define ALLOCATE_OBJ(vm, type, objectType) (type*)allocateObject(vm, sizeof(type), objectType)


static Obj* allocateObject(VM* vm, size_t size, ObjType type) {
    Obj* object = reallocate(nullptr, 0, size);
    object->type = type;

    object->next = vm->objects;
    vm->objects = object;

    return object;
}


static ObjString* allocateString(VM* vm, char* chars, uint32_t length, uint32_t hash) {
    ObjString* string = ALLOCATE_OBJ(vm, ObjString, OBJ_STRING);
    string->length = length;
    string->chars = chars;
    string->hash = hash;

    tableSet(&vm->strings, string, nilVal());

    return string;
}


static uint32_t hashString(char const* key, uint32_t length) {
    uint32_t hash = 2'166'136'261u;
    for (uint32_t i = 0; i < length; i++) {
        hash ^= (uint8_t)key[i];
        hash *= 16'777'619;
    }

    return hash;
}


static void printFunction(ObjFunction* function) {
    if (!function->name) {
        printf("<script>");
        return;
    }
    printf("<fn %s>", function->name->chars);
}


/* Public functions */

ObjString* copyString(VM* vm, char const* chars, uint32_t length) {
    uint32_t hash = hashString(chars, length);

    ObjString* interned = tableFindString(&vm->strings, chars, length, hash);
    if (interned)
        return interned;

    char* heapChars = ALLOCATE(char, length + 1);
    memcpy(heapChars, chars, length);
    heapChars[length] = '\0';

    return allocateString(vm, heapChars, length, hash);
}


ObjString* takeString(VM* vm, char* chars, uint32_t length) {
    uint32_t hash = hashString(chars, length);

    ObjString* interned = tableFindString(&vm->strings, chars, length, hash);
    if (interned) {
        assert(FREE_ARRAY(char, chars, length + 1) == nullptr);
        return interned;
    }

    return allocateString(vm, chars, length, hash);
}


ObjFunction* newFunction(VM* vm) {
    ObjFunction* function = ALLOCATE_OBJ(vm, ObjFunction, OBJ_FUNCTION);
    function->arity = 0;
    function->name = nullptr;
    initChunk(&function->chunk);

    return function;
}


ObjNative* newNative(VM* vm, NativeFn function) {
    ObjNative* native = ALLOCATE_OBJ(vm, ObjNative, OBJ_NATIVE);
    native->function = function;

    return native;
}

ObjClosure* newClosure(VM* vm, ObjFunction* function) {
    ObjClosure* closure = ALLOCATE_OBJ(vm, ObjClosure, OBJ_CLOSURE);
    closure->function = function;

    return closure;
}


void printObject(Value value) {
    switch (objType(value)) {
        case OBJ_FUNCTION: printFunction(asFunction(value));          break;
        case OBJ_NATIVE:   printf("<native fn>");                     break;
        case OBJ_CLOSURE:  printFunction(asClosure(value)->function); break;
        case OBJ_STRING:   printf("%s", asCString(value));            break;
    }
}
