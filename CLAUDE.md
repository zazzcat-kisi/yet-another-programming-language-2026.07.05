# Project conventions

This is a C project. In lieu of real classes, we use a "poor man's object"
convention: a typed struct is the object/state, and free functions
namespaced by prefix are its methods. Follow these rules for all code in
this repo.

## Root namespace

- Everything in this project nests under a single root namespace `y`. A
  "class" that would otherwise be called `stm` is actually namespace
  `y_stm` (type `y_stm_t`, e.g. `y_stm_handle_t`, `y_stm_constructor`,
  `y_stm_allocate_memory`, ...). There is no bare `stm_*`/`foo_*` naming
  anywhere — every namespace starts with `y_`.
- `y` itself is not a class: it has no state struct, constructor, or
  `include.h`/`implementation.c` of its own. It is just the root every
  other namespace nests under, and it corresponds to the `src/` directory
  itself (see below).
- Not every namespace is backed by a class. A purely-functional utility
  namespace (e.g. `y_stm_alloc`, `y_stm_mem`) groups related free
  functions with no state object; its functions don't take an object
  pointer as their first argument, because there is no object to take one.

## Classes / namespaces

- A "class" is a typed state struct, e.g. `typedef struct y_foo y_foo_t;`.
  Its namespace is the type's name with the `_t` suffix dropped (`y_foo_t`
  -> namespace `y_foo`).
- Every function that operates on a class takes a pointer to it as the
  **first** parameter, and is named `<namespace>_<verb>[_<detail>]`. The
  `_` is the namespace separator — there is no other scoping mechanism.
- Only give something its own namespace if it is an independently
  meaningful object. Purely internal helper structs (e.g. a linked-list
  node used to implement another class) stay unnamespaced implementation
  detail inside that class's `implementation.c` file.
- Favor a distinct typed pointer over a bare `void *` for handles/
  references (e.g. `y_stm_handle_t`, not `void *`). A `void *` silently
  accepts any pointer; a named type lets the compiler catch misuse.

## Folder hierarchy mirrors namespaces

- Every namespace lives under `src/` at a path matching its name: the
  root namespace `y` is `src/` itself. Namespace `y_stm` (nested under
  `y`) lives in `src/y/stm/`. A further-nested namespace `y_stm_alloc` (a
  sub-object of `y_stm`) lives in `src/y/stm/alloc/`.
- A namespace's implementation file would otherwise just repeat its own
  containing folder's name (e.g. `stm/stm.h`) — instead, name the files
  `include.h` and `implementation.c`: `src/y/stm/include.h` +
  `src/y/stm/implementation.c`, `src/y/stm/alloc/include.h` +
  `src/y/stm/alloc/implementation.c`, and so on. This "same name as parent
  directory -> include.h/implementation.c" rule applies generally,
  wherever it comes up, not just at namespace roots.
- Includes reference this same path from the `src/` root, e.g.
  `#include "y/stm/include.h"` or `#include "y/stm/alloc/include.h"`.
- Don't flatten files into `src/` directly — the directory structure is
  how you find a namespace's implementation, so it must match the name.

## Member naming

- **Constructor**: `<namespace>_constructor` (allocates/initializes the
  state object). **Destructor**: `<namespace>_destructor` (tears it down).
  Do not use `_create`/`_destroy`/`_init`/`_free`.
- **Boolean queries**: prefix with `is_`, e.g. `<namespace>_is_<x>`.
- **Transformations** (produce a different representation of self):
  prefix with `as_`, e.g. `<namespace>_as_<x>`.
- **Property-like access**: `<namespace>_get_<property>` /
  `<namespace>_set_<property>`.
- **Containers**: query membership with `contains`, and quantity with
  `count` — never `size`. E.g. `<namespace>_contains(...)`,
  `<namespace>_count(...)`.
- **Sizes/lengths in general**: always name the unit explicitly, e.g.
  `length_in_bytes`. Never use a bare `size` or `length`.

## Concurrency model

- All code in this repository runs on a single OS thread. There is no
  preemptive multitasking: control only ever transfers at explicit call/
  return boundaries, never in the middle of a function.
- Do not add mutexes, atomics, memory barriers, `volatile`, or
  compare-and-swap loops to protect shared state — plain sequential reads
  and writes are sufficient, since nothing can run concurrently with them.
- This does not mean there is only ever one logical task in flight: code
  (e.g. the STM library) may still model multiple independent, overlapping
  units of work (transactions, coroutines, ...) that interleave with each
  other. Handle that with ordinary sequential bookkeeping (versions,
  generation counters, explicit handles), not with locking primitives.

## Error handling

