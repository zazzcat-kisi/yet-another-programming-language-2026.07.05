#include "y/packed_array/include.h"

#include <stddef.h>
#include <stdint.h>

#include "y/blob/include.h"
#include "y/stm/alloc/include.h"
#include "y/stm/include.h"
#include "y/stm/mem/include.h"

/* The elements are packed contiguously into one fixed blob of
 * `element_length_in_bytes * count` bytes. The count and element length are
 * fixed, so they are plain host-struct fields; only the packed bytes are
 * versioned STM data (inside the blob). y_stm is cached so failures can be
 * reported without threading the STM through every call. */
struct y_packed_array {
    y_stm_t *stm;
    y_blob_t *blob;
    size_t element_length_in_bytes;
    size_t count;
};

y_packed_array_t *y_packed_array_constructor(y_stm_t *stm, y_stm_transaction_t *transaction,
                                             size_t element_length_in_bytes, size_t count) {
    if (transaction == NULL) {
        return NULL;
    }
    if (element_length_in_bytes != 0 && count > SIZE_MAX / element_length_in_bytes) {
        y_stm_fail_transaction(
            stm, transaction, Y_PACKED_ARRAY_ERROR_INVALID_ARGUMENT,
            "y_packed_array_constructor: element_length_in_bytes * count overflows");
        return NULL;
    }
    size_t total_length_in_bytes = element_length_in_bytes * count;

    unsigned char *zeros = NULL;
    if (total_length_in_bytes > 0) {
        zeros = y_stm_alloc_calloc(total_length_in_bytes, 1);
    }
    y_blob_t *blob = y_blob_constructor(stm, transaction, zeros, total_length_in_bytes);
    y_stm_alloc_free(zeros);
    if (blob == NULL) {
        return NULL;
    }

    y_packed_array_t *self = y_stm_alloc_malloc(sizeof(y_packed_array_t));
    self->stm = stm;
    self->blob = blob;
    self->element_length_in_bytes = element_length_in_bytes;
    self->count = count;
    return self;
}

void y_packed_array_destructor(y_packed_array_t *self, y_stm_transaction_t *transaction) {
    if (self == NULL) {
        return;
    }
    y_blob_destructor(self->blob, transaction);
    y_stm_alloc_free(self);
}

void y_packed_array_get_count(const y_packed_array_t *self, size_t *out_count) {
    if (self == NULL || out_count == NULL) {
        return;
    }
    *out_count = self->count;
}

void y_packed_array_get_element_length_in_bytes(const y_packed_array_t *self,
                                                size_t *out_element_length_in_bytes) {
    if (self == NULL || out_element_length_in_bytes == NULL) {
        return;
    }
    *out_element_length_in_bytes = self->element_length_in_bytes;
}

void y_packed_array_get(const y_packed_array_t *self, y_stm_transaction_t *transaction, size_t index,
                        void *out_element, size_t out_element_length_in_bytes) {
    if (self == NULL || transaction == NULL) {
        return;
    }
    if (out_element == NULL && self->element_length_in_bytes > 0) {
        y_stm_fail_transaction(self->stm, transaction, Y_PACKED_ARRAY_ERROR_INVALID_ARGUMENT,
                                "y_packed_array_get: out_element is NULL for a nonzero element length");
        return;
    }
    if (out_element_length_in_bytes != self->element_length_in_bytes) {
        y_stm_fail_transaction(self->stm, transaction, Y_PACKED_ARRAY_ERROR_LENGTH_MISMATCH,
                                "y_packed_array_get: out_element_length_in_bytes doesn't match the element length");
        return;
    }
    if (index >= self->count) {
        y_stm_fail_transaction(self->stm, transaction, Y_PACKED_ARRAY_ERROR_OUT_OF_RANGE,
                                "y_packed_array_get: index out of range");
        return;
    }

    size_t total_length_in_bytes = self->element_length_in_bytes * self->count;
    if (total_length_in_bytes == 0) {
        return; /* zero-size elements: nothing to copy */
    }
    unsigned char *buffer = y_stm_alloc_malloc(total_length_in_bytes);
    y_blob_read(self->blob, transaction, buffer, total_length_in_bytes);
    if (!y_stm_is_rolled_back(self->stm, transaction)) {
        y_stm_mem_memcpy(out_element, buffer + index * self->element_length_in_bytes,
                         self->element_length_in_bytes);
    }
    y_stm_alloc_free(buffer);
}

void y_packed_array_set(y_packed_array_t *self, y_stm_transaction_t *transaction, size_t index,
                        const void *element, size_t element_length_in_bytes) {
    if (self == NULL || transaction == NULL) {
        return;
    }
    if (element == NULL && self->element_length_in_bytes > 0) {
        y_stm_fail_transaction(self->stm, transaction, Y_PACKED_ARRAY_ERROR_INVALID_ARGUMENT,
                                "y_packed_array_set: element is NULL for a nonzero element length");
        return;
    }
    if (element_length_in_bytes != self->element_length_in_bytes) {
        y_stm_fail_transaction(self->stm, transaction, Y_PACKED_ARRAY_ERROR_LENGTH_MISMATCH,
                                "y_packed_array_set: element_length_in_bytes doesn't match the element length");
        return;
    }
    if (index >= self->count) {
        y_stm_fail_transaction(self->stm, transaction, Y_PACKED_ARRAY_ERROR_OUT_OF_RANGE,
                                "y_packed_array_set: index out of range");
        return;
    }

    size_t total_length_in_bytes = self->element_length_in_bytes * self->count;
    if (total_length_in_bytes == 0) {
        return; /* zero-size elements: nothing to store */
    }
    /* One packed slot, so setting an element is a read-modify-write of the
     * whole backing buffer to preserve the other elements. */
    unsigned char *buffer = y_stm_alloc_malloc(total_length_in_bytes);
    y_blob_read(self->blob, transaction, buffer, total_length_in_bytes);
    if (!y_stm_is_rolled_back(self->stm, transaction)) {
        y_stm_mem_memcpy(buffer + index * self->element_length_in_bytes, element,
                         self->element_length_in_bytes);
        y_blob_write(self->blob, transaction, buffer, total_length_in_bytes);
    }
    y_stm_alloc_free(buffer);
}
