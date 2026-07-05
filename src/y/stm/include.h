#ifndef Y_STM_INCLUDE_H
#define Y_STM_INCLUDE_H

#include <stdbool.h>
#include <stddef.h>

/*
 * Software Transactional Memory over a versioned heap, using MVCC
 * (multi-version concurrency control) rather than locks.
 *
 * `y_stm_t` is the state object ("class"); `y_stm_transaction_t` is an
 * independent, explicitly-handled unit of work against it — multiple
 * transactions may be open at the same time (e.g. representing separate
 * coroutines), each seeing a private snapshot of the world as of its
 * `y_stm_begin_transaction` call.
 *
 * Every value lives at a `y_stm_handle_t` handed back by
 * `y_stm_allocate_memory`. A handle is opaque: do not dereference it
 * directly, go through `y_stm_read`/`y_stm_write`. All memory operations
 * (allocate, release, read, write) require an open transaction; there is
 * no non-transactional fast path.
 *
 * Conflict handling is "first committer wins": a transaction's
 * `y_stm_commit_transaction` fails (returning false, and rolling the
 * transaction back for you) if any slot it read or wrote has a newer
 * committed version than the snapshot it started from. Since this project
 * runs on a single OS thread (see CLAUDE.md), this validation is plain
 * sequential bookkeeping — no locking is needed even though multiple
 * transactions can be open at once.
 *
 * A transaction handle is consumed by whichever of
 * `y_stm_commit_transaction`/`y_stm_rollback_transaction` is called on it;
 * don't reuse it afterwards.
 */
typedef struct y_stm y_stm_t;
typedef struct y_stm_transaction y_stm_transaction_t;

/* Opaque reference to a value in the STM heap. Never dereference it — it
 * is not a pointer to the value's bytes, only an identity that
 * y_stm_read/y_stm_write/y_stm_release_memory look up. Using a distinct
 * type instead of `void *` keeps handles from being silently mixed up
 * with unrelated pointers. */
typedef struct y_stm_slot *y_stm_handle_t;

y_stm_t *y_stm_constructor(void);
void y_stm_destructor(y_stm_t *stm);

y_stm_transaction_t *y_stm_begin_transaction(y_stm_t *stm);
bool y_stm_commit_transaction(y_stm_t *stm, y_stm_transaction_t *transaction);
void y_stm_rollback_transaction(y_stm_t *stm, y_stm_transaction_t *transaction);

y_stm_handle_t y_stm_allocate_memory(y_stm_t *stm, y_stm_transaction_t *transaction,
                                     size_t length_in_bytes);
bool y_stm_release_memory(y_stm_t *stm, y_stm_transaction_t *transaction, y_stm_handle_t handle);

bool y_stm_read(y_stm_t *stm, y_stm_transaction_t *transaction, y_stm_handle_t handle, void *out,
                size_t length_in_bytes);
bool y_stm_write(y_stm_t *stm, y_stm_transaction_t *transaction, y_stm_handle_t handle,
                 const void *data, size_t length_in_bytes);

#endif
