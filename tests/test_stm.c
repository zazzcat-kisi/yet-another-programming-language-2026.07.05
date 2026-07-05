#include "stm/stm.h"

#include <assert.h>
#include <stdio.h>

static void test_write_then_commit_is_visible_to_a_later_transaction(void) {
    stm_t *stm = stm_constructor();

    stm_transaction_t *writer = stm_begin_transaction(stm);
    void *handle = stm_allocate_memory(stm, writer, sizeof(int));
    assert(handle != NULL);
    int value = 42;
    assert(stm_write(stm, writer, handle, &value, sizeof(value)));
    assert(stm_commit_transaction(stm, writer));

    stm_transaction_t *reader = stm_begin_transaction(stm);
    int read_value = 0;
    assert(stm_read(stm, reader, handle, &read_value, sizeof(read_value)));
    assert(read_value == 42);
    assert(stm_commit_transaction(stm, reader));

    stm_destructor(stm);
}

static void test_read_your_own_write_before_commit(void) {
    stm_t *stm = stm_constructor();

    stm_transaction_t *transaction = stm_begin_transaction(stm);
    void *handle = stm_allocate_memory(stm, transaction, sizeof(int));
    int value = 7;
    assert(stm_write(stm, transaction, handle, &value, sizeof(value)));

    int read_value = 0;
    assert(stm_read(stm, transaction, handle, &read_value, sizeof(read_value)));
    assert(read_value == 7);

    assert(stm_commit_transaction(stm, transaction));
    stm_destructor(stm);
}

static void test_reading_unwritten_memory_fails(void) {
    stm_t *stm = stm_constructor();

    stm_transaction_t *transaction = stm_begin_transaction(stm);
    void *handle = stm_allocate_memory(stm, transaction, sizeof(int));
    int read_value = 0;
    assert(!stm_read(stm, transaction, handle, &read_value, sizeof(read_value)));

    stm_rollback_transaction(stm, transaction);
    stm_destructor(stm);
}

static void test_rollback_discards_uncommitted_write(void) {
    stm_t *stm = stm_constructor();

    stm_transaction_t *setup = stm_begin_transaction(stm);
    void *handle = stm_allocate_memory(stm, setup, sizeof(int));
    int original = 1;
    assert(stm_write(stm, setup, handle, &original, sizeof(original)));
    assert(stm_commit_transaction(stm, setup));

    stm_transaction_t *doomed = stm_begin_transaction(stm);
    int replacement = 999;
    assert(stm_write(stm, doomed, handle, &replacement, sizeof(replacement)));
    stm_rollback_transaction(stm, doomed);

    stm_transaction_t *reader = stm_begin_transaction(stm);
    int read_value = 0;
    assert(stm_read(stm, reader, handle, &read_value, sizeof(read_value)));
    assert(read_value == 1);
    assert(stm_commit_transaction(stm, reader));

    stm_destructor(stm);
}

static void test_release_then_commit_hides_memory_from_later_transactions(void) {
    stm_t *stm = stm_constructor();

    stm_transaction_t *setup = stm_begin_transaction(stm);
    void *handle = stm_allocate_memory(stm, setup, sizeof(int));
    int value = 5;
    assert(stm_write(stm, setup, handle, &value, sizeof(value)));
    assert(stm_commit_transaction(stm, setup));

    stm_transaction_t *releaser = stm_begin_transaction(stm);
    assert(stm_release_memory(stm, releaser, handle));
    assert(stm_commit_transaction(stm, releaser));

    stm_transaction_t *reader = stm_begin_transaction(stm);
    int read_value = 0;
    assert(!stm_read(stm, reader, handle, &read_value, sizeof(read_value)));
    assert(!stm_write(stm, reader, handle, &value, sizeof(value)));
    stm_rollback_transaction(stm, reader);

    stm_destructor(stm);
}

