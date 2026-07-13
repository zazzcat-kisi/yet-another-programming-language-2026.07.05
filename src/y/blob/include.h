#ifndef Y_BLOB_INCLUDE_H
#define Y_BLOB_INCLUDE_H

#include <stddef.h>

#include "y/stm/include.h"

/*
 * A fixed-length, transactional buffer of raw bytes built on top of `y_stm`
 * -- the fixed-size storage primitive that byte-container classes (e.g.
 * `y_string`, `y_packed_array`) are built on. `y_blob_t` is the state
 * object ("class").
 *
 * A blob's length is fixed at construction and never changes: there is
 * deliberately no append or resizing write. Growing/shrinking is a
 * higher-level concern -- a class that needs a resizable buffer (like
 * `y_string`) layers that on top by allocating a new blob and copying,
 * rather than having the blob grow underneath it. Because the length is
 * immutable, it is a plain property of the host struct (so
 * `y_blob_get_length_in_bytes` needs no transaction) rather than versioned
 * STM data, and a single fixed `y_stm` slot holds the bytes directly with
 * no extra indirection.
 *
 * The bytes themselves still live in the `y_stm_t` the blob was constructed
 * with, so every operation that reads or writes them takes a
 * `y_stm_transaction_t *` and follows y_stm's failure model (see
 * y_stm/include.h and CLAUDE.md's "Rollback on failure" section): on
 * failure (a stale snapshot, a conflicting committer, a bad argument, ...)
 * it reports the error via y_stm_fail_transaction instead of returning one,
 * which rolls `transaction` back. From then on every further y_blob/y_stm
 * operation against that transaction is a safe no-op; check
 * y_stm_is_rolled_back/y_stm_get_error if and when you actually need to
 * know. A `y_blob_t` caches which `y_stm_t` it belongs to, so callers only
 * pass the transaction, not the STM instance, to its operations.
 */
typedef struct y_blob y_blob_t;

/* Error codes y_blob attaches via y_stm_fail_transaction for its own
 * validation failures (as opposed to ones y_stm itself already reports
 * under Y_STM_ERROR_*). */
enum {
    Y_BLOB_ERROR_INVALID_ARGUMENT = 1,
    Y_BLOB_ERROR_LENGTH_MISMATCH,
};

y_blob_t *y_blob_constructor(y_stm_t *stm, y_stm_transaction_t *transaction, const void *data,
                             size_t length_in_bytes);

/* Releases the blob's backing memory (a no-op if `transaction` is already
 * rolled back) and unconditionally frees `self` either way. */
void y_blob_destructor(y_blob_t *self, y_stm_transaction_t *transaction);

/* The blob's fixed length. Immutable, so unlike every other operation this
 * needs no transaction. */
void y_blob_get_length_in_bytes(const y_blob_t *self, size_t *out_length_in_bytes);

/* Copies the blob's bytes into `destination`, which must be exactly
 * `destination_length_in_bytes` long (== the blob's fixed length, per
 * y_blob_get_length_in_bytes). */
void y_blob_read(const y_blob_t *self, y_stm_transaction_t *transaction, void *destination,
                 size_t destination_length_in_bytes);

/* Overwrites the blob's bytes in place. `length_in_bytes` must equal the
 * blob's fixed length -- a blob never resizes, so a differing length is a
 * Y_BLOB_ERROR_LENGTH_MISMATCH failure rather than a grow/shrink. */
void y_blob_write(y_blob_t *self, y_stm_transaction_t *transaction, const void *data,
                  size_t length_in_bytes);

#endif
