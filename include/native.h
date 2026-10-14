#ifndef clox_native_h
#define clox_native_h


#include "common.h"
#include "vm.h"
#include "object.h"
#include "value.h"


void defineNative(VM vm[static 1], char const* name, NativeFn function);

Value clockNative(uint8_t argCount, Value* args);


#endif
