cmake_minimum_required(VERSION 3.25)

project(clox
    VERSION 1.0.0
    LANGUAGES C
)

# Refusing MSVC, since it doesn't support proper C23
if(NOT CMAKE_C_COMPILER_ID MATCHES "^(GNU|Clang|AppleClang)$")
    message(FATAL_ERROR
        "\n[Error] This project uses C23 features that are not supported by"
        "\nMSVC yet. Use GCC (preferably) or Clang instead."
    )
endif()

if(MSVC)
    message(FATAL_ERROR "\n[Error] MSVC is not supported. Use GCC or Clang.")
endif()

# Use the C23 standard.
set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Main executable.
add_executable(clox)
target_sources(clox
    PRIVATE
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
option(DEBUG_PRINT_CODE "Print compiled chunks" OFF)
option(DEBUG_GC "Garbage-collection debugging mode" OFF)
option(NAN_BOXING "Use NaN boxing optimization" ON)

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

if (NAN_BOXING)
    target_compile_definitions(clox PRIVATE NAN_BOXING)
endif()

# Compilation flags
target_compile_options(clox
    PRIVATE
        -Werror
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wsign-conversion
)

