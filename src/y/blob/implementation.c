#include "y/blob/include.h"

#include <stddef.h>

#include "y/stm/alloc/include.h"
#include "y/stm/include.h"
#include "y/stm/mem/include.h"

/* Fixed-length record pointing at a blob's current content slot and
 * recording its length. Stored as the payload of a fixed-length "descriptor"
 * slot (y_blob_t::descriptor) so that which content slot is current, and how
 * long it is, is versioned STM data rather than a plain field on the
 * (possibly shared, possibly concurrently-transacted) host struct -- so a
 * y_blob_resize stays isolated across overlapping transactions. */
typedef struct y_blob_descriptor {
    y_stm_handle_t content;
    size_t length_in_bytes;
} y_blob_descriptor_t;

struct y_blob {
    y_stm_t *stm;
    y_stm_handle_t descriptor;
};

static y_blob_descriptor_t y_blob_descriptor_get(const y_blob_t *self,
                                                 y_stm_transaction_t *transaction) {
    y_blob_descriptor_t descriptor = {0};
    y_stm_read(self->stm, transaction, self->descriptor, &descriptor, sizeof(descriptor));
    return descriptor;
}

y_blob_t *y_blob_constructor(y_stm_t *stm, y_stm_transaction_t *transaction, const void *data,
                             size_t length_in_bytes) {
    if (transaction == NULL) {
        return NULL;
    }
    if (data == NULL && length_in_bytes > 0) {
        y_stm_fail_transaction(stm, transaction, Y_BLOB_ERROR_INVALID_ARGUMENT,
                                "y_blob_constructor: data is NULL for a nonzero length");
        return NULL;
    }

    y_stm_handle_t content = y_stm_allocate_memory(stm, transaction, length_in_bytes);
    y_stm_write(stm, transaction, content, data, length_in_bytes);

    y_stm_handle_t descriptor_handle =
        y_stm_allocate_memory(stm, transaction, sizeof(y_blob_descriptor_t));
    y_blob_descriptor_t descriptor;
    descriptor.content = content;
    descriptor.length_in_bytes = length_in_bytes;
    y_stm_write(stm, transaction, descriptor_handle, &descriptor, sizeof(descriptor));

    if (y_stm_is_rolled_back(stm, transaction)) {
        return NULL;
    }

    y_blob_t *self = y_stm_alloc_malloc(sizeof(y_blob_t));
    self->stm = stm;
    self->descriptor = descriptor_handle;
    return self;
}

void y_blob_destructor(y_blob_t *self, y_stm_transaction_t *transaction) {
    if (self == NULL) {
        return;
    }
    y_blob_descriptor_t descriptor = y_blob_descriptor_get(self, transaction);
    y_stm_release_memory(self->stm, transaction, descriptor.content);
    y_stm_release_memory(self->stm, transaction, self->descriptor);
    y_stm_alloc_free(self);
}

void y_blob_get_length_in_bytes(const y_blob_t *self, y_stm_transaction_t *transaction,
                                size_t *out_length_in_bytes) {
    if (self == NULL || transaction == NULL || out_length_in_bytes == NULL) {
        return;
    }
    y_blob_descriptor_t descriptor = y_blob_descriptor_get(self, transaction);
    if (y_stm_is_rolled_back(self->stm, transaction)) {
        return;
    }
    *out_length_in_bytes = descriptor.length_in_bytes;
}

void y_blob_read(const y_blob_t *self, y_stm_transaction_t *transaction, void *destination,
                 size_t destination_length_in_bytes) {
    if (self == NULL || transaction == NULL) {
        return;
    }
    y_blob_descriptor_t descriptor = y_blob_descriptor_get(self, transaction);
    if (y_stm_is_rolled_back(self->stm, transaction)) {
        return;
    }
    if (destination_length_in_bytes != descriptor.length_in_bytes) {
        y_stm_fail_transaction(self->stm, transaction, Y_BLOB_ERROR_LENGTH_MISMATCH,
                                "y_blob_read: destination_length_in_bytes doesn't match the blob's length");
        return;
    }
    y_stm_read(self->stm, transaction, descriptor.content, destination,
               destination_length_in_bytes);
}

void y_blob_write(y_blob_t *self, y_stm_transaction_t *transaction, const void *data,
                  size_t length_in_bytes) {
    if (self == NULL || transaction == NULL) {
        return;
    }
    if (data == NULL && length_in_bytes > 0) {
        y_stm_fail_transaction(self->stm, transaction, Y_BLOB_ERROR_INVALID_ARGUMENT,
                                "y_blob_write: data is NULL for a nonzero length");
        return;
    }

    y_blob_descriptor_t descriptor = y_blob_descriptor_get(self, transaction);
    if (y_stm_is_rolled_back(self->stm, transaction)) {
        return;
    }
    if (length_in_bytes != descriptor.length_in_bytes) {
        y_stm_fail_transaction(self->stm, transaction, Y_BLOB_ERROR_LENGTH_MISMATCH,
                                "y_blob_write: length_in_bytes doesn't match the blob's length (use y_blob_resize)");
        return;
    }
    y_stm_write(self->stm, transaction, descriptor.content, data, length_in_bytes);
}

void y_blob_resize(y_blob_t *self, y_stm_transaction_t *transaction, size_t new_length_in_bytes) {
    if (self == NULL || transaction == NULL) {
        return;
    }

    y_blob_descriptor_t old_descriptor = y_blob_descriptor_get(self, transaction);
    if (y_stm_is_rolled_back(self->stm, transaction)) {
        return;
    }
    if (new_length_in_bytes == old_descriptor.length_in_bytes) {
        return;
    }

    /* Allocate a fresh content slot at the new size, carry over the bytes
     * that fit, and zero-fill any growth. */
    y_stm_handle_t new_content = y_stm_allocate_memory(self->stm, transaction, new_length_in_bytes);
    if (new_length_in_bytes > 0) {
        unsigned char *buffer = y_stm_alloc_calloc(new_length_in_bytes, 1);
        size_t preserved_length_in_bytes = old_descriptor.length_in_bytes < new_length_in_bytes
                                               ? old_descriptor.length_in_bytes
                                               : new_length_in_bytes;
        if (preserved_length_in_bytes > 0) {
            unsigned char *old_bytes = y_stm_alloc_malloc(old_descriptor.length_in_bytes);
            y_stm_read(self->stm, transaction, old_descriptor.content, old_bytes,
                       old_descriptor.length_in_bytes);
            y_stm_mem_memcpy(buffer, old_bytes, preserved_length_in_bytes);
            y_stm_alloc_free(old_bytes);
        }
        y_stm_write(self->stm, transaction, new_content, buffer, new_length_in_bytes);
        y_stm_alloc_free(buffer);
    }

    y_stm_release_memory(self->stm, transaction, old_descriptor.content);

    y_blob_descriptor_t new_descriptor;
    new_descriptor.content = new_content;
    new_descriptor.length_in_bytes = new_length_in_bytes;
    y_stm_write(self->stm, transaction, self->descriptor, &new_descriptor, sizeof(new_descriptor));
}
