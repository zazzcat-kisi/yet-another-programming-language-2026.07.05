#ifndef Y_FATAL_INCLUDE_H
#define Y_FATAL_INCLUDE_H

/*
 * y_fatal is for conditions this project cannot recover from: print a
 * diagnostic and terminate the process immediately -- no cleanup, no
 * atexit handlers, no unwinding. There is no state object here (see
 * CLAUDE.md's root-namespace rules) -- it is a purely functional utility
 * namespace, the same shape as y_stm_alloc/y_stm_mem.
 *
 * Callers reaching for y_fatal_terminate have already decided recovery
 * isn't possible (e.g. y_stm_alloc's malloc/calloc wrappers on
 * out-of-memory) -- there is deliberately no return value to check.
 */
_Noreturn void y_fatal_terminate(const char *message);

#endif
