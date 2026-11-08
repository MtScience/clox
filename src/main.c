#include <stdio.h>   /* For fopen, fclose, fseek, ftell, rewind, fprintf, fgets, fread, putchar, printf, FILE, stderr, stdin */
#include <stdlib.h>  /* For malloc, free, exit, EXIT_SUCCESS */

#include "common.h"
#include "chunk.h"
#include "debug.h"
#include "vm.h"


/**
 * \brief Reads a file at the given location.
 *
 * Attempts to open the file, determine the size of its content, allocate
 * memory to hold said content in a string and read the file into that string.
 * If at any point an error happens, the function fails.
 *
 * \param[in] path C string containing the path to the file.
 *
 * \returns The pointer to the allocated source string.
 */
static char* readFile(char const* path) {
    FILE* file = fopen(path, "rb");
    if (!file) {
        fprintf(stderr, "Could not open file \"%s\".\n", path);
        exit(74);
    }

    fseek(file, 0, SEEK_END);
    size_t fileSize = (size_t)ftell(file);
    rewind(file);

    char* buffer = malloc(sizeof(char[fileSize + 1]));
    if (!buffer) {
        fprintf(stderr, "Not enough memory to read \"%s\".\n", path);
        exit(74);
    }

    size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
    if (bytesRead < fileSize) {
        fprintf(stderr, "Could not read file \"%s\".\n", path);
        exit(74);
    }

    buffer[bytesRead] = '\0';

    fclose(file);
    return buffer;
}


/**
 * \brief Runs the Lox REPL.
 *
 * Reads, compiles and runs user input in an endless loop. The size of the input line
 * must not exceed 1024 bytes.
 *
 * \param[in] vm Non-`NULL` pointer to the virtual machine to run the code.
 */
static void repl(VM* vm) {
    char line[1024] = {};

    for (;;) {
        printf("> ");

        if (!fgets(line, sizeof line, stdin)) {
            putchar('\n');
            break;
        }

        interpret(vm, line);
    }
}


/**
 * \brief Runs a Lox file.
 *
 * Reads, compiles and runs source from a file. Fails if the file cannot be read.
 *
 * \param[in] vm   Non-`NULL` pointer to the virtual machine to run the code.
 * \param[in] path C string containing the path to the Lox source file.
 */
static void runFile(VM* vm, char const* path) {
    char* source = readFile(path);
    InterpretResult result = interpret(vm, source);
    free(source);

    if (result == INTERPRET_COMPILE_ERROR) {
        exit(65);
    }
    if (result == INTERPRET_RUNTIME_ERROR) {
        exit(70);
    }
}


/**
 * \brief The `main` function.
 *
 * Program entry point. Returns one of the following exit codes:
 *
 * | Exit code      | Meaning                                                  |
 * |----------------|----------------------------------------------------------|
 * | `EXIT_SUCCESS` | Success                                                  |
 * | `EXIT_FAILURE` | Memory allocation failure                                |
 * | 64             | Incorrect program invocation (wrong number of arguments) |
 * | 65             | Compilation error                                        |
 * | 70             | Runtime error                                            |
 * | 74             | Could not open or read the specified Lox source file     |
 *
 * \param[in] argc Number of command-line arguments
 * \param[in] argv List of command-line arguments as C strings.
 *
 * \returns Exit code according to the table above.
 */
int main(int argc, char const* argv[static argc + 1]) {
    VM vm;
    initVM(&vm);

    if (argc == 1) {
        repl(&vm);
    } else if (argc == 2) {
        runFile(&vm, argv[1]);
    } else {
        fprintf(stderr, "Usage: clox [path]\n");
        exit(64);
    }

    freeVM(&vm);

    return EXIT_SUCCESS;
}
