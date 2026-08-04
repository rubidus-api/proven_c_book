#ifndef PROVEN_PLATFORM_SYS_THREAD_H
#define PROVEN_PLATFORM_SYS_THREAD_H

#include <stdbool.h>

/**
 * @file proven_sys_thread.h
 * @brief Platform Abstraction Layer for OS threads, yielding, and parking.
 */

typedef struct {
    void* internal;
} proven_sys_thread_t;

/**
 * @brief Opaque counting semaphore used to park threads without losing an
 *        earlier wake.
 */
typedef struct {
    void* internal;
} proven_sys_semaphore_t;

/**
 * @brief Thread routine signature.
 */
typedef void* (*proven_sys_thread_fn)(void* arg);

/**
 * @brief Spawns a physical OS thread.
 */
[[nodiscard]]
proven_sys_thread_t proven_sys_thread_create(proven_sys_thread_fn routine, void* arg);

/**
 * @brief Block calling thread until the target thread terminates.
 */
void proven_sys_thread_join(proven_sys_thread_t thread);

/**
 * @brief Advise the OS to reschedule CPU time away from the current thread.
 */
void proven_sys_thread_yield(void);

/**
 * @brief Initialize an empty counting semaphore.
 */
[[nodiscard]]
bool proven_sys_semaphore_init(proven_sys_semaphore_t *semaphore);

/**
 * @brief Release all resources after every waiter has stopped.
 */
void proven_sys_semaphore_destroy(proven_sys_semaphore_t *semaphore);

/**
 * @brief Block until one permit is available, then consume it.
 */
[[nodiscard]]
bool proven_sys_semaphore_wait(proven_sys_semaphore_t *semaphore);

/**
 * @brief Add one permit and wake one waiter. The permit is retained when no
 *        thread is currently waiting.
 */
[[nodiscard]]
bool proven_sys_semaphore_post(proven_sys_semaphore_t *semaphore);

#endif /* PROVEN_PLATFORM_SYS_THREAD_H */
