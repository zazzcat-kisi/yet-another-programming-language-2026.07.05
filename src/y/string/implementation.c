#include "y/string/include.h"

#include <stdbool.h>
#include <stddef.h>

#include "y/stm/alloc/include.h"
#include "y/stm/include.h"
#include "y/stm/mem/include.h"

/* Fixed-length record pointing at a string's current content slot and
 * recording its length. Stored as the payload of a fixed-length "descriptor"
 * slot (y_string_t::descriptor) so that which content slot is current, and
 * how long it is, is itself versioned STM data rather than a plain field on
 * the (possibly shared, possibly concurrently-transacted) host struct. */
typedef struct y_string_descriptor {
    y_stm_handle_t content;
    size_t length_in_bytes;
} y_string_descriptor_t;

struct y_string {
    y_stm_t *stm;
    y_stm_handle_t descriptor;
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

    y_stm_handle_t content = y_stm_allocate_memory(stm, transaction, length_in_bytes);
    y_stm_write(stm, transaction, content, characters, length_in_bytes);

    y_stm_handle_t descriptor =
        y_stm_allocate_memory(stm, transaction, sizeof(y_string_descriptor_t));
    y_string_descriptor_t value;
    value.content = content;
    value.length_in_bytes = length_in_bytes;
    y_stm_write(stm, transaction, descriptor, &value, sizeof(value));

    if (y_stm_is_rolled_back(stm, transaction)) {
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

    y_string_descriptor_t value = {0};
    y_stm_read(self->stm, transaction, self->descriptor, &value, sizeof(value));
    y_stm_release_memory(self->stm, transaction, value.content);
    y_stm_release_memory(self->stm, transaction, self->descriptor);

    y_stm_alloc_free(self);
}

void y_string_get_length_in_bytes(const y_string_t *self, y_stm_transaction_t *transaction,
                                   size_t *out_length_in_bytes) {
    if (self == NULL || transaction == NULL || out_length_in_bytes == NULL) {
        return;
    }
    y_string_descriptor_t value = {0};
    y_stm_read(self->stm, transaction, self->descriptor, &value, sizeof(value));
    if (y_stm_is_rolled_back(self->stm, transaction)) {
        return;
    }
    *out_length_in_bytes = value.length_in_bytes;
}

void y_string_read(const y_string_t *self, y_stm_transaction_t *transaction, char *destination,
                    size_t destination_length_in_bytes) {
    if (self == NULL || transaction == NULL) {
        return;
    }
    y_string_descriptor_t value = {0};
    y_stm_read(self->stm, transaction, self->descriptor, &value, sizeof(value));
    if (y_stm_is_rolled_back(self->stm, transaction)) {
        return;
    }
    if (destination_length_in_bytes != value.length_in_bytes) {
        y_stm_fail_transaction(self->stm, transaction, Y_STRING_ERROR_LENGTH_MISMATCH,
                                "y_string_read: destination_length_in_bytes doesn't match the string's length");
        return;
    }
    y_stm_read(self->stm, transaction, value.content, destination, destination_length_in_bytes);
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

    y_string_descriptor_t old_value = {0};
    y_stm_read(self->stm, transaction, self->descriptor, &old_value, sizeof(old_value));
    if (y_stm_is_rolled_back(self->stm, transaction)) {
        return;
    }

    if (length_in_bytes == old_value.length_in_bytes) {
        y_stm_write(self->stm, transaction, old_value.content, characters, length_in_bytes);
        return;
    }

    y_stm_handle_t new_content = y_stm_allocate_memory(self->stm, transaction, length_in_bytes);
    y_stm_write(self->stm, transaction, new_content, characters, length_in_bytes);
    y_stm_release_memory(self->stm, transaction, old_value.content);

    y_string_descriptor_t new_value;
    new_value.content = new_content;
    new_value.length_in_bytes = length_in_bytes;
    y_stm_write(self->stm, transaction, self->descriptor, &new_value, sizeof(new_value));
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

    y_string_descriptor_t old_value = {0};
    y_stm_read(self->stm, transaction, self->descriptor, &old_value, sizeof(old_value));
    if (y_stm_is_rolled_back(self->stm, transaction)) {
        return;
    }

    size_t combined_length_in_bytes = old_value.length_in_bytes + length_in_bytes;
    unsigned char *combined = y_stm_alloc_malloc(combined_length_in_bytes);

    if (old_value.length_in_bytes > 0) {
        y_stm_read(self->stm, transaction, old_value.content, combined, old_value.length_in_bytes);
    }
    y_stm_mem_memcpy(combined + old_value.length_in_bytes, characters, length_in_bytes);

    y_string_write(self, transaction, (const char *)combined, combined_length_in_bytes);
    y_stm_alloc_free(combined);
}

void y_string_compare(const y_string_t *self, const y_string_t *other,
                      y_stm_transaction_t *transaction, int *out_comparison) {
    if (self == NULL || other == NULL || transaction == NULL || out_comparison == NULL) {
        return;
    }

    y_string_descriptor_t self_value = {0};
    y_stm_read(self->stm, transaction, self->descriptor, &self_value, sizeof(self_value));
    y_string_descriptor_t other_value = {0};
    y_stm_read(other->stm, transaction, other->descriptor, &other_value, sizeof(other_value));

    unsigned char *self_bytes = NULL;
    if (self_value.length_in_bytes > 0) {
        self_bytes = y_stm_alloc_malloc(self_value.length_in_bytes);
        y_stm_read(self->stm, transaction, self_value.content, self_bytes,
                   self_value.length_in_bytes);
    }

    unsigned char *other_bytes = NULL;
    if (other_value.length_in_bytes > 0) {
        other_bytes = y_stm_alloc_malloc(other_value.length_in_bytes);
        y_stm_read(other->stm, transaction, other_value.content, other_bytes,
                   other_value.length_in_bytes);
    }

    if (!y_stm_is_rolled_back(self->stm, transaction)) {
        size_t shared_length_in_bytes = self_value.length_in_bytes < other_value.length_in_bytes
                                             ? self_value.length_in_bytes
                                             : other_value.length_in_bytes;
        int result = 0;
        if (shared_length_in_bytes > 0) {
            result = y_stm_mem_memcmp(self_bytes, other_bytes, shared_length_in_bytes);
        }
        if (result == 0 && self_value.length_in_bytes != other_value.length_in_bytes) {
            result = self_value.length_in_bytes < other_value.length_in_bytes ? -1 : 1;
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

    y_string_descriptor_t value = {0};
    y_stm_read(self->stm, transaction, self->descriptor, &value, sizeof(value));
    if (y_stm_is_rolled_back(self->stm, transaction)) {
        return NULL;
    }

    char *result = y_stm_alloc_malloc(value.length_in_bytes + 1);

    if (value.length_in_bytes > 0) {
        y_stm_read(self->stm, transaction, value.content, result, value.length_in_bytes);
        if (y_stm_is_rolled_back(self->stm, transaction)) {
            y_stm_alloc_free(result);
            return NULL;
        }
    }
    result[value.length_in_bytes] = '\0';
    return result;
}
