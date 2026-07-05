#ifndef Y_STM_ALLOC_INCLUDE_H
#define Y_STM_ALLOC_INCLUDE_H

#include <stddef.h>

/*
 * Thin wrappers around the C allocator. The rest of y_stm must go through
 * these instead of calling malloc/calloc/free directly (see CLAUDE.md),
 * so the allocation strategy can be swapped in one place later.
 *
 * For a nonzero-size request, y_stm_alloc_malloc/y_stm_alloc_calloc never
 * return NULL: running out of memory is treated as fatal (y_fatal) and
 * terminates the process instead of being propagated as an error code, so
 * callers don't need to (and must not) check their result for NULL. A
 * zero-size request may still return NULL, per the C standard's allowance
 * for that case.
 */
void *y_stm_alloc_malloc(size_t length_in_bytes);
void *y_stm_alloc_calloc(size_t count, size_t length_in_bytes);
void y_stm_alloc_free(void *ptr);

#endif
