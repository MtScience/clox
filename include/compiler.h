#ifndef clox_compiler_h
#define clox_compiler_h


#include "object.h"
#include "vm.h"


typedef struct Compiler Compiler;


ObjFunction* compile(VM vm[static 1], char const* source);

void markCompilerRoots(VM vm[static 1], Compiler* compiler);


#endif
