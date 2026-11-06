#ifndef clox_debug_h
#define clox_debug_h


#include "chunk.h"


void     disassembleChunk(Chunk chunk[static 1], char const* name);
uint32_t disassembleInstruction(Chunk chunk[static 1], uint32_t offset);


#endif
