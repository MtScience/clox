#ifndef clox_value_h
#define clox_value_h


#include "common.h"


typedef struct Obj Obj;
typedef struct ObjString ObjString;
typedef struct ObjFunction ObjFunction;
typedef struct ObjClosure ObjClosure;
typedef struct ObjUpvalue ObjUpvalue;


typedef enum : uint8_t {
    VAL_BOOL,
    VAL_NIL,
    VAL_NUMBER,
    VAL_OBJ,
} ValueType;


typedef struct {
    union {
        bool boolean;
        double number;
        Obj* obj;
    } as;
    ValueType type;
} Value;


typedef struct {
    uint32_t count;
    uint32_t capacity;
    Value* values;
} ValueArray;


/* Type-checks */

static inline bool isBool(Value value)   { return value.type == VAL_BOOL; }
static inline bool isNil(Value value)    { return value.type == VAL_NIL; }
static inline bool isNumber(Value value) { return value.type == VAL_NUMBER; }
static inline bool isObj(Value value)    { return value.type == VAL_OBJ; }


/* Type conversions */

static inline bool   asBool(Value value)   { return value.as.boolean; }
static inline double asNumber(Value value) { return value.as.number; }
static inline Obj*   asObj(Value value)    { return value.as.obj; }


/* Constuctors */

static inline Value boolVal(bool value) {
    return (Value){ .type = VAL_BOOL, .as = { .boolean = value } };
}

static inline Value nilVal(void) {
    return (Value){ .type = VAL_NIL, .as = { .number = 0 } };
}

static inline Value numberVal(double value) {
    return (Value){ .type = VAL_NUMBER, .as = { .number = value } };
}

static inline Value objVal(Obj* value) {
    return (Value){ .type = VAL_OBJ, .as = { .obj = value } };

}


/* Interface */

void initValueArray(ValueArray* restrict array);
void freeValueArray(ValueArray* restrict array);
void writeValueArray(ValueArray* array, Value value);
bool valuesEqual(Value a, Value b);
void printValue(Value value);


#endif