static void test_concurrent_writers_first_committer_wins(void) {
    stm_t *stm = stm_constructor();

    stm_transaction_t *setup = stm_begin_transaction(stm);
    void *handle = stm_allocate_memory(stm, setup, sizeof(int));
    int initial = 0;
    assert(stm_write(stm, setup, handle, &initial, sizeof(initial)));
    assert(stm_commit_transaction(stm, setup));

    /* Both transactions start from the same snapshot. */
    stm_transaction_t *a = stm_begin_transaction(stm);
    stm_transaction_t *b = stm_begin_transaction(stm);

    int value_a = 111;
    assert(stm_write(stm, a, handle, &value_a, sizeof(value_a)));
    assert(stm_commit_transaction(stm, a));

    int value_b = 222;
    assert(stm_write(stm, b, handle, &value_b, sizeof(value_b)));
    /* b's snapshot is stale now that a has committed a newer version. */
    assert(!stm_commit_transaction(stm, b));

    stm_transaction_t *reader = stm_begin_transaction(stm);
    int read_value = 0;
    assert(stm_read(stm, reader, handle, &read_value, sizeof(read_value)));
    assert(read_value == 111);
    assert(stm_commit_transaction(stm, reader));

    stm_destructor(stm);
}

static void test_stale_read_aborts_commit_even_without_a_write(void) {
    stm_t *stm = stm_constructor();

    stm_transaction_t *setup = stm_begin_transaction(stm);
    void *handle = stm_allocate_memory(stm, setup, sizeof(int));
    int initial = 1;
    assert(stm_write(stm, setup, handle, &initial, sizeof(initial)));
    assert(stm_commit_transaction(stm, setup));

    stm_transaction_t *reader = stm_begin_transaction(stm);
    int read_value = 0;
    assert(stm_read(stm, reader, handle, &read_value, sizeof(read_value)));

    stm_transaction_t *writer = stm_begin_transaction(stm);
    int updated = 2;
    assert(stm_write(stm, writer, handle, &updated, sizeof(updated)));
    assert(stm_commit_transaction(stm, writer));

    /* reader's view of `handle` is now stale even though it never wrote. */
    assert(!stm_commit_transaction(stm, reader));

    stm_destructor(stm);
}

static void test_operations_require_an_open_transaction(void) {
    stm_t *stm = stm_constructor();

    assert(stm_allocate_memory(stm, NULL, sizeof(int)) == NULL);

    stm_transaction_t *transaction = stm_begin_transaction(stm);
    void *handle = stm_allocate_memory(stm, transaction, sizeof(int));
    int value = 1;
    assert(stm_write(stm, transaction, handle, &value, sizeof(value)));
    assert(stm_commit_transaction(stm, transaction));

    assert(!stm_read(stm, NULL, handle, &value, sizeof(value)));
    assert(!stm_write(stm, NULL, handle, &value, sizeof(value)));
    assert(!stm_release_memory(stm, NULL, handle));

    stm_destructor(stm);
}

static void test_transaction_rejects_the_wrong_stm_instance(void) {
    stm_t *stm_one = stm_constructor();
    stm_t *stm_two = stm_constructor();

    stm_transaction_t *transaction = stm_begin_transaction(stm_one);
    assert(stm_allocate_memory(stm_two, transaction, sizeof(int)) == NULL);
    assert(!stm_commit_transaction(stm_two, transaction));

    stm_rollback_transaction(stm_one, transaction);
    stm_destructor(stm_one);
    stm_destructor(stm_two);
}

int main(void) {
    test_write_then_commit_is_visible_to_a_later_transaction();
    test_read_your_own_write_before_commit();
    test_reading_unwritten_memory_fails();
    test_rollback_discards_uncommitted_write();
    test_release_then_commit_hides_memory_from_later_transactions();
    test_concurrent_writers_first_committer_wins();
    test_stale_read_aborts_commit_even_without_a_write();
    test_operations_require_an_open_transaction();
    test_transaction_rejects_the_wrong_stm_instance();

    printf("All STM tests passed.\n");
    return 0;
}
