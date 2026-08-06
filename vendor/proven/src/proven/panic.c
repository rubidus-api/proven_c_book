#include "proven/panic.h"
#include <stdatomic.h>

/*
 * The panic handler is dispatched through a function pointer rather than an
 * overridable weak symbol. The weak-symbol approach linked on ELF but not on
 * PE/COFF (mingw-w64): a weak function definition in a separate object failed
 * to satisfy references, breaking every Windows link. The registration model
 * below is portable across ELF and PE and remains freestanding-safe (the
 * pointer has a constant static initializer; no runtime startup is required).
 */

static void proven_panic_default(const char *msg) {
    (void)msg;
#if defined(__GNUC__) || defined(__clang__)
    __builtin_trap();
#else
    while (1) {}
#endif
}

/*
 * Atomic because it is read on the *_or_panic allocator paths, which run on every
 * thread, and written by proven_set_panic_handler, which a program may call while those
 * threads are already running. A plain function pointer made that a data race - TSan
 * says so - and a torn or stale read here means the wrong handler, or none, at the exact
 * moment the program has decided it cannot continue. Relaxed ordering is enough: the
 * handler pointer is the only thing being published, and a caller that installs a handler
 * after other threads may already be panicking has a race in its own design, not in ours.
 *
 * _Atomic on a function pointer is lock-free on every target this library builds for, and
 * the initialiser is still a constant, so nothing about the freestanding build changes.
 */
static _Atomic(proven_panic_handler_t) g_panic_handler = proven_panic_default;

void proven_set_panic_handler(proven_panic_handler_t handler) {
    atomic_store_explicit(&g_panic_handler, handler ? handler : proven_panic_default, memory_order_relaxed);
}

void proven_panic(const char *msg) {
    proven_panic_handler_t h = atomic_load_explicit(&g_panic_handler, memory_order_relaxed);
    h(msg);
}
