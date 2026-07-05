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
