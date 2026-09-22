#ifndef clox_chunk_h
#define clox_chunk_h


#include "common.h"
#include "value.h"


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
} OpCode;


typedef struct {
    uint32_t count;
    uint32_t capacity;
    uint8_t* code;
    uint32_t* lines;
    ValueArray constants;
} Chunk;


void initChunk(Chunk* restrict chunk);
void freeChunk(Chunk* restrict chunk);
void writeChunk(Chunk* chunk, uint8_t byte, uint32_t line);
uint32_t addConstant(Chunk* chunk, Value value);


#endif
