#ifndef PROVEN_ALLOC_CHECK_H
#define PROVEN_ALLOC_CHECK_H

#include "proven/types.h"
#include "proven/allocator.h"

/**
 * @file alloc_check.h
 * @brief An allocator that knows which blocks are its own - for tests and bug hunts.
 *
 * Every owner in this library - a string, an array, a map - is freed through an allocator the
 * CALLER passes, and nothing checks that it is the allocator the memory came from. Freeing an
 * arena block through the heap, freeing twice, or growing a block with the wrong old size does
 * not fail where it happens: it corrupts the allocator, and the program breaks later, somewhere
 * else (B-023, B-040).
 *
 * This wrapper goes in front of ANY allocator and keeps a record of the blocks it has handed out,
 * in memory you supply. A free or realloc of a block that is not in the record - someone else's
 * block, or one already freed - is refused and reported through proven_panic at the call that
 * made the mistake. So is a realloc whose old size or alignment is not the one the block was
 * allocated with. Put every allocator a test uses behind one, and the wrong-allocator bug is
 * caught the moment it happens.
 *
 * **It is a checking tool, not a production allocator.** Every free and realloc searches the
 * record (linear in the number of live blocks). Where speed matters, leave it off:
 *
 *   - proven_alloc_check_wrap()  always wraps and always checks;
 *   - proven_alloc_checked()     wraps only in a translation unit where PROVEN_ALLOC_CHECK is
 *                                defined, and otherwise returns the inner allocator unchanged -
 *                                zero cost. Leave the call in place and switch it with the macro.
 *
 * @warning Define PROVEN_ALLOC_CHECK before the FIRST proven header the file includes - most
 *          simply as -DPROVEN_ALLOC_CHECK on the build. proven_alloc_checked is an inline
 *          function fixed when this header is first read; a #define placed after
 *          #include "proven.h" does nothing, and the checks you think are on are off.
 *
 * After proven_panic: the default handler does not return. A test handler that returns sees the
 * refused operation do nothing (the inner allocator is never handed the bad pointer) and
 * `faults` counted, so the program can go on to report it.
 */

/** @brief One live block in the record. */
typedef struct {
    void         *ptr;
    proven_size_t size;
    proven_size_t align;
} proven_alloc_check_entry_t;

/**
 * @brief The wrapper's state. Caller-owned; it must outlive every allocator made from it, and
 *        must not be copied once wrapped (the allocator points at it).
 */
typedef struct {
    proven_allocator_t          inner;
    proven_alloc_check_entry_t *entries;     /**< your record storage */
    proven_size_t               cap;         /**< how many live blocks it can hold */
    proven_size_t               live;        /**< blocks handed out and not yet freed */
    proven_size_t               live_bytes;
    proven_size_t               peak_live;
    proven_size_t               faults;      /**< refused operations (each also a panic) */
} proven_alloc_check_t;

/**
 * @brief Wrap `inner`, recording up to `cap` live blocks in `entries`. Always checks.
 *
 * An allocation that would exceed `cap` live blocks is a panic too - a record that silently
 * stopped recording would report later frees as foreign - and returns PROVEN_ERR_NOMEM if the
 * handler returns. Size the record for the test.
 *
 * @return an invalid allocator (proven_alloc_is_valid false) for a NULL state, an invalid inner
 *         allocator, or a NULL record with a non-zero cap.
 */
[[nodiscard]]
proven_allocator_t proven_alloc_check_wrap(proven_alloc_check_t *st, proven_allocator_t inner,
                                           proven_alloc_check_entry_t *entries, proven_size_t cap);

/** @brief Whether `ptr` is a live block from this wrapper. */
[[nodiscard]]
bool proven_alloc_check_owns(const proven_alloc_check_t *st, const void *ptr);

/**
 * @brief Blocks handed out and not freed. At the end of a test, anything but 0 is a leak - or
 *        memory that is owned somewhere on purpose.
 */
[[nodiscard]]
proven_size_t proven_alloc_check_live(const proven_alloc_check_t *st);

/**
 * @brief proven_alloc_check_wrap when PROVEN_ALLOC_CHECK is defined in this translation unit;
 *        otherwise `inner`, untouched, and `st`/`entries` unused.
 */
static inline proven_allocator_t proven_alloc_checked(proven_alloc_check_t *st, proven_allocator_t inner,
                                                      proven_alloc_check_entry_t *entries, proven_size_t cap) {
#ifdef PROVEN_ALLOC_CHECK
    return proven_alloc_check_wrap(st, inner, entries, cap);
#else
    (void)st; (void)entries; (void)cap;
    return inner;
#endif
}

#endif /* PROVEN_ALLOC_CHECK_H */
