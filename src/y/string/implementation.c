#include "y/string/include.h"

#include <stdbool.h>
#include <stddef.h>

#include "y/blob/include.h"
#include "y/stm/alloc/include.h"
#include "y/stm/include.h"
#include "y/stm/mem/include.h"

/* A y_string stores its bytes in a resizable y_blob (see y/blob/include.h)
 * and adds string semantics on top. The blob owns the storage -- including
 * the versioned descriptor that keeps its length isolated across overlapping
 * transactions -- so y_string grows/shrinks simply by commanding
 * y_blob_resize, never by having the blob auto-grow. y_string caches its
 * y_stm so it can report its own boundary-validation failures as
 * Y_STRING_ERROR_* codes, keeping y_blob an implementation detail its callers
 * never see. */
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

    size_t old_length_in_bytes = 0;
    y_blob_get_length_in_bytes(self->blob, transaction, &old_length_in_bytes);
    if (y_stm_is_rolled_back(self->stm, transaction)) {
        return;
    }

    /* Command the resize ourselves when the length changes; the blob never
     * grows on its own. */
    if (length_in_bytes != old_length_in_bytes) {
        y_blob_resize(self->blob, transaction, length_in_bytes);
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
    if (length_in_bytes == 0) {
        return;
    }

    size_t old_length_in_bytes = 0;
    y_blob_get_length_in_bytes(self->blob, transaction, &old_length_in_bytes);
    if (y_stm_is_rolled_back(self->stm, transaction)) {
        return;
    }

    size_t combined_length_in_bytes = old_length_in_bytes + length_in_bytes;
    unsigned char *combined = y_stm_alloc_malloc(combined_length_in_bytes);

    if (old_length_in_bytes > 0) {
        y_blob_read(self->blob, transaction, combined, old_length_in_bytes);
    }
    y_stm_mem_memcpy(combined + old_length_in_bytes, characters, length_in_bytes);

    y_string_write(self, transaction, (const char *)combined, combined_length_in_bytes);
    y_stm_alloc_free(combined);
}

void y_string_compare(const y_string_t *self, const y_string_t *other,
                      y_stm_transaction_t *transaction, int *out_comparison) {
    if (self == NULL || other == NULL || transaction == NULL || out_comparison == NULL) {
        return;
    }

    size_t self_length_in_bytes = 0;
    y_blob_get_length_in_bytes(self->blob, transaction, &self_length_in_bytes);
    size_t other_length_in_bytes = 0;
    y_blob_get_length_in_bytes(other->blob, transaction, &other_length_in_bytes);

    unsigned char *self_bytes = NULL;
    if (self_length_in_bytes > 0) {
        self_bytes = y_stm_alloc_malloc(self_length_in_bytes);
        y_blob_read(self->blob, transaction, self_bytes, self_length_in_bytes);
    }

    unsigned char *other_bytes = NULL;
    if (other_length_in_bytes > 0) {
        other_bytes = y_stm_alloc_malloc(other_length_in_bytes);
        y_blob_read(other->blob, transaction, other_bytes, other_length_in_bytes);
    }

    if (!y_stm_is_rolled_back(self->stm, transaction)) {
        size_t shared_length_in_bytes = self_length_in_bytes < other_length_in_bytes
                                             ? self_length_in_bytes
                                             : other_length_in_bytes;
        int result = 0;
        if (shared_length_in_bytes > 0) {
            result = y_stm_mem_memcmp(self_bytes, other_bytes, shared_length_in_bytes);
        }
        if (result == 0 && self_length_in_bytes != other_length_in_bytes) {
            result = self_length_in_bytes < other_length_in_bytes ? -1 : 1;
        }
        *out_comparison = result;
    }

    y_stm_alloc_free(self_bytes);
    y_stm_alloc_free(other_bytes);
}

void y_string_is_equal(const y_string_t *self, const y_string_t *other,
                       y_stm_transaction_t *transaction, bool *out_is_equal) {
    if (out_is_equal == NULL) {
        return;
    }
    int comparison = 0;
    y_string_compare(self, other, transaction, &comparison);
    if (self != NULL && other != NULL && transaction != NULL &&
        !y_stm_is_rolled_back(self->stm, transaction)) {
        *out_is_equal = (comparison == 0);
    }
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
