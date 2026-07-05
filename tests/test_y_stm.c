#include "y/stm/include.h"

#include <assert.h>
#include <stdio.h>

static void test_write_then_commit_is_visible_to_a_later_transaction(void) {
    y_stm_t *stm = y_stm_constructor();

    y_stm_transaction_t *writer = y_stm_begin_transaction(stm);
    y_stm_handle_t handle = y_stm_allocate_memory(stm, writer, sizeof(int));
    assert(handle != NULL);
    int value = 42;
    assert(y_stm_write(stm, writer, handle, &value, sizeof(value)));
    assert(y_stm_commit_transaction(stm, writer));

    y_stm_transaction_t *reader = y_stm_begin_transaction(stm);
    int read_value = 0;
    assert(y_stm_read(stm, reader, handle, &read_value, sizeof(read_value)));
    assert(read_value == 42);
    assert(y_stm_commit_transaction(stm, reader));

    y_stm_destructor(stm);
}

static void test_read_your_own_write_before_commit(void) {
    y_stm_t *stm = y_stm_constructor();

    y_stm_transaction_t *transaction = y_stm_begin_transaction(stm);
    y_stm_handle_t handle = y_stm_allocate_memory(stm, transaction, sizeof(int));
    int value = 7;
    assert(y_stm_write(stm, transaction, handle, &value, sizeof(value)));

    int read_value = 0;
    assert(y_stm_read(stm, transaction, handle, &read_value, sizeof(read_value)));
    assert(read_value == 7);

    assert(y_stm_commit_transaction(stm, transaction));
    y_stm_destructor(stm);
}

static void test_reading_unwritten_memory_fails(void) {
    y_stm_t *stm = y_stm_constructor();

    y_stm_transaction_t *transaction = y_stm_begin_transaction(stm);
    y_stm_handle_t handle = y_stm_allocate_memory(stm, transaction, sizeof(int));
    int read_value = 0;
    assert(!y_stm_read(stm, transaction, handle, &read_value, sizeof(read_value)));

    y_stm_rollback_transaction(stm, transaction);
    y_stm_destructor(stm);
}

static void test_rollback_discards_uncommitted_write(void) {
    y_stm_t *stm = y_stm_constructor();

    y_stm_transaction_t *setup = y_stm_begin_transaction(stm);
    y_stm_handle_t handle = y_stm_allocate_memory(stm, setup, sizeof(int));
    int original = 1;
    assert(y_stm_write(stm, setup, handle, &original, sizeof(original)));
    assert(y_stm_commit_transaction(stm, setup));

    y_stm_transaction_t *doomed = y_stm_begin_transaction(stm);
    int replacement = 999;
    assert(y_stm_write(stm, doomed, handle, &replacement, sizeof(replacement)));
    y_stm_rollback_transaction(stm, doomed);

    y_stm_transaction_t *reader = y_stm_begin_transaction(stm);
    int read_value = 0;
    assert(y_stm_read(stm, reader, handle, &read_value, sizeof(read_value)));
    assert(read_value == 1);
    assert(y_stm_commit_transaction(stm, reader));

    y_stm_destructor(stm);
}

static void test_release_then_commit_hides_memory_from_later_transactions(void) {
    y_stm_t *stm = y_stm_constructor();

    y_stm_transaction_t *setup = y_stm_begin_transaction(stm);
    y_stm_handle_t handle = y_stm_allocate_memory(stm, setup, sizeof(int));
    int value = 5;
    assert(y_stm_write(stm, setup, handle, &value, sizeof(value)));
    assert(y_stm_commit_transaction(stm, setup));

    y_stm_transaction_t *releaser = y_stm_begin_transaction(stm);
    assert(y_stm_release_memory(stm, releaser, handle));
    assert(y_stm_commit_transaction(stm, releaser));

    y_stm_transaction_t *reader = y_stm_begin_transaction(stm);
    int read_value = 0;
    assert(!y_stm_read(stm, reader, handle, &read_value, sizeof(read_value)));
    assert(!y_stm_write(stm, reader, handle, &value, sizeof(value)));
    y_stm_rollback_transaction(stm, reader);

    y_stm_destructor(stm);
}

