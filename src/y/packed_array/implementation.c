#include "y/packed_array/include.h"

#include <stddef.h>
#include <stdint.h>

#include "y/blob/include.h"
#include "y/stm/alloc/include.h"
#include "y/stm/include.h"
#include "y/stm/mem/include.h"

/* The elements are packed contiguously into one resizable blob of
 * `element_length_in_bytes * count` bytes. The element length is fixed, so
 * it is a plain host field; the count is not stored at all -- it is derived
 * from the blob's current (versioned) length, which is what makes the array
 * growable and keeps the count isolated across transactions. y_stm is cached
 * so failures can be reported without threading the STM through every call. */
struct y_packed_array {
    y_stm_t *stm;
    y_blob_t *blob;
    size_t element_length_in_bytes;
};

/* The count as of `transaction`, or 0 if the read rolled the transaction
 * back (callers check y_stm_is_rolled_back after). element_length_in_bytes is
 * guaranteed nonzero by the constructor, so the division is always valid. */
static size_t y_packed_array_count(const y_packed_array_t *self,
                                   y_stm_transaction_t *transaction) {
    size_t total_length_in_bytes = 0;
    y_blob_get_length_in_bytes(self->blob, transaction, &total_length_in_bytes);
    return total_length_in_bytes / self->element_length_in_bytes;
}

y_packed_array_t *y_packed_array_constructor(y_stm_t *stm, y_stm_transaction_t *transaction,
                                             size_t element_length_in_bytes, size_t count) {
    if (transaction == NULL) {
        return NULL;
    }
    if (element_length_in_bytes == 0) {
        y_stm_fail_transaction(stm, transaction, Y_PACKED_ARRAY_ERROR_INVALID_ARGUMENT,
                                "y_packed_array_constructor: element_length_in_bytes must be nonzero");
        return NULL;
    }
    if (count > SIZE_MAX / element_length_in_bytes) {
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
    return self;
}

void y_packed_array_destructor(y_packed_array_t *self, y_stm_transaction_t *transaction) {
    if (self == NULL) {
        return;
    }
    y_blob_destructor(self->blob, transaction);
    y_stm_alloc_free(self);
}

void y_packed_array_get_count(const y_packed_array_t *self, y_stm_transaction_t *transaction,
                              size_t *out_count) {
    if (self == NULL || transaction == NULL || out_count == NULL) {
        return;
    }
    size_t count = y_packed_array_count(self, transaction);
    if (y_stm_is_rolled_back(self->stm, transaction)) {
        return;
    }
    *out_count = count;
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
    if (self == NULL || transaction == NULL || out_element == NULL) {
        return;
    }
    if (out_element_length_in_bytes != self->element_length_in_bytes) {
        y_stm_fail_transaction(self->stm, transaction, Y_PACKED_ARRAY_ERROR_LENGTH_MISMATCH,
                                "y_packed_array_get: out_element_length_in_bytes doesn't match the element length");
        return;
    }

    size_t count = y_packed_array_count(self, transaction);
    if (y_stm_is_rolled_back(self->stm, transaction)) {
        return;
    }
    if (index >= count) {
        y_stm_fail_transaction(self->stm, transaction, Y_PACKED_ARRAY_ERROR_OUT_OF_RANGE,
                                "y_packed_array_get: index out of range");
        return;
    }

    size_t total_length_in_bytes = count * self->element_length_in_bytes;
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
    if (self == NULL || transaction == NULL || element == NULL) {
        return;
    }
    if (element_length_in_bytes != self->element_length_in_bytes) {
        y_stm_fail_transaction(self->stm, transaction, Y_PACKED_ARRAY_ERROR_LENGTH_MISMATCH,
                                "y_packed_array_set: element_length_in_bytes doesn't match the element length");
        return;
    }

    size_t count = y_packed_array_count(self, transaction);
    if (y_stm_is_rolled_back(self->stm, transaction)) {
        return;
    }
    if (index >= count) {
        y_stm_fail_transaction(self->stm, transaction, Y_PACKED_ARRAY_ERROR_OUT_OF_RANGE,
                                "y_packed_array_set: index out of range");
        return;
    }

    /* One packed slot, so setting an element is a read-modify-write of the
     * whole backing buffer to preserve the other elements. */
    size_t total_length_in_bytes = count * self->element_length_in_bytes;
    unsigned char *buffer = y_stm_alloc_malloc(total_length_in_bytes);
    y_blob_read(self->blob, transaction, buffer, total_length_in_bytes);
    if (!y_stm_is_rolled_back(self->stm, transaction)) {
        y_stm_mem_memcpy(buffer + index * self->element_length_in_bytes, element,
                         self->element_length_in_bytes);
        y_blob_write(self->blob, transaction, buffer, total_length_in_bytes);
    }
    y_stm_alloc_free(buffer);
}

void y_packed_array_resize(y_packed_array_t *self, y_stm_transaction_t *transaction,
                           size_t new_count) {
    if (self == NULL || transaction == NULL) {
        return;
    }
    if (new_count > SIZE_MAX / self->element_length_in_bytes) {
        y_stm_fail_transaction(self->stm, transaction, Y_PACKED_ARRAY_ERROR_INVALID_ARGUMENT,
                                "y_packed_array_resize: element_length_in_bytes * new_count overflows");
        return;
    }
    y_blob_resize(self->blob, transaction, new_count * self->element_length_in_bytes);
}
