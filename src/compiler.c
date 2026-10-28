#include <stdio.h>   /* For fprintf, stderr */
#include <stdlib.h>  /* For strtod */
#include <string.h>  /* For memcmp */

#include "common.h"
#include "compiler.h"
#include "memory.h"
#include "table.h"
#include "scanner.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif


typedef enum : uint8_t {
    PREC_NONE,
    PREC_ASSIGNMENT,  /* =         */
    PREC_OR,          /* or        */
    PREC_AND,         /* and       */
    PREC_EQUALITY,    /* == !=     */
    PREC_COMPARISON,  /* < > <= >= */
    PREC_TERM,        /* + -       */
    PREC_FACTOR,      /* * /       */
    PREC_UNARY,       /* ! -       */
    PREC_CALL,        /* . ()      */
    PREC_PRIMARY,
} Precedence;


typedef enum : uint8_t {
    TYPE_SCRIPT,
    TYPE_FUNCTION,
    TYPE_METHOD,
    TYPE_INITIALIZER,
} FunctionType;


typedef struct {
    Token current;
    Token previous;
    bool hadError;
    bool panicMode;
} Parser;


typedef struct {
    Token name;
    int32_t depth;
    bool isCaptured;
} Local;


typedef struct {
    uint8_t index;
    bool isLocal;
} Upvalue;


typedef struct Compiler {
    ObjFunction* function;
    FunctionType type;

    Upvalue upvalues[uint8Count];
    Local locals[uint8Count];
    int16_t localCount;
    int16_t scopeDepth;

    struct Compiler* enclosing;
} Compiler;


typedef struct ClassCompiler {
    struct ClassCompiler* enclosing;
} ClassCompiler;


typedef struct {
    VM* vm;
    Compiler* compiler;
    ClassCompiler* classCompiler;
    Parser* parser;
    Scanner* scanner;

    void (*prevMarkRoots)(GC* gc, void* arg);
    void* prevMarkArg;
    void (*prevCleanupWeakRefs)(void* arg);
    void* prevWeakArg;
} CompilationState;


typedef void (*ParseFn)(CompilationState state[static 1], bool canAssign);


typedef struct {
    ParseFn prefix;
    ParseFn infix;
    Precedence precedence;
} ParseRule;


static Chunk* currentChunk(Compiler* compiler) {
    return &compiler->function->chunk;
}


static void errorAt(
    Parser parser[static 1],
    Token token[static 1],
    char const* message
) {
    if (parser->panicMode) {
        return;
    }

    parser->panicMode = true;

    fprintf(stderr, "[line %u] Error", token->line);
    if (token->type == TOKEN_EOF) {
        fprintf(stderr, " at end");
    } else if (token->type != TOKEN_ERROR) {
        fprintf(stderr, " at '%.*s'", token->length, token->start);
    }

    fprintf(stderr, ": %s\n", message);
    parser->hadError = true;
}


static void errorAtCurrent(Parser parser[static 1], char const* message) {
    errorAt(parser, &parser->current, message);
}


static void error(Parser parser[static 1], char const* message) {
    errorAt(parser, &parser->previous, message);
}


static void advance(Parser parser[static 1], Scanner scanner[static 1]) {
    parser->previous = parser->current;

    for (;;) {
        parser->current = scanToken(scanner);
        if (parser->current.type != TOKEN_ERROR) {
            break;
        }

        errorAtCurrent(parser, parser->current.start);
    }
}


static bool check(Parser parser[static 1], TokenType type) {
    return parser->current.type == type;
}


static bool match(Parser parser[static 1], Scanner scanner[static 1], TokenType type) {
    if (!check(parser, type)) {
        return false;
    }

    advance(parser, scanner);
    return true;
}


static void consume(
    Parser parser[static 1],
    Scanner scanner[static 1],
    TokenType type,
    char const* message
) {
    if (parser->current.type == type) {
        advance(parser, scanner);
        return;
    }

    errorAtCurrent(parser, message);
}


static void emitByte(CompilationState state[static 1], uint8_t byte) {
    writeChunk(&state->vm->gc,
               currentChunk(state->compiler),
               byte,
               state->parser->previous.line);
}


static void emitBytes(CompilationState state[static 1], uint8_t byte1, uint8_t byte2) {
    emitByte(state, byte1);
    emitByte(state, byte2);
}


