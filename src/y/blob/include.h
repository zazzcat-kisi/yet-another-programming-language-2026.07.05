#ifndef Y_BLOB_INCLUDE_H
#define Y_BLOB_INCLUDE_H

#include <stddef.h>

#include "y/stm/include.h"

/*
 * A resizable, transactional buffer of raw bytes built on top of `y_stm` --
 * the storage primitive that byte-container classes (e.g. `y_string`,
 * `y_packed_array`) are built on. `y_blob_t` is the state object ("class").
 *
 * A blob resizes only when its owner explicitly commands it to, via
 * `y_blob_resize`. There is deliberately *no automatic growth*: no append,
 * and no write that quietly resizes to fit differently-sized data
 * (`y_blob_write` overwrites in place and rejects a differing length).
 * Growing is therefore always a deliberate act by the owner, which is what
 * lets higher-level containers stay in control of when and how they grow.
 *
 * Because the length is mutable (a `y_blob_resize` changes it), which `y_stm`
 * slot currently holds the bytes, and how long they are, is versioned STM
 * data (kept behind a small fixed-length "descriptor" slot) rather than a
 * plain field on the host struct. This matters because a `y_blob_t` can be
 * shared by overlapping transactions: if the current-content handle were
 * cached on the host struct instead, one transaction resizing the blob would
 * clobber what every other open transaction sees, breaking isolation.
 * Routing it through the STM means `y_blob_resize` participates in the same
 * first-committer-wins conflict detection as any other write, and even
 * `y_blob_get_length_in_bytes` needs a transaction.
 *
 * The bytes live in the `y_stm_t` the blob was constructed with, so every
 * operation takes a `y_stm_transaction_t *` and follows y_stm's failure
 * model (see y_stm/include.h and CLAUDE.md's "Rollback on failure"): on
 * failure it reports the error via y_stm_fail_transaction instead of
 * returning one, which rolls `transaction` back. A `y_blob_t` caches which
 * `y_stm_t` it belongs to, so callers only pass the transaction.
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

/* The blob's current length. */
void y_blob_get_length_in_bytes(const y_blob_t *self, y_stm_transaction_t *transaction,
                                size_t *out_length_in_bytes);

/* Copies the blob's bytes into `destination`, which must be exactly
 * `destination_length_in_bytes` long (== the blob's current length, per
 * y_blob_get_length_in_bytes). */
void y_blob_read(const y_blob_t *self, y_stm_transaction_t *transaction, void *destination,
                 size_t destination_length_in_bytes);

/* Overwrites the blob's bytes in place. `length_in_bytes` must equal the
 * blob's current length -- writing never resizes, so a differing length is a
 * Y_BLOB_ERROR_LENGTH_MISMATCH failure. Use y_blob_resize first to change
 * the size. */
void y_blob_write(y_blob_t *self, y_stm_transaction_t *transaction, const void *data,
                  size_t length_in_bytes);

/* Explicitly resizes the blob to `new_length_in_bytes`, preserving the first
 * min(old, new) bytes and zero-filling any growth. This is the only way a
 * blob changes size; it is a deliberate command by the owner, never an
 * automatic side effect of another operation. */
void y_blob_resize(y_blob_t *self, y_stm_transaction_t *transaction, size_t new_length_in_bytes);

#endif
