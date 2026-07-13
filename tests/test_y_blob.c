#include "y/blob/include.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "y/stm/include.h"

static void test_constructor_then_read_round_trips_content(void) {
    y_stm_t *stm = y_stm_constructor();

    y_stm_transaction_t *transaction = y_stm_begin_transaction(stm);
    y_blob_t *blob = y_blob_constructor(stm, transaction, "hello", 5);
    assert(blob != NULL);

    char buffer[5] = {0};
    y_blob_read(blob, transaction, buffer, sizeof(buffer));
    assert(memcmp(buffer, "hello", 5) == 0);
    assert(!y_stm_is_rolled_back(stm, transaction));

    y_blob_destructor(blob, transaction);
    assert(y_stm_commit_transaction(stm, transaction));

    y_stm_destructor(stm);
}

static void test_holds_arbitrary_binary_bytes_including_nuls(void) {
    y_stm_t *stm = y_stm_constructor();

    /* A blob is raw bytes, not a C string: embedded NULs are content, not
     * terminators, and every byte round-trips verbatim. */
    const unsigned char payload[6] = {0x00, 0xff, 'a', 0x00, 0x7f, 0x80};

    y_stm_transaction_t *transaction = y_stm_begin_transaction(stm);
    y_blob_t *blob = y_blob_constructor(stm, transaction, payload, sizeof(payload));

    unsigned char buffer[6] = {0};
    y_blob_read(blob, transaction, buffer, sizeof(buffer));
    assert(memcmp(buffer, payload, sizeof(payload)) == 0);
    assert(!y_stm_is_rolled_back(stm, transaction));

    y_blob_destructor(blob, transaction);
    assert(y_stm_commit_transaction(stm, transaction));

    y_stm_destructor(stm);
}

static void test_get_length_in_bytes_is_the_fixed_length(void) {
    y_stm_t *stm = y_stm_constructor();

    y_stm_transaction_t *transaction = y_stm_begin_transaction(stm);
    y_blob_t *blob = y_blob_constructor(stm, transaction, "abcdefg", 7);

    /* Immutable, so it needs no transaction. */
    size_t length_in_bytes = 0;
    y_blob_get_length_in_bytes(blob, &length_in_bytes);
    assert(length_in_bytes == 7);

    y_blob_destructor(blob, transaction);
    assert(y_stm_commit_transaction(stm, transaction));

    y_stm_destructor(stm);
}

static void test_write_overwrites_in_place(void) {
    y_stm_t *stm = y_stm_constructor();

    y_stm_transaction_t *transaction = y_stm_begin_transaction(stm);
    y_blob_t *blob = y_blob_constructor(stm, transaction, "aaaaa", 5);

    y_blob_write(blob, transaction, "bbbbb", 5);
    char buffer[5] = {0};
    y_blob_read(blob, transaction, buffer, sizeof(buffer));
    assert(memcmp(buffer, "bbbbb", 5) == 0);
    assert(!y_stm_is_rolled_back(stm, transaction));

    y_blob_destructor(blob, transaction);
    assert(y_stm_commit_transaction(stm, transaction));

    y_stm_destructor(stm);
}

static void test_write_with_a_different_length_rolls_back(void) {
    y_stm_t *stm = y_stm_constructor();

    y_stm_transaction_t *setup = y_stm_begin_transaction(stm);
    y_blob_t *blob = y_blob_constructor(stm, setup, "fixed", 5);
    assert(y_stm_commit_transaction(stm, setup));

    /* A blob never resizes: writing a different length is a mismatch, not a
     * grow. */
    y_stm_transaction_t *transaction = y_stm_begin_transaction(stm);
    y_blob_write(blob, transaction, "longer!", 7);
    assert(y_stm_is_rolled_back(stm, transaction));
    y_error_t error = y_error_get(y_stm_get_error(stm, transaction));
    assert(error.code == Y_BLOB_ERROR_LENGTH_MISMATCH);
    assert(!y_stm_commit_transaction(stm, transaction));

    /* Unchanged: the failing write committed nothing. */
    y_stm_transaction_t *reader = y_stm_begin_transaction(stm);
    char buffer[5] = {0};
    y_blob_read(blob, reader, buffer, sizeof(buffer));
    assert(memcmp(buffer, "fixed", 5) == 0);
    y_blob_destructor(blob, reader);
    assert(y_stm_commit_transaction(stm, reader));

    y_stm_destructor(stm);
}

static void test_write_is_visible_to_a_later_transaction(void) {
    y_stm_t *stm = y_stm_constructor();

    y_stm_transaction_t *writer = y_stm_begin_transaction(stm);
    y_blob_t *blob = y_blob_constructor(stm, writer, "first", 5);
    assert(y_stm_commit_transaction(stm, writer));

    y_stm_transaction_t *updater = y_stm_begin_transaction(stm);
    y_blob_write(blob, updater, "secnd", 5);
    assert(y_stm_commit_transaction(stm, updater));

    y_stm_transaction_t *reader = y_stm_begin_transaction(stm);
    char buffer[5] = {0};
    y_blob_read(blob, reader, buffer, sizeof(buffer));
    assert(memcmp(buffer, "secnd", 5) == 0);
    y_blob_destructor(blob, reader);
    assert(y_stm_commit_transaction(stm, reader));

    y_stm_destructor(stm);
}

