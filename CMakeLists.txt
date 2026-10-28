cmake_minimum_required(VERSION 3.25)

project(clox
    VERSION 1.0.0
    LANGUAGES C
)

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
    src/gc.c
)

# Include project headers, if any.
target_include_directories(clox
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Build flags
option(DEBUG_TRACE_EXECUTION "Trace execution" OFF)
option(DEBUG_PRINT_CODE "Print compiled chunks" ON)
option(DEBUG_GC "Garbage-collection debugging mode" OFF)

if (DEBUG_TRACE_EXECUTION)
    target_compile_definitions(clox PRIVATE DEBUG_TRACE_EXECUTION)
endif()

if (DEBUG_PRINT_CODE)
    target_compile_definitions(clox PRIVATE DEBUG_PRINT_CODE)
endif()

if (DEBUG_GC)
    target_compile_definitions(clox
        PRIVATE
            DEBUG_STRESS_GC
            DEBUG_LOG_GC
    )
endif()


# Optional: enable stricter warnings for GCC specifically.
target_compile_options(clox
    PRIVATE
        -Werror
)
