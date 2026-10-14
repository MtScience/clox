#include <stdlib.h>  /* For realloc, free, exit, EXIT_FAILURE */
#include <assert.h>  /* For assert */

#include "compiler.h"
#include "memory.h"

#ifdef DEBUG_LOG_GC
#include <stdio.h>   /* For printf, puts */
#include "debug.h"
#endif


constexpr uint32_t GCHeapGrowthFactor = 2;


/* Local functions */

static void freeObject(VM* vm, Obj* restrict object) {
#ifdef DEBUG_LOG_GC
    printf("%p free type %u\n", (void*)object, object->type);
#endif

    switch (object->type) {
        case OBJ_FUNCTION: {
                ObjFunction* function = (ObjFunction*)object;
                freeChunk(vm, &function->chunk);
                assert(FREE(vm, ObjFunction, object) == nullptr);
                break;
            }
        case OBJ_NATIVE: {
                assert(FREE(vm, ObjNative, object) == nullptr);
                break;
            }
        case OBJ_CLOSURE: {
                ObjClosure* closure = (ObjClosure*)object;
                assert(FREE_ARRAY(vm, ObjUpvalue*, closure->upvalues, closure->upvalueCount) == nullptr);
                assert(FREE(vm, ObjClosure, object) == nullptr);
                break;
            }
        case OBJ_UPVALUE: {
                assert(FREE(vm, ObjUpvalue, object) == nullptr);
                break;
            }
        case OBJ_STRING: {
                ObjString* string = (ObjString*)object;
                assert(FREE_ARRAY(vm, char, string->chars, string->length + 1) == nullptr);
                assert(FREE(vm, ObjString, object) == nullptr);
                break;
            }
        case OBJ_CLASS: {
                ObjClass* class = (ObjClass*)object;
                freeTable(vm, &class->methods);
                assert(FREE(vm, ObjClass, object) == nullptr);
                break;
            }
        case OBJ_INSTANCE: {
                ObjInstance* instance = (ObjInstance*)object;
                freeTable(vm, &instance->fields);
                assert(FREE(vm, ObjInstance, object) == nullptr);
                break;
            }
        case OBJ_BOUND_METHOD: {
                assert(FREE(vm, ObjBoundMethod, object) == nullptr);
                break;
            }
    }
}


static void markArray(VM* vm, ValueArray* array) {
    for (uint32_t i = 0; i < array->count; i++) {
        markValue(vm, array->values[i]);
    }
}


static void blackenObject(VM* vm, Obj* object) {
#ifdef DEBUG_LOG_GC
    printf("%p blacken ", (void*)object);
    printValue(objVal(object));
    putchar('\n');
#endif

    switch (object->type) {
        case OBJ_CLOSURE: {
                ObjClosure* closure = (ObjClosure*)object;
                markObject(vm, (Obj*)closure->function);
                for (uint8_t i = 0; i < closure->upvalueCount; i++) {
                    markObject(vm, (Obj*)closure->upvalues[i]);
                }
                break;
            }
        case OBJ_FUNCTION: {
                ObjFunction* function = (ObjFunction*)object;
                markObject(vm, (Obj*)function->name);
                markArray(vm, &function->chunk.constants);
                break;
            }
        case OBJ_UPVALUE: {
                markValue(vm, ((ObjUpvalue*)object)->closed);
                break;
            }
        case OBJ_CLASS: {
                ObjClass* class = (ObjClass*)object;
                markObject(vm, (Obj*)class->name);
                markTable(vm, &class->methods);
                break;
            }
        case OBJ_INSTANCE: {
                ObjInstance* instance = (ObjInstance*)object;
                markObject(vm, (Obj*)instance->class);
                markTable(vm, &instance->fields);
                break;
            }
        case OBJ_BOUND_METHOD: {
                ObjBoundMethod* bound = (ObjBoundMethod*)object;
                markValue(vm, bound->receiver);
                markObject(vm, (Obj*)bound->method);
                break;
            }
        case OBJ_NATIVE:
        case OBJ_STRING:
            break;
    }
}


