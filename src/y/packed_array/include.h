#ifndef Y_PACKED_ARRAY_INCLUDE_H
#define Y_PACKED_ARRAY_INCLUDE_H

#include <stddef.h>

#include "y/stm/include.h"

/*
 * A growable, transactional array of fixed-size elements, packed
 * contiguously into a single `y_blob` (see y/blob/include.h). `count`
 * elements each `element_length_in_bytes` long are stored back-to-back with
 * no padding, so the whole array is one `count * element_length_in_bytes`
 * blob. `y_packed_array_t` is the state object ("class").
 *
 * The element length is fixed at construction, but the count is not: an
 * owner grows or shrinks the array by explicitly commanding
 * `y_packed_array_resize`, which resizes the underlying blob (new elements
 * are zero-initialized). Like the blob, it never grows on its own. Because
 * the count is just the blob's current length divided by the fixed element
 * length, it is versioned STM data -- so `y_packed_array_get_count` takes a
 * transaction and a resize stays isolated across overlapping transactions.
 * (`element_length_in_bytes` must be nonzero, so the count is always
 * recoverable that way.) Elements are zero-initialized at construction.
 *
 * The bytes live in the `y_stm_t` the array was constructed with, so the
 * get/set/resize operations take a `y_stm_transaction_t *` and follow
 * y_stm's failure model (see CLAUDE.md's "Rollback on failure"): a failure
 * (out-of-range index, wrong element length, a stale snapshot, a conflicting
 * committer, ...) reports itself via y_stm_fail_transaction and rolls
 * `transaction` back rather than returning an error code. A
 * `y_packed_array_t` caches which `y_stm_t` it belongs to, so callers only
 * pass the transaction.
 *
 * Because the elements share one packed blob (one `y_stm` slot), an element
 * access reads or rewrites the whole backing buffer: a `y_packed_array_set`
 * is a read-modify-write of the entire array, and any two transactions that
 * each write *any* element (or resize) conflict at commit under
 * first-committer-wins. That is the trade-off of packing everything
 * contiguously into a single slot; callers wanting per-element concurrency
 * should use separate blobs.
 */
typedef struct y_packed_array y_packed_array_t;

/* Error codes y_packed_array attaches via y_stm_fail_transaction for its own
 * validation failures (as opposed to ones y_stm itself already reports
 * under Y_STM_ERROR_*). */
enum {
    Y_PACKED_ARRAY_ERROR_INVALID_ARGUMENT = 1,
    Y_PACKED_ARRAY_ERROR_OUT_OF_RANGE,
    Y_PACKED_ARRAY_ERROR_LENGTH_MISMATCH,
};

/* Creates a zero-initialized array of `count` elements, each
 * `element_length_in_bytes` long. Fails (returning NULL) if
 * `element_length_in_bytes` is zero, or if `element_length_in_bytes * count`
 * would overflow. */
y_packed_array_t *y_packed_array_constructor(y_stm_t *stm, y_stm_transaction_t *transaction,
                                             size_t element_length_in_bytes, size_t count);

/* Releases the array's backing memory (a no-op if `transaction` is already
 * rolled back) and unconditionally frees `self` either way. */
void y_packed_array_destructor(y_packed_array_t *self, y_stm_transaction_t *transaction);

/* The current element count. */
void y_packed_array_get_count(const y_packed_array_t *self, y_stm_transaction_t *transaction,
                              size_t *out_count);

/* The fixed element length. Immutable, so unlike the other operations it
 * needs no transaction. */
void y_packed_array_get_element_length_in_bytes(const y_packed_array_t *self,
                                                size_t *out_element_length_in_bytes);

/* Copies the element at `index` into `out_element`, which must be exactly
 * `out_element_length_in_bytes` long (== element_length_in_bytes). */
void y_packed_array_get(const y_packed_array_t *self, y_stm_transaction_t *transaction, size_t index,
                        void *out_element, size_t out_element_length_in_bytes);

/* Overwrites the element at `index`. `element_length_in_bytes` must equal
 * element_length_in_bytes. */
void y_packed_array_set(y_packed_array_t *self, y_stm_transaction_t *transaction, size_t index,
                        const void *element, size_t element_length_in_bytes);

/* Explicitly resizes the array to `new_count` elements. Growth appends
 * zero-initialized elements; shrinking drops the trailing ones. Fails if
 * `element_length_in_bytes * new_count` would overflow. */
void y_packed_array_resize(y_packed_array_t *self, y_stm_transaction_t *transaction,
                           size_t new_count);

#endif
