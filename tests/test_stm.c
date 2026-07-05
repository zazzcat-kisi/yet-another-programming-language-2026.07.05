#include "stm/stm.h"

#include <assert.h>
#include <stdio.h>

static void test_allocate_and_release_outside_transaction(void) {
    stm_t *stm = stm_constructor();
    assert(stm != NULL);

    int *value = stm_allocate_memory(stm, sizeof(int));
    assert(value != NULL);
    *value = 42;
    assert(*value == 42);

    assert(stm_release_memory(stm, value));
    assert(!stm_release_memory(stm, value));

    stm_destructor(stm);
}

static void test_commit_persists_allocation(void) {
    stm_t *stm = stm_constructor();

    assert(stm_begin_transaction(stm));
    assert(stm_is_in_transaction(stm));

    int *value = stm_allocate_memory(stm, sizeof(int));
    assert(value != NULL);
    *value = 7;

    assert(stm_commit_transaction(stm));
    assert(!stm_is_in_transaction(stm));

    assert(*value == 7);
    assert(stm_release_memory(stm, value));

    stm_destructor(stm);
}

static void test_rollback_undoes_allocation(void) {
    stm_t *stm = stm_constructor();

    assert(stm_begin_transaction(stm));
    void *value = stm_allocate_memory(stm, sizeof(int));
    assert(value != NULL);

    stm_rollback_transaction(stm);
    assert(!stm_is_in_transaction(stm));

    /* The block was freed by the rollback, so it's no longer known to stm. */
    assert(!stm_release_memory(stm, value));

    stm_destructor(stm);
}

static void test_rollback_resurrects_released_memory(void) {
    stm_t *stm = stm_constructor();

    int *value = stm_allocate_memory(stm, sizeof(int));
    *value = 99;

    assert(stm_begin_transaction(stm));
    assert(stm_release_memory(stm, value));
    stm_rollback_transaction(stm);

    /* The release never actually happened, so the value is still there. */
    assert(*value == 99);
    assert(stm_release_memory(stm, value));

    stm_destructor(stm);
}

static void test_commit_finalizes_release(void) {
    stm_t *stm = stm_constructor();

    void *value = stm_allocate_memory(stm, sizeof(int));

    assert(stm_begin_transaction(stm));
    assert(stm_release_memory(stm, value));
    assert(stm_commit_transaction(stm));

    /* The block is really gone now; a second release must fail. */
    assert(!stm_release_memory(stm, value));

    stm_destructor(stm);
}

static void test_nested_transactions_only_finalize_at_outermost_commit(void) {
    stm_t *stm = stm_constructor();

    assert(stm_begin_transaction(stm));
    assert(stm_begin_transaction(stm));

    int *value = stm_allocate_memory(stm, sizeof(int));
    *value = 3;

    assert(stm_commit_transaction(stm));
    assert(stm_is_in_transaction(stm));

    assert(stm_commit_transaction(stm));
    assert(!stm_is_in_transaction(stm));

    assert(*value == 3);
    assert(stm_release_memory(stm, value));

    stm_destructor(stm);
}

static void test_rollback_aborts_entire_nested_transaction(void) {
    stm_t *stm = stm_constructor();

    assert(stm_begin_transaction(stm));
    assert(stm_begin_transaction(stm));

    void *first = stm_allocate_memory(stm, sizeof(int));
    void *second = stm_allocate_memory(stm, sizeof(int));

    stm_rollback_transaction(stm);
    assert(!stm_is_in_transaction(stm));

    assert(!stm_release_memory(stm, first));
    assert(!stm_release_memory(stm, second));
    assert(!stm_commit_transaction(stm));

    stm_destructor(stm);
}

int main(void) {
    test_allocate_and_release_outside_transaction();
    test_commit_persists_allocation();
    test_rollback_undoes_allocation();
    test_rollback_resurrects_released_memory();
    test_commit_finalizes_release();
    test_nested_transactions_only_finalize_at_outermost_commit();
    test_rollback_aborts_entire_nested_transaction();

    printf("All STM tests passed.\n");
    return 0;
}
