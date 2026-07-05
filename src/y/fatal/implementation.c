#include "y/fatal/include.h"

#include <stdio.h>
#include <stdlib.h>

_Noreturn void y_fatal_terminate(const char *message) {
    fprintf(stderr, "fatal: %s\n", message);
    _Exit(EXIT_FAILURE);
}
