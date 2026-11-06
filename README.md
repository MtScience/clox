# clox

This is a reimplementation of clox in C23 (as opposed to C99 used for the original version). Written partly as a reference for another project and partly to practice C.

## Features

- Fully functional implementation.
- Uses flexible array members for `ObjString`.
- Includes a test suite.
- Includes Doxygen documentation describing what each function is supposed to do.

## Building and running

This project uses CMake for building. To build this, you will need:
- CMake 3.25,
- GCC 15.3 or Clang 21.1.8. I suggest GCC, since it provides the fullest support of the C23 standard.

To build, clone the repo and run (from root directory):
```console
$ cmake -S . -B ./build
$ cmake --build build/
```

For Windows users: this project uses C23 features that are not supported by MSVC (such as enumeration type specifiers). Therefore, it will not compile without modifications. If you want to build it on Windows, use MinGW or Clang.
