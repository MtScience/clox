#include <stdio.h>   /* fputc, fprintf, printf, vfprintf */
#include <stdarg.h>  /* For va_start, va_end, va_list */
#include <string.h>  /* For memcpy */

#include "common.h"
#include "compiler.h"
#include "memory.h"
#include "native.h"
#include "vm.h"

#ifdef DEBUG_TRACE_EXECUTION
#include "debug.h"
#endif


static void resetStack(VM* restrict vm) {
    vm->stackTop = vm->stack;
    vm->frameCount = 0;
    vm->openUpvalues = nullptr;
}


static Value peek(VM* vm, int distance) {
    return vm->stackTop[-1 - distance];
}


static bool isFalsey(Value value) [[unsequenced]] {
    return isNil(value) || (isBool(value) && !asBool(value));
}


static void concatenate(VM* restrict vm) {
    ObjString* b = asString(peek(vm, 0));
    ObjString* a = asString(peek(vm, 1));

    uint32_t length = a->length + b->length;
    char* chars = ALLOCATE(vm, char, length + 1);
    memcpy(chars, a->chars, a->length);
    memcpy(chars + a->length, b->chars, b->length);
    chars[length + 1] = '\0';

    ObjString* result = takeString(vm, chars, length);
    pop(vm);
    pop(vm);
    push(vm, objVal((Obj*)result));
}


static void runtimeError(VM* restrict vm, char const* format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);

    fputc('\n', stderr);

    for (int32_t i = vm->frameCount - 1; i >= 0; i--) {
        CallFrame* frame = &vm->frames[i];
        ObjFunction* function = frame->closure->function;
        uint32_t instruction = (uint32_t)(frame->ip - function->chunk.code - 1);
        fprintf(stderr, "[line %u] in ", function->chunk.lines[instruction]);
        if (!function->name) {
            fprintf(stderr, "script\n");
        } else {
            fprintf(stderr, "%s()\n", function->name->chars);
        }
    }

    resetStack(vm);
}


static bool call(VM*vm, ObjClosure* closure, uint8_t argCount) {
    if (argCount != closure->function->arity) {
        runtimeError(vm, "Expected %u arguments but got %u.",
                     closure->function->arity, argCount);
        return false;
    }

    if (vm->frameCount == framesMax) {
        runtimeError(vm, "Stack overflow.");
        return false;
    }

    CallFrame* frame = &vm->frames[vm->frameCount++];
    *frame = (CallFrame){
        .closure = closure,
        .ip = closure->function->chunk.code,
        .slots = vm->stackTop - argCount - 1,
    };
    return true;
}


static bool callValue(VM* vm, Value callee, uint8_t argCount) {
    if (isObj(callee)) {
        switch (objType(callee)) {
            case OBJ_CLOSURE:
                return call(vm, asClosure(callee), argCount);
            case OBJ_NATIVE: {
                    NativeFn native = asNative(callee);
                    Value result = native(argCount, vm->stackTop - argCount);
                    vm->stackTop -= argCount + 1;
                    push(vm, result);
                    return true;
                }
            case OBJ_CLASS: {
                    ObjClass* class = asClass(callee);
                    vm->stackTop[-argCount - 1] = objVal((Obj*)newInstance(vm, class));
                    return true;
                }
            default:
                break;
        }
    }
    runtimeError(vm, "Can only call functions and classes.");
    return false;
}


static ObjUpvalue* captureUpvalue(VM* vm, Value* local) {
    ObjUpvalue* prevUpvalue = nullptr;
    ObjUpvalue* upvalue = vm->openUpvalues;
    while (upvalue && upvalue->location > local) {
        prevUpvalue = upvalue;
        upvalue = upvalue->next;
    }

    if (upvalue && upvalue->location == local) {
        return upvalue;
    }

    ObjUpvalue* createdUpvalue = newUpvalue(vm, local);
    createdUpvalue->next = upvalue;

    if (!prevUpvalue) {
        vm->openUpvalues = createdUpvalue;
    } else {
        prevUpvalue->next = createdUpvalue;
    }

    return createdUpvalue;
}


static void closeUpvalues(VM* vm, Value* last) {
    while (vm->openUpvalues && vm->openUpvalues->location >= last) {
        ObjUpvalue* upvalue = vm->openUpvalues;
        upvalue->closed = *upvalue->location;
        upvalue->location = &upvalue->closed;
        vm->openUpvalues = upvalue->next;
    }
}


static void defineMethod(VM* restrict vm, ObjString* name) {
    Value method = peek(vm, 0);
    ObjClass* class = asClass(peek(vm, 1));
    tableSet(vm, &class->methods, name, method);
    pop(vm);
}