static uint8_t makeConstant(CompilationState state[static 1], Value value) {
    uint32_t constant = addConstant(state->vm, currentChunk(state->compiler), value);
    if (constant > UINT8_MAX) {
        error(state->parser, "Too many constants in one chunk.");
        return 0;
    }

    return (uint8_t)constant;
}


static void emitConstant(CompilationState state[static 1], Value value) {
    emitBytes(state, OP_CONSTANT, makeConstant(state, value));
}


static void emitReturn(CompilationState state[static 1]) {
    if (state->compiler->type == TYPE_INITIALIZER) {
        emitBytes(state, OP_GET_LOCAL, 0);
    } else {
        emitByte(state, OP_NIL);
    }
    emitByte(state, OP_RETURN);
}


static uint32_t emitJump(CompilationState state[static 1], OpCode instruction) {
    emitByte(state, instruction);
    emitByte(state, 0xff);
    emitByte(state, 0xff);

    return currentChunk(state->compiler)->count - 2;
}


static void patchJump(
    Compiler compiler[static 1],
    Parser parser[static 1],
    uint32_t offset
) {
    Chunk* chunk = currentChunk(compiler);
    uint32_t jump = chunk->count - offset - 2;
    if (jump > UINT16_MAX) {
        error(parser, "Too much code to jump over.");
    }

    chunk->code[offset] = (jump >> 8) & 0xff;
    chunk->code[offset + 1] = jump & 0xff;
}


static void emitLoop(CompilationState state[static 1], uint32_t loopStart) {
    emitByte(state, OP_LOOP);

    uint32_t offset = currentChunk(state->compiler)->count - loopStart + 2;
    if (offset > UINT16_MAX) {
        error(state->parser, "Loop body too large.");
    }

    emitByte(state, (offset >> 8) & 0xff);
    emitByte(state, offset & 0xff);
}

static void initCompiler(
    CompilationState state[static 1],
    Compiler compiler[static 1],
    FunctionType type
) {
    compiler->enclosing = state->compiler;
    compiler->function = nullptr;
    compiler->type = type;
    compiler->localCount = 0;
    compiler->scopeDepth = 0;
    compiler->function = newFunction(&state->vm->gc);

    state->compiler = compiler;

    if (type != TYPE_SCRIPT) {
        compiler->function->name = copyString(state->vm,
                                              state->parser->previous.start,
                                              state->parser->previous.length);
    }

    Local* local = &compiler->locals[compiler->localCount++];
    local->depth = 0;
    local->isCaptured = false;
    if (type != TYPE_FUNCTION) {
        local->name.start = "this";
        local->name.length = 4;
    } else {
        local->name.start = "";
        local->name.length = 0;
    };
}


static ObjFunction* endCompiler(CompilationState state[static 1]) {
    emitReturn(state);

    ObjFunction* function = state->compiler->function;

#ifdef DEBUG_PRINT_CODE
    if (!state->parser->hadError) {
        disassembleChunk(currentChunk(state->compiler),
                         function->name ? function->name->chars : "<script>");
    }
#endif

    state->compiler = state->compiler->enclosing;

    return function;
}


static void beginScope(Compiler compiler[static 1]) {
    compiler->scopeDepth++;
}


static void endScope(CompilationState state[static 1]) {
    state->compiler->scopeDepth--;

    while (
        state->compiler->localCount > 0 &&
        state->compiler->locals[state->compiler->localCount - 1].depth > state->compiler->scopeDepth
    ) {
        if (state->compiler->locals[state->compiler->localCount - 1].isCaptured) {
            emitByte(state, OP_CLOSE_UPVALUE);
        } else {
            emitByte(state, OP_POP);
        }
        state->compiler->localCount--;
    }
}


static uint8_t identifierConstant(CompilationState state[static 1], Token name[static 1]) {
    return makeConstant(state,
                        objVal((Obj*)copyString(state->vm,
                                                name->start,
                                                name->length)));
}


static bool identifiersEqual(Token a[static 1], Token b[static 1]) {
    if (a->length != b->length) {
        return false;
    }

    return !memcmp(a->start, b->start, a->length);
}


static void addLocal(Compiler compiler[static 1], Parser parser[static 1], Token name) {
    if (compiler->localCount == uint8Count) {
        error(parser, "Too many local variables in function.");
        return;
    }

    Local* local = &compiler->locals[compiler->localCount++];
    *local = (Local){
        .name = name,
        .depth = -1,
        .isCaptured = false,
    };
}


