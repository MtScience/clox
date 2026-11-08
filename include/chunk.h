#ifndef clox_chunk_h
#define clox_chunk_h


/**
 * \brief Forward declares the `VM` structure.
 */
typedef struct VM VM;


#include "common.h"
#include "value.h"
#include "gc.h"

/**
 * \brief Operation code type.
 *
 * An `enum` that lists all the supported instruction codes.
 */
typedef enum : uint8_t {
    OP_CONSTANT,      /**< Reads the value specified by the argument from the
                       * `Chunk::constants` field of the current chunk and pushes it onto the stack. */
    OP_NEGATE,        /**< Negates the number at the top of the stack. */
    OP_NIL,           /**< Pushes a `nil` value onto the stack. */
    OP_TRUE,          /**< Pushes a `true` value onto the stack. */
    OP_FALSE,         /**< Pushes a `false` value onto the stack. */
    OP_POP,           /**< Pops the topmost value from the stack. */
    OP_GET_LOCAL,     /**< Finds the value on the stack that corresponds to the local variable and pushes it to the top
                       *   of the stack */
    OP_SET_LOCAL,     /**< Finds the value on the stack that corresponds to the local variable and changes it to the
                       *   value at the top of the stack. */
    OP_GET_UPVALUE,   /**<  */
    OP_SET_UPVALUE,   /**<  */
    OP_GET_PROPERTY,  /**<  */
    OP_SET_PROPERTY,  /**<  */
    OP_GET_GLOBAL,    /**<  */
    OP_SET_GLOBAL,    /**<  */
    OP_DEFINE_GLOBAL, /**< Creates a new entry in the `VM::globals` hash table and sets it to the value at the
                       *   top of the stack. */
    OP_EQUAL,         /**< Checks the two topmost values on the stack for equality. */
    OP_GREATER,       /**< Checks whether the topmost number on the stack is less than the one below it. */
    OP_LESS,          /**< Checks whether the topmost number on the stack is greater than the one below it. */
    OP_ADD,           /**< Combines two topmost values on the stack. Adds numbers, concatenates strings. */
    OP_SUBTRACT,      /**< Subtracts the topmost number on the stack from the one below it. */
    OP_MULTIPLY,      /**< Multiplies the two topmost numbers on the stack. */
    OP_DIVIDE,        /**< Divides the second number from the top by the topmost number on the stack. */
    OP_NOT,           /**< Negates the boolean at the top of the stack. */
    OP_LOOP,          /**< Unconditionally decreases the instruction pointer by the amount specified by argument. */
    OP_JUMP,          /**< Unconditionally increases the instruction pointer by the amount specified by argument. */
    OP_JUMP_IF_FALSE, /**< Increases the instruction pointer if the topmost value on the stack is false. */
    OP_PRINT,         /**< Prints the topmost value on the stack. */
    OP_CALL,          /**<  */
    OP_CLOSURE,       /**< Creates a new closure from the function at the top of the stack. */
    OP_CLOSE_UPVALUE, /**<  */
    OP_RETURN,        /**< Indicates return from a function or end of script. */
    OP_CLASS,         /**< Defines a new class. */
    OP_METHOD,        /**< Defines a new method. */
    OP_INVOKE,        /**< Calls a method (optimized invocation cases like `obj.method()`). */
    OP_SUPER_INVOKE,  /**< Calls a superclass method (optimized invocation). */
    OP_INHERIT,       /**<  */
    OP_GET_SUPER,     /**<  */
} OpCode;


/**
 * \struct Chunk chunk.h
 * \brief Code chunk type.
 *
 * A `struct` that represents a chunk of code. A chunk is compiled from top-level code
 * (a script or REPL input) or a function. A chunk contains its own list of constant values
 * (literal numbers and strings, as well as identifiers). Lox `nil`, `true` and `false` do
 * not count as constants and are instead put into `Chunk::code` as `OP_NIL` \e etc.
 */
typedef struct {
    uint32_t   count;      /**< Number of bytes in the chunk. */
    uint32_t   capacity;   /**< Maximum number of bytes the chunk can hold. While theoretically
                            *   unlimited, it cannot exceed 4294967295 (otherwise, the counter would
                            *   overflow).
                            */
    uint8_t*   code;       /**< Bytes (instructions and their arguments) that constitute the chunk. */
    uint32_t*  lines;      /**< For a given index \a i specifies the source line, from
                            *   which the byte at \a i in `code` was compiled.
                            */
    ValueArray constants;  /**< List of constant values defined in the chunk. */
} Chunk;


/* Interface */


/**
 * \brief Initializes a chunk.
 *
 * Takes a newly-created `Chunk` and fills in its fields, putting the structure
 * in a well-defined state.
 *
 * \param[in,out] chunk Non-`NULL` pointer to the chunk to be initialized.
 */
void initChunk(Chunk chunk[restrict static 1]);

/**
 * \brief Frees a chunk.
 *
 * Takes a `Chunk` and frees the memory occupied by it.
 *
 * \param[in] gc    Non-`NULL` pointer to a `GC` structure. `gc` must point to the same structure
 *                  that was used for filling the chunk.
 * \param[in] chunk Non-`NULL` pointer to the chunk to be freed.
 */
void freeChunk(GC gc[restrict static 1], Chunk chunk[restrict static 1]);

/**
 * \brief Writes a byte to a chunk.
 *
 * Takes a `Chunk` and a `uint8_t` (byte) and writes the byte into the `Chunk::code` field of the
 * chunk. The byte can be an arbitrary `uint8_t` value (since this function is also used for
 * writing instruction arguments).
 *
 * \param[in] gc    Non-`NULL` pointer to a `GC` structure. To avoid problems with freeing and
 *                  garbage collection, this should always point to the same structure for a
 *                  given chunk.
 * \param[in] chunk Non-`NULL` pointer to the chunk to write the byte into.
 * \param[in] byte  The byte to be written.
 * \param[in] line  The line number corresponding to the byte being written.
 */
void writeChunk(GC gc[restrict static 1], Chunk chunk[static 1], uint8_t byte, uint32_t line);

/**
 * \brief Creates a constant in a chunk.
 *
 * Takes a `Chunk` and a `Value` and writes the value into the `Chunk::constants` field of the
 * chunk. Takes care not to let the garbage collector delete a value too early, by putting it
 * onto the stack of the VM, and popping it after it is safely written into the chunk.
 *
 * \param[in] vm    Non-`NULL` pointer to the VM governing the chunk.
 * \param[in] chunk Non-`NULL` pointer to the chunk to write the value into.
 * \param[in] value The value to be written.
 *
 * \returns The index of the newly-written value in the chunk's `constants` array.
 */
uint32_t addConstant(VM* vm, Chunk chunk[static 1], Value value);


#endif
