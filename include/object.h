#ifndef clox_object_h
#define clox_object_h


#include "common.h"
#include "value.h"
#include "vm.h"



typedef enum : uint8_t {
    OBJ_STRING,
} ObjType;


struct Obj {
    ObjType type;
    struct Obj* next;
};


struct ObjString {
    Obj obj;
    uint32_t length;
    uint32_t hash;
    char* chars;
};


/* Type checks */

static inline
bool isObjType(Value value, ObjType type) {
    return isObj(value) && asObj(value)->type == type;
}

static inline ObjType objType(Value value) { return asObj(value)->type; }
static inline bool isString(Value value) { return isObjType(value, OBJ_STRING); }


/* Type conversions */

static inline ObjString* asString(Value value) { return (ObjString*)asObj(value); }
static inline char* asCString(Value value) { return ((ObjString*)asObj(value))->chars; }


/* Interface */

ObjString* copyString(VM* vm, char const* chars, uint32_t length);
ObjString* takeString(VM* vm, char* chars, uint32_t length);

void printObject(Value value);


#endif
