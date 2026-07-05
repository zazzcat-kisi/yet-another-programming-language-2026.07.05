#include "y/stm/alloc/include.h"

#include <stdlib.h>

void *y_stm_alloc_malloc(size_t length_in_bytes) {
    return malloc(length_in_bytes);
}

void *y_stm_alloc_calloc(size_t count, size_t length_in_bytes) {
    return calloc(count, length_in_bytes);
}

void y_stm_alloc_free(void *ptr) {
    free(ptr);
}