static int16_t addUpvalue(
    Compiler compiler[static 1],
    Parser parser[static 1],
    uint8_t index,
    bool isLocal
) {
    uint8_t upvalueCount = compiler->function->upvalueCount;

    for (uint8_t i = 0; i < upvalueCount; i++) {
        Upvalue* upvalue = &compiler->upvalues[i];
        if (upvalue->index == index && upvalue->isLocal == isLocal) {
            return i;
        }
    }

    if (upvalueCount == UINT8_MAX) {
        error(parser, "Too many closure variables in function.");
        return 0;
    }

    compiler->upvalues[upvalueCount].isLocal = isLocal;
    compiler->upvalues[upvalueCount].index = index;

    return compiler->function->upvalueCount++;
}


static int16_t resolveLocal(
    Compiler compiler[static 1],
    Parser parser[static 1],
    Token name[static 1]
) {
    for (int16_t i = compiler->localCount - 1; i >= 0; i--) {
        Local* local = &compiler->locals[i];
        if (identifiersEqual(name, &local->name)) {
            if (local->depth == -1) {
                error(parser, "Can't read local variable in its own initializer.");
            }
            return i;
        }
    }

    return -1;
}


static int16_t resolveUpvalue(
    Compiler compiler[static 1],
    Parser parser[static 1],
    Token name[static 1]
) {
    if (!compiler->enclosing) {
        return -1;
    }

    int16_t local = resolveLocal(compiler->enclosing, parser, name);
    if (local != -1) {
        compiler->enclosing->locals[local].isCaptured = true;
        return addUpvalue(compiler, parser, (uint8_t)local, true);
    }

    int16_t upvalue = resolveUpvalue(compiler->enclosing, parser, name);
    if (upvalue != -1) {
        return addUpvalue(compiler, parser, (uint8_t)upvalue, false);
    }

    return -1;
}


static void declareVariable(Compiler compiler[static 1], Parser parser[static 1]) {
    if (compiler->scopeDepth == 0) {
        return;
    }

    Token* name = &parser->previous;
    for (int16_t i = compiler->localCount - 1; i >= 0; i--) {
        Local* local = &compiler->locals[i];
        if (local->depth != -1 && local->depth < compiler->scopeDepth) {
            break;
        }

        if (identifiersEqual(name, &local->name)) {
            error(parser, "Already a variable with this name in this scope.");
        }
    }

    addLocal(compiler, parser, *name);
}


static uint8_t parseVariable(CompilationState state[static 1], char const* errorMessage) {
    consume(state->parser, state->scanner, TOKEN_IDENTIFIER, errorMessage);

    declareVariable(state->compiler, state->parser);
    if (state->compiler->scopeDepth > 0) {
        return 0;
    }

    return identifierConstant(state, &state->parser->previous);
}


static void markInitialized(Compiler compiler[static 1]) {
    if (compiler->scopeDepth == 0) {
        return;
    }

    compiler->locals[compiler->localCount - 1].depth = compiler->scopeDepth;
}


static void defineVariable(CompilationState state[static 1], uint8_t global) {
    if (state->compiler->scopeDepth > 0) {
        markInitialized(state->compiler);
        return;
    }

    emitBytes(state, OP_DEFINE_GLOBAL, global);
}


static void synchronize(Parser parser[static 1],  Scanner scanner[static 1]) {
    parser->panicMode = false;

    while (parser->current.type != TOKEN_EOF) {
        if (parser->previous.type == TOKEN_SEMICOLON) {
            return;
        }

        switch (parser->current.type) {
            case TOKEN_CLASS:
            case TOKEN_FUN:
            case TOKEN_VAR:
            case TOKEN_FOR:
            case TOKEN_IF:
            case TOKEN_WHILE:
            case TOKEN_PRINT:
            case TOKEN_RETURN:
                return;
            default: ;
        }

        advance(parser, scanner);
    }
}


static void number(CompilationState state[static 1], [[maybe_unused]] bool canAssign) {
    double value = strtod(state->parser->previous.start, nullptr);
    emitConstant(state, numberVal(value));
}


static void literal(CompilationState state[static 1], [[maybe_unused]] bool canAssign) {
    switch (state->parser->previous.type) {
        case TOKEN_FALSE: emitByte(state, OP_FALSE); break;
        case TOKEN_NIL:   emitByte(state, OP_NIL);   break;
        case TOKEN_TRUE:  emitByte(state, OP_TRUE);  break;
        default:
            unreachable();
            return;
    }
}


