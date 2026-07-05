# Hello World

A minimal C "Hello, world!" project built with modern CMake.

## Requirements

- CMake >= 3.20
- A C11-compatible compiler (GCC, Clang, or MSVC)

## Build

```sh
cmake -S . -B build
cmake --build build
```

## Run

```sh
./build/hello_world
```

## Test

```sh
ctest --test-dir build
```
