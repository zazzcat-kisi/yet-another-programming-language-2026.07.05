#ifndef STM_H
#define STM_H

#include <stdbool.h>
#include <stddef.h>

/*
 * Software Transactional Memory over a heap-backed arena.
 *
 * `stm_t` is the state object ("class"); every operation is a function
 * named `stm_<verb>` taking a pointer to it as the first argument
 * (the namespaced "method" convention used throughout this project).
 *
 * Scope: this STM makes memory *lifetime* (allocate/release) transactional.
 * Rolling back a transaction undoes allocations and releases performed
 * inside it, but does not track or undo ordinary writes through the
 * returned pointers -- callers are free to build plain data structures on
 * top of STM-managed memory exactly as they would with malloc/free.
 */
typedef struct stm stm_t;

stm_t *stm_create(void);
void stm_destroy(stm_t *stm);

void *stm_allocate_memory(stm_t *stm, size_t size);
bool stm_release_memory(stm_t *stm, void *ptr);

bool stm_begin_transaction(stm_t *stm);
bool stm_commit_transaction(stm_t *stm);
void stm_rollback_transaction(stm_t *stm);

bool stm_in_transaction(const stm_t *stm);

#endif