static void string(CompilationState state[static 1], [[maybe_unused]] bool canAssign) {
    emitConstant(state,
                 objVal((Obj*)copyString(state->vm,
                                         state->parser->previous.start + 1,
                                         state->parser->previous.length - 2)));
}


static void grouping(CompilationState state[static 1], bool canAssign);
static void unary(CompilationState state[static 1], bool canAssign);
static void binary(CompilationState state[static 1], bool canAssign);
static void variable(CompilationState state[static 1], bool canAssign);
static void this(CompilationState state[static 1], bool canAssign);
static void and_(CompilationState state[static 1], bool canAssign);
static void or_(CompilationState state[static 1], bool canAssign);

static void expression(CompilationState state[static 1]);
static void declaration(CompilationState state[static 1]);
static void statement(CompilationState state[static 1]);


static uint8_t argumentList(CompilationState state[static 1]) {
    uint8_t argCount = 0;

    if (!check(state->parser, TOKEN_RIGHT_PAREN)) {
        do {
            expression(state);
            if (argCount == UINT8_MAX) {
                error(state->parser, "Can't have more than 255 arguments.");
            }
            argCount++;
        } while (match(state->parser, state->scanner, TOKEN_COMMA));
    }
    consume(state->parser, state->scanner, TOKEN_RIGHT_PAREN, "Expect ')' after arguments.");

    return argCount;
}


static void call(CompilationState state[static 1], [[maybe_unused]] bool canAssign) {
    uint8_t argCount = argumentList(state);
    emitBytes(state, OP_CALL, argCount);
}


static void dot(CompilationState state[static 1], bool canAssign) {
    consume(state->parser, state->scanner, TOKEN_IDENTIFIER, "Expect property name after '.'.");
    uint8_t name = identifierConstant(state, &state->parser->previous);
    if (canAssign && match(state->parser, state->scanner, TOKEN_EQUAL)) {
        expression(state);
        emitBytes(state, OP_SET_PROPERTY, name);
    } else if (match(state->parser, state->scanner, TOKEN_LEFT_PAREN)) {
        uint8_t argCount = argumentList(state);
        emitBytes(state, OP_INVOKE, name);
        emitByte(state, argCount);
    } else {
        emitBytes(state, OP_GET_PROPERTY, name);
    }
}


static ParseRule rules[] = {
    [TOKEN_LEFT_PAREN]    = {grouping, call,    PREC_CALL},
    [TOKEN_RIGHT_PAREN]   = {nullptr,  nullptr, PREC_NONE},
    [TOKEN_LEFT_BRACE]    = {nullptr,  nullptr, PREC_NONE},
    [TOKEN_RIGHT_BRACE]   = {nullptr,  nullptr, PREC_NONE},
    [TOKEN_COMMA]         = {nullptr,  nullptr, PREC_NONE},
    [TOKEN_DOT]           = {nullptr,  dot,     PREC_CALL},
    [TOKEN_MINUS]         = {unary,    binary,  PREC_TERM},
    [TOKEN_PLUS]          = {nullptr,  binary,  PREC_TERM},
    [TOKEN_SEMICOLON]     = {nullptr,  nullptr, PREC_NONE},
    [TOKEN_SLASH]         = {nullptr,  binary,  PREC_FACTOR},
    [TOKEN_STAR]          = {nullptr,  binary,  PREC_FACTOR},
    [TOKEN_BANG]          = {unary  ,  nullptr, PREC_NONE},
    [TOKEN_BANG_EQUAL]    = {nullptr,  binary,  PREC_EQUALITY},
    [TOKEN_EQUAL]         = {nullptr,  nullptr, PREC_NONE},
    [TOKEN_EQUAL_EQUAL]   = {nullptr,  binary,  PREC_EQUALITY},
    [TOKEN_GREATER]       = {nullptr,  binary,  PREC_COMPARISON},
    [TOKEN_GREATER_EQUAL] = {nullptr,  binary,  PREC_COMPARISON},
    [TOKEN_LESS]          = {nullptr,  binary,  PREC_COMPARISON},
    [TOKEN_LESS_EQUAL]    = {nullptr,  binary,  PREC_COMPARISON},
    [TOKEN_IDENTIFIER]    = {variable, nullptr, PREC_NONE},
    [TOKEN_STRING]        = {string,   nullptr, PREC_NONE},
    [TOKEN_NUMBER]        = {number,   nullptr, PREC_NONE},
    [TOKEN_AND]           = {nullptr,  and_,    PREC_AND},
    [TOKEN_CLASS]         = {nullptr,  nullptr, PREC_NONE},
    [TOKEN_ELSE]          = {nullptr,  nullptr, PREC_NONE},
    [TOKEN_FALSE]         = {literal,  nullptr, PREC_NONE},
    [TOKEN_FOR]           = {nullptr,  nullptr, PREC_NONE},
    [TOKEN_FUN]           = {nullptr,  nullptr, PREC_NONE},
    [TOKEN_IF]            = {nullptr,  nullptr, PREC_NONE},
    [TOKEN_NIL]           = {literal,  nullptr, PREC_NONE},
    [TOKEN_OR]            = {nullptr,  or_,     PREC_OR},
    [TOKEN_PRINT]         = {nullptr,  nullptr, PREC_NONE},
    [TOKEN_RETURN]        = {nullptr,  nullptr, PREC_NONE},
    [TOKEN_SUPER]         = {nullptr,  nullptr, PREC_NONE},
    [TOKEN_THIS]          = {this,     nullptr, PREC_NONE},
    [TOKEN_TRUE]          = {literal,  nullptr, PREC_NONE},
    [TOKEN_VAR]           = {nullptr,  nullptr, PREC_NONE},
    [TOKEN_WHILE]         = {nullptr,  nullptr, PREC_NONE},
    [TOKEN_ERROR]         = {nullptr,  nullptr, PREC_NONE},
    [TOKEN_EOF]           = {nullptr,  nullptr, PREC_NONE},
};


