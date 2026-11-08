#ifndef clox_native_h
#define clox_native_h


#include "common.h"
#include "vm.h"
#include "object.h"
#include "value.h"


/**
 * \brief Defines a Lox native function.
 *
 * Takes a `VM`, a pointer to a C function that implements the behaviour and the string
 * representing the function name and creates a reference to the function in the `VM::globals`
 * field of the vm.
 *
 * \param[in] vm       Non-`NULL` pointer to the virtual machine in which to define the function.
 * \param[in] name     String that contains the function name.
 * \param[in] function Pointer to the function of signature `NativeFn`.
 */
void defineNative(VM vm[static 1], char const* name, NativeFn function);

/**
 * \brief CPU clock function.
 *
 * An example Lox native function that implements timing. Defined in terms of C's `clock()`
 * function.
 *
 * \param[in] argCount Number of arguments the function takes. \b Unused.
 * \param[in] args     Array of arguments. \b Unused.
 *
 * \returns The result of C's `clock()` divided by `CLOCKS_PER_SEC`, wrapped in a Lox value.
 */
Value clockNative(uint8_t argCount, Value* args);


#endif
