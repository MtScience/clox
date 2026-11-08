#ifndef clox_debug_h
#define clox_debug_h


#include "chunk.h"

/**
 * \brief Prints debug information for a chunk.
 *
 * Takes a `Chunk` and a string representing the name of the chunk. Prints said name and
 * debug information for all instruction the chunk contains.
 *
 * \param[in] chunk Non-`NULL` pointer to the chunk to be disassembled.
 * \param[in] name  The name of the chunk.
 */
void disassembleChunk(Chunk chunk[static 1], char const* name);

/**
 * \brief Prints debug information for an instruction.
 *
 * Takes a `Chunk` and a `uint32_t` (offset) and prints the information about
 * the instruction, located at the given offset. Prints the number of the source lines,
 * from which the instruction was compiled, the type of instruction and additional information
 * (such as the value the instruction refers to, destination of a jump instruction, \e etc).
 * If the byte specified does not represent a known instruction code, the function prints
 * `Unknown opcode`, followed by the value of the byte.
 *
 * \param[in] chunk  Non-`NULL` pointer to the chunk being disassembled.
 * \param[in] offset The offset from the beginning of the chunk, at which the instruction
 *                   to be disassembled is located.
 *
 * \returns The offset of the next instruction or `offset + 1` if the byte was not
 *          recognized as a known instruction code.
 */
uint32_t disassembleInstruction(Chunk chunk[static 1], uint32_t offset);


#endif
