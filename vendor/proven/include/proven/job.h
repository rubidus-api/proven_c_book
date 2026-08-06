#ifndef PROVEN_JOB_H
#define PROVEN_JOB_H

#include "proven/types.h"
#include "proven/error.h"
#include "proven/allocator.h"

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

#endif /* PROVEN_JOB_H */
