#ifndef Y_STM_INCLUDE_H
#define Y_STM_INCLUDE_H

#include <stdbool.h>
#include <stddef.h>

#include "y/error/include.h"

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
 * Failure model (see CLAUDE.md's "Rollback on failure" section): a failure
 * in `y_stm_allocate_memory`/`y_stm_release_memory`/`y_stm_read`/
 * `y_stm_write` (wrong transaction, stale/missing handle, a conflicting
 * committer, ...) does not return an error code. It records the failure
 * via `y_error_set` and rolls the transaction back, instead of returning
 * one. From that point on, `y_stm_is_rolled_back` is true and every
 * further operation against that transaction is a safe no-op — no more
 * changes are ever applied to the STM by it, so it's safe to keep calling
 * things without checking each one, and check `y_stm_is_rolled_back` only
 * where it's actually useful (e.g. to skip expensive work). The
 * transaction handle itself stays valid (not freed) after such a rollback,
 * unlike the explicit `y_stm_rollback_transaction`/`y_stm_commit_transaction`
 * calls below, which are what actually free it — call one of those when
 * you're done with a transaction either way. `y_stm_get_error` reports the
 * most recent failure; that record is not itself STM-managed data, so it
 * survives the rollback that a failure triggers.
 *
 * Conflict handling is "first committer wins": a transaction's
 * `y_stm_commit_transaction` fails (returning false, and rolling the
 * transaction back for you) if any slot it read or wrote has a newer
 * committed version than the snapshot it started from. Since this project
 * runs on a single OS thread (see CLAUDE.md), this validation is plain
 * sequential bookkeeping — no locking is needed even though multiple
 * transactions can be open at once. Unlike the per-operation functions
 * above, `y_stm_commit_transaction` keeps returning `bool`: it's the one
 * meaningful, terminal outcome of the whole transaction, not a per-call
 * error code, and (like `y_stm_rollback_transaction`) it always frees the
 * transaction handle.
 */
typedef struct y_stm y_stm_t;
typedef struct y_stm_transaction y_stm_transaction_t;

/* Opaque reference to a value in the STM heap. Never dereference it — it
 * is not a pointer to the value's bytes, only an identity that
 * y_stm_read/y_stm_write/y_stm_release_memory look up. Using a distinct
 * type instead of `void *` keeps handles from being silently mixed up
 * with unrelated pointers. */
typedef struct y_stm_slot *y_stm_handle_t;

/* Error codes y_stm attaches via y_error_set when it rolls a transaction
 * back. Downstream libraries built on y_stm (e.g. y_string) define their
 * own codes for their own validation failures rather than reusing these. */
enum {
    Y_STM_ERROR_INVALID_TRANSACTION = 1,
    Y_STM_ERROR_INVALID_HANDLE,
    Y_STM_ERROR_LENGTH_MISMATCH,
    Y_STM_ERROR_NOT_VISIBLE,
    Y_STM_ERROR_ALREADY_RELEASED,
    Y_STM_ERROR_CONFLICT,
};

y_stm_t *y_stm_constructor(void);
void y_stm_destructor(y_stm_t *stm);

y_stm_transaction_t *y_stm_begin_transaction(y_stm_t *stm);
bool y_stm_commit_transaction(y_stm_t *stm, y_stm_transaction_t *transaction);
void y_stm_rollback_transaction(y_stm_t *stm, y_stm_transaction_t *transaction);

/* True once `transaction` has been rolled back, whether by an explicit
 * y_stm_rollback_transaction call or automatically by a failed operation.
 * Interested code may check this to skip expensive work it knows would be
 * discarded, but doesn't need to check it (or any per-operation result)
 * for correctness -- see the failure model above. */
bool y_stm_is_rolled_back(const y_stm_t *stm, const y_stm_transaction_t *transaction);

/* The most recent error recorded against `transaction`, or NULL if
 * `transaction`/`stm` are invalid. {0, NULL} (via y_error_get) if nothing
 * has failed yet. Pass the result to y_error_get to read it. */
const y_error_t *y_stm_get_error(const y_stm_t *stm, const y_stm_transaction_t *transaction);

/* Records `code`/`message` against `transaction` (unless it's already
 * rolled back, so the original failure isn't overwritten) and rolls it
 * back, exactly like an internal y_stm failure would. Exposed so libraries
 * built on y_stm (e.g. y_string) can report their own validation failures
 * through the same mechanism instead of returning an error code. */
void y_stm_fail_transaction(y_stm_t *stm, y_stm_transaction_t *transaction, int code,
                             const char *message);

y_stm_handle_t y_stm_allocate_memory(y_stm_t *stm, y_stm_transaction_t *transaction,
                                     size_t length_in_bytes);
void y_stm_release_memory(y_stm_t *stm, y_stm_transaction_t *transaction, y_stm_handle_t handle);

void y_stm_read(y_stm_t *stm, y_stm_transaction_t *transaction, y_stm_handle_t handle, void *out,
                size_t length_in_bytes);
void y_stm_write(y_stm_t *stm, y_stm_transaction_t *transaction, y_stm_handle_t handle,
                 const void *data, size_t length_in_bytes);

#endif
