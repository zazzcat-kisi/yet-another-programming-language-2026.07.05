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
- `y_stm_is_rolled_back` / `y_stm_get_error` — check a transaction's status
  and last recorded failure
- `y_stm_fail_transaction` — for libraries built on `y_stm` (e.g. `y_string`)
  to report their own validation failures the same way `y_stm` reports its own

`y_stm_allocate_memory` hands back a `y_stm_handle_t` — an opaque reference,
not a usable pointer. Don't dereference it; always go through `y_stm_read`/
`y_stm_write`.

`y_stm_allocate_memory`/`y_stm_release_memory`/`y_stm_read`/`y_stm_write`
don't return an error code. On failure (a stale snapshot, a conflicting
committer, a bad handle, ...) they record why via `y_error_set` and roll
the transaction back for you, instead of returning one. From that point,
`y_stm_is_rolled_back` is true and every further operation against that
transaction is a safe no-op that changes nothing — so it's fine to keep
calling things without checking each one, and check `y_stm_is_rolled_back`
only where it's actually useful, e.g. to skip expensive work you know would
just be discarded:

```c
#include "y/stm/include.h"

y_stm_t *stm = y_stm_constructor();

y_stm_transaction_t *transaction = y_stm_begin_transaction(stm);
y_stm_handle_t handle = y_stm_allocate_memory(stm, transaction, sizeof(node_t));
node_t node = {0};
/* ... build up the data structure ... */
y_stm_write(stm, transaction, handle, &node, sizeof(node));

if (y_stm_is_rolled_back(stm, transaction)) {
    y_error_t error = y_error_get(y_stm_get_error(stm, transaction));
    fprintf(stderr, "transaction failed (%d): %s\n", error.code, error.message);
}

if (!y_stm_commit_transaction(stm, transaction)) {
    /* rolled back already, or a conflicting change committed first; retry */
}

y_stm_destructor(stm);
```

### `y_error` — recording why something failed

`src/y/error/include.h` / `src/y/error/implementation.c` provide
`y_error_t`, a small value type holding a `code` and a `message`, along with
`y_error_set`/`y_error_get`. It's what `y_stm` uses to record *why* a
transaction was rolled back (see `y_stm_get_error` above) once it stopped
returning an error code for that. Its fields are declared in its own header
(unlike most other state objects here) since it needs to be embedded by
value inside its owner; go through `y_error_set`/`y_error_get` anyway.

### `y_fatal` — unrecoverable errors

`src/y/fatal/include.h` / `src/y/fatal/implementation.c` provide
`y_fatal_terminate(const char *message)`, this project's way to give up on a
condition it cannot recover from: it prints the message and terminates the
process immediately (no cleanup, no atexit handlers). There's no state
object and no return value — it's a purely functional utility, and it never
returns.

### `y_stm_alloc` / `y_stm_mem` — standard library replacements

`src/y/stm/alloc/` and `src/y/stm/mem/` are thin, namespaced wrappers around
the C allocator (`y_stm_alloc_malloc`/`y_stm_alloc_calloc`/`y_stm_alloc_free`)
and `<string.h>` byte functions (`y_stm_mem_memcpy`/`y_stm_mem_memcmp`). The
rest of the codebase goes through these instead of calling
`malloc`/`free`/`memcpy`/`memcmp` directly, so the underlying implementation
can be swapped in one place later. `y_stm_alloc_malloc`/`y_stm_alloc_calloc`
never return NULL for a nonzero-size request — out-of-memory calls
`y_fatal_terminate` instead of propagating a failure their callers would
have no way to recover from anyway.

### `y_string` — a transactional string, serving the role of `str*`

`src/y/string/include.h` / `src/y/string/implementation.c` provide
`y_string_t`, a dynamically-sized byte string built on top of `y_stm`,
playing the role `<string.h>`'s `str*` functions play for plain C strings:

- `y_string_constructor` / `y_string_destructor` — create/tear down a string
- `y_string_get_length_in_bytes` — current length (like `strlen`)
- `y_string_read` — copy the string's bytes out (destination must match
  the current length exactly)
- `y_string_write` — replace the string's content, resizing as needed
  (like `strcpy`)
- `y_string_append` — append to the string's content, resizing as needed
  (like `strcat`)
- `y_string_compare` / `y_string_is_equal` — lexicographic byte comparison
  (like `strcmp`) and an equality convenience
- `y_string_as_c_string` — a newly allocated, NUL-terminated copy (like
  `strdup`), released with `y_stm_alloc_free`

Every operation but the constructor and `y_string_as_c_string` takes just
`y_string_t *` (or `const y_string_t *`) and a `y_stm_transaction_t *` — no
separate `y_stm_t *`, since a string caches which STM it belongs to. Because
a `y_string_t` can be shared across overlapping transactions, its current
content handle and length are themselves stored as versioned STM data
behind a small fixed-length "descriptor" slot, rather than as plain fields
on the host struct — so even `y_string_get_length_in_bytes` takes a
transaction, and a `y_string_write`/`y_string_append` that resizes the
string participates in the same first-committer-wins conflict detection as
any other write. Like `y_stm`'s own operations, these don't return an error
code — a failure reports itself through `y_stm_fail_transaction` and rolls
`transaction` back, checkable via `y_stm_is_rolled_back`/`y_stm_get_error`.

```c
#include "y/string/include.h"

y_stm_t *stm = y_stm_constructor();

y_stm_transaction_t *transaction = y_stm_begin_transaction(stm);
y_string_t *name = y_string_constructor(stm, transaction, "foo", 3);
y_string_append(name, transaction, "bar", 3); /* now "foobar" */

size_t length_in_bytes = 0;
y_string_get_length_in_bytes(name, transaction, &length_in_bytes); /* 6 */

char *c_string = y_string_as_c_string(name, transaction); /* "foobar\0" */
/* ... use c_string ... */
y_stm_alloc_free(c_string);

y_string_destructor(name, transaction);
y_stm_commit_transaction(stm, transaction);

y_stm_destructor(stm);
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
