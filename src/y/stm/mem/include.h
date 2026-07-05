#ifndef Y_STM_MEM_INCLUDE_H
#define Y_STM_MEM_INCLUDE_H

#include <stddef.h>

/*
 * Thin wrappers around <string.h> byte-manipulation functions. The rest
 * of y_stm must go through these instead of calling memcpy/memset/etc
 * directly (see CLAUDE.md). Add more as they're actually needed elsewhere
 * in the codebase.
 */
void *y_stm_mem_memcpy(void *destination, const void *source, size_t length_in_bytes);

#endif
