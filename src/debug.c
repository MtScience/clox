#include <stdio.h>  /* For printf, puts */

#include "debug.h"
#include "object.h"
#include "value.h"


/**
 * \brief Prints a constant instruction.
 *
 * This function defines the output format for instructions that refer to various constants
 * (`OP_CONSTANT`, `OP_GET_PROPERTY` \e etc.). The format is:
 * \code{.unparsed}
 * [command name] [argument's index in the constant table] '[argument's value]'
 * \endcode
 *
 * \e E.g., for an instruction loading a value `10`, located at index 1 in the chunk's constant
 * table, onto the stack this function would produce this output:
 * \code{.unprocessed}
 * OP_CONSTANT      1 '10'
 * \endcode
 *
 * \param[in] name   A C string representing the command name.
 * \param[in] chunk  Non-`NULL` pointer to the chunk containing the instruction  to be printed.
 * \param[in] offset Offset of the instruction from the beginning of the chunk.
 *
 * \returns The offset of the next instruction (`offset + 2`).
 */
static uint32_t constantInstruction(char const* name, Chunk chunk[static 1], uint32_t offset) {
    uint8_t constant = chunk->code[offset + 1];
    printf("%-16s %4u '", name, constant);
    printValue(chunk->constants.values[constant]);
    puts("'");

    return offset + 2;
}


/**
 * \brief Prints an instruction with an argument.
 *
 * This function defines the output format for instructions that refer to bytes somewhere else
 * in the chunk's code (`OP_GET_LOCAL`, `OP_CALL`, \e etc.). The format is:
 * \code{.unparsed}
 * [command name] [argument's index in the code]
 * \endcode
 *
 * \e E.g., for an instruction loading a local variable, whose value is located at index 1 in
 * the chunk's code, this function would produce this output:
 * \code{.unprocessed}
 * OP_GET_LOCAL     1
 * \endcode
 *
 * \param[in] name   A C string representing the command name.
 * \param[in] chunk  Non-`NULL` pointer to the chunk containing the instruction  to be printed.
 * \param[in] offset Offset of the instruction from the beginning of the chunk.
 *
 * \returns The offset of the next instruction (`offset + 2`).
 */
static uint32_t byteInstruction(char const* name, Chunk chunk[static 1], uint32_t offset) {
    uint8_t slot = chunk->code[offset + 1];
    printf("%-16s %4u\n", name, slot);

    return offset + 2;
}


/**
 * \brief Prints a jump instruction.
 *
 * This function defines the output format for instructions that perform various kinds of jumps
 * (`OP_JUMP`, `OP_JUMP_IF_FALSE` and `OP_LOOP`). The format is
 * \code{.unparsed}
 * [command name] [start point instruction index] -> [destination instruction index]
 * \endcode
 *
 * \e E.g., for an loop instruction that defines a jump from instruction 28 to instruction 14
 * this function would produce this output:
 * \code{.unprocessed}
 * OP_LOOP          28 -> 14
 * \endcode
 *
 * \param[in] name   A C string representing the command name.
 * \param[in] sign   Direction of the jump. Positive for forward jumps, negative or zero for
 *                   backward jumps (loops).
 * \param[in] chunk  Non-`NULL` pointer to the chunk containing the instruction  to be printed.
 * \param[in] offset Offset of the instruction from the beginning of the chunk.
 *
 * \returns The offset of the next instruction (`offset + 3`).
 */
static uint32_t jumpInstruction(char const* name, int8_t sign, Chunk chunk[static 1], uint32_t offset) {
    uint16_t jump = (uint16_t)(chunk->code[offset + 1] << 8);
    jump |= chunk->code[offset + 2];

    /*
     * NOTE: Doing this instead of just (offset + 3 + sign * jump)
     * in order to avoid potentially dangerous implicit cast
     */
    uint32_t target = offset + 3;
    if (sign > 0) {
        target += jump;
    } else {
        target -= jump;
    }

    printf("%-16s %4u -> %u\n", name, offset, target);

    return offset + 3;
}


/**
 * \brief Prints a basic instruction.
 *
 * This function defines the output format for basic instructions that take no argument
 * (`OP_ADD` \e etc.) The format is:
 * \code{.unparsed}
 * [command name]
 * \endcode
 *
 * \e E.g., for an addition of two numbers on the top of the Lox stack this function would
 * produce this output:
 * \code{.unprocessed}
 * OP_ADD
 * \endcode
 *
 * \param[in] name   A C string representing the command name.
 * \param[in] offset Offset of the instruction from the beginning of the chunk.
 *
 * \returns The offset of the next instruction (`offset + 1`).
 */
static uint32_t simpleInstruction(char const* name, uint32_t offset) {
    puts(name);
    return offset + 1;
}


/**
 * \brief Prints an invocation instruction.
 *
 * This function defines the output format for invocation instructions (`OP_INVOKE` and
 * `OP_SUPER_INVOKE`). The format is
 * \code{.unparsed}
 * [command name] ([no. of arguments] args) [method name no. in the constant table] '[method name]'
 * \endcode
 *
 * \e E.g., for an invocation of a method called "add", that takes 2 arguments and whose name is
 * saved under no. 1 in the chunk's constant table, this function would produce this output:
 * \code{.unprocessed}
 * OP_INVOKE        (2 args) 1 'add'
 * \endcode
 *
 * \param[in] name   A C string representing the command name.
 * \param[in] chunk  Non-`NULL` pointer to the chunk containing the instruction  to be printed.
 * \param[in] offset Offset of the instruction from the beginning of the chunk.
 *
 * \returns The offset of the next instruction (`offset + 3`).
 */
