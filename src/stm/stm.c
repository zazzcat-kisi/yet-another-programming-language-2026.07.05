#include "stm/stm.h"

#include <stdlib.h>
#include <string.h>

typedef struct stm_version {
    unsigned long long version;
    bool tombstone;
    unsigned char *data;
    struct stm_version *next;
} stm_version_t;

/* A slot's identity *is* its address: an stm_handle_t returned by
 * stm_allocate_memory is simply a pointer to its stm_slot_t.
 * length_in_bytes is fixed at allocation time; every version committed to
 * a slot holds that many bytes. */
typedef struct stm_slot {
    size_t length_in_bytes;
    stm_version_t *versions; /* newest first; NULL until a version is committed */
    struct stm_slot *next;
} stm_slot_t;

typedef struct stm_slot_ref {
    stm_slot_t *slot;
    struct stm_slot_ref *next;
} stm_slot_ref_t;

typedef struct stm_write_record {
    stm_slot_t *slot;
    unsigned char *data; /* NULL for a release record; slot->length_in_bytes long otherwise */
    bool is_release;
    struct stm_write_record *next;
} stm_write_record_t;

struct stm_transaction {
    stm_t *owner;
    unsigned long long snapshot_version;
    stm_slot_ref_t *reads;
    stm_slot_ref_t *owned; /* slots allocated by this transaction, not yet committed */
    stm_write_record_t *writes; /* newest first */
};

struct stm {
    stm_slot_t *slots;
    unsigned long long next_version; /* version to be assigned to the next commit */
};

static bool stm_slot_ref_push(stm_slot_ref_t **list, stm_slot_t *slot) {
    stm_slot_ref_t *ref = malloc(sizeof(stm_slot_ref_t));
    if (ref == NULL) {
        return false;
    }
    ref->slot = slot;
    ref->next = *list;
    *list = ref;
    return true;
}

static void stm_slot_ref_list_free(stm_slot_ref_t *list) {
    while (list != NULL) {
        stm_slot_ref_t *next = list->next;
        free(list);
        list = next;
    }
}

static bool stm_transaction_owns(const stm_transaction_t *transaction, const stm_slot_t *slot) {
    for (const stm_slot_ref_t *ref = transaction->owned; ref != NULL; ref = ref->next) {
        if (ref->slot == slot) {
            return true;
        }
    }
    return false;
}

/* Returns the most recent write/release record this transaction has made
 * against `slot`, or NULL. Records are prepended, so the first match found
 * scanning from the head is always the newest one for that slot. */
static stm_write_record_t *stm_transaction_find_write(const stm_transaction_t *transaction,
                                                       const stm_slot_t *slot) {
    for (stm_write_record_t *record = transaction->writes; record != NULL; record = record->next) {
        if (record->slot == slot) {
            return record;
        }
    }
    return NULL;
}

static const stm_version_t *stm_slot_visible_version(const stm_slot_t *slot,
                                                      unsigned long long snapshot_version) {
    for (const stm_version_t *version = slot->versions; version != NULL; version = version->next) {
        if (version->version <= snapshot_version) {
            return version;
        }
    }
    return NULL;
}

static void stm_unlink_slot(stm_t *stm, stm_slot_t *target) {
    stm_slot_t **link = &stm->slots;
    while (*link != NULL) {
        if (*link == target) {
            *link = target->next;
            return;
        }
        link = &(*link)->next;
    }
}

static void stm_slot_free(stm_slot_t *slot) {
    stm_version_t *version = slot->versions;
    while (version != NULL) {
        stm_version_t *next = version->next;
        free(version->data);
        free(version);
        version = next;
    }
    free(slot);
}

static void stm_transaction_destroy(stm_transaction_t *transaction) {
    stm_write_record_t *record = transaction->writes;
    while (record != NULL) {
        stm_write_record_t *next = record->next;
        free(record->data);
        free(record);
        record = next;
    }
    stm_slot_ref_list_free(transaction->reads);
    stm_slot_ref_list_free(transaction->owned);
    free(transaction);
}

stm_t *stm_constructor(void) {
    stm_t *stm = calloc(1, sizeof(stm_t));
    if (stm == NULL) {
        return NULL;
    }
    stm->next_version = 1;
    return stm;
}

void stm_destructor(stm_t *stm) {
    if (stm == NULL) {
        return;
    }
    stm_slot_t *slot = stm->slots;
    while (slot != NULL) {
        stm_slot_t *next = slot->next;
        stm_slot_free(slot);
        slot = next;
    }
    free(stm);
}

stm_transaction_t *stm_begin_transaction(stm_t *stm) {
    stm_transaction_t *transaction = calloc(1, sizeof(stm_transaction_t));
    if (transaction == NULL) {
        return NULL;
    }
    transaction->owner = stm;
    transaction->snapshot_version = stm->next_version - 1;
    return transaction;
}

