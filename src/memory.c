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

static void freeObject(GC* gc, Obj* restrict object) {
#ifdef DEBUG_LOG_GC
    printf("%p free type %u\n", (void*)object, object->type);
#endif

    switch (object->type) {
        case OBJ_FUNCTION: {
                ObjFunction* function = (ObjFunction*)object;
                freeChunk(gc, &function->chunk);
                assert(FREE(gc, ObjFunction, object) == nullptr);
                break;
            }
        case OBJ_NATIVE: {
                assert(FREE(gc, ObjNative, object) == nullptr);
                break;
            }
        case OBJ_CLOSURE: {
                ObjClosure* closure = (ObjClosure*)object;
                assert(FREE_ARRAY(gc, ObjUpvalue*, closure->upvalues, closure->upvalueCount) == nullptr);
                assert(FREE(gc, ObjClosure, object) == nullptr);
                break;
            }
        case OBJ_UPVALUE: {
                assert(FREE(gc, ObjUpvalue, object) == nullptr);
                break;
            }
        case OBJ_STRING: {
                ObjString* string = (ObjString*)object;
                assert(FREE_ARRAY(gc, char, string->chars, string->length + 1) == nullptr);
                assert(FREE(gc, ObjString, object) == nullptr);
                break;
            }
        case OBJ_CLASS: {
                ObjClass* class = (ObjClass*)object;
                freeTable(gc, &class->methods);
                assert(FREE(gc, ObjClass, object) == nullptr);
                break;
            }
        case OBJ_INSTANCE: {
                ObjInstance* instance = (ObjInstance*)object;
                freeTable(gc, &instance->fields);
                assert(FREE(gc, ObjInstance, object) == nullptr);
                break;
            }
        case OBJ_BOUND_METHOD: {
                assert(FREE(gc, ObjBoundMethod, object) == nullptr);
                break;
            }
    }
}


static void markArray(GC* gc, ValueArray* array) {
    for (uint32_t i = 0; i < array->count; i++) {
        markValue(gc, array->values[i]);
    }
}


static void blackenObject(GC* gc, Obj* object) {
#ifdef DEBUG_LOG_GC
    printf("%p blacken ", (void*)object);
    printValue(objVal(object));
    putchar('\n');
#endif

    switch (object->type) {
        case OBJ_CLOSURE: {
                ObjClosure* closure = (ObjClosure*)object;
                markObject(gc, (Obj*)closure->function);
                for (uint8_t i = 0; i < closure->upvalueCount; i++) {
                    markObject(gc, (Obj*)closure->upvalues[i]);
                }
                break;
            }
        case OBJ_FUNCTION: {
                ObjFunction* function = (ObjFunction*)object;
                markObject(gc, (Obj*)function->name);
                markArray(gc, &function->chunk.constants);
                break;
            }
        case OBJ_UPVALUE: {
                markValue(gc, ((ObjUpvalue*)object)->closed);
                break;
            }
        case OBJ_CLASS: {
                ObjClass* class = (ObjClass*)object;
                markObject(gc, (Obj*)class->name);
                markTable(gc, &class->methods);
                break;
            }
        case OBJ_INSTANCE: {
                ObjInstance* instance = (ObjInstance*)object;
                markObject(gc, (Obj*)instance->class);
                markTable(gc, &instance->fields);
                break;
            }
        case OBJ_BOUND_METHOD: {
                ObjBoundMethod* bound = (ObjBoundMethod*)object;
                markValue(gc, bound->receiver);
                markObject(gc, (Obj*)bound->method);
                break;
            }
        case OBJ_NATIVE:
        case OBJ_STRING:
            break;
    }
}


static void traceReferences(GC* gc) {
    while (gc->grayCount > 0) {
        Obj* object = gc->grayStack[--gc->grayCount];
        blackenObject(gc, object);
    }
}


static void sweep(GC* gc) {
    Obj* previous = nullptr;
    Obj* object = gc->objects;

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
                gc->objects = object;
            }

            freeObject(gc, unreached);
        }
    }
}


/* Public functions */

void* reallocate(GC* gc, void* pointer, size_t oldSize, size_t newSize) {
    gc->bytesAllocated += newSize - oldSize;
    if (newSize > oldSize) {
#ifdef DEBUG_STRESS_GC
        collectGarbage(gc);
#endif

        if (gc->bytesAllocated > gc->nextGC) {
            collectGarbage(gc);
        }
    }

    if (newSize == 0) {
        free(pointer);
        return nullptr;
    }

    void* result = realloc(pointer, newSize);
    if (!result) {
        exit(EXIT_FAILURE);
    }

    return result;
}


void freeObjects(GC* gc) {
    Obj* object = gc->objects;
    while (object) {
        Obj* next = object->next;
        freeObject(gc, object);
        object = next;
    }

    free(gc->grayStack);
}


void markObject(GC* gc, Obj* object) {
    if (!object || object->isMarked) {
        return;
    }

#ifdef DEBUG_LOG_GC
    printf("%p mark ", (void*)object);
    printValue(objVal(object));
    putchar('\n');
#endif

    object->isMarked = true;

    if (gc->grayCapacity < gc->grayCount + 1) {
        gc->grayCapacity = growCapacity(gc->grayCapacity);
        gc->grayStack = realloc(gc->grayStack, gc->grayCapacity * sizeof(Obj*));  /* TODO: Try Obj*[gc->grayCapacity] */
        /*
         * Technically, the above is a bad idea, but we are going to exit
         * immediately if allocation fails anyway, so it doesn't really matter.
         */

        if (!gc->grayStack) {
            exit(EXIT_FAILURE);
        }
    }

    gc->grayStack[gc->grayCount++] = object;
}


void markValue(GC* gc, Value value) {
    if (isObj(value)) {
        markObject(gc, asObj(value));
    }
}


void collectGarbage(GC* gc) {
#ifdef DEBUG_LOG_GC
    puts("-- gc begin");
    size_t before = gc->bytesAllocated;
#endif

    if (gc->markRoots && gc->markArg) {
        gc->markRoots(gc, gc->markArg);
    }
    traceReferences(gc);

    if (gc->cleanupWeakRefs && gc->weakArg) {
        gc->cleanupWeakRefs(gc->weakArg);
    }
    sweep(gc);

    gc->nextGC = gc->bytesAllocated * GCHeapGrowthFactor;

#ifdef DEBUG_LOG_GC
    puts("-- gc end");
    printf("   collected %zu bytes (from %zu to %zu), next at %zu\n",
           before - gc->bytesAllocated, before, gc->bytesAllocated, gc->nextGC);
#endif
    return;
}
