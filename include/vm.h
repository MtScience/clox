#ifndef clox_vm_h
#define clox_vm_h


typedef struct VM VM;


#include "gc.h"
#include "object.h"
#include "table.h"
#include "value.h"


/**
 * \brief Maximum number of call frames the stack can hold before overflowing.
 */
constexpr uint16_t framesMax = 64;

/**
 * \brief Maximum number of values the stack can hold.
 *
 * Number of values for a call frame must not exceed `uint8Count`.
 */
static constexpr uint16_t stackMax = framesMax * uint8Count;


/**
 * \struct CallFrame vm.h
 * \brief Call frame type.
 *
 * A `struct` that represents a Lox call frame. A call frame keeps references to the
 * chunk it represents and the first stack slot it is allowed to use.
 */
typedef struct {
    ObjClosure* closure;  /**< Pointer to the closure being called. */
    uint8_t*    ip;       /**< Pointer to the next instruction in the `CallFrame::closure`'s
                           *   chunk to be executed.
                           */
    Value*      slots;    /**< Pointer to the first value on the stack that the frame can
                           *   reference (start of the frame).
                           */
} CallFrame;


/**
 * \struct VM vm.h
 * \brief Virtual machine type.
 *
 * A `struct` that represents a Lox virtual machine. It keeps a call stack, a garbage
 * collector, references to all the strings in the user's code and other data.
 */
struct VM {
    CallFrame   frames[framesMax];  /**< List of currently active call frames. */
    uint16_t    frameCount;         /**< Number of currently active call frames. */

    Value       stack[stackMax];    /**< The value stack. */
    Value*      stackTop;           /**< Pointer to the stack slot 1 past the topmost occupied one. */
    Table       strings;            /**< Hash set of all the strings in the code (including
                                     *   identifiers). Used for string interning.
                                     */
    Table       globals;            /**< Hash map of all the global names in the code being run. */

    ObjString*  initString;         /**< Name for class initializer methods. */
    ObjUpvalue* openUpvalues;       /**< Linked list of `ObjUpvalue`s referring to values still on
                                      *  the stack.
                                      */

    GC          gc;                 /**< The virtual machine's associated garbage collector. */
};


/**
 * \brief Interpretation result type.
 *
 * An `enum` that lists all the interpretation result codes.
 */
typedef enum : uint8_t {
    INTERPRET_OK,             /**< Indicates a successful run. */
    INTERPRET_COMPILE_ERROR,  /**< Indicates a compilation error (invalid syntax, \e etc.). */
    INTERPRET_RUNTIME_ERROR,  /**< Indicates a runtime error (division by zero, \e etc.). */
} InterpretResult;


/* Interface */

/**
 * \brief Initializes a virtual machine.
 *
 * Takes a newly-created `VM` and fills in its fields, putting the structure
 * in a well-defined state.
 *
 * \param[in,out] vm Non-`NULL` pointer to the virtual machine to be initialized.
 */
void initVM(VM vm[restrict static 1]);

/**
 * \brief Frees a virtual machine.
 *
 * Takes a `VM` and frees the memory occupied by it.
 *
 * \param[in] vm Non-`NULL` pointer to the virtual machine to be freed.
 */
void freeVM(VM vm[restrict static 1]);


/**
 * \brief Compiles and runs Lox source code.
 *
 * This function implements the main logic of the whole project. Takes a `VM` and a piece of
 * source code and run that source with the given virtual machine.
 *
 * \param[in] vm     Non-`NULL` pointer to the virtual machine to run the source with.
 * \param[in] source C string containing the source code to be run.
 */
InterpretResult interpret(VM vm[static 1], char const* source);


/**
 * \brief Pushes a value onto a virtual machine's stack.
 *
 * \param[in] vm    Non-`NULL` pointer to the virtual machine whose stack to push the value to.
 * \param[in] value The value to be pushed.
 */
void push(VM vm[static 1], Value value);

/**
 * \brief Pops a value from a virtual machine's stack.
 *
 * \param[in] vm Non-`NULL` pointer to the virtual machine whose stack to pop from.
 *
 * \returns The removed Lox value.
 */
Value pop(VM vm[static 1]);


#endif
