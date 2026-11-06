#ifndef clox_vm_h
#define clox_vm_h


typedef struct VM VM;


#include "gc.h"
#include "object.h"
#include "table.h"
#include "value.h"


constexpr uint16_t framesMax = 64;
constexpr uint16_t stackMax = framesMax * uint8Count;


typedef struct {
    ObjClosure* closure;
    uint8_t*    ip;
    Value*      slots;
} CallFrame;


struct VM {
    CallFrame   frames[framesMax];
    uint16_t    frameCount;

    Value       stack[stackMax];
    Value*      stackTop;
    Table       strings;
    Table       globals;

    ObjString*  initString;
    ObjUpvalue* openUpvalues;

    GC          gc;
};


typedef enum : uint8_t {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR,
} InterpretResult;


void initVM(VM vm[restrict static 1]);
void freeVM(VM vm[restrict static 1]);

InterpretResult interpret(VM vm[static 1], char const* source);

void  push(VM vm[static 1], Value value);
Value pop(VM vm[static 1]);


#endif
