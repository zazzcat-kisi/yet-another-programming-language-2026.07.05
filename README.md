# Hello World

A minimal C "Hello, world!" project built with modern CMake.

## Requirements

- CMake >= 3.20
- A C11-compatible compiler (GCC, Clang, or MSVC)

## Libraries

### `stm` — Software Transactional Memory

`src/stm.h` / `src/stm.c` provide a small STM library used to allocate and
release memory transactionally, so data structures built on top of it can
have their allocations committed or rolled back as a unit.

The state object is `stm_t`, and every operation is a function named
`stm_<verb>` taking a `stm_t *` as its first argument:

- `stm_create` / `stm_destroy` — create/tear down an STM instance
- `stm_allocate_memory` / `stm_release_memory` — manage memory through the STM
- `stm_begin_transaction` / `stm_commit_transaction` / `stm_rollback_transaction`
  — group allocations and releases into a transaction; rolling back undoes
  every allocation and release made since the matching `stm_begin_transaction`
- `stm_in_transaction` — check whether a transaction is currently active

```c
stm_t *stm = stm_create();

stm_begin_transaction(stm);
node_t *node = stm_allocate_memory(stm, sizeof(node_t));
/* ... build up the data structure ... */
if (something_went_wrong) {
    stm_rollback_transaction(stm); /* node is freed automatically */
} else {
    stm_commit_transaction(stm);   /* node persists */
}

stm_destroy(stm);
```

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
