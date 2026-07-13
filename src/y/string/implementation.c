#include "y/string/include.h"

#include <stdbool.h>
#include <stddef.h>

#include "y/blob/include.h"
#include "y/stm/alloc/include.h"
#include "y/stm/include.h"
#include "y/stm/mem/include.h"

/* Fixed-length record naming a string's current content slot and its
 * length. It is stored in a `y_blob` (y_string_t::descriptor) so that which
 * content slot is current, and how long it is, is versioned STM data:
 * because a `y_string_t` can be shared by overlapping transactions, one
 * transaction resizing the string must not clobber what another open
 * transaction sees. The record itself is fixed-length, which is exactly
 * what a `y_blob` stores -- so the versioned descriptor is a blob, while the
 * variable-length *content* it points at is a raw slot that y_string grows
 * and shrinks by hand (allocating a new slot and releasing the old one). A
 * blob never resizes; the growing lives here. */
typedef struct y_string_content {
    y_stm_handle_t handle;
    size_t length_in_bytes;
} y_string_content_t;

struct y_string {
    y_stm_t *stm;
    y_blob_t *descriptor;
};

static y_string_content_t y_string_content_get(const y_string_t *self,
                                                y_stm_transaction_t *transaction) {
    y_string_content_t content = {0};
    y_blob_read(self->descriptor, transaction, &content, sizeof(content));
    return content;
}

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

    y_stm_handle_t handle = y_stm_allocate_memory(stm, transaction, length_in_bytes);
    y_stm_write(stm, transaction, handle, characters, length_in_bytes);

    y_string_content_t content;
    content.handle = handle;
    content.length_in_bytes = length_in_bytes;
    y_blob_t *descriptor = y_blob_constructor(stm, transaction, &content, sizeof(content));
    if (descriptor == NULL) {
        return NULL;
    }

    y_string_t *self = y_stm_alloc_malloc(sizeof(y_string_t));
    self->stm = stm;
    self->descriptor = descriptor;
    return self;
}

void y_string_destructor(y_string_t *self, y_stm_transaction_t *transaction) {
    if (self == NULL) {
        return;
    }
    y_string_content_t content = y_string_content_get(self, transaction);
    y_stm_release_memory(self->stm, transaction, content.handle);
    y_blob_destructor(self->descriptor, transaction);
    y_stm_alloc_free(self);
}

void y_string_get_length_in_bytes(const y_string_t *self, y_stm_transaction_t *transaction,
                                   size_t *out_length_in_bytes) {
    if (self == NULL || transaction == NULL || out_length_in_bytes == NULL) {
        return;
    }
    y_string_content_t content = y_string_content_get(self, transaction);
    if (y_stm_is_rolled_back(self->stm, transaction)) {
        return;
    }
    *out_length_in_bytes = content.length_in_bytes;
}

void y_string_read(const y_string_t *self, y_stm_transaction_t *transaction, char *destination,
                    size_t destination_length_in_bytes) {
    if (self == NULL || transaction == NULL) {
        return;
    }
    y_string_content_t content = y_string_content_get(self, transaction);
    if (y_stm_is_rolled_back(self->stm, transaction)) {
        return;
    }
    if (destination_length_in_bytes != content.length_in_bytes) {
        y_stm_fail_transaction(self->stm, transaction, Y_STRING_ERROR_LENGTH_MISMATCH,
                                "y_string_read: destination_length_in_bytes doesn't match the string's length");
        return;
    }
    y_stm_read(self->stm, transaction, content.handle, destination, destination_length_in_bytes);
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

    y_string_content_t old_content = y_string_content_get(self, transaction);
    if (y_stm_is_rolled_back(self->stm, transaction)) {
        return;
    }

    if (length_in_bytes == old_content.length_in_bytes) {
        y_stm_write(self->stm, transaction, old_content.handle, characters, length_in_bytes);
        return;
    }

    /* The length changed: grow/shrink by allocating a fresh content slot,
     * releasing the old one, and versioning the swap through the descriptor
     * blob (a same-length, in-place write, since the record is fixed-size). */
    y_stm_handle_t new_handle = y_stm_allocate_memory(self->stm, transaction, length_in_bytes);
    y_stm_write(self->stm, transaction, new_handle, characters, length_in_bytes);
    y_stm_release_memory(self->stm, transaction, old_content.handle);

    y_string_content_t new_content;
    new_content.handle = new_handle;
    new_content.length_in_bytes = length_in_bytes;
    y_blob_write(self->descriptor, transaction, &new_content, sizeof(new_content));
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

    y_string_content_t old_content = y_string_content_get(self, transaction);
    if (y_stm_is_rolled_back(self->stm, transaction)) {
        return;
    }

    size_t combined_length_in_bytes = old_content.length_in_bytes + length_in_bytes;
    unsigned char *combined = y_stm_alloc_malloc(combined_length_in_bytes);

    if (old_content.length_in_bytes > 0) {
        y_stm_read(self->stm, transaction, old_content.handle, combined,
                   old_content.length_in_bytes);
    }
    y_stm_mem_memcpy(combined + old_content.length_in_bytes, characters, length_in_bytes);

    y_string_write(self, transaction, (const char *)combined, combined_length_in_bytes);
    y_stm_alloc_free(combined);
}

void y_string_compare(const y_string_t *self, const y_string_t *other,
                      y_stm_transaction_t *transaction, int *out_comparison) {
    if (self == NULL || other == NULL || transaction == NULL || out_comparison == NULL) {
        return;
    }

    y_string_content_t self_content = y_string_content_get(self, transaction);
    y_string_content_t other_content = y_string_content_get(other, transaction);

    unsigned char *self_bytes = NULL;
    if (self_content.length_in_bytes > 0) {
        self_bytes = y_stm_alloc_malloc(self_content.length_in_bytes);
        y_stm_read(self->stm, transaction, self_content.handle, self_bytes,
                   self_content.length_in_bytes);
    }

    unsigned char *other_bytes = NULL;
    if (other_content.length_in_bytes > 0) {
        other_bytes = y_stm_alloc_malloc(other_content.length_in_bytes);
        y_stm_read(other->stm, transaction, other_content.handle, other_bytes,
                   other_content.length_in_bytes);
    }

    if (!y_stm_is_rolled_back(self->stm, transaction)) {
        size_t shared_length_in_bytes =
            self_content.length_in_bytes < other_content.length_in_bytes
                ? self_content.length_in_bytes
                : other_content.length_in_bytes;
        int result = 0;
        if (shared_length_in_bytes > 0) {
            result = y_stm_mem_memcmp(self_bytes, other_bytes, shared_length_in_bytes);
        }
        if (result == 0 && self_content.length_in_bytes != other_content.length_in_bytes) {
            result = self_content.length_in_bytes < other_content.length_in_bytes ? -1 : 1;
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

    y_string_content_t content = y_string_content_get(self, transaction);
    if (y_stm_is_rolled_back(self->stm, transaction)) {
        return NULL;
    }

    char *result = y_stm_alloc_malloc(content.length_in_bytes + 1);

    if (content.length_in_bytes > 0) {
        y_stm_read(self->stm, transaction, content.handle, result, content.length_in_bytes);
        if (y_stm_is_rolled_back(self->stm, transaction)) {
            y_stm_alloc_free(result);
            return NULL;
        }
    }
    result[content.length_in_bytes] = '\0';
    return result;
}