static void markRoots(VM* vm) {
    for (Value* slot = vm->stack; slot < vm->stackTop; slot++) {
        markValue(vm, *slot);
    }

    for (uint16_t i = 0; i < vm->frameCount; i++) {
        markObject(vm, (Obj*)vm->frames[i].closure);
    }

    for (
        ObjUpvalue* upvalue = vm->openUpvalues;
        upvalue;
        upvalue = upvalue->next
    ) {
        markObject(vm, (Obj*)upvalue);
    }

    markTable(vm, &vm->globals);
    /* markCompilerRoots() */
    /*
     * TODO: Fix this!!! Since I don't have a global Compiler, I can't really
     * use 'markCompilerRoots' all the time. The only real place I can do this
     * is from inside 'compile'. Also, I need to carefully check
     * where there're memory allocations to ensure there are no leaks.
     */
    markObject(vm,  (Obj*)vm->initString);
}


static void traceReferences(VM* vm) {
    while (vm->grayCount > 0) {
        Obj* object = vm->grayStack[--vm->grayCount];
        blackenObject(vm, object);
    }
}


static void sweep(VM* vm) {
    Obj* previous = nullptr;
    Obj* object = vm->objects;
    while (object) {
        if (object->isMarked) {
            object->isMarked = false;
            previous = object;
            object = object->next;
        } else {
            Obj* unreached = object;
            object = object->next;
            if (previous) {
                previous->next = object;
            } else {
                vm->objects = object;
            }

            freeObject(vm, unreached);
        }
    }
}


/* Public functions */

void* reallocate(VM* vm, void* pointer, size_t oldSize, size_t newSize) {
    vm->bytesAllocated += newSize - oldSize;
    if (newSize > oldSize) {
#ifdef DEBUG_STRESS_GC
        collectGarbage(vm);
#endif

        if (vm->bytesAllocated > vm->nextGC) {
            collectGarbage(vm);
        }
    }

    if (newSize == 0) {
        free(pointer);
        return nullptr;
    }

    void* result = realloc(pointer, newSize);
    if (!result)
        exit(EXIT_FAILURE);

    return result;
}


void freeObjects(VM* vm) {
    Obj* object = vm->objects;
    while (object) {
        Obj* next = object->next;
        freeObject(vm, object);
        object = next;
    }

    free(vm->grayStack);
}


void markObject(VM* vm, Obj* object) {
    if (!object || object->isMarked) {
        return;
    }

#ifdef DEBUG_LOG_GC
    printf("%p mark ", (void*)object);
    printValue(objVal(object));
    putchar('\n');
#endif

    object->isMarked = true;

    if (vm->grayCapacity < vm->grayCount + 1) {
        vm->grayCapacity = growCapacity(vm->grayCapacity);
        vm->grayStack = realloc(vm->grayStack, vm->grayCapacity * sizeof(Obj*));
        /*
         * Technically, the above is a bad idea, I know, but we are going to exit
         * immediately if allocation fails anyway, so it doesn't really matter.
         */

        if (!vm->grayStack) {
            exit(EXIT_FAILURE);
        }
    }

    vm->grayStack[vm->grayCount++] = object;
}


void markValue(VM* vm, Value value) {
    if (isObj(value)) {
        markObject(vm, asObj(value));
    }
}


void collectGarbage(VM* vm) {
#ifdef DEBUG_LOG_GC
    puts("-- gc begin");
    size_t before = vm->bytesAllocated;
#endif

    markRoots(vm);
    traceReferences(vm);
    tableRemoveWhite(&vm->strings);
    sweep(vm);

    vm->nextGC = vm->bytesAllocated * GCHeapGrowthFactor;

#ifdef DEBUG_LOG_GC
    puts("-- gc end");
    printf("   collected %zu bytes (from %zu to %zu), next at %zu\n",
           before - vm->bytesAllocated, before, vm->bytesAllocated, vm->nextGC);
#endif
    return;
}
