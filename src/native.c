#include <time.h>    /* For clock */
#include <string.h>  /* For strlen */

#include "native.h"
#include "table.h"


void defineNative(VM vm[static 1], char const* name, NativeFn function) {
    push(vm, objVal((Obj*)copyString(vm, name, (uint32_t)strlen(name))));
    push(vm, objVal((Obj*)newNative(&vm->gc, function)));

    tableSet(&vm->gc, &vm->globals, asString(vm->stack[0]), vm->stack[1]);

    pop(vm);
    pop(vm);
}


Value clockNative([[maybe_unused]] uint8_t argCount, [[maybe_unused]] Value* args) {
    return numberVal((double)clock() / CLOCKS_PER_SEC);
}
