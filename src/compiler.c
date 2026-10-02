#include <stdio.h>   /* For fprintf, stderr */
#include <stdlib.h>  /* For strtod */
#include <string.h>  /* For memcmp */

#include "common.h"
#include "compiler.h"
#include "memory.h"
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


struct Compiler {
    ObjFunction* function;
    FunctionType type;

    Upvalue upvalues[uint8Count];
    Local locals[uint8Count];
    int16_t localCount;
    int16_t scopeDepth;

    Compiler* enclosing;
};


typedef void (*ParseFn)(VM* vm, Compiler* compiler, Parser*, Scanner*, bool canAssign);


typedef struct {
    ParseFn prefix;
    ParseFn infix;
    Precedence precedence;
} ParseRule;


static Chunk* currentChunk(Compiler* compiler) {
    return &compiler->function->chunk;
}


static void errorAt(Parser* parser, Token* token, char const* message) {
    if (parser->panicMode)
        return;

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


static void errorAtCurrent(Parser* parser, char const* message) {
    errorAt(parser, &parser->current, message);
}


static void error(Parser* parser, char const* message) {
    errorAt(parser, &parser->previous, message);
}


static void advance(Parser* parser, Scanner* scanner) {
    parser->previous = parser->current;

    for (;;) {
        parser->current = scanToken(scanner);
        if (parser->current.type != TOKEN_ERROR) break;

        errorAtCurrent(parser, parser->current.start);
    }
}


static bool check(Parser* parser, TokenType type) {
    return parser->current.type == type;
}


static bool match(Parser* parser, Scanner* scanner, TokenType type) {
    if (!check(parser, type))
        return false;
    advance(parser, scanner);
    return true;
}


static void consume(
    Parser* parser,
    Scanner* scanner,
    TokenType type,
    char const* message
) {
    if (parser->current.type == type) {
        advance(parser, scanner);
        return;
    }

    errorAtCurrent(parser, message);
}


static void emitByte(VM* restrict vm, Compiler* compiler, Parser* parser, uint8_t byte) {
    writeChunk(vm, currentChunk(compiler), byte, parser->previous.line);
}


static void emitBytes(VM* restrict vm, Compiler* compiler, Parser* parser, uint8_t byte1, uint8_t byte2) {
    emitByte(vm, compiler, parser, byte1);
    emitByte(vm, compiler, parser, byte2);
}


static uint8_t makeConstant(VM* restrict vm, Compiler* compiler, Parser* parser, Value value) {
    uint32_t constant = addConstant(vm, currentChunk(compiler), value);
    if (constant > UINT8_MAX) {
        error(parser, "Too many constants in one chunk.");
        return 0;
    }

    return (uint8_t)constant;
}


static void emitConstant(VM* restrict vm, Compiler* compiler, Parser* parser, Value value) {
    emitBytes(vm, compiler, parser, OP_CONSTANT, makeConstant(vm, compiler, parser, value));
}


static void emitReturn(VM* restrict vm, Compiler* compiler, Parser* parser) {
    emitByte(vm, compiler, parser, OP_NIL);  /* TODO: change to emitBytes if nothing is gonna go in between */
    emitByte(vm, compiler, parser, OP_RETURN);
}


static uint32_t emitJump(VM* restrict vm, Compiler* compiler, Parser* parser, OpCode instruction) {
    emitByte(vm, compiler, parser, instruction);
    emitByte(vm, compiler, parser, 0xff);
    emitByte(vm, compiler, parser, 0xff);

    return currentChunk(compiler)->count - 2;
}


static void patchJump(Compiler* compiler, Parser* parser, uint32_t offset) {
    Chunk* chunk = currentChunk(compiler);
    uint32_t jump = chunk->count - offset - 2;
    if (jump > UINT16_MAX) {
        error(parser, "Too much code to jump over.");
    }

    chunk->code[offset] = (jump >> 8) & 0xff;
    chunk->code[offset + 1] = jump & 0xff;
}


static void emitLoop(VM* restrict vm, Compiler* compiler, Parser* parser, uint32_t loopStart) {
    emitByte(vm, compiler, parser, OP_LOOP);

    uint32_t offset = currentChunk(compiler)->count - loopStart + 2;
    if (offset > UINT16_MAX) {
        error(parser, "Loop body too large.");
    }

    emitByte(vm, compiler, parser, (offset >> 8) & 0xff);
    emitByte(vm, compiler, parser, offset & 0xff);
}


static void initCompiler(
    VM* vm,
    Compiler* compiler,
    Compiler* enclosing,
    Parser* parser,
    FunctionType type
) {
    compiler->function = nullptr;
    compiler->type = type;
    compiler->localCount = 0;
    compiler->scopeDepth = 0;
    compiler->enclosing = enclosing;
    compiler->function = newFunction(vm);

    if (type != TYPE_SCRIPT) {
        compiler->function->name = copyString(vm,
                                              parser->previous.start,
                                              parser->previous.length);
    }

    Local* local = &compiler->locals[compiler->localCount++];
    local->depth = 0;
    local->name.start = "";
    local->name.length = 0;
    local->isCaptured = false;
}


static ObjFunction* endCompiler(VM* restrict vm, Compiler* compiler, Parser* parser) {
    emitReturn(vm, compiler, parser);

    ObjFunction* function = compiler->function;

#ifdef DEBUG_PRINT_CODE
    if (!parser->hadError)
        disassembleChunk(currentChunk(compiler),
                         function->name ? function->name->chars : "<script>");
#endif

    return function;
}


static void beginScope(Compiler* compiler) {
    compiler->scopeDepth++;
}


static void endScope(VM* restrict vm, Compiler* compiler, Parser* parser) {
    compiler->scopeDepth--;

    while (
        compiler->localCount > 0 &&
        compiler->locals[compiler->localCount - 1].depth > compiler->scopeDepth
    ) {
        if (compiler->locals[compiler->localCount - 1].isCaptured) {
            emitByte(vm, compiler, parser, OP_CLOSE_UPVALUE);
        } else {
            emitByte(vm, compiler, parser, OP_POP);
        }
        compiler->localCount--;
    }
}


static uint8_t identifierConstant(VM* restrict vm, Compiler* compiler, Parser* parser, Token* name) {
    return makeConstant(vm, compiler, parser,
                        objVal((Obj*)copyString(vm, name->start, name->length)));
}


static bool identifiersEqual(Token* a, Token* b) {
    if (a->length != b->length)
        return false;

    return !memcmp(a->start, b->start, a->length);
}


static void addLocal(Compiler* compiler, Parser* parser, Token name) {
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


static int16_t addUpvalue(Compiler* compiler, Parser* parser, uint8_t index, bool isLocal) {
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


static int16_t resolveLocal(Compiler* compiler, Parser* parser, Token* name) {
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


static int16_t resolveUpvalue(Compiler* compiler, Parser* parser, Token* name) {
    if (!compiler->enclosing)
        return -1;

    int16_t local = resolveLocal(compiler->enclosing, parser, name);
    if (local != -1) {
        compiler->enclosing->locals[local].isCaptured = true;
        return addUpvalue(compiler, parser, (uint8_t)local, true);
    }

    int16_t upvalue = resolveUpvalue(compiler->enclosing, parser, name);
    if (upvalue != -1)
        return addUpvalue(compiler, parser, (uint8_t)upvalue, false);

    return -1;
}


static void declareVariable(Compiler* compiler, Parser* parser) {
    if (compiler->scopeDepth == 0)
        return;

    Token* name = &parser->previous;
    for (int16_t i = compiler->localCount - 1; i >= 0; i--) {
        Local* local = &compiler->locals[i];
        if (local->depth != -1 && local->depth < compiler->scopeDepth)
            break;

        if (identifiersEqual(name, &local->name)) {
            error(parser, "Already a variable with this name in this scope.");
        }
    }

    addLocal(compiler, parser, *name);
}


static uint8_t parseVariable(
    VM* vm,
    Compiler* compiler,
    Parser* parser,
    Scanner* scanner,
    char const* errorMessage
) {
    consume(parser, scanner, TOKEN_IDENTIFIER, errorMessage);

    declareVariable(compiler, parser);
    if (compiler->scopeDepth > 0)
        return 0;

    return identifierConstant(vm, compiler, parser, &parser->previous);
}


static void markInitialized(Compiler* compiler) {
    if (compiler->scopeDepth == 0)
        return;

    compiler->locals[compiler->localCount - 1].depth = compiler->scopeDepth;
}


static void defineVariable(VM* restrict vm, Compiler* compiler, Parser* parser, uint8_t global) {
    if (compiler->scopeDepth > 0) {
        markInitialized(compiler);
        return;
    }

    emitBytes(vm, compiler, parser, OP_DEFINE_GLOBAL, global);
}


static void synchronize(Parser* parser,  Scanner* scanner) {
    parser->panicMode = false;

    while (parser->current.type != TOKEN_EOF) {
        if (parser->previous.type == TOKEN_SEMICOLON)
            return;

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


static void number(
    VM* restrict vm,
    Compiler* compiler,
    Parser* parser,
    [[maybe_unused]] Scanner* scanner,
    [[maybe_unused]] bool canAssign
) {
    double value = strtod(parser->previous.start, nullptr);
    emitConstant(vm, compiler, parser, numberVal(value));
}


static void literal(
    VM* restrict vm,
    Compiler* compiler,
    Parser* parser,
    [[maybe_unused]] Scanner* scanner,
    [[maybe_unused]] bool canAssign
) {
    switch (parser->previous.type) {
        case TOKEN_FALSE: emitByte(vm, compiler, parser, OP_FALSE); break;
        case TOKEN_NIL:   emitByte(vm, compiler, parser, OP_NIL);   break;
        case TOKEN_TRUE:  emitByte(vm, compiler, parser, OP_TRUE);  break;
        default:
            unreachable();
            return;
    }
}


static void string(
    VM* restrict vm,
    Compiler* compiler,
    Parser* parser,
    [[maybe_unused]] Scanner* scanner,
    [[maybe_unused]] bool canAssign
) {
    emitConstant(vm, compiler, parser,
                 objVal((Obj*)copyString(vm, parser->previous.start + 1,
                                         parser->previous.length - 2)));
}


static void grouping(VM* vm, Compiler* compiler, Parser* parser, Scanner* scanner, bool canAssign);
static void unary(VM* vm, Compiler* compiler, Parser* parser, Scanner* scanner, bool canAssign);
static void binary(VM* vm, Compiler* compiler, Parser* parser, Scanner* scanner, bool canAssign);
static void variable(VM* vm, Compiler* compiler, Parser* parser, Scanner* scanner, bool canAssign);
static void and_(VM* vm, Compiler* compiler, Parser* parser, Scanner* scanner, bool canAssign);
static void or_(VM* vm, Compiler* compiler, Parser* parser, Scanner* scanner, bool canAssign);

static void expression(VM* vm, Compiler* compiler, Parser* parser, Scanner* scanner);
static void declaration(VM* vm, Compiler* compiler, Parser* parser, Scanner* scanner);
static void statement(VM* vm, Compiler* compiler, Parser* parser, Scanner* scanner);


static uint8_t argumentList(
    VM* vm,
    Compiler* compiler,
    Parser* parser,
    Scanner* scanner
) {
    uint8_t argCount = 0;

    if (!check(parser, TOKEN_RIGHT_PAREN)) {
        do {
            expression(vm, compiler, parser, scanner);
            if (argCount == UINT8_MAX) {
                error(parser, "Can't have more than 255 arguments.");
            }
            argCount++;
        } while (match(parser, scanner, TOKEN_COMMA));
    }
    consume(parser, scanner, TOKEN_RIGHT_PAREN, "Expect ')' after arguments.");

    return argCount;
}


static void call(
    VM* restrict vm,
    Compiler* compiler,
    Parser* parser,
    Scanner* scanner,
    [[maybe_unused]] bool canAssign
) {
    uint8_t argCount = argumentList(vm, compiler, parser, scanner);
    emitBytes(vm, compiler, parser, OP_CALL, argCount);
}


static void dot(
    VM* restrict vm,
    Compiler* compiler,
    Parser* parser,
    Scanner* scanner,
    bool canAssign
) {
    consume(parser, scanner, TOKEN_IDENTIFIER, "Expect property name after '.'.");
    uint8_t name = identifierConstant(vm, compiler, parser, &parser->previous);
    if (canAssign && match(parser, scanner, TOKEN_EQUAL)) {
        expression(vm, compiler, parser, scanner);
        emitBytes(vm, compiler, parser, OP_SET_PROPERTY, name);
    } else {
        emitBytes(vm, compiler, parser, OP_GET_PROPERTY, name);
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
    [TOKEN_THIS]          = {nullptr,  nullptr, PREC_NONE},
    [TOKEN_TRUE]          = {literal,  nullptr, PREC_NONE},
    [TOKEN_VAR]           = {nullptr,  nullptr, PREC_NONE},
    [TOKEN_WHILE]         = {nullptr,  nullptr, PREC_NONE},
    [TOKEN_ERROR]         = {nullptr,  nullptr, PREC_NONE},
    [TOKEN_EOF]           = {nullptr,  nullptr, PREC_NONE},
};


static ParseRule* getRule(TokenType type) {
    return &rules[type];
}


static void parsePrecedence(
    VM* vm,
    Compiler* compiler,
    Parser* parser,
    Scanner* scanner,
    Precedence precedence
) {
    advance(parser, scanner);
    ParseFn prefixRule = getRule(parser->previous.type)->prefix;
    if (!prefixRule) {
        error(parser, "Expect expression.");
        return;
    }

    bool canAssign = precedence <= PREC_ASSIGNMENT;
    prefixRule(vm, compiler, parser, scanner, canAssign);

    while (precedence <= getRule(parser->current.type)->precedence) {
        advance(parser, scanner);
        ParseFn infixRule = getRule(parser->previous.type)->infix;
        infixRule(vm, compiler, parser, scanner, canAssign);
    }

    if (canAssign && match(parser, scanner, TOKEN_EQUAL)) {
        error(parser, "Invalid assignment target.");
    }
}


static void expression(
    VM* vm,
    Compiler* compiler,
    Parser* parser,
    Scanner* scanner
) {
    parsePrecedence(vm, compiler, parser, scanner, PREC_ASSIGNMENT);
}


static void grouping(
    VM* vm,
    Compiler* compiler,
    Parser* parser,
    Scanner* scanner,
    [[maybe_unused]] bool canAssign
) {
    expression(vm, compiler, parser, scanner);
    consume(parser, scanner, TOKEN_RIGHT_PAREN, "Expect ')' after expression");
}


static void unary(
    VM* restrict vm,
    Compiler* compiler,
    Parser* parser,
    [[maybe_unused]] Scanner* scanner,
    [[maybe_unused]] bool canAssign
) {
    TokenType operatorType = parser->previous.type;

    parsePrecedence(vm, compiler, parser, scanner, PREC_UNARY);

    switch (operatorType) {
        case TOKEN_BANG:  emitByte(vm, compiler, parser, OP_NOT);    break;
        case TOKEN_MINUS: emitByte(vm, compiler, parser, OP_NEGATE); break;
        default:
            unreachable();
            return;
    }
}


static void binary(
    VM* restrict vm,
    Compiler* compiler,
    Parser* parser,
    [[maybe_unused]] Scanner* scanner,
    [[maybe_unused]] bool canAssign
) {
    TokenType operatorType = parser->previous.type;
    ParseRule* rule = getRule(operatorType);
    parsePrecedence(vm, compiler, parser, scanner, (Precedence)(rule->precedence + 1));

    switch (operatorType) {
        case TOKEN_BANG_EQUAL:    emitBytes(vm, compiler, parser, OP_EQUAL, OP_NOT);   break;
        case TOKEN_EQUAL_EQUAL:   emitByte(vm, compiler, parser, OP_EQUAL);            break;
        case TOKEN_GREATER:       emitByte(vm, compiler, parser, OP_GREATER);          break;
        case TOKEN_GREATER_EQUAL: emitBytes(vm, compiler, parser, OP_LESS, OP_NOT);    break;
        case TOKEN_LESS:          emitByte(vm, compiler, parser, OP_LESS);             break;
        case TOKEN_LESS_EQUAL:    emitBytes(vm, compiler, parser, OP_GREATER, OP_NOT); break;
        case TOKEN_PLUS:          emitByte(vm, compiler, parser, OP_ADD);              break;
        case TOKEN_MINUS:         emitByte(vm, compiler, parser, OP_SUBTRACT);         break;
        case TOKEN_STAR:          emitByte(vm, compiler, parser, OP_MULTIPLY);         break;
        case TOKEN_SLASH:         emitByte(vm, compiler, parser, OP_DIVIDE);           break;
        default:
            unreachable();
            return;
    }
}


static void and_(
    VM* restrict vm,
    Compiler* compiler,
    Parser* parser,
    Scanner* scanner,
    [[maybe_unused]] bool canAssign
) {
    uint32_t endJump = emitJump(vm, compiler, parser, OP_JUMP_IF_FALSE);

    emitByte(vm, compiler, parser, OP_POP);
    parsePrecedence(vm, compiler, parser, scanner, PREC_AND);

    patchJump(compiler, parser, endJump);
}


static void or_(
    VM* restrict vm,
    Compiler* compiler,
    Parser* parser,
    Scanner* scanner,
    [[maybe_unused]] bool canAssign
) {
    uint32_t elseJump = emitJump(vm, compiler, parser, OP_JUMP_IF_FALSE);
    uint32_t endJump = emitJump(vm, compiler, parser, OP_JUMP);

    patchJump(compiler, parser, elseJump);
    emitByte(vm, compiler, parser, OP_POP);

    parsePrecedence(vm, compiler, parser, scanner, PREC_OR);
    patchJump(compiler, parser, endJump);
}


static void namedVariable(
    VM* vm,
    Compiler* compiler,
    Parser* parser,
    Scanner* scanner,
    Token name,
    bool canAssign
) {
    uint8_t getOp,
            setOp;
    int16_t arg = resolveLocal(compiler, parser, &name);
    if (arg != -1) {
        getOp = OP_GET_LOCAL;
        setOp = OP_SET_LOCAL;
    } else if ((arg = resolveUpvalue(compiler, parser, &name)) != -1) {
        getOp = OP_GET_UPVALUE;
        setOp = OP_SET_UPVALUE;
    } else {
        arg = identifierConstant(vm, compiler, parser, &name);
        getOp = OP_GET_GLOBAL;
        setOp = OP_SET_GLOBAL;
    }

    if (canAssign && match(parser, scanner, TOKEN_EQUAL)) {
        expression(vm, compiler, parser, scanner);
        emitBytes(vm, compiler, parser, setOp, (uint8_t)arg);
    } else
        emitBytes(vm, compiler, parser, getOp, (uint8_t)arg);
}


static void variable(
    VM* vm,
    Compiler* compiler,
    Parser* parser,
    Scanner* scanner,
    bool canAssign
) {
    namedVariable(vm, compiler, parser, scanner, parser->previous, canAssign);
}


static void printStatement(
    VM* restrict vm,
    Compiler* compiler,
    Parser* parser,
    Scanner* scanner
) {
    expression(vm, compiler, parser, scanner);
    consume(parser, scanner, TOKEN_SEMICOLON, "Expect ';' after value.");
    emitByte(vm, compiler, parser, OP_PRINT);
}


static void expressionStatement(
    VM* restrict vm,
    Compiler* compiler,
    Parser* parser,
    Scanner* scanner
) {
    expression(vm, compiler, parser, scanner);
    consume(parser, scanner, TOKEN_SEMICOLON, "Expect ';' after expression.");
    emitByte(vm, compiler, parser, OP_POP);
}


static void block(
    VM* vm,
    Compiler* compiler,
    Parser* parser,
    Scanner* scanner
) {
    while (!check(parser, TOKEN_RIGHT_BRACE) && !check(parser, TOKEN_EOF)) {
        declaration(vm, compiler, parser, scanner);
    }

    consume(parser, scanner, TOKEN_RIGHT_BRACE, "Expect '}' after block.");
}


static void varDeclaration(VM* restrict vm,  Compiler* compiler, Parser* parser, Scanner* scanner) {
    uint8_t global = parseVariable(vm, compiler, parser, scanner, "Expect variable name.");

    if (match(parser, scanner, TOKEN_EQUAL)) {
        expression(vm, compiler, parser, scanner);
    } else {
        emitByte(vm, compiler, parser, OP_NIL);
    }

    consume(parser, scanner, TOKEN_SEMICOLON, "Expect ';' after variable declaration.");

    defineVariable(vm, compiler, parser, global);
}


static void ifStatement(
    VM* restrict vm,
    Compiler* compiler,
    Parser* parser,
    Scanner* scanner
) {
    consume(parser, scanner, TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
    expression(vm, compiler, parser, scanner);
    consume(parser, scanner, TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

    uint32_t thenJump = emitJump(vm, compiler, parser, OP_JUMP_IF_FALSE);
    emitByte(vm, compiler, parser, OP_POP);
    statement(vm, compiler, parser, scanner);

    uint32_t elseJump = emitJump(vm, compiler, parser, OP_JUMP);
    patchJump(compiler, parser, thenJump);
    emitByte(vm, compiler, parser, OP_POP);

    if (match(parser, scanner, TOKEN_ELSE)) {
        statement(vm, compiler, parser, scanner);
    }
    patchJump(compiler, parser, elseJump);
}


static void whileStatement(
    VM* restrict vm,
    Compiler* compiler,
    Parser* parser,
    Scanner* scanner
) {
    uint32_t loopStart = currentChunk(compiler)->count;

    consume(parser, scanner, TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
    expression(vm, compiler, parser, scanner);
    consume(parser, scanner, TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

    uint32_t exitJump = emitJump(vm, compiler, parser, OP_JUMP_IF_FALSE);
    emitByte(vm, compiler, parser, OP_POP);
    statement(vm, compiler, parser, scanner);

    emitLoop(vm, compiler, parser, loopStart);

    patchJump(compiler, parser, exitJump);
    emitByte(vm, compiler, parser, OP_POP);
}


static void forStatement(
    VM* restrict vm,
    Compiler* compiler,
    Parser* parser,
    Scanner* scanner
) {
    beginScope(compiler);

    consume(parser, scanner, TOKEN_LEFT_PAREN, "Expect '(' after 'for'.");
    if (match(parser, scanner, TOKEN_SEMICOLON)) {
        /* Empty initializer */
    } else if (match(parser, scanner, TOKEN_VAR)) {
        varDeclaration(vm, compiler, parser, scanner);
    } else {
        expressionStatement(vm, compiler, parser, scanner);
    }

    uint32_t loopStart = currentChunk(compiler)->count;
    uint32_t exitJump;
    bool haveCondition;  /* Avoiding a downcast into a signed type */
    if ((haveCondition = !match(parser, scanner, TOKEN_SEMICOLON))) {
        expression(vm, compiler, parser, scanner);
        consume(parser, scanner, TOKEN_SEMICOLON, "Expect ';' after loop condition.");

        exitJump = emitJump(vm, compiler, parser, OP_JUMP_IF_FALSE);
        emitByte(vm, compiler, parser, OP_POP);
    }

    if (!match(parser, scanner, TOKEN_RIGHT_PAREN)) {
        uint32_t bodyJump = emitJump(vm, compiler, parser, OP_JUMP);
        uint32_t incrementStart = currentChunk(compiler)->count;

        expression(vm, compiler, parser, scanner);
        emitByte(vm, compiler, parser, OP_POP);

        consume(parser, scanner, TOKEN_RIGHT_PAREN, "Expect ')' after for clauses.");

        emitLoop(vm, compiler, parser, loopStart);
        loopStart = incrementStart;
        patchJump(compiler, parser, bodyJump);
    }

    statement(vm, compiler, parser, scanner);

    emitLoop(vm, compiler, parser, loopStart);
    if (haveCondition) {
        patchJump(compiler, parser, exitJump);
        emitByte(vm, compiler, parser, OP_POP);
    }

    endScope(vm, compiler, parser);
}


static void returnStatement(
    VM* restrict vm,
    Compiler* compiler,
    Parser* parser,
    Scanner* scanner
) {
    if (compiler->type == TYPE_SCRIPT) {
        error(parser, "Can't return from top-level code.");
    }

    if (match(parser, scanner, TOKEN_SEMICOLON)) {
        emitReturn(vm, compiler, parser);
    } else {
        expression(vm, compiler, parser, scanner);
        consume(parser, scanner, TOKEN_SEMICOLON, "Expect ';' after return value.");
        emitByte(vm, compiler, parser, OP_RETURN);
    }
}


static void statement(
    VM* restrict vm,
    Compiler* compiler,
    Parser* parser,
    Scanner* scanner
) {
    if (match(parser, scanner, TOKEN_PRINT)) {
        printStatement(vm, compiler, parser, scanner);
    } else if (match(parser, scanner, TOKEN_IF)) {
        ifStatement(vm, compiler, parser, scanner);
    } else if (match(parser, scanner, TOKEN_RETURN)) {
        returnStatement(vm, compiler, parser, scanner);
    } else if (match(parser, scanner, TOKEN_WHILE)) {
        whileStatement(vm, compiler, parser, scanner);
    } else if (match(parser, scanner, TOKEN_FOR)) {
        forStatement(vm, compiler, parser, scanner);
    } else if (match(parser, scanner, TOKEN_LEFT_BRACE)) {
        beginScope(compiler);
        block(vm, compiler, parser, scanner);
        endScope(vm, compiler, parser);
    } else
        expressionStatement(vm, compiler, parser, scanner);
}


static void function(
    VM* restrict vm,
    Compiler* enclosing,
    Parser* parser,
    Scanner* scanner,
    FunctionType type
) {
    Compiler compiler;
    initCompiler(vm, &compiler, enclosing, parser, type);

    beginScope(&compiler);

    consume(parser, scanner, TOKEN_LEFT_PAREN, "Expect '(' after function name.");
    if (!check(parser, TOKEN_RIGHT_PAREN)) {
        do {
            compiler.function->arity++;
            if (compiler.function->arity > UINT8_MAX) {
                errorAtCurrent(parser, "Can't have more than 255 parameters.");
            }
            uint8_t constant = parseVariable(vm, &compiler, parser, scanner, "Expect parameter name.");
            defineVariable(vm, &compiler, parser, constant);
        } while (match(parser, scanner, TOKEN_COMMA));
    }
    consume(parser, scanner, TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");

    consume(parser, scanner, TOKEN_LEFT_BRACE, "Expect '{' before function body.");
    block(vm, &compiler, parser, scanner);

    ObjFunction* function = endCompiler(vm, &compiler, parser);
    emitBytes(vm, enclosing, parser, OP_CLOSURE,
              makeConstant(vm, enclosing, parser, objVal((Obj*)function)));

    for (uint8_t i = 0; i < function->upvalueCount; i++) {
        emitByte(vm, enclosing, parser, compiler.upvalues[i].isLocal);
        emitByte(vm, enclosing, parser, compiler.upvalues[i].index);
    }
}


static void funDeclaration(
    VM* restrict vm,
    Compiler* compiler,
    Parser* parser,
    Scanner* scanner
) {
    uint8_t global = parseVariable(vm, compiler, parser, scanner, "Expect function name.");
    markInitialized(compiler);
    function(vm, compiler, parser, scanner, TYPE_FUNCTION);
    defineVariable(vm, compiler, parser, global);
}


static void method(
    VM* restrict vm,
    Compiler* compiler,
    Parser* parser,
    Scanner* scanner
) {
    consume(parser, scanner, TOKEN_IDENTIFIER, "Expect method name.");
    uint8_t constant = identifierConstant(vm, compiler, parser, &parser->previous);

    FunctionType type = TYPE_FUNCTION;
    function(vm, compiler, parser, scanner, type);
    emitBytes(vm, compiler, parser, OP_METHOD, constant);
}


static void classDeclaration(
    VM* restrict vm,
    Compiler* compiler,
    Parser* parser,
    Scanner* scanner
) {
    consume(parser, scanner, TOKEN_IDENTIFIER, "Expect class name.");
    Token className = parser->previous;
    uint8_t nameConstant = identifierConstant(vm, compiler, parser, &parser->previous);
    declareVariable(compiler, parser);

    emitBytes(vm, compiler, parser, OP_CLASS, nameConstant);
    defineVariable(vm, compiler, parser, nameConstant);

    namedVariable(vm, compiler, parser, scanner, className, false);
    consume(parser, scanner, TOKEN_LEFT_BRACE, "Expect '{' before class body.");
    while (!check(parser, TOKEN_RIGHT_BRACE) && !check(parser, TOKEN_EOF)) {
        method(vm, compiler, parser, scanner);
    }
    consume(parser, scanner, TOKEN_RIGHT_BRACE, "Expect '}' after class body.");
    emitByte(vm, compiler, parser, OP_POP);
}


static void declaration(
    VM* vm,
    Compiler* compiler,
    Parser* parser,
    Scanner* scanner
) {
    if (match(parser, scanner, TOKEN_CLASS)) {
        classDeclaration(vm, compiler, parser, scanner);
    } else if (match(parser, scanner, TOKEN_FUN)) {
        funDeclaration(vm, compiler, parser, scanner);
    } else if (match(parser, scanner, TOKEN_VAR)) {
        varDeclaration(vm, compiler, parser, scanner);
    } else {
        statement(vm, compiler, parser, scanner);
    }

    if (parser->panicMode) {
        synchronize(parser, scanner);
    }
}


void markCompilerRoots(VM* restrict vm, Compiler* compiler) {
    while (compiler) {
        markObject(vm, (Obj*)compiler->function);
        compiler = compiler->enclosing;
    }
}


ObjFunction* compile(VM* restrict vm, char const* source) {
    Scanner scanner;
    Parser parser = {
        .hadError = false,
        .panicMode = false,
    };
    Compiler compiler;

    initScanner(&scanner, source);
    initCompiler(vm, &compiler, nullptr, &parser, TYPE_SCRIPT);

    advance(&parser, &scanner);
    while (!match(&parser, &scanner, TOKEN_EOF)) {
        declaration(vm, &compiler, &parser, &scanner);
    }

    ObjFunction* function = endCompiler(vm, &compiler, &parser);
    return parser.hadError ? nullptr : function;
}
