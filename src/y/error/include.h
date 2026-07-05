#ifndef Y_ERROR_INCLUDE_H
#define Y_ERROR_INCLUDE_H

/*
 * y_error_t is a small, transparent value type carrying an error code and
 * a diagnostic message -- the payload attached to a y_stm transaction when
 * it's rolled back due to a failure (see y_stm_get_error). Its fields are
 * declared here (rather than kept opaque like y_stm_t) because it needs to
 * be embeddable by value inside other structs; even so, code should go
 * through y_error_set/y_error_get rather than touching the fields
 * directly, the same as any other class in this project.
 *
 * `message` is expected to have static storage duration (a string
 * literal) -- y_error_set does not copy or take ownership of it, and
 * y_error is not backed by a class with its own constructor/destructor
 * managing that memory.
 */
typedef struct y_error {
    int code;
    const char *message;
} y_error_t;

void y_error_set(y_error_t *self, int code, const char *message);
y_error_t y_error_get(const y_error_t *self);

#endif
