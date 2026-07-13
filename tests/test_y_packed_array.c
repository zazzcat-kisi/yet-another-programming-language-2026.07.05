#include "y/packed_array/include.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "y/stm/include.h"

static void test_constructor_get_set_round_trips_elements(void) {
    y_stm_t *stm = y_stm_constructor();

    y_stm_transaction_t *transaction = y_stm_begin_transaction(stm);
    y_packed_array_t *array = y_packed_array_constructor(stm, transaction, sizeof(uint32_t), 4);
    assert(array != NULL);

    for (uint32_t i = 0; i < 4; i++) {
        uint32_t value = i * 1000 + 7;
        y_packed_array_set(array, transaction, i, &value, sizeof(value));
    }
    for (uint32_t i = 0; i < 4; i++) {
        uint32_t value = 0;
        y_packed_array_get(array, transaction, i, &value, sizeof(value));
        assert(value == i * 1000 + 7);
    }
    assert(!y_stm_is_rolled_back(stm, transaction));

    y_packed_array_destructor(array, transaction);
    assert(y_stm_commit_transaction(stm, transaction));

    y_stm_destructor(stm);
}

static void test_get_count_and_element_length(void) {
    y_stm_t *stm = y_stm_constructor();

    y_stm_transaction_t *transaction = y_stm_begin_transaction(stm);
    y_packed_array_t *array = y_packed_array_constructor(stm, transaction, 8, 3);

    size_t count = 0;
    y_packed_array_get_count(array, transaction, &count);
    assert(count == 3);
    /* The element length is immutable, so it needs no transaction. */
    size_t element_length_in_bytes = 0;
    y_packed_array_get_element_length_in_bytes(array, &element_length_in_bytes);
    assert(element_length_in_bytes == 8);

    y_packed_array_destructor(array, transaction);
    assert(y_stm_commit_transaction(stm, transaction));

    y_stm_destructor(stm);
}

static void test_elements_start_zero_initialized(void) {
    y_stm_t *stm = y_stm_constructor();

    y_stm_transaction_t *transaction = y_stm_begin_transaction(stm);
    y_packed_array_t *array = y_packed_array_constructor(stm, transaction, sizeof(uint32_t), 5);

    for (uint32_t i = 0; i < 5; i++) {
        uint32_t value = 0xdeadbeef;
        y_packed_array_get(array, transaction, i, &value, sizeof(value));
        assert(value == 0);
    }
    assert(!y_stm_is_rolled_back(stm, transaction));

    y_packed_array_destructor(array, transaction);
    assert(y_stm_commit_transaction(stm, transaction));

    y_stm_destructor(stm);
}

static void test_set_preserves_other_elements(void) {
    y_stm_t *stm = y_stm_constructor();

    y_stm_transaction_t *transaction = y_stm_begin_transaction(stm);
    y_packed_array_t *array = y_packed_array_constructor(stm, transaction, sizeof(uint32_t), 4);

    uint32_t seed[4] = {10, 20, 30, 40};
    for (uint32_t i = 0; i < 4; i++) {
        y_packed_array_set(array, transaction, i, &seed[i], sizeof(seed[i]));
    }

    uint32_t replacement = 999;
    y_packed_array_set(array, transaction, 2, &replacement, sizeof(replacement));

    uint32_t expected[4] = {10, 20, 999, 40};
    for (uint32_t i = 0; i < 4; i++) {
        uint32_t value = 0;
        y_packed_array_get(array, transaction, i, &value, sizeof(value));
        assert(value == expected[i]);
    }
    assert(!y_stm_is_rolled_back(stm, transaction));

    y_packed_array_destructor(array, transaction);
    assert(y_stm_commit_transaction(stm, transaction));

    y_stm_destructor(stm);
}

