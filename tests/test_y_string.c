#include "y/string/include.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "y/stm/alloc/include.h"
#include "y/stm/include.h"

static void test_constructor_then_read_round_trips_content(void) {
    y_stm_t *stm = y_stm_constructor();

    y_stm_transaction_t *transaction = y_stm_begin_transaction(stm);
    y_string_t *string = y_string_constructor(stm, transaction, "hello", 5);
    assert(string != NULL);
    size_t length_in_bytes = 0;
    assert(y_string_get_length_in_bytes(string, transaction, &length_in_bytes));
    assert(length_in_bytes == 5);

    char buffer[5] = {0};
    assert(y_string_read(string, transaction, buffer, sizeof(buffer)));
    assert(memcmp(buffer, "hello", 5) == 0);

    assert(y_string_destructor(string, transaction));
    assert(y_stm_commit_transaction(stm, transaction));

    y_stm_destructor(stm);
}

static void test_write_replaces_content_and_can_change_length(void) {
    y_stm_t *stm = y_stm_constructor();

    y_stm_transaction_t *transaction = y_stm_begin_transaction(stm);
    y_string_t *string = y_string_constructor(stm, transaction, "hi", 2);

    assert(y_string_write(string, transaction, "goodbye", 7));
    size_t length_in_bytes = 0;
    assert(y_string_get_length_in_bytes(string, transaction, &length_in_bytes));
    assert(length_in_bytes == 7);

    char buffer[7] = {0};
    assert(y_string_read(string, transaction, buffer, sizeof(buffer)));
    assert(memcmp(buffer, "goodbye", 7) == 0);

    assert(y_string_destructor(string, transaction));
    assert(y_stm_commit_transaction(stm, transaction));

    y_stm_destructor(stm);
}

static void test_append_grows_content(void) {
    y_stm_t *stm = y_stm_constructor();

    y_stm_transaction_t *transaction = y_stm_begin_transaction(stm);
    y_string_t *string = y_string_constructor(stm, transaction, "foo", 3);

    assert(y_string_append(string, transaction, "bar", 3));
    size_t length_in_bytes = 0;
    assert(y_string_get_length_in_bytes(string, transaction, &length_in_bytes));
    assert(length_in_bytes == 6);

    char buffer[6] = {0};
    assert(y_string_read(string, transaction, buffer, sizeof(buffer)));
    assert(memcmp(buffer, "foobar", 6) == 0);

    assert(y_string_destructor(string, transaction));
    assert(y_stm_commit_transaction(stm, transaction));

    y_stm_destructor(stm);
}

static void test_compare_and_is_equal(void) {
    y_stm_t *stm = y_stm_constructor();

    y_stm_transaction_t *transaction = y_stm_begin_transaction(stm);
    y_string_t *apple = y_string_constructor(stm, transaction, "apple", 5);
    y_string_t *applesauce = y_string_constructor(stm, transaction, "applesauce", 10);
    y_string_t *apple_again = y_string_constructor(stm, transaction, "apple", 5);

    int comparison = 0;
    assert(y_string_compare(apple, applesauce, transaction, &comparison));
    assert(comparison < 0);
    assert(y_string_compare(applesauce, apple, transaction, &comparison));
    assert(comparison > 0);

    bool is_equal = false;
    assert(y_string_is_equal(apple, apple_again, transaction, &is_equal));
    assert(is_equal);
    assert(y_string_is_equal(apple, applesauce, transaction, &is_equal));
    assert(!is_equal);

    assert(y_string_destructor(apple, transaction));
    assert(y_string_destructor(applesauce, transaction));
    assert(y_string_destructor(apple_again, transaction));
    assert(y_stm_commit_transaction(stm, transaction));

    y_stm_destructor(stm);
}

static void test_as_c_string_is_nul_terminated(void) {
    y_stm_t *stm = y_stm_constructor();

    y_stm_transaction_t *transaction = y_stm_begin_transaction(stm);
    y_string_t *string = y_string_constructor(stm, transaction, "hello", 5);

    char *c_string = y_string_as_c_string(string, transaction);
    assert(c_string != NULL);
    assert(strcmp(c_string, "hello") == 0);
    y_stm_alloc_free(c_string);

    assert(y_string_destructor(string, transaction));
    assert(y_stm_commit_transaction(stm, transaction));

    y_stm_destructor(stm);
}