static uint32_t invokeInstruction(char const* name, Chunk chunk[static 1], uint32_t offset) {
    uint8_t constant = chunk->code[offset + 1];
    uint8_t argCount = chunk->code[offset + 2];
    printf("%-16s (%u args) %4u '", name, argCount, constant);
    printValue(chunk->constants.values[constant]);
    puts("'");

    return offset + 3;
}


/* Public functions */

void disassembleChunk(Chunk chunk[static 1], char const* name) {
    printf("== %s ==\n", name);

    for (uint32_t offset = 0; offset < chunk->count; ) {
        offset = disassembleInstruction(chunk, offset);
    }
}


uint32_t disassembleInstruction(Chunk chunk[static 1], uint32_t offset) {
    printf("%04u ", offset);

    if (offset > 0 && chunk->lines[offset] == chunk->lines[offset - 1]) {
        printf("   | ");
    } else {
        printf("%4u ", chunk->lines[offset]);
    }

    uint8_t instruction = chunk->code[offset];
    switch (instruction) {
        case OP_CONSTANT:      return constantInstruction("OP_CONSTANT",      chunk, offset);
        case OP_CLASS:         return constantInstruction("OP_CLASS",         chunk, offset);
        case OP_METHOD:        return constantInstruction("OP_METHOD",        chunk, offset);
        case OP_GET_PROPERTY:  return constantInstruction("OP_GET_PROPERTY",  chunk, offset);
        case OP_SET_PROPERTY:  return constantInstruction("OP_SET_PROPERTY",  chunk, offset);
        case OP_GET_GLOBAL:    return constantInstruction("OP_GET_GLOBAL",    chunk, offset);
        case OP_SET_GLOBAL:    return constantInstruction("OP_SET_GLOBAL",    chunk, offset);
        case OP_DEFINE_GLOBAL: return constantInstruction("OP_DEFINE_GLOBAL", chunk, offset);
        case OP_GET_SUPER:     return constantInstruction("OP_GET_SUPER",     chunk, offset);
        case OP_LOOP:          return jumpInstruction("OP_LOOP",          -1, chunk, offset);
        case OP_JUMP:          return jumpInstruction("OP_JUMP",          1,  chunk, offset);
        case OP_JUMP_IF_FALSE: return jumpInstruction("OP_JUMP_IF_FALSE", 1,  chunk, offset);
        case OP_GET_LOCAL:     return byteInstruction("OP_GET_LOCAL",   chunk, offset);
        case OP_SET_LOCAL:     return byteInstruction("OP_SET_LOCAL",   chunk, offset);
        case OP_GET_UPVALUE:   return byteInstruction("OP_GET_UPVALUE", chunk, offset);
        case OP_SET_UPVALUE:   return byteInstruction("OP_SET_UPVALUE", chunk, offset);
        case OP_CALL:          return byteInstruction("OP_CALL",        chunk, offset);
        case OP_INVOKE:        return invokeInstruction("OP_INVOKE",       chunk, offset);
        case OP_SUPER_INVOKE:  return invokeInstruction("OP_SUPER_INVOKE", chunk, offset);
        case OP_NIL:           return simpleInstruction("OP_NIL",           offset);
        case OP_TRUE:          return simpleInstruction("OP_TRUE",          offset);
        case OP_FALSE:         return simpleInstruction("OP_FALSE",         offset);
        case OP_POP:           return simpleInstruction("OP_POP",           offset);
        case OP_NEGATE:        return simpleInstruction("OP_NEGATE",        offset);
        case OP_ADD:           return simpleInstruction("OP_ADD",           offset);
        case OP_SUBTRACT:      return simpleInstruction("OP_SUBTRACT",      offset);
        case OP_MULTIPLY:      return simpleInstruction("OP_MULTIPLY",      offset);
        case OP_DIVIDE:        return simpleInstruction("OP_DIVIDE",        offset);
        case OP_NOT:           return simpleInstruction("OP_NOT",           offset);
        case OP_EQUAL:         return simpleInstruction("OP_EQUAL",         offset);
        case OP_GREATER:       return simpleInstruction("OP_GREATER",       offset);
        case OP_LESS:          return simpleInstruction("OP_LESS",          offset);
        case OP_PRINT:         return simpleInstruction("OP_PRINT",         offset);
        case OP_RETURN:        return simpleInstruction("OP_RETURN",        offset);
        case OP_INHERIT:       return simpleInstruction("OP_INHERIT",       offset);
        case OP_CLOSE_UPVALUE: return simpleInstruction("OP_CLOSE_UPVALUE", offset);
        case OP_CLOSURE: {
                offset++;
                uint8_t constant = chunk->code[offset++];
                printf("%-16s %4u ", "OP_CLOSURE", constant);
                printValue(chunk->constants.values[constant]);
                putchar('\n');

                ObjFunction* function = asFunction(chunk->constants.values[constant]);
                for (uint8_t i = 0; i < function->upvalueCount; i++) {
                    uint8_t isLocal = chunk->code[offset++];
                    uint8_t index = chunk->code[offset++];
                    printf("%04u      |                     %s %u\n",
                           offset - 2, isLocal ? "local" : "upvalue", index);
                }

                return offset;
            }
        default:
            printf("Unknown opcode %u\n", instruction);
            return offset + 1;
    }
}
