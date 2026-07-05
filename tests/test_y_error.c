#include "y/error/include.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_set_then_get_round_trips(void) {
    y_error_t error = {0};
    y_error_set(&error, 42, "something went wrong");

    y_error_t value = y_error_get(&error);
    assert(value.code == 42);
    assert(strcmp(value.message, "something went wrong") == 0);
}

static void test_set_overwrites_a_previous_value(void) {
    y_error_t error = {0};
    y_error_set(&error, 1, "first");
    y_error_set(&error, 2, "second");

    y_error_t value = y_error_get(&error);
    assert(value.code == 2);
    assert(strcmp(value.message, "second") == 0);
}

int main(void) {
    test_set_then_get_round_trips();
    test_set_overwrites_a_previous_value();

    printf("All y_error tests passed.\n");
    return 0;
}
