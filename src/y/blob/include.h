#ifndef Y_BLOB_INCLUDE_H
#define Y_BLOB_INCLUDE_H

#include <stdbool.h>
#include <stddef.h>

#include "y/stm/include.h"

/*
 * A dynamically-sized, transactional buffer of raw bytes built on top of
 * `y_stm` -- the resizable-storage primitive that string-like and other
 * byte-container classes (e.g. `y_string`) are built on. `y_blob_t` is the
 * state object ("class"); its bytes live in the `y_stm_t` it was
 * constructed with, so every operation that touches them takes a
 * `y_stm_transaction_t *` and follows y_stm's failure model (see
 * y_stm/include.h and CLAUDE.md's "Rollback on failure" section): on
 * failure (a stale snapshot, a conflicting committer, a bad argument, ...)
 * it reports the error via y_stm_fail_transaction instead of returning one,
 * which rolls `transaction` back. From then on every further y_blob/y_stm
 * operation against that transaction is a safe no-op; check
 * y_stm_is_rolled_back/y_stm_get_error if and when you actually need to
 * know.
 *
 * A `y_blob_t` caches which `y_stm_t` it belongs to, so callers only need
 * to pass the transaction, not the STM instance, to its operations. An
 * individual `y_stm` slot's length is fixed at allocation, but a blob's
 * content is not -- so which slot currently holds a blob's bytes, and how
 * long they are, is itself versioned data living in the STM (behind a small
 * fixed-length "descriptor" slot), not a plain field cached on the
 * `y_blob_t` host struct. This matters because a `y_blob_t` can be shared
 * by multiple overlapping transactions: if the current-content handle were
 * cached on the host struct instead, one transaction resizing the blob
 * would clobber what every other open transaction sees, breaking isolation.
 * Routing it through the STM means `y_blob_write`/`y_blob_append` (which
 * allocate a new slot and release the old one whenever the length changes)
 * participate in the same first-committer-wins conflict detection as any
 * other write, and even read-only operations like
 * `y_blob_get_length_in_bytes` need a transaction.
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

void y_blob_get_length_in_bytes(const y_blob_t *self, y_stm_transaction_t *transaction,
                                size_t *out_length_in_bytes);

/* Copies the blob's current bytes into `destination`, which must be exactly
 * `destination_length_in_bytes` long (== the blob's current length, per
 * y_blob_get_length_in_bytes). */
void y_blob_read(const y_blob_t *self, y_stm_transaction_t *transaction, void *destination,
                 size_t destination_length_in_bytes);

/* Replaces the blob's content, resizing its backing storage as needed. */
void y_blob_write(y_blob_t *self, y_stm_transaction_t *transaction, const void *data,
                  size_t length_in_bytes);

/* Appends to the blob's content, resizing its backing storage as needed. */
void y_blob_append(y_blob_t *self, y_stm_transaction_t *transaction, const void *data,
                   size_t length_in_bytes);

/* Lexicographic byte comparison (shorter length wins ties), written to
 * `*out_comparison` as <0, 0, or >0. Leaves `*out_comparison` untouched if
 * either blob's content could not be read. */
void y_blob_compare(const y_blob_t *self, const y_blob_t *other, y_stm_transaction_t *transaction,
                    int *out_comparison);
void y_blob_is_equal(const y_blob_t *self, const y_blob_t *other, y_stm_transaction_t *transaction,
                     bool *out_is_equal);

#endif
