#include <stdio.h>   /* For printf */
#include <string.h>  /* For memcpy */
#include <assert.h>  /* For assert */

#include "memory.h"
#include "value.h"
#include "object.h"
#include "table.h"


/* Local functions */

#define ALLOCATE_OBJ(vm, type, objectType) (type*)allocateObject(vm, sizeof(type), objectType)


static Obj* allocateObject(VM* restrict vm, size_t size, ObjType type) {
    Obj* object = reallocate(vm, nullptr, 0, size);
    object->type = type;
    object->isMarked = false;

    object->next = vm->objects;
    vm->objects = object;

#ifdef DEBUG_LOG_GC
    printf("%p allocate %zu for %u\n", (void*)object, size, type);
#endif

    return object;
}


static ObjString* allocateString(VM* restrict vm, char* chars, uint32_t length, uint32_t hash) {
    ObjString* string = ALLOCATE_OBJ(vm, ObjString, OBJ_STRING);
    string->length = length;
    string->chars = chars;
    string->hash = hash;

    push(vm, objVal((Obj*)string));
    tableSet(vm, &vm->strings, string, nilVal());
    pop(vm);

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

ObjString* copyString(VM* restrict vm, char const* chars, uint32_t length) {
    uint32_t hash = hashString(chars, length);

    ObjString* interned = tableFindString(&vm->strings, chars, length, hash);
    if (interned)
        return interned;

    char* heapChars = ALLOCATE(vm, char, length + 1);
    memcpy(heapChars, chars, length);
    heapChars[length] = '\0';

    return allocateString(vm, heapChars, length, hash);
}


ObjString* takeString(VM* restrict vm, char* chars, uint32_t length) {
    uint32_t hash = hashString(chars, length);

    ObjString* interned = tableFindString(&vm->strings, chars, length, hash);
    if (interned) {
        assert(FREE_ARRAY(vm, char, chars, length + 1) == nullptr);
        return interned;
    }

    return allocateString(vm, chars, length, hash);
}


ObjUpvalue* newUpvalue(VM* restrict vm, Value* slot) {
    ObjUpvalue* upvalue = ALLOCATE_OBJ(vm, ObjUpvalue, OBJ_UPVALUE);
    upvalue->location = slot;
    upvalue->closed = nilVal();
    upvalue->next = nullptr;

    return upvalue;
}


ObjFunction* newFunction(VM* restrict vm) {
    ObjFunction* function = ALLOCATE_OBJ(vm, ObjFunction, OBJ_FUNCTION);
    function->arity = 0;
    function->upvalueCount = 0;
    function->name = nullptr;
    initChunk(&function->chunk);

    return function;
}


ObjNative* newNative(VM* restrict vm, NativeFn function) {
    ObjNative* native = ALLOCATE_OBJ(vm, ObjNative, OBJ_NATIVE);
    native->function = function;

    return native;
}

ObjClosure* newClosure(VM* restrict vm, ObjFunction* function) {
    ObjUpvalue** upvalues = ALLOCATE(vm, ObjUpvalue*, function->upvalueCount);
    for (uint8_t i = 0; i < function->upvalueCount; i++) {
        upvalues[i] = nullptr;
    }

    ObjClosure* closure = ALLOCATE_OBJ(vm, ObjClosure, OBJ_CLOSURE);
    closure->function = function;
    closure->upvalues = upvalues;
    closure->upvalueCount = function->upvalueCount;

    return closure;
}


ObjClass* newClass(VM* restrict vm, ObjString* name) {
    ObjClass* class = ALLOCATE_OBJ(vm, ObjClass, OBJ_CLASS);
    class->name = name;

    return class;
}


ObjInstance* newInstance(VM* restrict vm, ObjClass* class) {
    ObjInstance* instance = ALLOCATE_OBJ(vm, ObjInstance, OBJ_INSTANCE);
    instance->class = class;
    initTable(&instance->fields);

    return instance;
}


void printObject(Value value) {
    switch (objType(value)) {
        case OBJ_FUNCTION: printFunction(asFunction(value));          break;
        case OBJ_NATIVE:   printf("<native fn>");                     break;
        case OBJ_CLOSURE:  printFunction(asClosure(value)->function); break;
        case OBJ_STRING:   printf("%s", asCString(value));            break;
        case OBJ_UPVALUE:  printf("upvalue");                         break;
        case OBJ_CLASS:    printf("%s", asClass(value)->name->chars); break;
        case OBJ_INSTANCE:
            printf("%s instance", asInstance(value)->class->name->chars);
            break;
    }
}
