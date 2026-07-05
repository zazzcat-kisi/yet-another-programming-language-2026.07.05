# Project conventions

This is a C project. In lieu of real classes, we use a "poor man's object"
convention: a typed struct is the object/state, and free functions
namespaced by prefix are its methods. Follow these rules for all code in
this repo.

## Classes / namespaces

- A "class" is a typed state struct, e.g. `typedef struct foo foo_t;`. Its
  namespace is the type's name with the `_t` suffix dropped (`foo_t` ->
  namespace `foo`).
- Every function that operates on a class takes a pointer to it as the
  **first** parameter, and is named `<namespace>_<verb>[_<detail>]`. The
  `_` is the namespace separator — there is no other scoping mechanism.
- Only give something its own namespace if it is an independently
  meaningful object. Purely internal helper structs (e.g. a linked-list
  node used to implement another class) stay unnamespaced implementation
  detail inside that class's `.c` file.

## Folder hierarchy mirrors namespaces

- Every namespace lives under `src/` at a path matching its name: a
  top-level namespace `foo` lives in `src/foo/foo.h` + `src/foo/foo.c`. A
  nested namespace `foo_bar` (a sub-object of `foo`) lives in
  `src/foo/bar/bar.h` + `src/foo/bar/bar.c`.
- Includes reference this same path from the `src/` root, e.g.
  `#include "foo/foo.h"` or `#include "foo/bar/bar.h"`.
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