static void test_holds_arbitrary_binary_elements(void) {
    y_stm_t *stm = y_stm_constructor();

    y_stm_transaction_t *transaction = y_stm_begin_transaction(stm);
    y_packed_array_t *array = y_packed_array_constructor(stm, transaction, 3, 2);

    const unsigned char e0[3] = {0x00, 0xff, 0x00};
    const unsigned char e1[3] = {0x80, 0x00, 0x7f};
    y_packed_array_set(array, transaction, 0, e0, sizeof(e0));
    y_packed_array_set(array, transaction, 1, e1, sizeof(e1));

    unsigned char got[3] = {0};
    y_packed_array_get(array, transaction, 0, got, sizeof(got));
    assert(memcmp(got, e0, sizeof(e0)) == 0);
    y_packed_array_get(array, transaction, 1, got, sizeof(got));
    assert(memcmp(got, e1, sizeof(e1)) == 0);
    assert(!y_stm_is_rolled_back(stm, transaction));

    y_packed_array_destructor(array, transaction);
    assert(y_stm_commit_transaction(stm, transaction));

    y_stm_destructor(stm);
}

static void test_resize_grows_with_zero_initialized_elements(void) {
    y_stm_t *stm = y_stm_constructor();

    y_stm_transaction_t *transaction = y_stm_begin_transaction(stm);
    y_packed_array_t *array = y_packed_array_constructor(stm, transaction, sizeof(uint32_t), 2);
    uint32_t a = 10, b = 20;
    y_packed_array_set(array, transaction, 0, &a, sizeof(a));
    y_packed_array_set(array, transaction, 1, &b, sizeof(b));

    y_packed_array_resize(array, transaction, 4);
    size_t count = 0;
    y_packed_array_get_count(array, transaction, &count);
    assert(count == 4);

    /* Original elements survive; the new ones are zero. */
    uint32_t value = 0xffffffff;
    y_packed_array_get(array, transaction, 0, &value, sizeof(value));
    assert(value == 10);
    y_packed_array_get(array, transaction, 1, &value, sizeof(value));
    assert(value == 20);
    y_packed_array_get(array, transaction, 2, &value, sizeof(value));
    assert(value == 0);
    y_packed_array_get(array, transaction, 3, &value, sizeof(value));
    assert(value == 0);

    /* And the newly grown space is writable. */
    uint32_t c = 30;
    y_packed_array_set(array, transaction, 3, &c, sizeof(c));
    y_packed_array_get(array, transaction, 3, &value, sizeof(value));
    assert(value == 30);
    assert(!y_stm_is_rolled_back(stm, transaction));

    y_packed_array_destructor(array, transaction);
    assert(y_stm_commit_transaction(stm, transaction));

    y_stm_destructor(stm);
}

static void test_resize_shrinks_dropping_trailing_elements(void) {
    y_stm_t *stm = y_stm_constructor();

    y_stm_transaction_t *setup = y_stm_begin_transaction(stm);
    y_packed_array_t *array = y_packed_array_constructor(stm, setup, sizeof(uint32_t), 4);
    uint32_t seed[4] = {10, 20, 30, 40};
    for (uint32_t i = 0; i < 4; i++) {
        y_packed_array_set(array, setup, i, &seed[i], sizeof(seed[i]));
    }
    y_packed_array_resize(array, setup, 2);
    size_t count = 0;
    y_packed_array_get_count(array, setup, &count);
    assert(count == 2);
    uint32_t value = 0;
    y_packed_array_get(array, setup, 0, &value, sizeof(value));
    assert(value == 10);
    y_packed_array_get(array, setup, 1, &value, sizeof(value));
    assert(value == 20);
    assert(y_stm_commit_transaction(stm, setup));

    /* The dropped elements are now out of range. */
    y_stm_transaction_t *transaction = y_stm_begin_transaction(stm);
    y_packed_array_get(array, transaction, 2, &value, sizeof(value));
    assert(y_stm_is_rolled_back(stm, transaction));
    y_error_t error = y_error_get(y_stm_get_error(stm, transaction));
    assert(error.code == Y_PACKED_ARRAY_ERROR_OUT_OF_RANGE);
    assert(!y_stm_commit_transaction(stm, transaction));

    y_stm_transaction_t *cleanup = y_stm_begin_transaction(stm);
    y_packed_array_destructor(array, cleanup);
    assert(y_stm_commit_transaction(stm, cleanup));

    y_stm_destructor(stm);
}

