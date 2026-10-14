#ifndef clox_object_h
#define clox_object_h


#include "common.h"
#include "chunk.h"
#include "table.h"
#include "value.h"
#include "vm.h"



typedef enum : uint8_t {
    OBJ_FUNCTION,
    OBJ_NATIVE,
    OBJ_CLOSURE,
    OBJ_STRING,
    OBJ_UPVALUE,
    OBJ_CLASS,
    OBJ_INSTANCE,
    OBJ_BOUND_METHOD,
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


typedef struct {
    Obj obj;
    ObjString* name;
    Table methods;
} ObjClass;


typedef struct {
    Obj obj;
    ObjClass* class;
    Table fields;
} ObjInstance;


typedef struct {
    Obj obj;
    Value receiver;
    ObjClosure* method;
} ObjBoundMethod;


/* Type checks */

static inline
bool isObjType(Value value, ObjType type) {
    return isObj(value) && asObj(value)->type == type;
}

static inline ObjType objType(Value value)    { return asObj(value)->type;                 }
static inline bool isString(Value value)      { return isObjType(value, OBJ_STRING);       }
static inline bool isFunction(Value value)    { return isObjType(value, OBJ_FUNCTION);     }
static inline bool isNative(Value value)      { return isObjType(value, OBJ_NATIVE);       }
static inline bool isClosure(Value value)     { return isObjType(value, OBJ_CLOSURE);      }
static inline bool isClass(Value value)       { return isObjType(value, OBJ_CLASS);        }
static inline bool isInstance(Value value)    { return isObjType(value, OBJ_INSTANCE);     }
static inline bool isBoundMethod(Value value) { return isObjType(value, OBJ_BOUND_METHOD); }


/* Type conversions */

static inline char*           asCString(Value value)     { return ((ObjString*)asObj(value))->chars;    }
static inline ObjString*      asString(Value value)      { return (ObjString*)asObj(value);             }
static inline ObjFunction*    asFunction(Value value)    { return (ObjFunction*)asObj(value);           }
static inline NativeFn        asNative(Value value)      { return ((ObjNative*)asObj(value))->function; }
static inline ObjClosure*     asClosure(Value value)     { return (ObjClosure*)asObj(value);            }
static inline ObjClass*       asClass(Value value)       { return (ObjClass*)asObj(value);              }
static inline ObjInstance*    asInstance(Value value)    { return (ObjInstance*)asObj(value);           }
static inline ObjBoundMethod* asBoundMethod(Value value) { return (ObjBoundMethod*)asObj(value);        }


/* Interface */

ObjString* copyString(VM* vm, char const* chars, uint32_t length);
ObjString* takeString(VM* vm, char* chars, uint32_t length);

ObjUpvalue*     newUpvalue(VM* vm, Value* slot);
ObjFunction*    newFunction(VM* vm);
ObjNative*      newNative(VM* vm, NativeFn function);
ObjClosure*     newClosure(VM* vm, ObjFunction function[static 1]);
ObjClass*       newClass(VM* vm, ObjString* name);
ObjInstance*    newInstance(VM* vm, ObjClass* class);
ObjBoundMethod* newBoundMethod(VM* vm, Value receiver, ObjClosure* method);

void printObject(Value value);


#endif
