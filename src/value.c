#include <assert.h>  /* For assert */
#include <stdio.h>   /* For printf */

#include "object.h"
#include "memory.h"
#include "value.h"


/* Public functions */

void initValueArray(ValueArray* restrict array) {
    *array = (ValueArray){
        .capacity = 0,
        .count = 0,
        .values = nullptr,
    };
}


void freeValueArray(ValueArray* restrict array) {
    assert(FREE_ARRAY(Value, array->values, array->capacity) == nullptr);
    initValueArray(array);
}


void writeValueArray(ValueArray* array, Value value) {
    if (array->capacity < array->count + 1) {
        uint32_t oldCapacity = array->capacity;
        array->capacity = growCapacity(oldCapacity);
        array->values = GROW_ARRAY(Value, array->values, oldCapacity, array->capacity);
    }

    array->values[array->count++] = value;
}


bool valuesEqual(Value a, Value b) {
    if (a.type != b.type)
        return false;

    switch (a.type) {
        case VAL_BOOL:   return asBool(a) == asBool(b);
        case VAL_NIL:    return true;
        case VAL_NUMBER: return asNumber(a) == asNumber(b);
        case VAL_OBJ:    return asObj(a) == asObj(b);
        default:
            unreachable();
            return false;
    }
}


void printValue(Value value) {
    switch (value.type) {
        case VAL_BOOL:   printf(asBool(value) ? "true" : "false"); break;
        case VAL_NIL:    printf("nil");                            break;
        case VAL_NUMBER: printf("%g", asNumber(value));            break;
        case VAL_OBJ:    printObject(value);                       break;
    }
}
