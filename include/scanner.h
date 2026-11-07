#ifndef clox_scanner_h
#define clox_scanner_h


/**
 * \struct Scanner scanner.h
 * \brief Code scanner type.
 *
 * A `struct` that represents the code scanner. Keeps pointers to two characters
 * in the source code string. The `Scanner` is lazy: it does not immediately generate
 * a list of `Token`s; instead the user is supposed to request them by calling
 * `scanToken()`.
 */
typedef struct {
    char const* start;    /**< Pointer to the character at the start of the token. */
    char const* current;  /**< Pointer to the current character. */
    uint32_t    line;     /**< Number of source line currently being scanned. */
} Scanner;


/**
 * \brief Token types enumeration.
 *
 * An `enum` that lists all the valid types for source tokens.
 */
typedef enum : uint8_t {
  /* Single-character tokens */

  TOKEN_LEFT_PAREN, TOKEN_RIGHT_PAREN,
  TOKEN_LEFT_BRACE, TOKEN_RIGHT_BRACE,
  TOKEN_COMMA, TOKEN_DOT, TOKEN_MINUS, TOKEN_PLUS,
  TOKEN_SEMICOLON, TOKEN_SLASH, TOKEN_STAR,

  /* One or two character tokens */

  TOKEN_BANG, TOKEN_BANG_EQUAL,
  TOKEN_EQUAL, TOKEN_EQUAL_EQUAL,
  TOKEN_GREATER, TOKEN_GREATER_EQUAL,
  TOKEN_LESS, TOKEN_LESS_EQUAL,

  /* Literals. */

  TOKEN_IDENTIFIER, TOKEN_STRING, TOKEN_NUMBER,

  /* Keywords */

  TOKEN_AND, TOKEN_CLASS, TOKEN_ELSE, TOKEN_FALSE,
  TOKEN_FOR, TOKEN_FUN, TOKEN_IF, TOKEN_NIL, TOKEN_OR,
  TOKEN_PRINT, TOKEN_RETURN, TOKEN_SUPER, TOKEN_THIS,
  TOKEN_TRUE, TOKEN_VAR, TOKEN_WHILE,

  TOKEN_ERROR, TOKEN_EOF,
} TokenType;


/**
 * \struct Token scanner.h
 * \brief Token type.
 *
 * A `struct` that represents a source token. Keeps information about the
 * corresponding lexeme.
 */
typedef struct {
    char const* start;   /**< Pointer to the start of the corresponding lexeme. */
    uint32_t    line;    /**< Number of source line from the token was compiled. */
    uint16_t    length;  /**< Length of the corresponding lexeme in bytes. */
    TokenType   type;    /**< Type of the token. */
} Token;


/**
 * \brief Initializes a scanner.
 *
 * Takes a newly-created `Scanner` and fills in its fields, putting the structure
 * in a well-defined state.
 *
 * \param[in,out] scanner Non-`NULL` pointer to the scanner to be Initialized.
 * \param[in]     source  The string containing the source code to scan.
 */
void initScanner(Scanner scanner[static 1], char const* source);

/**
 * \brief Scans one token from the source.
 *
 * \param[in,out] scanner The scanner to request a token from.
 *
 * \returns A token scanned from the current position in the source
 *          or an error token in case scan failed.
 */
Token scanToken(Scanner scanner[static 1]);


#endif
