#ifndef clox_compiler_h
#define clox_compiler_h


#include "object.h"
#include "vm.h"


typedef struct Compiler Compiler;


ObjFunction* compile(VM* restrict vm, char const* source);

void markCompilerRoots(VM* restrict vm, Compiler* compiler);


#endif
