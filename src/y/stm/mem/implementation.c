#include "y/stm/mem/include.h"

#include <string.h>

void *y_stm_mem_memcpy(void *destination, const void *source, size_t length_in_bytes) {
    return memcpy(destination, source, length_in_bytes);
}

int y_stm_mem_memcmp(const void *a, const void *b, size_t length_in_bytes) {
    return memcmp(a, b, length_in_bytes);
}
