#include "y/stm/alloc/include.h"

#include <stdlib.h>

#include "y/fatal/include.h"

void *y_stm_alloc_malloc(size_t length_in_bytes) {
    void *ptr = malloc(length_in_bytes);
    if (ptr == NULL && length_in_bytes > 0) {
        y_fatal_terminate("y_stm_alloc_malloc: out of memory");
    }
    return ptr;
}

void *y_stm_alloc_calloc(size_t count, size_t length_in_bytes) {
    void *ptr = calloc(count, length_in_bytes);
    if (ptr == NULL && count > 0 && length_in_bytes > 0) {
        y_fatal_terminate("y_stm_alloc_calloc: out of memory");
    }
    return ptr;
}

void y_stm_alloc_free(void *ptr) {
    free(ptr);
}
