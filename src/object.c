#include <stdio.h>   /* For printf */
#include <string.h>  /* For memcpy */
#include <assert.h>  /* For assert */

#include "memory.h"
#include "value.h"
#include "object.h"
#include "table.h"


/* Local functions */

/**
 * \ingroup AllocationMacros
 * \brief Convenience macro for object allocation.
 *
 * \param[in] type       C object type (`ObjFunction`, \e etc.) of the object to be be allocated.
 * \param[in] objectType Type tag of the object.
 *
 * \returns The pointer to the allocated memory cast to the specified type pointer.
 *
 * \sa `allocateObject()`, `allocateString()`
 */
#define ALLOCATE_OBJ(gc_ptr, type, objectType) (type*)allocateObject(gc_ptr, sizeof(type), objectType)


/**
 * \brief Allocates an `Obj`.
 *
 * Takes an amount of memory to be allocated and an `ObjType` associated with that memory,
 * performs an allocation, sets the type and registers the allocated memory with a garbage
 * collector.
 *
 * The result of this function is supposed to be cast to a specific object pointer type.
 *
 * \param[in] gc   Non-`NULL` pointer to a garbage collector that will manage the object.
 * \param[in] size The size of the memory block to be allocated in bytes.
 * \param[in] type The type tag of the requested object.
 *
 * \returns The pointer to the allocated object.
 *
 * \sa `reallocate()`
 */
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


/**
 * \brief Allocates and fills an `ObjString`.
 *
 * This function creates a new `ObjString` on the heap, initializes it and registers it in the
 * virtual machine's `VM::strings` table.
 *
 * \param[in] vm     Non-`NULL` pointer to the virtual machine that will manage the resulting
 *                   Lox string.
 * \param[in] hash   The pre-computed hash value of the string.
 * \param[in] length The length of the string being processed
 * \param[in] chars  An array of characters that contains the string data. Does not have to be
 *                   a null-terminated C string.
 *
 * \returns The pointer to the newly-constructed Lox string.
 */
static ObjString* allocateString(VM vm[static 1], uint32_t hash, uint32_t length, char const chars[static length]) {
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


/**
 * \brief The <a href="https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function">FNV-1a</a> hash function.
 *
 * Implements the Fowler—Noll—Vo hashing algorithm (32-bit version). In this
 * project it is used for hashing strings, but can be used for any value, \e e.g.
 * with a `union`:
 * \code{.c}
 * union {
 *     int num;
 *     unsigned char[sizeof(int)] bytes;
 * } value = { .num = number };
 *
 * hashString(sizeof(int), value.bytes);
 * \endcode
 *
 * \param[in] length The length of the memory chunk to be hashed (in bytes)
 * \param[in] key    The array of `char`s to be hashed (not necessarily a
 *                   null-terminated string).
 *
 * \returns The computed hash.
 */
static uint32_t hashString(uint32_t length, char const key[static length]) {
    /* This is the 32-bit FNV-1a hash function */

    uint32_t hash = 2'166'136'261u;
    for (uint32_t i = 0; i < length; i++) {
        hash ^= (uint8_t)key[i];
        hash *= 16'777'619u;
    }

    return hash;
}


/**
 * \brief Prints a Lox function.
 *
 * If the function has no name (meaning it represents a script, since this
 * implementation does not support anonymous functions), this funtion prints
 * `"<script>"`, otherwise it prints `"<fn [name]>"`.
 *
 * \param[in] function Non-`NULL` pointer to the function to be printed.
 */
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

    return allocateString(vm, hash, length, chars);
}


ObjString* takeString(VM* vm, uint32_t length, char chars[static length]) {
    uint32_t hash = hashString(length, chars);

    ObjString* interned = tableFindString(&vm->strings, hash, length, chars);
    if (interned) {
        FREE_ARRAY(&vm->gc, char, chars, length + 1);
        return interned;
    }

    return allocateString(vm, hash, length, chars);
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