static void test_get_out_of_range_rolls_back(void) {
    y_stm_t *stm = y_stm_constructor();

    y_stm_transaction_t *setup = y_stm_begin_transaction(stm);
    y_packed_array_t *array = y_packed_array_constructor(stm, setup, sizeof(uint32_t), 3);
    assert(y_stm_commit_transaction(stm, setup));

    y_stm_transaction_t *transaction = y_stm_begin_transaction(stm);
    uint32_t value = 0;
    y_packed_array_get(array, transaction, 3, &value, sizeof(value)); /* index == count */
    assert(y_stm_is_rolled_back(stm, transaction));
    y_error_t error = y_error_get(y_stm_get_error(stm, transaction));
    assert(error.code == Y_PACKED_ARRAY_ERROR_OUT_OF_RANGE);
    assert(!y_stm_commit_transaction(stm, transaction));

    y_stm_transaction_t *cleanup = y_stm_begin_transaction(stm);
    y_packed_array_destructor(array, cleanup);
    assert(y_stm_commit_transaction(stm, cleanup));

    y_stm_destructor(stm);
}

static void test_set_out_of_range_rolls_back(void) {
    y_stm_t *stm = y_stm_constructor();

    y_stm_transaction_t *setup = y_stm_begin_transaction(stm);
    y_packed_array_t *array = y_packed_array_constructor(stm, setup, sizeof(uint32_t), 3);
    assert(y_stm_commit_transaction(stm, setup));

    y_stm_transaction_t *transaction = y_stm_begin_transaction(stm);
    uint32_t value = 5;
    y_packed_array_set(array, transaction, 100, &value, sizeof(value));
    assert(y_stm_is_rolled_back(stm, transaction));
    y_error_t error = y_error_get(y_stm_get_error(stm, transaction));
    assert(error.code == Y_PACKED_ARRAY_ERROR_OUT_OF_RANGE);
    assert(!y_stm_commit_transaction(stm, transaction));

    y_stm_transaction_t *cleanup = y_stm_begin_transaction(stm);
    y_packed_array_destructor(array, cleanup);
    assert(y_stm_commit_transaction(stm, cleanup));

    y_stm_destructor(stm);
}

static void test_wrong_element_length_rolls_back(void) {
    y_stm_t *stm = y_stm_constructor();

    y_stm_transaction_t *setup = y_stm_begin_transaction(stm);
    y_packed_array_t *array = y_packed_array_constructor(stm, setup, sizeof(uint32_t), 3);
    assert(y_stm_commit_transaction(stm, setup));

    y_stm_transaction_t *transaction = y_stm_begin_transaction(stm);
    uint16_t too_small = 1;
    y_packed_array_set(array, transaction, 0, &too_small, sizeof(too_small));
    assert(y_stm_is_rolled_back(stm, transaction));
    y_error_t error = y_error_get(y_stm_get_error(stm, transaction));
    assert(error.code == Y_PACKED_ARRAY_ERROR_LENGTH_MISMATCH);
    assert(!y_stm_commit_transaction(stm, transaction));

    y_stm_transaction_t *cleanup = y_stm_begin_transaction(stm);
    y_packed_array_destructor(array, cleanup);
    assert(y_stm_commit_transaction(stm, cleanup));

    y_stm_destructor(stm);
}

