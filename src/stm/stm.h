#ifndef STM_H
#define STM_H

#include <stdbool.h>
#include <stddef.h>

/*
 * Software Transactional Memory over a versioned heap, using MVCC
 * (multi-version concurrency control) rather than locks.
 *
 * `stm_t` is the state object ("class"); `stm_transaction_t` is an
 * independent, explicitly-handled unit of work against it — multiple
 * transactions may be open at the same time (e.g. representing separate
 * coroutines), each seeing a private snapshot of the world as of its
 * `stm_begin_transaction` call.
 *
 * Every value lives at a `stm_allocate_memory` handle, which is opaque:
 * do not dereference it directly, go through `stm_read`/`stm_write`. All
 * memory operations (allocate, release, read, write) require an open
 * transaction; there is no non-transactional fast path.
 *
 * Conflict handling is "first committer wins": a transaction's
 * `stm_commit_transaction` fails (returning false, and rolling the
 * transaction back for you) if any slot it read or wrote has a newer
 * committed version than the snapshot it started from. Since this project
 * runs on a single OS thread (see CLAUDE.md), this validation is plain
 * sequential bookkeeping — no locking is needed even though multiple
 * transactions can be open at once.
 *
 * A transaction handle is consumed by whichever of
 * `stm_commit_transaction`/`stm_rollback_transaction` is called on it;
 * don't reuse it afterwards.
 */
typedef struct stm stm_t;
typedef struct stm_transaction stm_transaction_t;

stm_t *stm_constructor(void);
void stm_destructor(stm_t *stm);

stm_transaction_t *stm_begin_transaction(stm_t *stm);
bool stm_commit_transaction(stm_t *stm, stm_transaction_t *transaction);
void stm_rollback_transaction(stm_t *stm, stm_transaction_t *transaction);

void *stm_allocate_memory(stm_t *stm, stm_transaction_t *transaction, size_t length_in_bytes);
bool stm_release_memory(stm_t *stm, stm_transaction_t *transaction, void *handle);

bool stm_read(stm_t *stm, stm_transaction_t *transaction, void *handle, void *out,
              size_t length_in_bytes);
bool stm_write(stm_t *stm, stm_transaction_t *transaction, void *handle, const void *data,
               size_t length_in_bytes);

#endif
