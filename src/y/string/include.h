#ifndef Y_STRING_INCLUDE_H
#define Y_STRING_INCLUDE_H

#include <stdbool.h>
#include <stddef.h>

#include "y/stm/include.h"

/*
 * A dynamically-sized, transactional byte string built on top of `y_stm`,
 * serving the role `<string.h>`'s `str*` functions play for plain C
 * strings. `y_string_t` is the state object ("class"); its bytes live in
 * the `y_stm_t` it was constructed with, so every operation that touches
 * them takes a `y_stm_transaction_t *` and can fail exactly like
 * `y_stm_read`/`y_stm_write` can (a stale snapshot, a conflicting
 * committer, ...).
 *
 * A `y_string_t` caches which `y_stm_t` it belongs to, so callers only
 * need to pass the transaction, not the STM instance, to its operations.
 * An individual `y_stm` slot's length is fixed at allocation, but a
 * string's content is not — so which slot currently holds a string's
 * bytes, and how long they are, is itself versioned data living in the
 * STM (behind a small fixed-length "descriptor" slot), not a plain field
 * cached on the `y_string_t` host struct. This matters because a
 * `y_string_t` can be shared by multiple overlapping transactions: if the
 * current-content handle were cached on the host struct instead, one
 * transaction resizing the string would clobber what every other open
 * transaction sees, breaking isolation. Routing it through the STM means
 * `y_string_write`/`y_string_append` (which allocate a new slot and
 * release the old one whenever the length changes) participate in the
 * same first-committer-wins conflict detection as any other write, and
 * even read-only operations like `y_string_get_length_in_bytes` need a
 * transaction.
 */
typedef struct y_string y_string_t;

y_string_t *y_string_constructor(y_stm_t *stm, y_stm_transaction_t *transaction,
                                  const char *characters, size_t length_in_bytes);

/* Releases the string's backing memory and frees `self`. On failure (e.g.
 * a conflicting committer released it first, or `transaction` is invalid),
 * `self` is left intact so the caller can retry with a valid transaction. */
bool y_string_destructor(y_string_t *self, y_stm_transaction_t *transaction);

bool y_string_get_length_in_bytes(const y_string_t *self, y_stm_transaction_t *transaction,
                                   size_t *out_length_in_bytes);

/* Copies the string's current bytes into `destination`, which must be
 * exactly `destination_length_in_bytes` long (== the string's current
 * length, per y_string_get_length_in_bytes). */
bool y_string_read(const y_string_t *self, y_stm_transaction_t *transaction, char *destination,
                    size_t destination_length_in_bytes);

/* Replaces the string's content, resizing its backing storage as needed. */
bool y_string_write(y_string_t *self, y_stm_transaction_t *transaction, const char *characters,
                     size_t length_in_bytes);

/* Appends to the string's content, resizing its backing storage as needed. */
bool y_string_append(y_string_t *self, y_stm_transaction_t *transaction, const char *characters,
                      size_t length_in_bytes);

/* Lexicographic byte comparison (shorter length wins ties), written to
 * `*out_comparison` as <0, 0, or >0. Returns false (leaving *out_comparison
 * untouched) if either string's content could not be read. */
bool y_string_compare(const y_string_t *self, const y_string_t *other,
                       y_stm_transaction_t *transaction, int *out_comparison);
bool y_string_is_equal(const y_string_t *self, const y_string_t *other,
                        y_stm_transaction_t *transaction, bool *out_is_equal);

/* Returns a newly allocated, NUL-terminated copy of the string's bytes,
 * owned by the caller and released with y_stm_alloc_free. Returns NULL on
 * failure. */
char *y_string_as_c_string(const y_string_t *self, y_stm_transaction_t *transaction);

#endif