static void test_set_is_isolated_from_a_concurrent_transaction(void) {
    y_stm_t *stm = y_stm_constructor();

    y_stm_transaction_t *setup = y_stm_begin_transaction(stm);
    y_packed_array_t *array = y_packed_array_constructor(stm, setup, sizeof(uint32_t), 2);
    uint32_t a = 100, b = 200;
    y_packed_array_set(array, setup, 0, &a, sizeof(a));
    y_packed_array_set(array, setup, 1, &b, sizeof(b));
    assert(y_stm_commit_transaction(stm, setup));

    /* `concurrent` snapshots before `writer` commits, so it must keep seeing
     * the original element and count even after `writer` overwrites and
     * grows the array. */
    y_stm_transaction_t *concurrent = y_stm_begin_transaction(stm);

    y_stm_transaction_t *writer = y_stm_begin_transaction(stm);
    uint32_t replacement = 999;
    y_packed_array_set(array, writer, 0, &replacement, sizeof(replacement));
    y_packed_array_resize(array, writer, 5);
    assert(y_stm_commit_transaction(stm, writer));

    size_t count = 0;
    y_packed_array_get_count(array, concurrent, &count);
    assert(count == 2);
    uint32_t seen = 0;
    y_packed_array_get(array, concurrent, 0, &seen, sizeof(seen));
    assert(seen == 100);
    assert(!y_stm_is_rolled_back(stm, concurrent));
    y_stm_rollback_transaction(stm, concurrent);

    y_stm_transaction_t *cleanup = y_stm_begin_transaction(stm);
    y_packed_array_destructor(array, cleanup);
    assert(y_stm_commit_transaction(stm, cleanup));

    y_stm_destructor(stm);
}

static void test_concurrent_setters_conflict_on_the_packed_slot(void) {
    y_stm_t *stm = y_stm_constructor();

    y_stm_transaction_t *setup = y_stm_begin_transaction(stm);
    y_packed_array_t *array = y_packed_array_constructor(stm, setup, sizeof(uint32_t), 4);
    assert(y_stm_commit_transaction(stm, setup));

    y_stm_transaction_t *a = y_stm_begin_transaction(stm);
    y_stm_transaction_t *b = y_stm_begin_transaction(stm);

    /* Different indices, but the whole array is one packed slot, so the two
     * writers still conflict under first-committer-wins. */
    uint32_t va = 1;
    y_packed_array_set(array, a, 0, &va, sizeof(va));
    assert(y_stm_commit_transaction(stm, a));

    uint32_t vb = 2;
    y_packed_array_set(array, b, 3, &vb, sizeof(vb));
    assert(!y_stm_is_rolled_back(stm, b));
    assert(!y_stm_commit_transaction(stm, b));

    y_stm_transaction_t *reader = y_stm_begin_transaction(stm);
    uint32_t seen0 = 0, seen3 = 0;
    y_packed_array_get(array, reader, 0, &seen0, sizeof(seen0));
    y_packed_array_get(array, reader, 3, &seen3, sizeof(seen3));
    assert(seen0 == 1); /* a's write */
    assert(seen3 == 0); /* b never committed */
    y_packed_array_destructor(array, reader);
    assert(y_stm_commit_transaction(stm, reader));

    y_stm_destructor(stm);
}

static void test_constructor_rejects_zero_element_length(void) {
    y_stm_t *stm = y_stm_constructor();

    y_stm_transaction_t *transaction = y_stm_begin_transaction(stm);
    y_packed_array_t *array = y_packed_array_constructor(stm, transaction, 0, 4);
    assert(array == NULL);
    assert(y_stm_is_rolled_back(stm, transaction));
    y_error_t error = y_error_get(y_stm_get_error(stm, transaction));
    assert(error.code == Y_PACKED_ARRAY_ERROR_INVALID_ARGUMENT);
    assert(!y_stm_commit_transaction(stm, transaction));

    y_stm_destructor(stm);
}

static void test_constructor_rejects_overflowing_size(void) {
    y_stm_t *stm = y_stm_constructor();

    y_stm_transaction_t *transaction = y_stm_begin_transaction(stm);
    y_packed_array_t *array = y_packed_array_constructor(stm, transaction, SIZE_MAX, 2);
    assert(array == NULL);
    assert(y_stm_is_rolled_back(stm, transaction));
    y_error_t error = y_error_get(y_stm_get_error(stm, transaction));
    assert(error.code == Y_PACKED_ARRAY_ERROR_INVALID_ARGUMENT);
    assert(!y_stm_commit_transaction(stm, transaction));

    y_stm_destructor(stm);
}