static void test_write_is_visible_to_a_later_transaction(void) {
    y_stm_t *stm = y_stm_constructor();

    y_stm_transaction_t *writer = y_stm_begin_transaction(stm);
    y_string_t *string = y_string_constructor(stm, writer, "first", 5);
    assert(y_stm_commit_transaction(stm, writer));

    y_stm_transaction_t *updater = y_stm_begin_transaction(stm);
    assert(y_string_write(string, updater, "second try", 10));
    assert(y_stm_commit_transaction(stm, updater));

    y_stm_transaction_t *reader = y_stm_begin_transaction(stm);
    size_t length_in_bytes = 0;
    assert(y_string_get_length_in_bytes(string, reader, &length_in_bytes));
    assert(length_in_bytes == 10);
    char buffer[10] = {0};
    assert(y_string_read(string, reader, buffer, sizeof(buffer)));
    assert(memcmp(buffer, "second try", 10) == 0);
    assert(y_string_destructor(string, reader));
    assert(y_stm_commit_transaction(stm, reader));

    y_stm_destructor(stm);
}

static void test_resize_is_isolated_from_a_concurrent_transaction(void) {
    y_stm_t *stm = y_stm_constructor();

    y_stm_transaction_t *setup = y_stm_begin_transaction(stm);
    y_string_t *string = y_string_constructor(stm, setup, "base", 4);
    assert(y_stm_commit_transaction(stm, setup));

    /* `concurrent` takes its snapshot before `resizer` commits a length
     * change, so it must keep seeing the original 4-byte content even
     * while `resizer` is (and after it has) resized the string. */
    y_stm_transaction_t *concurrent = y_stm_begin_transaction(stm);

    y_stm_transaction_t *resizer = y_stm_begin_transaction(stm);
    assert(y_string_write(string, resizer, "much longer content", 20));
    assert(y_stm_commit_transaction(stm, resizer));

    size_t length_in_bytes = 0;
    assert(y_string_get_length_in_bytes(string, concurrent, &length_in_bytes));
    assert(length_in_bytes == 4);
    char buffer[4] = {0};
    assert(y_string_read(string, concurrent, buffer, sizeof(buffer)));
    assert(memcmp(buffer, "base", 4) == 0);
    y_stm_rollback_transaction(stm, concurrent);

    y_stm_transaction_t *cleanup = y_stm_begin_transaction(stm);
    assert(y_string_destructor(string, cleanup));
    assert(y_stm_commit_transaction(stm, cleanup));

    y_stm_destructor(stm);
}

static void test_concurrent_writers_first_committer_wins(void) {
    y_stm_t *stm = y_stm_constructor();

    y_stm_transaction_t *setup = y_stm_begin_transaction(stm);
    y_string_t *string = y_string_constructor(stm, setup, "base", 4);
    assert(y_stm_commit_transaction(stm, setup));

    y_stm_transaction_t *a = y_stm_begin_transaction(stm);
    y_stm_transaction_t *b = y_stm_begin_transaction(stm);

    assert(y_string_write(string, a, "winner", 6));
    assert(y_stm_commit_transaction(stm, a));

    assert(y_string_write(string, b, "loser!", 6));
    assert(!y_stm_commit_transaction(stm, b));

    y_stm_transaction_t *reader = y_stm_begin_transaction(stm);
    char buffer[6] = {0};
    assert(y_string_read(string, reader, buffer, sizeof(buffer)));
    assert(memcmp(buffer, "winner", 6) == 0);
    assert(y_string_destructor(string, reader));
    assert(y_stm_commit_transaction(stm, reader));

    y_stm_destructor(stm);
}

static void test_operations_require_an_open_transaction(void) {
    y_stm_t *stm = y_stm_constructor();

    assert(y_string_constructor(stm, NULL, "x", 1) == NULL);

    y_stm_transaction_t *transaction = y_stm_begin_transaction(stm);
    y_string_t *string = y_string_constructor(stm, transaction, "x", 1);
    assert(y_stm_commit_transaction(stm, transaction));

    char buffer[1] = {0};
    assert(!y_string_read(string, NULL, buffer, sizeof(buffer)));
    assert(!y_string_write(string, NULL, "y", 1));
    assert(!y_string_append(string, NULL, "y", 1));
    assert(!y_string_destructor(string, NULL));

    y_stm_transaction_t *cleanup = y_stm_begin_transaction(stm);
    assert(y_string_destructor(string, cleanup));
    assert(y_stm_commit_transaction(stm, cleanup));

    y_stm_destructor(stm);
}

int main(void) {
    test_constructor_then_read_round_trips_content();
    test_write_replaces_content_and_can_change_length();
    test_append_grows_content();
    test_compare_and_is_equal();
    test_as_c_string_is_nul_terminated();
    test_write_is_visible_to_a_later_transaction();
    test_resize_is_isolated_from_a_concurrent_transaction();
    test_concurrent_writers_first_committer_wins();
    test_operations_require_an_open_transaction();

    printf("All y_string tests passed.\n");
    return 0;
}