static ParseRule* getRule(TokenType type) {
    return &rules[type];
}


static void parsePrecedence(CompilationState state[static 1], Precedence precedence) {
    advance(state->parser, state->scanner);
    ParseFn prefixRule = getRule(state->parser->previous.type)->prefix;
    if (!prefixRule) {
        error(state->parser, "Expect expression.");
        return;
    }

    bool canAssign = precedence <= PREC_ASSIGNMENT;
    prefixRule(state, canAssign);

    while (precedence <= getRule(state->parser->current.type)->precedence) {
        advance(state->parser, state->scanner);
        ParseFn infixRule = getRule(state->parser->previous.type)->infix;
        infixRule(state, canAssign);
    }

    if (canAssign && match(state->parser, state->scanner, TOKEN_EQUAL)) {
        error(state->parser, "Invalid assignment target.");
    }
}


static void expression(CompilationState state[static 1]) {
    parsePrecedence(state, PREC_ASSIGNMENT);
}


static void grouping(CompilationState state[static 1], [[maybe_unused]] bool canAssign) {
    expression(state);
    consume(state->parser, state->scanner, TOKEN_RIGHT_PAREN, "Expect ')' after expression");
}


static void unary(CompilationState state[static 1], [[maybe_unused]] bool canAssign) {
    TokenType operatorType = state->parser->previous.type;

    parsePrecedence(state, PREC_UNARY);

    switch (operatorType) {
        case TOKEN_BANG:  emitByte(state, OP_NOT);    break;
        case TOKEN_MINUS: emitByte(state, OP_NEGATE); break;
        default:
            unreachable();
            return;
    }
}


static void binary(CompilationState state[static 1], [[maybe_unused]] bool canAssign) {
    TokenType operatorType = state->parser->previous.type;
    ParseRule* rule = getRule(operatorType);
    parsePrecedence(state, (Precedence)(rule->precedence + 1));

    switch (operatorType) {
        case TOKEN_BANG_EQUAL:    emitBytes(state, OP_EQUAL, OP_NOT);   break;
        case TOKEN_EQUAL_EQUAL:   emitByte(state, OP_EQUAL);            break;
        case TOKEN_GREATER:       emitByte(state, OP_GREATER);          break;
        case TOKEN_GREATER_EQUAL: emitBytes(state, OP_LESS, OP_NOT);    break;
        case TOKEN_LESS:          emitByte(state, OP_LESS);             break;
        case TOKEN_LESS_EQUAL:    emitBytes(state, OP_GREATER, OP_NOT); break;
        case TOKEN_PLUS:          emitByte(state, OP_ADD);              break;
        case TOKEN_MINUS:         emitByte(state, OP_SUBTRACT);         break;
        case TOKEN_STAR:          emitByte(state, OP_MULTIPLY);         break;
        case TOKEN_SLASH:         emitByte(state, OP_DIVIDE);           break;
        default:
            unreachable();
            return;
    }
}