static void test_zero_count_array_can_grow(void) {
    y_stm_t *stm = y_stm_constructor();

    y_stm_transaction_t *transaction = y_stm_begin_transaction(stm);
    y_packed_array_t *array = y_packed_array_constructor(stm, transaction, sizeof(uint32_t), 0);
    assert(array != NULL);
    size_t count = 123;
    y_packed_array_get_count(array, transaction, &count);
    assert(count == 0);

    /* An empty array is not a dead end: it can be grown. */
    y_packed_array_resize(array, transaction, 3);
    y_packed_array_get_count(array, transaction, &count);
    assert(count == 3);
    uint32_t value = 7;
    y_packed_array_set(array, transaction, 2, &value, sizeof(value));
    uint32_t seen = 0;
    y_packed_array_get(array, transaction, 2, &seen, sizeof(seen));
    assert(seen == 7);
    assert(!y_stm_is_rolled_back(stm, transaction));

    y_packed_array_destructor(array, transaction);
    assert(y_stm_commit_transaction(stm, transaction));

    y_stm_destructor(stm);
}

static void test_operations_on_a_null_transaction_are_safe_no_ops(void) {
    y_stm_t *stm = y_stm_constructor();

    assert(y_packed_array_constructor(stm, NULL, sizeof(uint32_t), 4) == NULL);

    y_stm_transaction_t *transaction = y_stm_begin_transaction(stm);
    y_packed_array_t *array = y_packed_array_constructor(stm, transaction, sizeof(uint32_t), 4);
    uint32_t value = 42;
    y_packed_array_set(array, transaction, 0, &value, sizeof(value));
    assert(y_stm_commit_transaction(stm, transaction));

    /* No transaction to roll back or attach an error to: outputs untouched. */
    uint32_t out = 0xabcd;
    y_packed_array_get(array, NULL, 0, &out, sizeof(out));
    assert(out == 0xabcd);

    uint32_t ignored = 7;
    y_packed_array_set(array, NULL, 0, &ignored, sizeof(ignored));
    y_packed_array_resize(array, NULL, 99);

    size_t count = 555;
    y_packed_array_get_count(array, NULL, &count);
    assert(count == 555);

    /* The element length is immutable and always available. */
    size_t element_length_in_bytes = 0;
    y_packed_array_get_element_length_in_bytes(array, &element_length_in_bytes);
    assert(element_length_in_bytes == sizeof(uint32_t));

    /* Nothing above changed the array: still 4 elements, [0] == 42. */
    y_stm_transaction_t *reader = y_stm_begin_transaction(stm);
    y_packed_array_get_count(array, reader, &count);
    assert(count == 4);
    uint32_t seen = 0;
    y_packed_array_get(array, reader, 0, &seen, sizeof(seen));
    assert(seen == 42);
    assert(y_stm_commit_transaction(stm, reader));

    y_stm_transaction_t *cleanup = y_stm_begin_transaction(stm);
    y_packed_array_destructor(array, cleanup);
    assert(y_stm_commit_transaction(stm, cleanup));

    y_stm_destructor(stm);
}

int main(void) {
    test_constructor_get_set_round_trips_elements();
    test_get_count_and_element_length();
    test_elements_start_zero_initialized();
    test_set_preserves_other_elements();
    test_holds_arbitrary_binary_elements();
    test_resize_grows_with_zero_initialized_elements();
    test_resize_shrinks_dropping_trailing_elements();
    test_get_out_of_range_rolls_back();
    test_set_out_of_range_rolls_back();
    test_wrong_element_length_rolls_back();
    test_set_is_isolated_from_a_concurrent_transaction();
    test_concurrent_setters_conflict_on_the_packed_slot();
    test_constructor_rejects_zero_element_length();
    test_constructor_rejects_overflowing_size();
    test_zero_count_array_can_grow();
    test_operations_on_a_null_transaction_are_safe_no_ops();

    printf("All y_packed_array tests passed.\n");
    return 0;
}