static InterpretResult run(VM* restrict vm) {
    CallFrame* frame = &vm->frames[vm->frameCount - 1];

#define READ_BYTE()     (*frame->ip++)
#define READ_SHORT()    (frame->ip += 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))
#define READ_CONSTANT() (frame->closure->function->chunk.constants.values[READ_BYTE()])
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
        disassembleInstruction(&frame->closure->function->chunk,
                               (uint32_t)(frame->ip - frame->closure->function->chunk.code));
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
                push(vm, frame->slots[slot]);
                break;
            }
            case OP_SET_LOCAL: {
                uint8_t slot = READ_BYTE();
                frame->slots[slot] = peek(vm, 0);
                break;
            }
            case OP_GET_UPVALUE: {
                uint8_t slot = READ_BYTE();
                push(vm, *frame->closure->upvalues[slot]->location);
                break;
            }
            case OP_SET_UPVALUE: {
                uint8_t slot = READ_BYTE();
                *frame->closure->upvalues[slot]->location = peek(vm, 0);
                break;
            }
            case OP_GET_PROPERTY: {
                if (!isInstance(peek(vm, 0))) {
                    runtimeError(vm, "Only instances have properties.");
                    return INTERPRET_RUNTIME_ERROR;
                }

                ObjInstance* instance = asInstance(peek(vm, 0));
                ObjString* name= READ_STRING();

                Value value;
                if (tableGet(&instance->fields, name, &value)) {
                    pop(vm);
                    push(vm, value);
                    break;
                }

                runtimeError(vm, "Undefined property '%s'.", name->chars);
                return INTERPRET_RUNTIME_ERROR;
            }
            case OP_SET_PROPERTY: {
                if (!isInstance(peek(vm, 1))) {
                    runtimeError(vm, "Only instances have fields.");
                    return INTERPRET_RUNTIME_ERROR;
                }

                ObjInstance* instance = asInstance(peek(vm, 1));
                tableSet(vm, &instance->fields, READ_STRING(), peek(vm, 0));
                Value value = pop(vm);
                pop(vm);
                push(vm, value);
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
                    if (tableSet(vm, &vm->globals, name, peek(vm, 0))) {
                        tableDelete(&vm->globals, name);
                        runtimeError(vm, "Undefined variable '%s'.", name->chars);
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    break;
                }
            case OP_DEFINE_GLOBAL: {
                    ObjString* name = READ_STRING();
                    tableSet(vm, &vm->globals, name, peek(vm, 0));
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
                    frame->ip -= offset;
                    break;
                }
            case OP_JUMP: {
                    uint16_t offset = READ_SHORT();
                    frame->ip += offset;
                    break;
                }
            case OP_JUMP_IF_FALSE: {
                    uint16_t offset = READ_SHORT();
                    if (isFalsey(peek(vm, 0))) {
                        frame->ip += offset;
                    }
                    break;
                }
            case OP_PRINT: {
                    printValue(pop(vm));
                    putchar('\n');
                    break;
                }
            case OP_CALL: {
                    uint8_t argCount = READ_BYTE();
                    if (!callValue(vm, peek(vm, argCount), argCount)) {
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    frame = &vm->frames[vm->frameCount - 1];
                    break;
                }
            case OP_CLOSURE: {
                    ObjFunction* function = asFunction(READ_CONSTANT());
                    ObjClosure* closure = newClosure(vm, function);
                    push(vm, objVal((Obj*)closure));

                    for (uint8_t i = 0; i < closure->upvalueCount; i++) {
                        uint8_t isLocal = READ_BYTE();
                        uint8_t index = READ_BYTE();

                        if (isLocal) {
                            closure->upvalues[i] = captureUpvalue(vm, frame->slots + index);
                        } else {
                            closure->upvalues[i] = frame->closure->upvalues[index];
                        }
                    }

                    break;
                }
            case OP_CLOSE_UPVALUE: {
                    closeUpvalues(vm, vm->stackTop - 1);
                    pop(vm);
                    break;
                }
            case OP_RETURN: {
                    Value result = pop(vm);
                    closeUpvalues(vm, frame->slots);
                    vm->frameCount--;
                    if (vm->frameCount == 0) {
                        pop(vm);
                        return INTERPRET_OK;
                    }

                    vm->stackTop = frame->slots;
                    push(vm, result);
                    frame = &vm->frames[vm->frameCount - 1];
                    break;
                }
            case OP_CLASS: {
                    push(vm, objVal((Obj*)newClass(vm, READ_STRING())));
                    break;
                }
            case OP_METHOD: {
                    defineMethod(vm, READ_STRING());
                    break;
                }
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

    vm->grayCount = 0;
    vm->grayCapacity = 0;
    vm->grayStack = nullptr;

    vm->bytesAllocated = 0;
    vm->nextGC = 1024 * 1024;

    defineNative(vm, "clock", clockNative);
}


void freeVM(VM* restrict vm) {
    freeTable(vm, &vm->strings);
    freeTable(vm, &vm->globals);
    freeObjects(vm);
}


InterpretResult interpret(VM* restrict vm, char const* source) {
    ObjFunction* function = compile(vm, source);

    if (!function)
        return INTERPRET_COMPILE_ERROR;

    push(vm, objVal((Obj*)function));
    ObjClosure* closure = newClosure(vm, function);
    pop(vm);
    push(vm, objVal((Obj*)closure));
    call(vm, closure, 0);

    return run(vm);
}


void push(VM* vm, Value value) {
    *vm->stackTop = value;
    vm->stackTop++;
}


Value pop(VM* vm) {
    vm->stackTop--;
    return *vm->stackTop;
}
