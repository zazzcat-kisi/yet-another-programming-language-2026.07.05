#include "y/error/include.h"

void y_error_set(y_error_t *self, int code, const char *message) {
    self->code = code;
    self->message = message;
}

y_error_t y_error_get(const y_error_t *self) {
    return *self;
}
