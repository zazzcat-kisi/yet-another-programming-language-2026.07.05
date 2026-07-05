# Hello World

A minimal C "Hello, world!" project built with modern CMake.

## Requirements

- CMake >= 3.20
- A C11-compatible compiler (GCC, Clang, or MSVC)

## Libraries

### `stm` — Software Transactional Memory

`src/stm/stm.h` / `src/stm/stm.c` provide a small STM library used to allocate,
read, write, and release memory transactionally, so data structures built on
top of it can have their changes committed or rolled back as a unit. It's
implemented with MVCC (multi-version concurrency control): each
`stm_begin_transaction` call hands back an independent transaction that sees
a private snapshot of the world, and multiple transactions can be open at
once. `stm_commit_transaction` uses first-committer-wins conflict detection —
it fails if anything the transaction read or wrote has a newer committed
version than its snapshot. See `CLAUDE.md` for the class/namespace, naming,
and concurrency conventions this library follows (in particular: this project
is single-threaded, so the MVCC bookkeeping needs no locks).

The state object is `stm_t`, and every operation is a function named
`stm_<verb>` taking a `stm_t *` as its first argument. A `stm_transaction_t *`
handle (from `stm_begin_transaction`) is required by every memory operation,
and is consumed by whichever of commit/rollback is called on it:

- `stm_constructor` / `stm_destructor` — create/tear down an STM instance
- `stm_begin_transaction` / `stm_commit_transaction` / `stm_rollback_transaction`
  — start a transaction, and either commit it (fails on conflict) or abort it
- `stm_allocate_memory` / `stm_release_memory` — manage memory through the STM
- `stm_read` / `stm_write` — read/write the bytes at a handle

```c
#include "stm/stm.h"

stm_t *stm = stm_constructor();

stm_transaction_t *transaction = stm_begin_transaction(stm);
node_t *handle = stm_allocate_memory(stm, transaction, sizeof(node_t));
node_t node = {0};
/* ... build up the data structure ... */
stm_write(stm, transaction, handle, &node, sizeof(node));

if (something_went_wrong) {
    stm_rollback_transaction(stm, transaction); /* node is discarded */
} else if (!stm_commit_transaction(stm, transaction)) {
    /* another transaction committed a conflicting change; retry */
}

stm_destructor(stm);
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
