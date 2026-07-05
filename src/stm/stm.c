#include "stm/stm.h"

#include <stdlib.h>

typedef struct stm_block {
    void *ptr;
    size_t length_in_bytes;
    bool pending_free;
    struct stm_block *next;
} stm_block_t;

typedef enum {
    STM_UNDO_ALLOC,
    STM_UNDO_FREE,
} stm_undo_kind_t;

typedef struct stm_undo_entry {
    stm_undo_kind_t kind;
    void *ptr;
    struct stm_undo_entry *next;
} stm_undo_entry_t;

struct stm {
    stm_block_t *blocks;
    stm_undo_entry_t *undo_log;
    unsigned depth;
};

static stm_block_t *stm_find_block(const stm_t *stm, void *ptr) {
    for (stm_block_t *block = stm->blocks; block != NULL; block = block->next) {
        if (block->ptr == ptr) {
            return block;
        }
    }
    return NULL;
}

static void stm_unlink_block(stm_t *stm, stm_block_t *target) {
    stm_block_t **link = &stm->blocks;
    while (*link != NULL) {
        if (*link == target) {
            *link = target->next;
            return;
        }
        link = &(*link)->next;
    }
}

/* Undo entries are pushed LIFO so replaying the list head-to-tail during
 * rollback naturally undoes operations in reverse chronological order. */
static bool stm_push_undo(stm_t *stm, stm_undo_kind_t kind, void *ptr) {
    stm_undo_entry_t *entry = malloc(sizeof(stm_undo_entry_t));
    if (entry == NULL) {
        return false;
    }
    entry->kind = kind;
    entry->ptr = ptr;
    entry->next = stm->undo_log;
    stm->undo_log = entry;
    return true;
}

static void stm_discard_undo_log(stm_t *stm) {
    stm_undo_entry_t *entry = stm->undo_log;
    while (entry != NULL) {
        stm_undo_entry_t *next = entry->next;
        free(entry);
        entry = next;
    }
    stm->undo_log = NULL;
}

stm_t *stm_constructor(void) {
    return calloc(1, sizeof(stm_t));
}

void stm_destructor(stm_t *stm) {
    if (stm == NULL) {
        return;
    }
    if (stm->depth > 0) {
        stm_rollback_transaction(stm);
    }
    stm_block_t *block = stm->blocks;
    while (block != NULL) {
        stm_block_t *next = block->next;
        free(block->ptr);
        free(block);
        block = next;
    }
    free(stm);
}

void *stm_allocate_memory(stm_t *stm, size_t length_in_bytes) {
    void *ptr = malloc(length_in_bytes);
    if (ptr == NULL) {
        return NULL;
    }

    stm_block_t *block = malloc(sizeof(stm_block_t));
    if (block == NULL) {
        free(ptr);
        return NULL;
    }
    block->ptr = ptr;
    block->length_in_bytes = length_in_bytes;
    block->pending_free = false;
    block->next = stm->blocks;
    stm->blocks = block;

    if (stm->depth > 0 && !stm_push_undo(stm, STM_UNDO_ALLOC, ptr)) {
        stm_unlink_block(stm, block);
        free(block);
        free(ptr);
        return NULL;
    }

    return ptr;
}

bool stm_release_memory(stm_t *stm, void *ptr) {
    stm_block_t *block = stm_find_block(stm, ptr);
    if (block == NULL || block->pending_free) {
        return false;
    }

    if (stm->depth > 0) {
        if (!stm_push_undo(stm, STM_UNDO_FREE, ptr)) {
            return false;
        }
        block->pending_free = true;
        return true;
    }

    stm_unlink_block(stm, block);
    free(block->ptr);
    free(block);
    return true;
}

bool stm_begin_transaction(stm_t *stm) {
    stm->depth++;
    return true;
}

bool stm_commit_transaction(stm_t *stm) {
    if (stm->depth == 0) {
        return false;
    }

    stm->depth--;
    if (stm->depth > 0) {
        return true;
    }

    stm_block_t **link = &stm->blocks;
    while (*link != NULL) {
        stm_block_t *block = *link;
        if (block->pending_free) {
            *link = block->next;
            free(block->ptr);
            free(block);
        } else {
            link = &block->next;
        }
    }

    stm_discard_undo_log(stm);
    return true;
}

/* Rollback always aborts the entire outermost transaction, regardless of
 * nesting depth: this STM flattens nested begin/commit pairs rather than
 * tracking a separate undo checkpoint per nesting level. */
void stm_rollback_transaction(stm_t *stm) {
    if (stm->depth == 0) {
        return;
    }

    stm_undo_entry_t *entry = stm->undo_log;
    while (entry != NULL) {
        stm_undo_entry_t *next = entry->next;

        stm_block_t *block = stm_find_block(stm, entry->ptr);
        if (block != NULL) {
            if (entry->kind == STM_UNDO_ALLOC) {
                stm_unlink_block(stm, block);
                free(block->ptr);
                free(block);
            } else {
                block->pending_free = false;
            }
        }

        free(entry);
        entry = next;
    }

    stm->undo_log = NULL;
    stm->depth = 0;
}

bool stm_is_in_transaction(const stm_t *stm) {
    return stm->depth > 0;
}
