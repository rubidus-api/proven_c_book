#ifndef PROVEN_PANIC_H
#define PROVEN_PANIC_H

/**
 * @file panic.h
 * @brief Global panic handler for deterministic failure paths.
 */

/**
 * @brief Type of a panic handler callback.
 * @param msg A descriptive message of the failure.
 */
typedef void (*proven_panic_handler_t)(const char *msg);

/**
 * @brief Raise a panic.
 *
 * Called internally on terminal failures (e.g. OOM in the `*_or_panic`
 * allocator variants). It dispatches to the currently installed handler.
 *
 * The default handler does not return (it executes `__builtin_trap()`).
 * Production handlers should not return. Test handlers may return only when
 * intentionally verifying panic paths; if a handler returns, the validity of a
 * `*_or_panic` result is not guaranteed.
 *
 * @param msg A descriptive message of the failure.
 */
void proven_panic(const char *msg);

/**
 * @brief Install a custom panic handler, replacing the default.
 *
 * Passing NULL restores the default trapping handler. This replaces the former
 * weakly-linked `proven_panic_handler` override symbol, which did not link on
 * PE/COFF (Windows / mingw-w64) toolchains because a weak function definition
 * in a separate object does not satisfy references there.
 *
 * @param handler The handler to install, or NULL to restore the default.
 */
void proven_set_panic_handler(proven_panic_handler_t handler);

#endif // PROVEN_PANIC_H
