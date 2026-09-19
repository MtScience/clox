#include <stdio.h>   /* fputc, fprintf, printf, vfprintf */
#include <stdarg.h>  /* For va_start, va_end, va_list */
#include <string.h>  /* For memcpy */

#include "common.h"
#include "compiler.h"
#include "object.h"
#include "memory.h"
#include "vm.h"

#ifdef DEBUG_TRACE_EXECUTION
#include "debug.h"
#endif


static void resetStack(VM* restrict vm) {
    vm->stackTop = vm->stack;
}


static Value peek(VM* vm, int distance) {
    return vm->stackTop[-1 - distance];
}


static bool isFalsey(Value value) [[unsequenced]] {
    return isNil(value) || (isBool(value) && !asBool(value));
}


static void concatenate(VM* vm) {
    ObjString* b = asString(pop(vm));
    ObjString* a = asString(pop(vm));

    uint32_t length = a->length + b->length;
    char* chars = ALLOCATE(char, length + 1);
    memcpy(chars, a->chars, a->length);
    memcpy(chars + a->length, b->chars, b->length);
    chars[length + 1] = '\0';

    ObjString* result = takeString(vm, chars, length);
    push(vm, objVal((Obj*)result));
}


static void runtimeError(VM* restrict vm, char const* format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputc('\n', stderr);

    size_t instruction = (size_t)(vm->ip - vm->chunk->code - 1);
    uint32_t line = vm->chunk->lines[instruction];
    fprintf(stderr, "[line %u] in script\n", line);

    resetStack(vm);
}


static InterpretResult run(VM* restrict vm) {
#define READ_BYTE()     (*vm->ip++)
#define READ_SHORT()    (vm->ip += 2, (uint16_t)((vm->ip[-2] << 8) | vm->ip[-1]))
#define READ_CONSTANT() (vm->chunk->constants.values[READ_BYTE()])
#define READ_STRING()   asString(READ_CONSTANT())
#define BINARY_OP(valueType, op)                                \
    do {                                                        \
        if (!isNumber(peek(vm, 0)) || !isNumber(peek(vm, 1))) { \
            runtimeError(vm, "Operands must be numbers.");      \
            return INTERPRET_RUNTIME_ERROR;                     \
        }                                                       \
        double b = asNumber(pop(vm));                           \
        double a = asNumber(pop(vm));                           \
        push(vm, valueType(a op b));                            \
    } while (false)

    for (;;) {
#ifdef DEBUG_TRACE_EXECUTION
        printf("          ");
        for (Value* slot = vm->stack; slot < vm->stackTop; slot++) {
            printf("[ ");
            printValue(*slot);
            printf(" ]");
        }
        putchar('\n');
        disassembleInstruction(vm->chunk, (uint32_t)(vm->ip - vm->chunk->code));
#endif
        uint8_t instruction;
        switch (instruction = READ_BYTE()) {
            case OP_CONSTANT: {
                Value constant = READ_CONSTANT();
                push(vm, constant);
                break;
            }
            case OP_NEGATE:
                if (!isNumber(peek(vm, 0))) {
                    runtimeError(vm, "Operand must be a number");
                    return INTERPRET_RUNTIME_ERROR;
                }
                push(vm, numberVal(-asNumber(pop(vm))));
                break;
            case OP_NIL:   push(vm, nilVal());       break;
            case OP_TRUE:  push(vm, boolVal(true));  break;
            case OP_FALSE: push(vm, boolVal(false)); break;
            case OP_GET_LOCAL: {
                uint8_t slot = READ_BYTE();
                push(vm, vm->stack[slot]);
                break;
            }
            case OP_SET_LOCAL: {
                uint8_t slot = READ_BYTE();
                vm->stack[slot] = peek(vm, 0);
                break;
            }
            case OP_GET_GLOBAL: {
                    ObjString* name = READ_STRING();
                    Value value;
                    if (!tableGet(&vm->globals, name, &value)) {
                        runtimeError(vm, "Undefined variable '%s'.", name->chars);
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    push(vm, value);
                    break;
                }
            case OP_SET_GLOBAL: {
                    ObjString* name = READ_STRING();
                    if (tableSet(&vm->globals, name, peek(vm, 0))) {
                        tableDelete(&vm->globals, name);
                        runtimeError(vm, "Undefined variable '%s'.", name->chars);
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    break;
                }
            case OP_DEFINE_GLOBAL: {
                    ObjString* name = READ_STRING();
                    tableSet(&vm->globals, name, peek(vm, 0));
                    pop(vm);
                    break;
                }
            case OP_EQUAL: {
                    Value b = pop(vm);
                    Value a = pop(vm);
                    push(vm, boolVal(valuesEqual(a, b)));
                    break;
                }
            case OP_GREATER:  BINARY_OP(boolVal, >); break;
            case OP_LESS:     BINARY_OP(boolVal, <); break;
            case OP_ADD:
                    if (isString(peek(vm, 0)) && isString(peek(vm, 1))) {
                        concatenate(vm);
                    } else if (isNumber(peek(vm, 0)) && isNumber(peek(vm, 1))) {
                        double b = asNumber(pop(vm));
                        double a = asNumber(pop(vm));
                        push(vm, numberVal(a + b));
                    } else {
                        runtimeError(vm, "Operands must be two numbers or two strings.");
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    break;
            case OP_SUBTRACT: BINARY_OP(numberVal, -); break;
            case OP_MULTIPLY: BINARY_OP(numberVal, *); break;
            case OP_DIVIDE:   BINARY_OP(numberVal, /); break;
            case OP_NOT:
                push(vm, boolVal(isFalsey(pop(vm))));
                break;
            case OP_POP: pop(vm); break;
            case OP_LOOP: {
                    uint16_t offset = READ_SHORT();
                    vm->ip -= offset;
                    break;
                }
            case OP_JUMP: {
                    uint16_t offset = READ_SHORT();
                    vm->ip += offset;
                    break;
                }
            case OP_JUMP_IF_FALSE: {
                    uint16_t offset = READ_SHORT();
                    if (isFalsey(peek(vm, 0)))
                        vm->ip += offset;
                    break;
                }
            case OP_PRINT: {
                    printValue(pop(vm));
                    putchar('\n');
                    break;
                }
            case OP_RETURN: return INTERPRET_OK;
        }
    }

#undef BINARY_OP
#undef READ_STRING
#undef READ_CONSTANT
#undef READ_SHORT
#undef READ_BYTE

    unreachable();
    return INTERPRET_OK;
}


void initVM(VM* restrict vm) {
    resetStack(vm);
    vm->objects = nullptr;
    initTable(&vm->globals);
    initTable(&vm->strings);
}


void freeVM(VM* restrict vm) {
    freeTable(&vm->strings);
    freeTable(&vm->globals);
    freeObjects(vm);
}


InterpretResult interpret(VM* restrict vm, char const* source) {
    Chunk chunk;
    initChunk(&chunk);

    if (!compile(vm, source, &chunk)) {
        freeChunk(&chunk);
        return INTERPRET_COMPILE_ERROR;
    }

    vm->chunk = &chunk;
    vm->ip = vm->chunk->code;

    InterpretResult result = run(vm);

    freeChunk(&chunk);
    return result;
}


void push(VM* vm, Value value) {
    *vm->stackTop = value;
    vm->stackTop++;
}


Value pop(VM* vm) {
    vm->stackTop--;
    return *vm->stackTop;
}