static void test_concurrent_writers_first_committer_wins(void) {
    y_stm_t *stm = y_stm_constructor();

    y_stm_transaction_t *setup = y_stm_begin_transaction(stm);
    y_blob_t *blob = y_blob_constructor(stm, setup, "base!", 5);
    assert(y_stm_commit_transaction(stm, setup));

    y_stm_transaction_t *a = y_stm_begin_transaction(stm);
    y_stm_transaction_t *b = y_stm_begin_transaction(stm);

    y_blob_write(blob, a, "aaaaa", 5);
    assert(y_stm_commit_transaction(stm, a));

    y_blob_write(blob, b, "bbbbb", 5);
    /* b's write succeeds locally; the conflict only surfaces at commit. */
    assert(!y_stm_is_rolled_back(stm, b));
    assert(!y_stm_commit_transaction(stm, b));

    y_stm_transaction_t *reader = y_stm_begin_transaction(stm);
    char buffer[5] = {0};
    y_blob_read(blob, reader, buffer, sizeof(buffer));
    assert(memcmp(buffer, "aaaaa", 5) == 0);
    y_blob_destructor(blob, reader);
    assert(y_stm_commit_transaction(stm, reader));

    y_stm_destructor(stm);
}

static void test_read_with_wrong_length_rolls_back_the_transaction(void) {
    y_stm_t *stm = y_stm_constructor();

    y_stm_transaction_t *setup = y_stm_begin_transaction(stm);
    y_blob_t *blob = y_blob_constructor(stm, setup, "hello", 5);
    assert(y_stm_commit_transaction(stm, setup));

    y_stm_transaction_t *transaction = y_stm_begin_transaction(stm);
    char buffer[4] = {0};
    y_blob_read(blob, transaction, buffer, sizeof(buffer)); /* wrong length */

    assert(y_stm_is_rolled_back(stm, transaction));
    y_error_t error = y_error_get(y_stm_get_error(stm, transaction));
    assert(error.code == Y_BLOB_ERROR_LENGTH_MISMATCH);
    assert(error.message != NULL);
    assert(!y_stm_commit_transaction(stm, transaction));

    y_stm_transaction_t *cleanup = y_stm_begin_transaction(stm);
    y_blob_destructor(blob, cleanup);
    assert(y_stm_commit_transaction(stm, cleanup));

    y_stm_destructor(stm);
}

static void test_constructor_rejects_null_data_for_nonzero_length(void) {
    y_stm_t *stm = y_stm_constructor();

    y_stm_transaction_t *transaction = y_stm_begin_transaction(stm);
    y_blob_t *blob = y_blob_constructor(stm, transaction, NULL, 3);
    assert(blob == NULL);
    assert(y_stm_is_rolled_back(stm, transaction));
    y_error_t error = y_error_get(y_stm_get_error(stm, transaction));
    assert(error.code == Y_BLOB_ERROR_INVALID_ARGUMENT);
    assert(!y_stm_commit_transaction(stm, transaction));

    y_stm_destructor(stm);
}

static void test_operations_on_a_null_transaction_are_safe_no_ops(void) {
    y_stm_t *stm = y_stm_constructor();

    assert(y_blob_constructor(stm, NULL, "x", 1) == NULL);

    y_stm_transaction_t *transaction = y_stm_begin_transaction(stm);
    y_blob_t *blob = y_blob_constructor(stm, transaction, "x", 1);
    assert(y_stm_commit_transaction(stm, transaction));

    /* None of these have a transaction to roll back or attach an error to;
     * they just do nothing, leaving every output untouched. */
    char buffer[1] = {'?'};
    y_blob_read(blob, NULL, buffer, sizeof(buffer));
    assert(buffer[0] == '?');

    y_blob_write(blob, NULL, "y", 1);

    /* get_length needs no transaction at all and always reports the fixed
     * length. */
    size_t length_in_bytes = 999;
    y_blob_get_length_in_bytes(blob, &length_in_bytes);
    assert(length_in_bytes == 1);

    y_stm_transaction_t *cleanup = y_stm_begin_transaction(stm);
    y_blob_destructor(blob, cleanup);
    assert(y_stm_commit_transaction(stm, cleanup));

    y_stm_destructor(stm);
}

int main(void) {
    test_constructor_then_read_round_trips_content();
    test_holds_arbitrary_binary_bytes_including_nuls();
    test_get_length_in_bytes_is_the_fixed_length();
    test_write_overwrites_in_place();
    test_write_with_a_different_length_rolls_back();
    test_write_is_visible_to_a_later_transaction();
    test_concurrent_writers_first_committer_wins();
    test_read_with_wrong_length_rolls_back_the_transaction();
    test_constructor_rejects_null_data_for_nonzero_length();
    test_operations_on_a_null_transaction_are_safe_no_ops();

    printf("All y_blob tests passed.\n");
    return 0;
}