static void and_(CompilationState state[static 1], [[maybe_unused]] bool canAssign) {
    uint32_t endJump = emitJump(state, OP_JUMP_IF_FALSE);

    emitByte(state, OP_POP);
    parsePrecedence(state, PREC_AND);

    patchJump(state->compiler, state->parser, endJump);
}


static void or_(CompilationState state[static 1], [[maybe_unused]] bool canAssign) {
    uint32_t elseJump = emitJump(state, OP_JUMP_IF_FALSE);
    uint32_t endJump = emitJump(state, OP_JUMP);

    patchJump(state->compiler, state->parser, elseJump);
    emitByte(state, OP_POP);

    parsePrecedence(state, PREC_OR);
    patchJump(state->compiler, state->parser, endJump);
}


static void namedVariable(CompilationState state[static 1], Token name, bool canAssign) {
    uint8_t getOp,
            setOp;
    int16_t arg = resolveLocal(state->compiler, state->parser, &name);
    if (arg != -1) {
        getOp = OP_GET_LOCAL;
        setOp = OP_SET_LOCAL;
    } else if ((arg = resolveUpvalue(state->compiler, state->parser, &name)) != -1) {
        getOp = OP_GET_UPVALUE;
        setOp = OP_SET_UPVALUE;
    } else {
        arg = identifierConstant(state, &name);
        getOp = OP_GET_GLOBAL;
        setOp = OP_SET_GLOBAL;
    }

    if (canAssign && match(state->parser, state->scanner, TOKEN_EQUAL)) {
        expression(state);
        emitBytes(state, setOp, (uint8_t)arg);
    } else {
        emitBytes(state, getOp, (uint8_t)arg);
    }
}


static void variable(CompilationState state[static 1], bool canAssign) {
    namedVariable(state, state->parser->previous, canAssign);
}


static void this(CompilationState state[static 1], [[maybe_unused]] bool canAssign) {
    if (!state->classCompiler) {
        error(state->parser, "Can't use 'this' outside of a class.");
        return;
    }
    variable(state, false);
}


static void printStatement(CompilationState state[static 1]) {
    expression(state);
    consume(state->parser, state->scanner, TOKEN_SEMICOLON, "Expect ';' after value.");
    emitByte(state, OP_PRINT);
}


static void expressionStatement(CompilationState state[static 1]) {
    expression(state);
    consume(state->parser, state->scanner, TOKEN_SEMICOLON, "Expect ';' after expression.");
    emitByte(state, OP_POP);
}


static void block(CompilationState state[static 1]) {
    while (!check(state->parser, TOKEN_RIGHT_BRACE) && !check(state->parser, TOKEN_EOF)) {
        declaration(state);
    }

    consume(state->parser, state->scanner, TOKEN_RIGHT_BRACE, "Expect '}' after block.");
}


static void varDeclaration(CompilationState state[static 1]) {
    uint8_t global = parseVariable(state, "Expect variable name.");

    if (match(state->parser, state->scanner, TOKEN_EQUAL)) {
        expression(state);
    } else {
        emitByte(state, OP_NIL);
    }

    consume(state->parser, state->scanner, TOKEN_SEMICOLON, "Expect ';' after variable declaration.");

    defineVariable(state, global);
}


