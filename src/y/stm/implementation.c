#include "y/stm/include.h"

#include <stdbool.h>
#include <stddef.h>

#include "y/stm/alloc/include.h"
#include "y/stm/mem/include.h"

typedef struct y_stm_version {
    unsigned long long version;
    bool tombstone;
    unsigned char *data;
    struct y_stm_version *next;
} y_stm_version_t;

/* A slot's identity *is* its address: a y_stm_handle_t returned by
 * y_stm_allocate_memory is simply a pointer to its y_stm_slot_t.
 * length_in_bytes is fixed at allocation time; every version committed to
 * a slot holds that many bytes. */
typedef struct y_stm_slot {
    size_t length_in_bytes;
    y_stm_version_t *versions; /* newest first; NULL until a version is committed */
    struct y_stm_slot *next;
} y_stm_slot_t;

typedef struct y_stm_slot_ref {
    y_stm_slot_t *slot;
    struct y_stm_slot_ref *next;
} y_stm_slot_ref_t;

typedef struct y_stm_write_record {
    y_stm_slot_t *slot;
    unsigned char *data; /* NULL for a release record; slot->length_in_bytes long otherwise */
    bool is_release;
    struct y_stm_write_record *next;
} y_stm_write_record_t;

struct y_stm_transaction {
    y_stm_t *owner;
    unsigned long long snapshot_version;
    y_stm_slot_ref_t *reads;
    y_stm_slot_ref_t *owned; /* slots allocated by this transaction, not yet committed */
    y_stm_write_record_t *writes; /* newest first */
};

struct y_stm {
    y_stm_slot_t *slots;
    unsigned long long next_version; /* version to be assigned to the next commit */
};

static void y_stm_slot_ref_push(y_stm_slot_ref_t **list, y_stm_slot_t *slot) {
    y_stm_slot_ref_t *ref = y_stm_alloc_malloc(sizeof(y_stm_slot_ref_t));
    ref->slot = slot;
    ref->next = *list;
    *list = ref;
}

static void y_stm_slot_ref_list_free(y_stm_slot_ref_t *list) {
    while (list != NULL) {
        y_stm_slot_ref_t *next = list->next;
        y_stm_alloc_free(list);
        list = next;
    }
}

static bool y_stm_transaction_owns(const y_stm_transaction_t *transaction,
                                    const y_stm_slot_t *slot) {
    for (const y_stm_slot_ref_t *ref = transaction->owned; ref != NULL; ref = ref->next) {
        if (ref->slot == slot) {
            return true;
        }
    }
    return false;
}

/* Returns the most recent write/release record this transaction has made
 * against `slot`, or NULL. Records are prepended, so the first match found
 * scanning from the head is always the newest one for that slot. */
static y_stm_write_record_t *y_stm_transaction_find_write(const y_stm_transaction_t *transaction,
                                                           const y_stm_slot_t *slot) {
    for (y_stm_write_record_t *record = transaction->writes; record != NULL;
         record = record->next) {
        if (record->slot == slot) {
            return record;
        }
    }
    return NULL;
}

static const y_stm_version_t *y_stm_slot_visible_version(const y_stm_slot_t *slot,
                                                          unsigned long long snapshot_version) {
    for (const y_stm_version_t *version = slot->versions; version != NULL;
         version = version->next) {
        if (version->version <= snapshot_version) {
            return version;
        }
    }
    return NULL;
}

static void y_stm_unlink_slot(y_stm_t *stm, y_stm_slot_t *target) {
    y_stm_slot_t **link = &stm->slots;
    while (*link != NULL) {
        if (*link == target) {
            *link = target->next;
            return;
        }
        link = &(*link)->next;
    }
}

static void y_stm_slot_free(y_stm_slot_t *slot) {
    y_stm_version_t *version = slot->versions;
    while (version != NULL) {
        y_stm_version_t *next = version->next;
        y_stm_alloc_free(version->data);
        y_stm_alloc_free(version);
        version = next;
    }
    y_stm_alloc_free(slot);
}

