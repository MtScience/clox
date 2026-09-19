#include <assert.h>  /* For assert */

#include "chunk.h"
#include "memory.h"


void initChunk(Chunk* restrict chunk) {
    chunk->capacity = 0;
    chunk->count = 0;
    chunk->code = nullptr;
    chunk->lines = nullptr;
    initValueArray(&chunk->constants);

}


void freeChunk(Chunk* restrict chunk) {
    assert(FREE_ARRAY(uint8_t, chunk->code, chunk->capacity) == nullptr);
    assert(FREE_ARRAY(uint32_t, chunk->lines, chunk->capacity) == nullptr);

    freeValueArray(&chunk->constants);
    initChunk(chunk);
}


void writeChunk(Chunk* chunk, uint8_t byte, uint32_t line) {
    if (chunk->capacity < chunk->count + 1) {
        uint32_t oldCapacity = chunk->capacity;
        chunk->capacity = growCapacity(oldCapacity);
        chunk->code = GROW_ARRAY(uint8_t, chunk->code, oldCapacity, chunk->capacity);
        chunk->lines = GROW_ARRAY(uint32_t, chunk->lines, oldCapacity, chunk->capacity);
    }


    chunk->code[chunk->count] = byte;
    chunk->lines[chunk->count] = line;
    chunk->count++;
}


uint32_t addConstant(Chunk* chunk, Value value) {
    writeValueArray(&chunk->constants, value);
    return chunk->constants.count - 1;
}
