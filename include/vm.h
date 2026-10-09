#ifndef clox_vm_h
#define clox_vm_h


typedef struct VM VM;


#include "object.h"
#include "table.h"
#include "value.h"


constexpr uint16_t framesMax = 64;
constexpr uint16_t stackMax = framesMax * uint8Count;


typedef struct {
    ObjClosure* closure;
    uint8_t* ip;
    Value* slots;
} CallFrame;


struct VM {
    CallFrame frames[framesMax];
    uint16_t frameCount;

    Value stack[stackMax];
    Value* stackTop;
    Table strings;
    Table globals;
    Obj* objects;

    ObjString* initString;
    ObjUpvalue* openUpvalues;

    size_t bytesAllocated;
    size_t nextGC;

    uint32_t grayCount;
    uint32_t grayCapacity;
    Obj** grayStack;
};


typedef enum : uint8_t {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR,
} InterpretResult;


void initVM(VM* restrict vm);
void freeVM(VM* restrict vm);

InterpretResult interpret(VM* restrict vm, char const* source);

void push(VM*, Value value);
Value pop(VM* vm);


#endif