bool stm_commit_transaction(stm_t *stm, stm_transaction_t *transaction) {
    if (transaction == NULL || transaction->owner != stm) {
        return false;
    }

    for (stm_slot_ref_t *ref = transaction->reads; ref != NULL; ref = ref->next) {
        if (ref->slot->versions != NULL &&
            ref->slot->versions->version > transaction->snapshot_version) {
            stm_rollback_transaction(stm, transaction);
            return false;
        }
    }
    for (stm_write_record_t *record = transaction->writes; record != NULL; record = record->next) {
        if (stm_transaction_owns(transaction, record->slot)) {
            continue; /* nobody else could have touched a slot we haven't published yet */
        }
        if (stm_transaction_find_write(transaction, record->slot) != record) {
            continue; /* shadowed by a newer record for the same slot */
        }
        if (record->slot->versions != NULL &&
            record->slot->versions->version > transaction->snapshot_version) {
            stm_rollback_transaction(stm, transaction);
            return false;
        }
    }

    unsigned long long commit_version = stm->next_version;
    stm->next_version++;

    for (stm_write_record_t *record = transaction->writes; record != NULL; record = record->next) {
        if (stm_transaction_find_write(transaction, record->slot) != record) {
            continue;
        }

        if (record->is_release) {
            if (stm_transaction_owns(transaction, record->slot)) {
                continue; /* never published; the cleanup pass below discards it */
            }
            stm_version_t *version = malloc(sizeof(stm_version_t));
            if (version == NULL) {
                continue; /* the release is lost, but nothing is left inconsistent */
            }
            version->version = commit_version;
            version->tombstone = true;
            version->data = NULL;
            version->next = record->slot->versions;
            record->slot->versions = version;
        } else {
            stm_version_t *version = malloc(sizeof(stm_version_t));
            if (version == NULL) {
                continue; /* the write is lost, but nothing is left inconsistent */
            }
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
    for (stm_slot_ref_t *ref = transaction->owned; ref != NULL; ref = ref->next) {
        if (ref->slot->versions == NULL) {
            stm_unlink_slot(stm, ref->slot);
            stm_slot_free(ref->slot);
        }
    }

    stm_transaction_destroy(transaction);
    return true;
}

void stm_rollback_transaction(stm_t *stm, stm_transaction_t *transaction) {
    if (transaction == NULL || transaction->owner != stm) {
        return;
    }

    /* Nothing this transaction allocated was ever published (it has no
     * version yet), so it's safe to discard unconditionally. */
    for (stm_slot_ref_t *ref = transaction->owned; ref != NULL; ref = ref->next) {
        stm_unlink_slot(stm, ref->slot);
        stm_slot_free(ref->slot);
    }

    stm_transaction_destroy(transaction);
}

stm_handle_t stm_allocate_memory(stm_t *stm, stm_transaction_t *transaction,
                                  size_t length_in_bytes) {
    if (transaction == NULL || transaction->owner != stm) {
        return NULL;
    }

    stm_slot_t *slot = calloc(1, sizeof(stm_slot_t));
    if (slot == NULL) {
        return NULL;
    }
    slot->length_in_bytes = length_in_bytes;
    slot->next = stm->slots;
    stm->slots = slot;

    if (!stm_slot_ref_push(&transaction->owned, slot)) {
        stm->slots = slot->next;
        free(slot);
        return NULL;
    }
    return slot;
}

bool stm_release_memory(stm_t *stm, stm_transaction_t *transaction, stm_handle_t handle) {
    if (transaction == NULL || transaction->owner != stm || handle == NULL) {
        return false;
    }
    stm_slot_t *slot = handle;

    bool owned = stm_transaction_owns(transaction, slot);
    if (!owned) {
        const stm_version_t *visible = stm_slot_visible_version(slot, transaction->snapshot_version);
        if (visible == NULL || visible->tombstone) {
            return false;
        }
    }

    stm_write_record_t *existing = stm_transaction_find_write(transaction, slot);
    if (existing != NULL && existing->is_release) {
        return false;
    }

    stm_write_record_t *record = malloc(sizeof(stm_write_record_t));
    if (record == NULL) {
        return false;
    }
    record->slot = slot;
    record->data = NULL;
    record->is_release = true;
    record->next = transaction->writes;
    transaction->writes = record;
    return true;
}

bool stm_read(stm_t *stm, stm_transaction_t *transaction, stm_handle_t handle, void *out,
              size_t length_in_bytes) {
    if (transaction == NULL || transaction->owner != stm || handle == NULL || out == NULL) {
        return false;
    }
    stm_slot_t *slot = handle;
    if (length_in_bytes != slot->length_in_bytes) {
        return false;
    }

    stm_write_record_t *pending = stm_transaction_find_write(transaction, slot);
    if (pending != NULL) {
        if (pending->is_release) {
            return false;
        }
        memcpy(out, pending->data, length_in_bytes);
        return true;
    }

    const stm_version_t *visible = stm_slot_visible_version(slot, transaction->snapshot_version);
    if (visible == NULL || visible->tombstone) {
        return false;
    }
    memcpy(out, visible->data, length_in_bytes);
    return stm_slot_ref_push(&transaction->reads, slot);
}

bool stm_write(stm_t *stm, stm_transaction_t *transaction, stm_handle_t handle, const void *data,
               size_t length_in_bytes) {
    if (transaction == NULL || transaction->owner != stm || handle == NULL ||
        (data == NULL && length_in_bytes > 0)) {
        return false;
    }
    stm_slot_t *slot = handle;
    if (length_in_bytes != slot->length_in_bytes) {
        return false;
    }

    bool owned = stm_transaction_owns(transaction, slot);
    if (!owned) {
        const stm_version_t *visible = stm_slot_visible_version(slot, transaction->snapshot_version);
        if (visible == NULL || visible->tombstone) {
            return false;
        }
    }

    stm_write_record_t *existing = stm_transaction_find_write(transaction, slot);
    if (existing != NULL && existing->is_release) {
        return false;
    }

    unsigned char *copy = NULL;
    if (length_in_bytes > 0) {
        copy = malloc(length_in_bytes);
        if (copy == NULL) {
            return false;
        }
        memcpy(copy, data, length_in_bytes);
    }

    stm_write_record_t *record = malloc(sizeof(stm_write_record_t));
    if (record == NULL) {
        free(copy);
        return false;
    }
    record->slot = slot;
    record->data = copy;
    record->is_release = false;
    record->next = transaction->writes;
    transaction->writes = record;
    return true;
}
