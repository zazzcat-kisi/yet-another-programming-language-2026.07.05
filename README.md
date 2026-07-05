# Hello World

A minimal C "Hello, world!" project built with modern CMake.

## Requirements

- CMake >= 3.20
- A C11-compatible compiler (GCC, Clang, or MSVC)

## Libraries

Everything under `src/` nests under a single root namespace `y` (see
`CLAUDE.md` for the full class/namespace, naming, error-handling, and
concurrency conventions this project follows).

### `y_stm` — Software Transactional Memory

`src/y/stm/include.h` / `src/y/stm/implementation.c` provide a small STM
library used to allocate, read, write, and release memory transactionally,
so data structures built on top of it can have their changes committed or
rolled back as a unit. It's implemented with MVCC (multi-version
concurrency control): each `y_stm_begin_transaction` call hands back an
independent transaction that sees a private snapshot of the world, and
multiple transactions can be open at once. `y_stm_commit_transaction` uses
first-committer-wins conflict detection — it fails if anything the
transaction read or wrote has a newer committed version than its snapshot.
This project is single-threaded, so the MVCC bookkeeping needs no locks.

The state object is `y_stm_t`, and every operation is a function named
`y_stm_<verb>` taking a `y_stm_t *` as its first argument. A
`y_stm_transaction_t *` handle (from `y_stm_begin_transaction`) is required
by every memory operation, and is consumed by whichever of commit/rollback
is called on it:

- `y_stm_constructor` / `y_stm_destructor` — create/tear down an STM instance
- `y_stm_begin_transaction` / `y_stm_commit_transaction` / `y_stm_rollback_transaction`
  — start a transaction, and either commit it (fails on conflict) or abort it
- `y_stm_allocate_memory` / `y_stm_release_memory` — manage memory through the STM
- `y_stm_read` / `y_stm_write` — read/write the bytes at a handle

`y_stm_allocate_memory` hands back a `y_stm_handle_t` — an opaque reference,
not a usable pointer. Don't dereference it; always go through `y_stm_read`/
`y_stm_write`.

```c
#include "y/stm/include.h"

y_stm_t *stm = y_stm_constructor();

y_stm_transaction_t *transaction = y_stm_begin_transaction(stm);
y_stm_handle_t handle = y_stm_allocate_memory(stm, transaction, sizeof(node_t));
node_t node = {0};
/* ... build up the data structure ... */
y_stm_write(stm, transaction, handle, &node, sizeof(node));

if (something_went_wrong) {
    y_stm_rollback_transaction(stm, transaction); /* node is discarded */
} else if (!y_stm_commit_transaction(stm, transaction)) {
    /* another transaction committed a conflicting change; retry */
}

y_stm_destructor(stm);
```

### `y_stm_alloc` / `y_stm_mem` — standard library replacements

`src/y/stm/alloc/` and `src/y/stm/mem/` are thin, namespaced wrappers around
the C allocator (`y_stm_alloc_malloc`/`y_stm_alloc_calloc`/`y_stm_alloc_free`)
and `<string.h>` byte functions (`y_stm_mem_memcpy`). The rest of `y_stm`
goes through these instead of calling `malloc`/`free`/`memcpy` directly, so
the underlying implementation can be swapped in one place later.

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
