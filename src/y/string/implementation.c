#include "y/string/include.h"

#include <stdbool.h>
#include <stddef.h>

#include "y/blob/include.h"
#include "y/stm/alloc/include.h"
#include "y/stm/include.h"

/* A y_string is a y_blob (its resizable, versioned byte storage; see
 * y/blob/include.h) presented with string semantics. The blob owns all the
 * storage mechanics -- the descriptor slot, resizing on write/append, and
 * the isolation across overlapping transactions. y_string caches its y_stm
 * only so it can report its own boundary-validation failures as
 * Y_STRING_ERROR_* codes (keeping y_blob an implementation detail its
 * callers never see); everything else forwards straight through to the
 * underlying blob. */
struct y_string {
    y_stm_t *stm;
    y_blob_t *blob;
};

y_string_t *y_string_constructor(y_stm_t *stm, y_stm_transaction_t *transaction,
                                  const char *characters, size_t length_in_bytes) {
    if (transaction == NULL) {
        return NULL;
    }
    if (characters == NULL && length_in_bytes > 0) {
        y_stm_fail_transaction(stm, transaction, Y_STRING_ERROR_INVALID_ARGUMENT,
                                "y_string_constructor: characters is NULL for a nonzero length");
        return NULL;
    }

    y_blob_t *blob = y_blob_constructor(stm, transaction, characters, length_in_bytes);
    if (blob == NULL) {
        return NULL;
    }

    y_string_t *self = y_stm_alloc_malloc(sizeof(y_string_t));
    self->stm = stm;
    self->blob = blob;
    return self;
}

void y_string_destructor(y_string_t *self, y_stm_transaction_t *transaction) {
    if (self == NULL) {
        return;
    }
    y_blob_destructor(self->blob, transaction);
    y_stm_alloc_free(self);
}

void y_string_get_length_in_bytes(const y_string_t *self, y_stm_transaction_t *transaction,
                                   size_t *out_length_in_bytes) {
    if (self == NULL) {
        return;
    }
    y_blob_get_length_in_bytes(self->blob, transaction, out_length_in_bytes);
}

void y_string_read(const y_string_t *self, y_stm_transaction_t *transaction, char *destination,
                    size_t destination_length_in_bytes) {
    if (self == NULL || transaction == NULL) {
        return;
    }
    /* Validate the destination length here so a mismatch surfaces as a
     * y_string error rather than the equivalent y_blob one. */
    size_t length_in_bytes = 0;
    y_blob_get_length_in_bytes(self->blob, transaction, &length_in_bytes);
    if (y_stm_is_rolled_back(self->stm, transaction)) {
        return;
    }
    if (destination_length_in_bytes != length_in_bytes) {
        y_stm_fail_transaction(self->stm, transaction, Y_STRING_ERROR_LENGTH_MISMATCH,
                                "y_string_read: destination_length_in_bytes doesn't match the string's length");
        return;
    }
    y_blob_read(self->blob, transaction, destination, destination_length_in_bytes);
}

void y_string_write(y_string_t *self, y_stm_transaction_t *transaction, const char *characters,
                     size_t length_in_bytes) {
    if (self == NULL || transaction == NULL) {
        return;
    }
    if (characters == NULL && length_in_bytes > 0) {
        y_stm_fail_transaction(self->stm, transaction, Y_STRING_ERROR_INVALID_ARGUMENT,
                                "y_string_write: characters is NULL for a nonzero length");
        return;
    }
    y_blob_write(self->blob, transaction, characters, length_in_bytes);
}

void y_string_append(y_string_t *self, y_stm_transaction_t *transaction, const char *characters,
                      size_t length_in_bytes) {
    if (self == NULL || transaction == NULL) {
        return;
    }
    if (characters == NULL && length_in_bytes > 0) {
        y_stm_fail_transaction(self->stm, transaction, Y_STRING_ERROR_INVALID_ARGUMENT,
                                "y_string_append: characters is NULL for a nonzero length");
        return;
    }
    y_blob_append(self->blob, transaction, characters, length_in_bytes);
}

void y_string_compare(const y_string_t *self, const y_string_t *other,
                      y_stm_transaction_t *transaction, int *out_comparison) {
    y_blob_compare(self != NULL ? self->blob : NULL, other != NULL ? other->blob : NULL,
                   transaction, out_comparison);
}

void y_string_is_equal(const y_string_t *self, const y_string_t *other,
                       y_stm_transaction_t *transaction, bool *out_is_equal) {
    y_blob_is_equal(self != NULL ? self->blob : NULL, other != NULL ? other->blob : NULL,
                    transaction, out_is_equal);
}

char *y_string_as_c_string(const y_string_t *self, y_stm_transaction_t *transaction) {
    if (self == NULL || transaction == NULL) {
        return NULL;
    }

    size_t length_in_bytes = 0;
    y_blob_get_length_in_bytes(self->blob, transaction, &length_in_bytes);
    if (y_stm_is_rolled_back(self->stm, transaction)) {
        return NULL;
    }

    char *result = y_stm_alloc_malloc(length_in_bytes + 1);

    if (length_in_bytes > 0) {
        y_blob_read(self->blob, transaction, result, length_in_bytes);
        if (y_stm_is_rolled_back(self->stm, transaction)) {
            y_stm_alloc_free(result);
            return NULL;
        }
    }
    result[length_in_bytes] = '\0';
    return result;
}
