#include "y/stm/mem/include.h"

#include <string.h>

void *y_stm_mem_memcpy(void *destination, const void *source, size_t length_in_bytes) {
    return memcpy(destination, source, length_in_bytes);
}
