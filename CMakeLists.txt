cmake_minimum_required(VERSION 3.25)

project(clox
    VERSION 1.0.0
    LANGUAGES C
)

# Require GCC.
if (NOT CMAKE_C_COMPILER_ID STREQUAL "GNU")
    message(FATAL_ERROR "This project requires GCC.")
endif()

# Use the C23 standard.
set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Enable useful warnings.
add_compile_options(
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wsign-conversion
)

# Main executable.
add_executable(clox
    src/main.c
    src/chunk.c
    src/memory.c
    src/debug.c
    src/value.c
    src/vm.c
    src/compiler.c
    src/scanner.c
    src/object.c
    src/table.c
)

# Include project headers, if any.
target_include_directories(clox
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Optional: enable stricter warnings for GCC specifically.
target_compile_options(clox
    PRIVATE
        -Werror
)
