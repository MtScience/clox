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


void freeChunk(VM* restrict vm, Chunk chunk[restrict static 1]) {
    assert(FREE_ARRAY(vm, uint8_t, chunk->code, chunk->capacity) == nullptr);
    assert(FREE_ARRAY(vm, uint32_t, chunk->lines, chunk->capacity) == nullptr);

    freeValueArray(vm, &chunk->constants);
    initChunk(chunk);
}


void writeChunk(VM* restrict vm, Chunk chunk[static 1], uint8_t byte, uint32_t line) {
    if (chunk->capacity < chunk->count + 1) {
        uint32_t oldCapacity = chunk->capacity;
        chunk->capacity = growCapacity(oldCapacity);
        chunk->code = GROW_ARRAY(vm, uint8_t, chunk->code, oldCapacity, chunk->capacity);
        chunk->lines = GROW_ARRAY(vm, uint32_t, chunk->lines, oldCapacity, chunk->capacity);
    }


    chunk->code[chunk->count] = byte;
    chunk->lines[chunk->count] = line;
    chunk->count++;
}


uint32_t addConstant(VM* restrict vm, Chunk chunk[static 1], Value value) {
    push(vm, value);
    writeValueArray(vm, &chunk->constants, value);
    pop(vm);

    return chunk->constants.count - 1;
}
