#ifndef clox_value_h
#define clox_value_h


#include "common.h"


typedef struct GC GC;

typedef struct Obj Obj;
typedef struct ObjString ObjString;
typedef struct ObjFunction ObjFunction;
typedef struct ObjClosure ObjClosure;
typedef struct ObjUpvalue ObjUpvalue;


#ifdef NAN_BOXING


#include <string.h>  /* For memcpy */


typedef uint64_t Value;


/* Auxiliary definitions */

enum Tag : uint64_t {
    TAG_NIL   = 1,
    TAG_FALSE = 2,
    TAG_TRUE  = 3,
};

static constexpr uint64_t signBit  = 0x8000000000000000;
static constexpr uint64_t QNaN     = 0x7ffc000000000000;
static constexpr uint64_t trueVal  = QNaN | TAG_TRUE;
static constexpr uint64_t falseVal = QNaN | TAG_FALSE;


/* Constuctors */

static inline Value boolVal(bool value) { return value ? trueVal : falseVal; }
static inline Value nilVal(void)        { return QNaN | TAG_NIL; }
static inline Value objVal(Obj* obj)    { return signBit | QNaN | (uintptr_t)obj; }

static inline Value numberVal(double num) {
    Value value;
    memcpy(&value, &num, sizeof num);
    return value;
}


/* Type-conversions */

static inline bool asBool(Value value) { return value == trueVal; }
static inline Obj* asObj(Value value)  { return (Obj*)(uintptr_t)(value & ~(signBit | QNaN)); }

static inline double asNumber(Value value) {
    double num;
    memcpy(&num, &value, sizeof value);
    return num;
}


/* Type checks */

static inline bool isBool(Value value)   { return value == trueVal || value == falseVal; }
static inline bool isNil(Value value)    { return value == nilVal(); }
static inline bool isNumber(Value value) { return (value & QNaN) != QNaN; }
static inline bool isObj(Value value)    { return (value & (QNaN | signBit)) == (QNaN | signBit); }

#else

typedef enum : uint8_t {
    VAL_BOOL,
    VAL_NIL,
    VAL_NUMBER,
    VAL_OBJ,
} ValueType;


typedef struct {
    union {
        bool   boolean;
        double number;
        Obj*   obj;
    } as;
    ValueType type;
} Value;


/* Type checks */

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

#endif


typedef struct {
    uint32_t count;
    uint32_t capacity;
    Value*   values;
} ValueArray;


/* Interface */

void initValueArray(ValueArray array[restrict static 1]);
void freeValueArray(GC* gc, ValueArray array[restrict static 1]);
void writeValueArray(GC* gc, ValueArray array[static 1], Value value);
bool valuesEqual(Value a, Value b);
void printValue(Value value);


#endif