static void ifStatement(CompilationState state[static 1]) {
    consume(state->parser, state->scanner, TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
    expression(state);
    consume(state->parser, state->scanner, TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

    uint32_t thenJump = emitJump(state, OP_JUMP_IF_FALSE);
    emitByte(state, OP_POP);
    statement(state);

    uint32_t elseJump = emitJump(state, OP_JUMP);
    patchJump(state->compiler, state->parser, thenJump);
    emitByte(state, OP_POP);

    if (match(state->parser, state->scanner, TOKEN_ELSE)) {
        statement(state);
    }
    patchJump(state->compiler, state->parser, elseJump);
}


static void whileStatement(CompilationState state[static 1]) {
    uint32_t loopStart = currentChunk(state->compiler)->count;

    consume(state->parser, state->scanner, TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
    expression(state);
    consume(state->parser, state->scanner, TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

    uint32_t exitJump = emitJump(state, OP_JUMP_IF_FALSE);
    emitByte(state, OP_POP);
    statement(state);

    emitLoop(state, loopStart);

    patchJump(state->compiler, state->parser, exitJump);
    emitByte(state, OP_POP);
}


static void forStatement(CompilationState state[static 1]) {
    beginScope(state->compiler);

    consume(state->parser, state->scanner, TOKEN_LEFT_PAREN, "Expect '(' after 'for'.");
    if (match(state->parser, state->scanner, TOKEN_SEMICOLON)) {
        /* Empty initializer */
    } else if (match(state->parser, state->scanner, TOKEN_VAR)) {
        varDeclaration(state);
    } else {
        expressionStatement(state);
    }

    uint32_t loopStart = currentChunk(state->compiler)->count;
    uint32_t exitJump;
    bool haveCondition;  /* Avoiding a downcast into a signed type */
    if ((haveCondition = !match(state->parser, state->scanner, TOKEN_SEMICOLON))) {
        expression(state);
        consume(state->parser, state->scanner, TOKEN_SEMICOLON, "Expect ';' after loop condition.");

        exitJump = emitJump(state, OP_JUMP_IF_FALSE);
        emitByte(state, OP_POP);
    }

    if (!match(state->parser, state->scanner, TOKEN_RIGHT_PAREN)) {
        uint32_t bodyJump = emitJump(state, OP_JUMP);
        uint32_t incrementStart = currentChunk(state->compiler)->count;

        expression(state);
        emitByte(state, OP_POP);

        consume(state->parser, state->scanner, TOKEN_RIGHT_PAREN, "Expect ')' after for clauses.");

        emitLoop(state, loopStart);
        loopStart = incrementStart;
        patchJump(state->compiler, state->parser, bodyJump);
    }

    statement(state);

    emitLoop(state, loopStart);
    if (haveCondition) {
        patchJump(state->compiler, state->parser, exitJump);
        emitByte(state, OP_POP);
    }

    endScope(state);
}


static void returnStatement(CompilationState state[static 1]) {
    if (state->compiler->type == TYPE_SCRIPT) {
        error(state->parser, "Can't return from top-level code.");
    }

    if (match(state->parser, state->scanner, TOKEN_SEMICOLON)) {
        emitReturn(state);
    } else {
        if (state->compiler->type == TYPE_INITIALIZER) {
            error(state->parser, "Can't return a value from an initializer.");
        }

        expression(state);
        consume(state->parser, state->scanner, TOKEN_SEMICOLON, "Expect ';' after return value.");
        emitByte(state, OP_RETURN);
    }
}


static void statement(CompilationState state[static 1]) {
    if (match(state->parser, state->scanner, TOKEN_PRINT)) {
        printStatement(state);
    } else if (match(state->parser, state->scanner, TOKEN_IF)) {
        ifStatement(state);
    } else if (match(state->parser, state->scanner, TOKEN_RETURN)) {
        returnStatement(state);
    } else if (match(state->parser, state->scanner, TOKEN_WHILE)) {
        whileStatement(state);
    } else if (match(state->parser, state->scanner, TOKEN_FOR)) {
        forStatement(state);
    } else if (match(state->parser, state->scanner, TOKEN_LEFT_BRACE)) {
        beginScope(state->compiler);
        block(state);
        endScope(state);
    } else {
        expressionStatement(state);
    }
}


static void function(CompilationState state[static 1], FunctionType type) {
    Compiler compiler;
    initCompiler(state, &compiler, type);

    beginScope(&compiler);

    consume(state->parser, state->scanner, TOKEN_LEFT_PAREN, "Expect '(' after function name.");
    if (!check(state->parser, TOKEN_RIGHT_PAREN)) {
        do {
            compiler.function->arity++;
            if (compiler.function->arity > UINT8_MAX) {
                errorAtCurrent(state->parser, "Can't have more than 255 parameters.");
            }
            uint8_t constant = parseVariable(state, "Expect parameter name.");
            defineVariable(state, constant);
        } while (match(state->parser, state->scanner, TOKEN_COMMA));
    }
    consume(state->parser, state->scanner, TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");

    consume(state->parser, state->scanner, TOKEN_LEFT_BRACE, "Expect '{' before function body.");
    block(state);

    ObjFunction* function = endCompiler(state);
    emitBytes(state, OP_CLOSURE,
              makeConstant(state, objVal((Obj*)function)));

    for (uint8_t i = 0; i < function->upvalueCount; i++) {
        emitByte(state, compiler.upvalues[i].isLocal);
        emitByte(state, compiler.upvalues[i].index);
    }
}


static void funDeclaration(CompilationState state[static 1]) {
    uint8_t global = parseVariable(state, "Expect function name.");
    markInitialized(state->compiler);
    function(state, TYPE_FUNCTION);
    defineVariable(state, global);
}


static void method(CompilationState state[static 1]) {
    consume(state->parser, state->scanner, TOKEN_IDENTIFIER, "Expect method name.");
    uint8_t constant = identifierConstant(state, &state->parser->previous);

    FunctionType type = TYPE_METHOD;
    if (
        state->parser->previous.length == 4 &&
        !memcmp(state->parser->previous.start, "init", 4)
    ) {
        type = TYPE_INITIALIZER;
    }
    function(state, type);
    emitBytes(state, OP_METHOD, constant);
}


static void classDeclaration(CompilationState state[static 1]) {
    consume(state->parser, state->scanner, TOKEN_IDENTIFIER, "Expect class name.");
    Token className = state->parser->previous;
    uint8_t nameConstant = identifierConstant(state, &state->parser->previous);
    declareVariable(state->compiler, state->parser);

    emitBytes(state, OP_CLASS, nameConstant);
    defineVariable(state, nameConstant);

    ClassCompiler classCompiler = { .enclosing = state->classCompiler };
    state->classCompiler = &classCompiler;

    namedVariable(state, className, false);
    consume(state->parser, state->scanner, TOKEN_LEFT_BRACE, "Expect '{' before class body.");
    while (!check(state->parser, TOKEN_RIGHT_BRACE) && !check(state->parser, TOKEN_EOF)) {
        method(state);
    }
    consume(state->parser, state->scanner, TOKEN_RIGHT_BRACE, "Expect '}' after class body.");
    emitByte(state, OP_POP);
}


static void declaration(CompilationState state[static 1]) {
    if (match(state->parser, state->scanner, TOKEN_CLASS)) {
        classDeclaration(state);
    } else if (match(state->parser, state->scanner, TOKEN_FUN)) {
        funDeclaration(state);
    } else if (match(state->parser, state->scanner, TOKEN_VAR)) {
        varDeclaration(state);
    } else {
        statement(state);
    }

    if (state->parser->panicMode) {
        synchronize(state->parser, state->scanner);
    }
}


static void markCompilerRoots(GC gc[static 1], void* arg) {
    CompilationState* state = arg;
    Compiler* compiler = state->compiler;

    while (compiler) {
        markObject(gc, (Obj*)compiler->function);
        compiler = compiler->enclosing;
    }

    if (state->prevMarkRoots && state->prevMarkArg) {
        state->prevMarkRoots(gc, state->prevMarkArg);
    }
}


static void cleanupCompilerWeakRefs(void* arg) {
    CompilationState* state = arg;

    tableRemoveWhite(&state->vm->strings);

    if (state->prevCleanupWeakRefs && state->prevWeakArg) {
        state->prevCleanupWeakRefs(state->prevWeakArg);
    }
}


static void configureGC(GC gc[static 1], CompilationState state[static 1]) {
    state->prevMarkRoots = gc->markRoots;
    state->prevMarkArg = gc->markArg;
    state->prevCleanupWeakRefs = gc->cleanupWeakRefs;
    state->prevWeakArg = gc->weakArg;

    gc->markRoots = markCompilerRoots;
    gc->markArg = state;
    if (
        gc->cleanupWeakRefs != (void (*)(void*))tableRemoveWhite ||
        gc->weakArg != &state->vm->strings
    ) {
        gc->cleanupWeakRefs = cleanupCompilerWeakRefs;
        gc->weakArg = state;
    }
}


static void resetGC(CompilationState state[static 1]) {
    GC* gc = &state->vm->gc;

    gc->markRoots = state->prevMarkRoots;
    gc->markArg = state->prevMarkArg;
    gc->cleanupWeakRefs = state->prevCleanupWeakRefs;
    gc->weakArg = state->prevWeakArg;
}


ObjFunction* compile(VM vm[static 1], char const* source) {
    Scanner scanner;
    initScanner(&scanner, source);

    Parser parser = {
        .hadError = false,
        .panicMode = false,
    };

    CompilationState state = {
        .vm = vm,
        .compiler = nullptr,
        .classCompiler = nullptr,
        .parser = &parser,
        .scanner = &scanner,
    };

    configureGC(&vm->gc, &state);

    Compiler compiler;
    initCompiler(&state, &compiler, TYPE_SCRIPT);

    advance(&parser, &scanner);
    while (!match(&parser, &scanner, TOKEN_EOF)) {
        declaration(&state);
    }

    ObjFunction* function = endCompiler(&state);

    resetGC(&state);

    return parser.hadError ? nullptr : function;
}
