#include <stdio.h>   /* For fopen, fclose, fseek, ftell, rewind, fprintf, fgets, fread, putchar, printf, FILE, stderr, stdin */
#include <stdlib.h>  /* For malloc, free, exit, EXIT_SUCCESS */

#include "common.h"
#include "chunk.h"
#include "debug.h"
#include "vm.h"


static char* readFile(char const* path) {
    FILE* file = fopen(path, "rb");
    if (!file) {
        fprintf(stderr, "Could not open file \"%s\".\n", path);
        exit(74);
    }

    fseek(file, 0, SEEK_END);
    size_t fileSize = (size_t)ftell(file);
    rewind(file);

    char* buffer = malloc(fileSize + 1);
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


static void runFile(VM* vm, char const* path) {
    char* source = readFile(path);
    InterpretResult result = interpret(vm, source);
    free(source);

    if (result == INTERPRET_COMPILE_ERROR) exit(65);
    if (result == INTERPRET_RUNTIME_ERROR) exit(70);
}


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