static void test_concurrent_writers_first_committer_wins(void) {
    y_stm_t *stm = y_stm_constructor();

    y_stm_transaction_t *setup = y_stm_begin_transaction(stm);
    y_stm_handle_t handle = y_stm_allocate_memory(stm, setup, sizeof(int));
    int initial = 0;
    assert(y_stm_write(stm, setup, handle, &initial, sizeof(initial)));
    assert(y_stm_commit_transaction(stm, setup));

    /* Both transactions start from the same snapshot. */
    y_stm_transaction_t *a = y_stm_begin_transaction(stm);
    y_stm_transaction_t *b = y_stm_begin_transaction(stm);

    int value_a = 111;
    assert(y_stm_write(stm, a, handle, &value_a, sizeof(value_a)));
    assert(y_stm_commit_transaction(stm, a));

    int value_b = 222;
    assert(y_stm_write(stm, b, handle, &value_b, sizeof(value_b)));
    /* b's snapshot is stale now that a has committed a newer version. */
    assert(!y_stm_commit_transaction(stm, b));

    y_stm_transaction_t *reader = y_stm_begin_transaction(stm);
    int read_value = 0;
    assert(y_stm_read(stm, reader, handle, &read_value, sizeof(read_value)));
    assert(read_value == 111);
    assert(y_stm_commit_transaction(stm, reader));

    y_stm_destructor(stm);
}

static void test_stale_read_aborts_commit_even_without_a_write(void) {
    y_stm_t *stm = y_stm_constructor();

    y_stm_transaction_t *setup = y_stm_begin_transaction(stm);
    y_stm_handle_t handle = y_stm_allocate_memory(stm, setup, sizeof(int));
    int initial = 1;
    assert(y_stm_write(stm, setup, handle, &initial, sizeof(initial)));
    assert(y_stm_commit_transaction(stm, setup));

    y_stm_transaction_t *reader = y_stm_begin_transaction(stm);
    int read_value = 0;
    assert(y_stm_read(stm, reader, handle, &read_value, sizeof(read_value)));

    y_stm_transaction_t *writer = y_stm_begin_transaction(stm);
    int updated = 2;
    assert(y_stm_write(stm, writer, handle, &updated, sizeof(updated)));
    assert(y_stm_commit_transaction(stm, writer));

    /* reader's view of `handle` is now stale even though it never wrote. */
    assert(!y_stm_commit_transaction(stm, reader));

    y_stm_destructor(stm);
}

static void test_operations_require_an_open_transaction(void) {
    y_stm_t *stm = y_stm_constructor();

    assert(y_stm_allocate_memory(stm, NULL, sizeof(int)) == NULL);

    y_stm_transaction_t *transaction = y_stm_begin_transaction(stm);
    y_stm_handle_t handle = y_stm_allocate_memory(stm, transaction, sizeof(int));
    int value = 1;
    assert(y_stm_write(stm, transaction, handle, &value, sizeof(value)));
    assert(y_stm_commit_transaction(stm, transaction));

    assert(!y_stm_read(stm, NULL, handle, &value, sizeof(value)));
    assert(!y_stm_write(stm, NULL, handle, &value, sizeof(value)));
    assert(!y_stm_release_memory(stm, NULL, handle));

    y_stm_destructor(stm);
}

static void test_transaction_rejects_the_wrong_stm_instance(void) {
    y_stm_t *stm_one = y_stm_constructor();
    y_stm_t *stm_two = y_stm_constructor();

    y_stm_transaction_t *transaction = y_stm_begin_transaction(stm_one);
    assert(y_stm_allocate_memory(stm_two, transaction, sizeof(int)) == NULL);
    assert(!y_stm_commit_transaction(stm_two, transaction));

    y_stm_rollback_transaction(stm_one, transaction);
    y_stm_destructor(stm_one);
    y_stm_destructor(stm_two);
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