static void y_stm_transaction_destroy(y_stm_transaction_t *transaction) {
    y_stm_write_record_t *record = transaction->writes;
    while (record != NULL) {
        y_stm_write_record_t *next = record->next;
        y_stm_alloc_free(record->data);
        y_stm_alloc_free(record);
        record = next;
    }
    y_stm_slot_ref_list_free(transaction->reads);
    y_stm_slot_ref_list_free(transaction->owned);
    y_stm_alloc_free(transaction);
}

y_stm_t *y_stm_constructor(void) {
    y_stm_t *stm = y_stm_alloc_calloc(1, sizeof(y_stm_t));
    stm->next_version = 1;
    return stm;
}

void y_stm_destructor(y_stm_t *stm) {
    if (stm == NULL) {
        return;
    }
    y_stm_slot_t *slot = stm->slots;
    while (slot != NULL) {
        y_stm_slot_t *next = slot->next;
        y_stm_slot_free(slot);
        slot = next;
    }
    y_stm_alloc_free(stm);
}

y_stm_transaction_t *y_stm_begin_transaction(y_stm_t *stm) {
    y_stm_transaction_t *transaction = y_stm_alloc_calloc(1, sizeof(y_stm_transaction_t));
    transaction->owner = stm;
    transaction->snapshot_version = stm->next_version - 1;
    return transaction;
}

bool y_stm_commit_transaction(y_stm_t *stm, y_stm_transaction_t *transaction) {
    if (transaction == NULL || transaction->owner != stm) {
        return false;
    }

    for (y_stm_slot_ref_t *ref = transaction->reads; ref != NULL; ref = ref->next) {
        if (ref->slot->versions != NULL &&
            ref->slot->versions->version > transaction->snapshot_version) {
            y_stm_rollback_transaction(stm, transaction);
            return false;
        }
    }
    for (y_stm_write_record_t *record = transaction->writes; record != NULL;
         record = record->next) {
        if (y_stm_transaction_owns(transaction, record->slot)) {
            continue; /* nobody else could have touched a slot we haven't published yet */
        }
        if (y_stm_transaction_find_write(transaction, record->slot) != record) {
            continue; /* shadowed by a newer record for the same slot */
        }
        if (record->slot->versions != NULL &&
            record->slot->versions->version > transaction->snapshot_version) {
            y_stm_rollback_transaction(stm, transaction);
            return false;
        }
    }

    unsigned long long commit_version = stm->next_version;
    stm->next_version++;

    for (y_stm_write_record_t *record = transaction->writes; record != NULL;
         record = record->next) {
        if (y_stm_transaction_find_write(transaction, record->slot) != record) {
            continue;
        }

        if (record->is_release) {
            if (y_stm_transaction_owns(transaction, record->slot)) {
                continue; /* never published; the cleanup pass below discards it */
            }
            y_stm_version_t *version = y_stm_alloc_malloc(sizeof(y_stm_version_t));
            version->version = commit_version;
            version->tombstone = true;
            version->data = NULL;
            version->next = record->slot->versions;
            record->slot->versions = version;
        } else {
            y_stm_version_t *version = y_stm_alloc_malloc(sizeof(y_stm_version_t));
            version->version = commit_version;
            version->tombstone = false;
            version->data = record->data; /* ownership transfers to the version */
            version->next = record->slot->versions;
            record->slot->versions = version;
            record->data = NULL;
        }
    }

    /* Slots this transaction allocated but never wrote to (or released before
     * ever publishing them) still have no version and never will; nobody
     * else can reach them, so free them now instead of leaking forever. */
    for (y_stm_slot_ref_t *ref = transaction->owned; ref != NULL; ref = ref->next) {
        if (ref->slot->versions == NULL) {
            y_stm_unlink_slot(stm, ref->slot);
            y_stm_slot_free(ref->slot);
        }
    }

    y_stm_transaction_destroy(transaction);
    return true;
}

void y_stm_rollback_transaction(y_stm_t *stm, y_stm_transaction_t *transaction) {
    if (transaction == NULL || transaction->owner != stm) {
        return;
    }

    /* Nothing this transaction allocated was ever published (it has no
     * version yet), so it's safe to discard unconditionally. */
    for (y_stm_slot_ref_t *ref = transaction->owned; ref != NULL; ref = ref->next) {
        y_stm_unlink_slot(stm, ref->slot);
        y_stm_slot_free(ref->slot);
    }

    y_stm_transaction_destroy(transaction);
}

