#include <string.h>  /* For memcmp */
#include <ctype.h>   /* For isdigit, isalpha, isalnum */

#include "common.h"
#include "scanner.h"


static bool isAtEnd(Scanner scanner[static 1]) {
    return *scanner->current == '\0';
}


static bool isAlpha(char c) [[unsequenced]] {
    return c == '_' || isalpha(c);
}


static bool isAlphaNum(char c) [[unsequenced]] {
    return c == '_' || isalnum(c);
}


static Token makeToken(Scanner scanner[static 1], TokenType type) {
    return (Token){
        .type = type,
        .start = scanner->start,
        .length = (uint16_t)(scanner->current - scanner->start),
        .line = scanner->line,
    };
}


static Token errorToken(Scanner scanner[static 1], char const* message) {
    return (Token){
        .type = TOKEN_ERROR,
        .start = message,
        .length = (uint16_t)strlen(message),
        .line = scanner->line,
    };
}


static char advance(Scanner scanner[static 1]) {
    scanner->current++;
    return scanner->current[-1];
}


static bool match(Scanner scanner[static 1], char expected) {
    if (isAtEnd(scanner) || *scanner->current != expected)
        return false;

    scanner->current++;
    return true;
}


static char peek(Scanner scanner[static 1]) {
    return *scanner->current;
}


static char peekNext(Scanner scanner[static 1]) {
    return isAtEnd(scanner) ? '\0': scanner->current[1];
}


static void skipWhitespace(Scanner scanner[static 1]) {
    for (char c; (c = peek(scanner)); )
        switch (c) {
            case '\n':
                scanner->line++;
                [[fallthrough]];
            case ' ':
            case '\t':
            case '\r':
                advance(scanner);
                break;
            case '/':
                if (peekNext(scanner) == '/') {
                    while (!isAtEnd(scanner) && peek(scanner) != '\n') {
                        advance(scanner);
                    }
                } else {
                    return;
                }
                break;
            default:
                return;
        }
}


static Token string(Scanner scanner[static 1]) {
    for (char c; !isAtEnd(scanner) && (c = peek(scanner)) != '"'; ) {
        if (c == '\n')
            scanner->line++;
        advance(scanner);
    }

    if (isAtEnd(scanner))
        return errorToken(scanner, "Unterminated string");

    advance(scanner);
    return makeToken(scanner, TOKEN_STRING);
}


static Token number(Scanner scanner[static 1]) {
    while (isdigit(peek(scanner)))
        advance(scanner);

    if (peek(scanner) == '.' && isdigit(peekNext(scanner))) {
        advance(scanner);
        while (isdigit(peek(scanner))) {
            advance(scanner);
        }
    }

    return makeToken(scanner, TOKEN_NUMBER);
}


static TokenType checkKeyword(
    Scanner scanner[static 1],
    uint32_t start,
    uint32_t length,
    char const* rest,
    TokenType type
) {
    bool lengthEqual = scanner->current - scanner->start == start + length;
    if (lengthEqual && !memcmp(scanner->start + start, rest, length))
        return type;

    return TOKEN_IDENTIFIER;
}


static TokenType identifierType(Scanner scanner[static 1]) {
    switch (scanner->start[0]) {
        case 'a': return checkKeyword(scanner, 1, 2, "nd", TOKEN_AND);
        case 'c': return checkKeyword(scanner, 1, 4, "lass", TOKEN_CLASS);
        case 'e': return checkKeyword(scanner, 1, 3, "lse", TOKEN_ELSE);
        case 'f':
            if (scanner->current - scanner->start > 1) {
                switch (scanner->start[1]) {
                    case 'a': return checkKeyword(scanner, 2, 3, "lse", TOKEN_FALSE);
                    case 'o': return checkKeyword(scanner, 2, 1, "r", TOKEN_FOR);
                    case 'u': return checkKeyword(scanner, 2, 1, "n", TOKEN_FUN);
                }
            }
            break;
        case 'i': return checkKeyword(scanner, 1, 1, "f", TOKEN_IF);
        case 'n': return checkKeyword(scanner, 1, 2, "il", TOKEN_NIL);
        case 'o': return checkKeyword(scanner, 1, 1, "r", TOKEN_OR);
        case 'p': return checkKeyword(scanner, 1, 4, "rint", TOKEN_PRINT);
        case 'r': return checkKeyword(scanner, 1, 5, "eturn", TOKEN_RETURN);
        case 's': return checkKeyword(scanner, 1, 4, "uper", TOKEN_SUPER);
        case 't':
            if (scanner->current - scanner->start > 1) {
                switch (scanner->start[1]) {
                    case 'h': return checkKeyword(scanner, 2, 2, "is", TOKEN_THIS);
                    case 'r': return checkKeyword(scanner, 2, 2, "ue", TOKEN_TRUE);
                }
            }
            break;
        case 'v': return checkKeyword(scanner, 1, 2, "ar", TOKEN_VAR);
        case 'w': return checkKeyword(scanner, 1, 4, "hile", TOKEN_WHILE);
    }

    return TOKEN_IDENTIFIER;
}


static Token identifier(Scanner scanner[static 1]) {
    for (char c = peek(scanner); isAlphaNum(c); c = peek(scanner))
        advance(scanner);

    return makeToken(scanner, identifierType(scanner));
}


void initScanner(Scanner scanner[static 1], char const* source) {
    *scanner = (Scanner) {
        .start = source,
        .current = source,
        .line = 1,
    };
}


Token scanToken(Scanner scanner[static 1]) {
    skipWhitespace(scanner);
    scanner->start = scanner->current;

    if (isAtEnd(scanner))
        return makeToken(scanner, TOKEN_EOF);

    char c = advance(scanner);
    if (isdigit(c))
        return number(scanner);
    if (isAlpha(c))
        return identifier(scanner);

    switch (c) {
        case '(': return makeToken(scanner, TOKEN_LEFT_PAREN);
        case ')': return makeToken(scanner, TOKEN_RIGHT_PAREN);
        case '{': return makeToken(scanner, TOKEN_LEFT_BRACE);
        case '}': return makeToken(scanner, TOKEN_RIGHT_BRACE);
        case ';': return makeToken(scanner, TOKEN_SEMICOLON);
        case ',': return makeToken(scanner, TOKEN_COMMA);
        case '.': return makeToken(scanner, TOKEN_DOT);
        case '-': return makeToken(scanner, TOKEN_MINUS);
        case '+': return makeToken(scanner, TOKEN_PLUS);
        case '/': return makeToken(scanner, TOKEN_SLASH);
        case '*': return makeToken(scanner, TOKEN_STAR);
        case '!':
            return makeToken(scanner, match(scanner, '=') ? TOKEN_BANG_EQUAL : TOKEN_BANG);
        case '=':
            return makeToken(scanner, match(scanner, '=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
        case '<':
            return makeToken(scanner, match(scanner, '=') ? TOKEN_LESS_EQUAL : TOKEN_LESS);
        case '>':
            return makeToken(scanner, match(scanner, '=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER);
        case '"': return string(scanner);
    }

    return errorToken(scanner, "Unexpected character.");
}
