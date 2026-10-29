#ifndef clox_chunk_h
#define clox_chunk_h


typedef struct VM VM;


#include "common.h"
#include "value.h"
#include "gc.h"


typedef enum : uint8_t {
    OP_CONSTANT,
    OP_NEGATE,
    OP_NIL,
    OP_TRUE,
    OP_FALSE,
    OP_POP,
    OP_GET_LOCAL,
    OP_SET_LOCAL,
    OP_GET_UPVALUE,
    OP_SET_UPVALUE,
    OP_GET_PROPERTY,
    OP_SET_PROPERTY,
    OP_GET_GLOBAL,
    OP_SET_GLOBAL,
    OP_DEFINE_GLOBAL,
    OP_EQUAL,
    OP_GREATER,
    OP_LESS,
    OP_ADD,
    OP_SUBTRACT,
    OP_MULTIPLY,
    OP_DIVIDE,
    OP_NOT,
    OP_LOOP,
    OP_JUMP,
    OP_JUMP_IF_FALSE,
    OP_PRINT,
    OP_CALL,
    OP_CLOSURE,
    OP_CLOSE_UPVALUE,
    OP_RETURN,
    OP_CLASS,
    OP_METHOD,
    OP_INVOKE,
    OP_SUPER_INVOKE,
    OP_INHERIT,
    OP_GET_SUPER,
} OpCode;


typedef struct {
    uint32_t count;
    uint32_t capacity;
    uint8_t* code;
    uint32_t* lines;
    ValueArray constants;
} Chunk;


void initChunk(Chunk chunk[restrict static 1]);
void freeChunk(GC gc[restrict static 1], Chunk chunk[restrict static 1]);
void writeChunk(GC gc[restrict static 1], Chunk chunk[static 1], uint8_t byte, uint32_t line);
uint32_t addConstant(VM* vm, Chunk chunk[static 1], Value value);


#endif