y_stm_handle_t y_stm_allocate_memory(y_stm_t *stm, y_stm_transaction_t *transaction,
                                     size_t length_in_bytes) {
    if (transaction == NULL || transaction->owner != stm) {
        return NULL;
    }

    y_stm_slot_t *slot = y_stm_alloc_calloc(1, sizeof(y_stm_slot_t));
    slot->length_in_bytes = length_in_bytes;
    slot->next = stm->slots;
    stm->slots = slot;

    y_stm_slot_ref_push(&transaction->owned, slot);
    return slot;
}

bool y_stm_release_memory(y_stm_t *stm, y_stm_transaction_t *transaction, y_stm_handle_t handle) {
    if (transaction == NULL || transaction->owner != stm || handle == NULL) {
        return false;
    }
    y_stm_slot_t *slot = handle;

    bool owned = y_stm_transaction_owns(transaction, slot);
    if (!owned) {
        const y_stm_version_t *visible =
            y_stm_slot_visible_version(slot, transaction->snapshot_version);
        if (visible == NULL || visible->tombstone) {
            return false;
        }
    }

    y_stm_write_record_t *existing = y_stm_transaction_find_write(transaction, slot);
    if (existing != NULL && existing->is_release) {
        return false;
    }

    y_stm_write_record_t *record = y_stm_alloc_malloc(sizeof(y_stm_write_record_t));
    record->slot = slot;
    record->data = NULL;
    record->is_release = true;
    record->next = transaction->writes;
    transaction->writes = record;
    return true;
}

bool y_stm_read(y_stm_t *stm, y_stm_transaction_t *transaction, y_stm_handle_t handle, void *out,
                size_t length_in_bytes) {
    if (transaction == NULL || transaction->owner != stm || handle == NULL || out == NULL) {
        return false;
    }
    y_stm_slot_t *slot = handle;
    if (length_in_bytes != slot->length_in_bytes) {
        return false;
    }

    y_stm_write_record_t *pending = y_stm_transaction_find_write(transaction, slot);
    if (pending != NULL) {
        if (pending->is_release) {
            return false;
        }
        y_stm_mem_memcpy(out, pending->data, length_in_bytes);
        return true;
    }

    const y_stm_version_t *visible =
        y_stm_slot_visible_version(slot, transaction->snapshot_version);
    if (visible == NULL || visible->tombstone) {
        return false;
    }
    y_stm_mem_memcpy(out, visible->data, length_in_bytes);
    y_stm_slot_ref_push(&transaction->reads, slot);
    return true;
}

bool y_stm_write(y_stm_t *stm, y_stm_transaction_t *transaction, y_stm_handle_t handle,
                  const void *data, size_t length_in_bytes) {
    if (transaction == NULL || transaction->owner != stm || handle == NULL ||
        (data == NULL && length_in_bytes > 0)) {
        return false;
    }
    y_stm_slot_t *slot = handle;
    if (length_in_bytes != slot->length_in_bytes) {
        return false;
    }

    bool owned = y_stm_transaction_owns(transaction, slot);
    if (!owned) {
        const y_stm_version_t *visible =
            y_stm_slot_visible_version(slot, transaction->snapshot_version);
        if (visible == NULL || visible->tombstone) {
            return false;
        }
    }

    y_stm_write_record_t *existing = y_stm_transaction_find_write(transaction, slot);
    if (existing != NULL && existing->is_release) {
        return false;
    }

    unsigned char *copy = NULL;
    if (length_in_bytes > 0) {
        copy = y_stm_alloc_malloc(length_in_bytes);
        y_stm_mem_memcpy(copy, data, length_in_bytes);
    }

    y_stm_write_record_t *record = y_stm_alloc_malloc(sizeof(y_stm_write_record_t));
    record->slot = slot;
    record->data = copy;
    record->is_release = false;
    record->next = transaction->writes;
    transaction->writes = record;
    return true;
}
