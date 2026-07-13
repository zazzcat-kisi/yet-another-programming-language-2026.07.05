#include "y/blob/include.h"

#include <stddef.h>

#include "y/stm/alloc/include.h"
#include "y/stm/include.h"

/* A blob is a single fixed-length y_stm slot plus the STM it lives in. The
 * length never changes, so both it and the content handle are plain fields
 * on the host struct rather than versioned STM data -- there is nothing to
 * keep isolated across transactions, since no operation ever swaps the
 * content slot or changes its length. */
struct y_blob {
    y_stm_t *stm;
    y_stm_handle_t content;
    size_t length_in_bytes;
};

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

    if (y_stm_is_rolled_back(stm, transaction)) {
        return NULL;
    }

    y_blob_t *self = y_stm_alloc_malloc(sizeof(y_blob_t));
    self->stm = stm;
    self->content = content;
    self->length_in_bytes = length_in_bytes;
    return self;
}

void y_blob_destructor(y_blob_t *self, y_stm_transaction_t *transaction) {
    if (self == NULL) {
        return;
    }
    y_stm_release_memory(self->stm, transaction, self->content);
    y_stm_alloc_free(self);
}

void y_blob_get_length_in_bytes(const y_blob_t *self, size_t *out_length_in_bytes) {
    if (self == NULL || out_length_in_bytes == NULL) {
        return;
    }
    *out_length_in_bytes = self->length_in_bytes;
}

void y_blob_read(const y_blob_t *self, y_stm_transaction_t *transaction, void *destination,
                 size_t destination_length_in_bytes) {
    if (self == NULL || transaction == NULL) {
        return;
    }
    if (destination_length_in_bytes != self->length_in_bytes) {
        y_stm_fail_transaction(self->stm, transaction, Y_BLOB_ERROR_LENGTH_MISMATCH,
                                "y_blob_read: destination_length_in_bytes doesn't match the blob's length");
        return;
    }
    y_stm_read(self->stm, transaction, self->content, destination, destination_length_in_bytes);
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
    if (length_in_bytes != self->length_in_bytes) {
        y_stm_fail_transaction(self->stm, transaction, Y_BLOB_ERROR_LENGTH_MISMATCH,
                                "y_blob_write: length_in_bytes doesn't match the blob's fixed length");
        return;
    }
    y_stm_write(self->stm, transaction, self->content, data, length_in_bytes);
}
