#ifndef clox_object_h
#define clox_object_h


#include "common.h"
#include "chunk.h"
#include "value.h"
#include "vm.h"



typedef enum : uint8_t {
    OBJ_FUNCTION,
    OBJ_NATIVE,
    OBJ_CLOSURE,
    OBJ_STRING,
    OBJ_UPVALUE,
} ObjType;


struct Obj {
    ObjType type;
    bool isMarked;
    struct Obj* next;
};


struct ObjString {
    Obj obj;
    uint32_t length;
    uint32_t hash;
    char* chars;
};


struct ObjUpvalue {
    Obj obj;
    Value* location;
    Value closed;
    struct ObjUpvalue* next;
};


struct ObjFunction {
    Obj obj;
    uint16_t arity;
    uint8_t upvalueCount;
    Chunk chunk;
    ObjString* name;
};


typedef Value (*NativeFn)(uint8_t arg, Value* args);


typedef struct {
    Obj obj;
    NativeFn function;
} ObjNative;


struct ObjClosure {
    Obj obj;
    ObjFunction* function;
    ObjUpvalue** upvalues;
    uint8_t upvalueCount;
};


/* Type checks */

static inline
bool isObjType(Value value, ObjType type) {
    return isObj(value) && asObj(value)->type == type;
}

static inline ObjType objType(Value value) { return asObj(value)->type; }
static inline bool isString(Value value)   { return isObjType(value, OBJ_STRING); }
static inline bool isFunction(Value value) { return isObjType(value, OBJ_FUNCTION); }
static inline bool isNative(Value value)   { return isObjType(value, OBJ_NATIVE); }
static inline bool isClosure(Value value)  { return isObjType(value, OBJ_CLOSURE); }


/* Type conversions */

static inline char*        asCString(Value value)  { return ((ObjString*)asObj(value))->chars; }
static inline ObjString*   asString(Value value)   { return (ObjString*)asObj(value); }
static inline ObjFunction* asFunction(Value value) { return (ObjFunction*)asObj(value); }
static inline NativeFn     asNative(Value value)   { return ((ObjNative*)asObj(value))->function; }
static inline ObjClosure*  asClosure(Value value)  { return (ObjClosure*)asObj(value); }


/* Interface */

ObjString* copyString(VM* restrict vm, char const* chars, uint32_t length);
ObjString* takeString(VM* restrict vm, char* chars, uint32_t length);

ObjUpvalue*  newUpvalue(VM* restrict vm, Value* slot);
ObjFunction* newFunction(VM* restrict vm);
ObjNative*   newNative(VM* restrict vm, NativeFn function);
ObjClosure*  newClosure(VM* restrict vm, ObjFunction* function);

void printObject(Value value);


#endif
