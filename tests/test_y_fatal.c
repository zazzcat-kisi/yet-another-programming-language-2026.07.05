#include "y/fatal/include.h"

/* y_fatal_terminate terminates the process immediately, so there's nothing
 * to assert on within this process; this executable's only job is to call
 * it and let CTest observe that the process aborts abnormally (registered
 * with WILL_FAIL in tests/CMakeLists.txt). */
int main(void) {
    y_fatal_terminate("deliberate test termination");
    return 0;
}
