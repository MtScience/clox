#ifndef clox_vm_h
#define clox_vm_h


#include "chunk.h"
#include "table.h"
#include "value.h"


constexpr uint16_t stackMax = 256;


typedef struct {
    Chunk* chunk;
    uint8_t* ip;
    Value stack[stackMax];
    Value* stackTop;
    Table strings;
    Table globals;
    Obj* objects;
} VM;


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
