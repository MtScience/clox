#include <stdio.h>   /* For printf */
#include <string.h>  /* For memcpy */
#include <assert.h>  /* For assert */

#include "memory.h"
#include "value.h"
#include "object.h"
#include "table.h"


/* Local functions */

#define ALLOCATE_OBJ(gc, type, objectType) (type*)allocateObject(gc, sizeof(type), objectType)


static Obj* allocateObject(GC gc[static 1], size_t size, ObjType type) {
    Obj* object = reallocate(gc, nullptr, 0, size);
    object->type = type;
    object->isMarked = false;

    object->next = gc->objects;
    gc->objects = object;

#ifdef DEBUG_LOG_GC
    printf("%p allocate %zu for %u\n", (void*)object, size, type);
#endif

    return object;
}


static ObjString* allocateString(VM vm[static 1], char const* chars, uint32_t length, uint32_t hash) {
    ObjString* string = (ObjString*)allocateObject(&vm->gc,
                                                   sizeof(ObjString) + (length + 1) * sizeof(char),
                                                   OBJ_STRING);
    string->length = length;
    memcpy(string->chars, chars, length * sizeof(char));
    string->chars[length] = '\0';
    string->hash = hash;

    push(vm, objVal((Obj*)string));
    tableSet(&vm->gc, &vm->strings, string, nilVal());
    pop(vm);

    return string;
}


static uint32_t hashString(uint32_t length, char const key[static length]) {
    /* This is the 32-bit FNV-1a hash function */

    uint32_t hash = 2'166'136'261u;
    for (uint32_t i = 0; i < length; i++) {
        hash ^= (uint8_t)key[i];
        hash *= 16'777'619u;
    }

    return hash;
}


static void printFunction(ObjFunction function[static 1]) {
    if (!function->name) {
        printf("<script>");
        return;
    }
    printf("<fn %s>", function->name->chars);
}


/* Public functions */

ObjString* copyString(VM* vm, uint32_t length, char const chars[static length]) {
    uint32_t hash = hashString(length, chars);

    ObjString* interned = tableFindString(&vm->strings, hash, length, chars);
    if (interned) {
        return interned;
    }

    return allocateString(vm, chars, length, hash);
}


ObjString* takeString(VM* vm, uint32_t length, char chars[static length]) {
    uint32_t hash = hashString(length, chars);

    ObjString* interned = tableFindString(&vm->strings, hash, length, chars);
    if (interned) {
        FREE_ARRAY(&vm->gc, char, chars, length + 1);
        return interned;
    }

    return allocateString(vm, chars, length, hash);
}


ObjUpvalue* newUpvalue(GC gc[static 1], Value slot[static 1]) {
    ObjUpvalue* upvalue = ALLOCATE_OBJ(gc, ObjUpvalue, OBJ_UPVALUE);
    upvalue->location = slot;
    upvalue->closed = nilVal();
    upvalue->next = nullptr;

    return upvalue;
}


ObjFunction* newFunction(GC gc[static 1]) {
    ObjFunction* function = ALLOCATE_OBJ(gc, ObjFunction, OBJ_FUNCTION);
    function->arity = 0;
    function->upvalueCount = 0;
    function->name = nullptr;
    initChunk(&function->chunk);

    return function;
}


ObjNative* newNative(GC gc[static 1], NativeFn function) {
    ObjNative* native = ALLOCATE_OBJ(gc, ObjNative, OBJ_NATIVE);
    native->function = function;

    return native;
}

ObjClosure* newClosure(GC gc[static 1], ObjFunction function[static 1]) {
    ObjUpvalue** upvalues = ALLOCATE(gc, ObjUpvalue*, function->upvalueCount);
    for (uint8_t i = 0; i < function->upvalueCount; i++) {
        upvalues[i] = nullptr;
    }

    ObjClosure* closure = ALLOCATE_OBJ(gc, ObjClosure, OBJ_CLOSURE);
    closure->function = function;
    closure->upvalues = upvalues;
    closure->upvalueCount = function->upvalueCount;

    return closure;
}


ObjClass* newClass(GC gc[static 1], ObjString* name) {
    ObjClass* class = ALLOCATE_OBJ(gc, ObjClass, OBJ_CLASS);
    class->name = name;
    initTable(&class->methods);

    return class;
}


ObjInstance* newInstance(GC gc[static 1], ObjClass class[static 1]) {
    ObjInstance* instance = ALLOCATE_OBJ(gc, ObjInstance, OBJ_INSTANCE);
    instance->class = class;
    initTable(&instance->fields);

    return instance;
}


ObjBoundMethod* newBoundMethod(
    GC gc[static 1],
    Value receiver,
    ObjClosure method[static 1]
) {
    ObjBoundMethod* bound = ALLOCATE_OBJ(gc, ObjBoundMethod, OBJ_BOUND_METHOD);
    bound->receiver = receiver;
    bound->method = method;

    return bound;
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
        case OBJ_BOUND_METHOD:
            printFunction(asBoundMethod(value)->method->function);
            break;
    }
}


#undef ALLOCATE_OBJ
