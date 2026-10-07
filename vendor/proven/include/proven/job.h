#ifndef PROVEN_JOB_H
#define PROVEN_JOB_H

#include "proven/types.h"
#include "proven/error.h"
#include "proven/allocator.h"
#include <stdatomic.h>

/**
 * @file job.h
 * @brief Low-overhead MPMC (Multi-Producer Multi-Consumer) Job Scheduler.
 * 
 * Submits work to a pre-allocated pool of worker threads.
 * Atomic sequence counters coordinate the bounded queue. Idle workers park on
 * a platform counting semaphore until a submission or close wakes them.
 */

/**
 * @brief Defines a unit of independent work.
 */
typedef struct {
    void (*routine)(void* arg);
    void* arg;
} proven_job_t;

typedef struct proven_job_sys proven_job_sys_t;

/**
 * @brief Initialize a Job System instance.
 * 
 * @param alloc The allocator for the Job Queue Buffer.
 * @param num_workers Total OS threads to reserve.
 * @param max_queue_capacity Size of the command buffer. MUST be a power of 2!
 * @param out_sys Pointer to store the created system instance.
 * 
 * @return PROVEN_OK if successful.
 */
[[nodiscard]]
proven_err_t proven_job_system_init(proven_allocator_t alloc, proven_size_t num_workers, proven_size_t max_queue_capacity, proven_job_sys_t **out_sys);

/**
 * @brief Signals the Job System to stop accepting new jobs.
 * Further calls to proven_job_submit() will fail.
 *
 * This function may race with proven_job_submit(). It waits for submissions
 * that entered before close to finish committing or rejecting their work, then
 * wakes every parked worker so accepted jobs drain and the workers can exit.
 */
void proven_job_system_close(proven_job_sys_t *sys);

/**
 * @brief Destroys the Job System.
 * Blocks calling thread until the queue is completely exhausted and all workers are gracefully joined.
 * This closes the system if needed. It must not race with any thread that can
 * still call proven_job_submit(); close first, join all producers, then destroy.
 */
void proven_job_system_destroy(proven_job_sys_t *sys);

/**
 * @brief Enqueue work to the Ring-Buffer.
 * This is thread-safe and can be called simultaneously from hundreds of threads.
 * Uses atomic operations internally to manage queue indices.
 * 
 * @note proven_job_system_destroy() must not race with proven_job_submit().
 * The caller must externally synchronize close/destroy against all producer threads.
 * The job queue assumes sequence counters do not wrap beyond the signed 
 * pointer-difference range during the lifetime of a job system.
 * 
 * @return true if enqueued successfully. false if the ring buffer is full or
 *         the system is closed.
 */
[[nodiscard]]
bool proven_job_submit(proven_job_sys_t *sys, void (*routine)(void*), void* arg);

/**
 * @brief Forces the calling thread to attempt executing one task from the queue.
 * Useful for the main thread to contribute to the job pool while waiting for completion.
 * 
 * @return true if a job was found and executed. false if the queue was empty.
 */
[[nodiscard]]
bool proven_job_execute_one(proven_job_sys_t *sys);

/**
 * @brief proven_job_submit, saying why a refusal happened (RFC-0009 X-003).
 *
 * @return PROVEN_OK when queued; PROVEN_ERR_AGAIN when the queue is full - retry, or run a job
 *         yourself with proven_job_execute_one; PROVEN_ERR_INVALID_STATE when the system is
 *         closed - stop submitting; PROVEN_ERR_INVALID_ARG for a NULL system. proven_job_submit
 *         returns false for all three, which call for opposite responses.
 */
[[nodiscard]]
proven_err_t proven_job_submit_ex(proven_job_sys_t *sys, void (*routine)(void*), void* arg);

/**
 * @brief A count of submitted jobs that have not finished, to wait on a batch.
 *
 * Initialise with proven_job_group_init, submit with proven_job_group_submit, then
 * proven_job_group_wait. A group may be reused once its wait has returned. It lives as long
 * as any job submitted to it: do not let it go out of scope before the wait.
 */
typedef struct {
    _Atomic(proven_size_t) pending;
} proven_job_group_t;

void proven_job_group_init(proven_job_group_t *group);

/**
 * @brief Submit a job counted by `group`. Same results as proven_job_submit_ex; a refused job
 *        is not counted.
 */
[[nodiscard]]
proven_err_t proven_job_group_submit(proven_job_sys_t *sys, proven_job_group_t *group,
                                     void (*routine)(void*), void* arg);

/** @brief How many of the group's jobs have not finished yet. 0 for NULL. */
[[nodiscard]]
proven_size_t proven_job_group_pending(proven_job_group_t *group);

/**
 * @brief Return once every job submitted to `group` has finished.
 *
 * The waiting thread helps: while the group has pending jobs it runs queued jobs itself
 * (proven_job_execute_one - any job, not only this group's) and yields when the queue is
 * empty. So a wait from a thread that is not a worker makes progress even when every worker
 * is busy, and what the jobs wrote is visible after it returns.
 */
void proven_job_group_wait(proven_job_sys_t *sys, proven_job_group_t *group);

#endif /* PROVEN_JOB_H */
