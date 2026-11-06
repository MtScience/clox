#include <assert.h>  /* For assert */


#include "chunk.h"
#include "memory.h"


void initChunk(Chunk chunk[restrict static 1]) {
    chunk->capacity = 0;
    chunk->count = 0;
    chunk->code = nullptr;
    chunk->lines = nullptr;
    initValueArray(&chunk->constants);
}


void freeChunk(GC gc[restrict static 1], Chunk chunk[restrict static 1]) {
    FREE_ARRAY(gc, uint8_t, chunk->code, chunk->capacity);
    FREE_ARRAY(gc, uint32_t, chunk->lines, chunk->capacity);

    freeValueArray(gc, &chunk->constants);
    initChunk(chunk);
}


void writeChunk(GC gc[restrict static 1], Chunk chunk[static 1], uint8_t byte, uint32_t line) {
    if (chunk->capacity < chunk->count + 1) {
        uint32_t oldCapacity = chunk->capacity;
        chunk->capacity = growCapacity(oldCapacity);
        chunk->code = GROW_ARRAY(gc, uint8_t, chunk->code, oldCapacity, chunk->capacity);
        chunk->lines = GROW_ARRAY(gc, uint32_t, chunk->lines, oldCapacity, chunk->capacity);
    }


    chunk->code[chunk->count] = byte;
    chunk->lines[chunk->count] = line;
    chunk->count++;
}


uint32_t addConstant(VM* vm, Chunk chunk[static 1], Value value) {
    push(vm, value);
    writeValueArray(&vm->gc, &chunk->constants, value);
    pop(vm);

    return chunk->constants.count - 1;
}