- Agents must never ignore an error code. Every call that can fail
  (returns `bool`, a null pointer, a negative/sentinel value, an `errno`,
  etc.) must have its result checked at the call site; propagate the
  failure to the caller or handle it there, never silently discard it.
- There are two deliberate exceptions, described in their own sections
  below: a condition routed through `y_fatal_terminate` ("Fatal errors"),
  and a `y_stm` transactional operation that fails and rolls its
  transaction back instead of returning an error code ("Rollback on
  failure"). In both cases there is no error code to check or propagate
  by design — don't add a dead NULL/failure check for either.

## Fatal errors

- `y_fatal_terminate` (namespace `y_fatal`, in `src/y/fatal/`) is this
  project's way to give up on an unrecoverable condition: it prints a
  diagnostic and terminates the process immediately. It takes no state
  object (it's a purely functional utility, like `y_stm_alloc`/`y_stm_mem`)
  and never returns.
- Out-of-memory is the current trigger: `y_stm_alloc_malloc`/
  `y_stm_alloc_calloc` call `y_fatal_terminate` instead of returning NULL
  for a nonzero-size allocation that fails, since this project has no
  strategy for continuing after an allocation failure. Because of this,
  their callers must not (and don't need to) check the result for NULL —
  see "Error handling" above.
- Agents must not call `abort`/`exit`/`_exit` directly elsewhere in this
  repo for an unrecoverable condition — route it through
  `y_fatal_terminate` instead, the same way allocation goes through
  `y_stm_alloc`.

## Rollback on failure

- Transactional operations against `y_stm` (`y_stm_allocate_memory`,
  `y_stm_release_memory`, `y_stm_read`, `y_stm_write`) do not return an
  error code. On failure (a stale snapshot, a conflicting committer, a
  bad handle, ...) they record the failure via `y_error_set` and roll the
  transaction back via `y_stm_rollback_transaction` instead of returning
  one. This is the second deliberate exception to "never ignore an error
  code" above: there's no bool/NULL result to check, because the failure
  already happened and was already handled for you.
- `y_error_t` (namespace `y_error`, in `src/y/error/`) is a small,
  transparent value type holding a code and a message — this project's way
  of recording *why* something failed once it no longer returns an error
  code for it. It's a "class" per the "Classes / namespaces" rules above,
  but its fields are declared in its own `include.h` (rather than kept
  opaque) because it needs to be embedded by value inside its owner. Go
  through `y_error_set`/`y_error_get` rather than touching the fields
  directly anyway.
- `y_stm_is_rolled_back` reports whether a transaction has been rolled
  back, whether by an explicit `y_stm_rollback_transaction` call or a
  failure inside one of the operations above. Once true, no further
  operation against that transaction ever changes anything in the STM —
  every one of them becomes a safe no-op, silently, without touching any
  output parameters. Code may check `y_stm_is_rolled_back` where it's
  actually useful (e.g. to skip an expensive computation whose result
  would just be discarded), but does not need to check it, or any
  per-operation result, for correctness.
- `y_stm_get_error` returns the most recently recorded error for a
  transaction. That record is plain (non-STM-managed) state, so it
  survives the rollback it describes and stays inspectable right up
  until the transaction is finally consumed by
  `y_stm_commit_transaction`/`y_stm_rollback_transaction` (same as the
  transaction handle itself) — don't call `y_stm_is_rolled_back`/
  `y_stm_get_error` after either of those.
- Libraries built on `y_stm` (e.g. `y_string`) must follow the same
  pattern for their own validation failures, using `y_stm_fail_transaction`
  instead of introducing an error code of their own — don't reintroduce a
  bool/NULL return for something that can be reported this way.
  `y_stm_commit_transaction` is the one exception that keeps returning
  `bool`: it's a transaction's one meaningful, terminal outcome, not a
  per-operation error code.

## Standard library replacements

- `y_stm_alloc` provides this project's replacements for the C allocator
  — `y_stm_alloc_malloc`, `y_stm_alloc_calloc`, `y_stm_alloc_free` — and
  `y_stm_mem` provides replacements for `<string.h>` byte-manipulation
  functions as they're needed (currently `y_stm_mem_memcpy`,
  `y_stm_mem_memcmp`). Add to either only the functions actually used
  elsewhere in the codebase, not the full libc surface speculatively.
- Agents must never call a stdlib function that has a `y_*` replacement
  (e.g. never call `malloc`/`calloc`/`free`/`memcpy` directly anywhere
  else in this repo) — always go through the `y_*` wrapper instead. The
  one exception is inside `y_stm_alloc`/`y_stm_mem`'s own
  `implementation.c`, which is the sole place allowed to call the
  underlying libc function, since that's what it wraps.
