#include <assert.h>  /* For assert */
#include <stdio.h>   /* For printf */

#include "object.h"
#include "memory.h"
#include "value.h"


/* Public functions */

void initValueArray(ValueArray array[restrict static 1]) {
    *array = (ValueArray){
        .capacity = 0,
        .count = 0,
        .values = nullptr,
    };
}


void freeValueArray(GC* gc, ValueArray array[restrict static 1]) {
    FREE_ARRAY(gc, Value, array->values, array->capacity);
    initValueArray(array);
}


void writeValueArray(GC* gc, ValueArray array[static 1], Value value) {
    if (array->capacity < array->count + 1) {
        uint32_t oldCapacity = array->capacity;
        array->capacity = growCapacity(oldCapacity);
        array->values = GROW_ARRAY(gc, Value, array->values, oldCapacity, array->capacity);
    }

    array->values[array->count++] = value;
}


bool valuesEqual(Value a, Value b) [[reproducible]] {
#ifdef NAN_BOXING
    if (isNumber(a) && isNumber(b)) {
        return asNumber(a) == asNumber(b);
    }

    return a == b;
#else
    if (a.type != b.type) {
        return false;
    }

    switch (a.type) {
        case VAL_BOOL:   return asBool(a) == asBool(b);
        case VAL_NIL:    return true;
        case VAL_NUMBER: return asNumber(a) == asNumber(b);
        case VAL_OBJ:    return asObj(a) == asObj(b);
        default:
            unreachable();
            return false;
    }
#endif
}


void printValue(Value value) {
#ifdef NAN_BOXING
    if (isBool(value)) {
        printf(asBool(value) ? "true" : "false");
    } else if (isNil(value)) {
        printf("nil");
    } else if (isNumber(value)) {
        printf("%g", asNumber(value));
    } else if (isObj(value)) {
        printObject(value);
    }
#else
    switch (value.type) {
        case VAL_BOOL:   printf(asBool(value) ? "true" : "false"); break;
        case VAL_NIL:    printf("nil");                            break;
        case VAL_NUMBER: printf("%g", asNumber(value));            break;
        case VAL_OBJ:    printObject(value);                       break;
    }
#endif
}
