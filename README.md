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

### `y_blob` — a fixed-length, transactional byte buffer

`src/y/blob/include.h` / `src/y/blob/implementation.c` provide `y_blob_t`,
a fixed-length buffer of raw bytes built on top of `y_stm`. It's the
fixed-size storage primitive that byte-container classes like `y_string` and
`y_packed_array` are built on:

- `y_blob_constructor` / `y_blob_destructor` — create/tear down a blob
- `y_blob_get_length_in_bytes` — the blob's fixed length (immutable, so it
  needs no transaction)
- `y_blob_read` — copy the blob's bytes out (destination must match the
  fixed length exactly)
- `y_blob_write` — overwrite the bytes in place (the new length must equal
  the fixed length)

A blob's length is fixed at construction and never changes: there is
deliberately **no append or resizing write**. Growing and shrinking are a
higher-level concern — a class that needs a resizable buffer (like
`y_string`) layers that on top by allocating a new blob and copying, rather
than having the blob grow underneath it. Because the length is immutable, a
single fixed `y_stm` slot holds the bytes directly (no versioned
"descriptor" indirection), and `y_blob_get_length_in_bytes` needs no
transaction.

Unlike `y_string`, a blob is just bytes: it takes `const void *`/`void *`
rather than `const char *`, and holds arbitrary binary data (embedded NUL
bytes are content, not terminators). Like `y_stm`'s own operations, its
read/write don't return an error code — a failure (including a length that
doesn't match the fixed length) reports itself through
`y_stm_fail_transaction` (under `Y_BLOB_ERROR_*`) and rolls `transaction`
back.

### `y_packed_array` — a fixed-count, transactional array

`src/y/packed_array/include.h` / `src/y/packed_array/implementation.c`
provide `y_packed_array_t`, a fixed-count array of fixed-size elements packed
contiguously into a single `y_blob`:

- `y_packed_array_constructor` / `y_packed_array_destructor` — create
  (zero-initialized) / tear down an array
- `y_packed_array_get_count` / `y_packed_array_get_element_length_in_bytes` —
  the fixed dimensions (immutable, so no transaction)
- `y_packed_array_get` / `y_packed_array_set` — read/overwrite the element at
  an index (both validate the index and the element length)

The count and element length are fixed at construction — like the blob it
sits on, a packed array never grows, which is exactly why it fits a fixed
blob so directly. Its `Y_PACKED_ARRAY_ERROR_*` failures (out-of-range index,
wrong element length, an overflowing size at construction) report themselves
through `y_stm_fail_transaction` and roll `transaction` back. Because all the
elements share one packed `y_stm` slot, a `y_packed_array_set` is a
read-modify-write of the whole buffer, and any two transactions that each
write *any* element conflict at commit under first-committer-wins — the
trade-off of packing everything contiguously into one slot.

### `y_string` — a transactional string, serving the role of `str*`

`src/y/string/include.h` / `src/y/string/implementation.c` provide
`y_string_t`, a dynamically-sized byte string that owns the growing concern
itself: it stores a fixed-length "descriptor" record (naming its current
content slot and length) in a `y_blob`, and resizes by allocating a new
content slot and versioning the swap through that descriptor. It adds string
semantics on top, playing the role `<string.h>`'s `str*` functions play for
plain C strings:

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
separate `y_stm_t *`, since a string caches which STM it belongs to. The
string's descriptor is a fixed `y_blob`, so which content slot is current
and how long it is stays versioned STM data — keeping it isolated across
overlapping transactions. That is why even `y_string_get_length_in_bytes`
takes a transaction, and a `y_string_write`/`y_string_append` that resizes
the string (allocating a fresh content slot and updating the descriptor)
participates in the same first-committer-wins conflict detection as any
other write. The blob itself never resizes; the growing lives in `y_string`.
Like `y_stm`'s own operations, these don't return an error code — a failure
reports itself through `y_stm_fail_transaction` and rolls `transaction`
back, checkable via `y_stm_is_rolled_back`/`y_stm_get_error`. `y_string`
keeps its own `Y_STRING_ERROR_*` codes for the validation failures at its
public boundary (a NULL/length-mismatched argument), so callers never see
the `y_blob` that backs it.

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
