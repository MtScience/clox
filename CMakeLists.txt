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
    src/native.c
)

# Include project headers, if any.
target_include_directories(clox
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Build flags
option(DEBUG_TRACE_EXECUTION "Trace execution" ON)
option(DEBUG_PRINT_CODE "Print compiled chunks" ON)

if (DEBUG_TRACE_EXECUTION)
    target_compile_definitions(clox PRIVATE DEBUG_TRACE_EXECUTION)
endif()

if (DEBUG_PRINT_CODE)
    target_compile_definitions(clox PRIVATE DEBUG_PRINT_CODE)
endif()


# Optional: enable stricter warnings for GCC specifically.
target_compile_options(clox
    PRIVATE
        -Werror
)
