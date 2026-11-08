#ifndef clox_compiler_h
#define clox_compiler_h


#include "object.h"
#include "vm.h"


/**
 * \brief Compiles a piece of Lox source code.
 *
 * Takes a `VM` and a string containing the code to be compiled. Returns a pointer
 * to an object that represents a Lox function. Each call sets up its own scanner,
 * parser, compilation state \e etc.
 *
 * \param[in,out] vm     Non-`NULL` pointer to the VM that will execute the compiled
 *                       code.
 * \param[in]     source The source code to be compiled.
 *
 * \returns The pointer to an `ObjFunction` that contains the compiled code or `NULL`
 *          in case compilation failed.
 */
ObjFunction* compile(VM vm[static 1], char const* source);


#endif
