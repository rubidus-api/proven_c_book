#include "proven/job.h"
#include "proven/memory.h"
#include "proven/panic.h"
#include "../../platform/proven_sys_thread.h"
#include <stdatomic.h>

typedef struct {
    _Atomic(proven_size_t) sequence;
    proven_job_t data;
} proven_job_cell_t;

typedef struct {
    proven_size_t buffer_mask;
    proven_job_cell_t* buffer;
    _Atomic(proven_size_t) enqueue_pos;
    _Atomic(proven_size_t) dequeue_pos;
} proven_job_queue_t;

struct proven_job_sys {
    proven_job_queue_t queue;
    _Atomic(proven_size_t) admission_state;
    proven_sys_semaphore_t work_ready;
    proven_sys_thread_t* threads;
    proven_size_t num_threads;
    proven_allocator_t alloc;
};

#define PROVEN_JOB_ADMISSION_CLOSED ((proven_size_t)1u << (sizeof(proven_size_t) * 8u - 1u))
#define PROVEN_JOB_ADMISSION_ACTIVE_MASK (~PROVEN_JOB_ADMISSION_CLOSED)

static bool proven_job_is_closed(proven_job_sys_t *sys) {
    return (atomic_load_explicit(&sys->admission_state, memory_order_acquire) & PROVEN_JOB_ADMISSION_CLOSED) != 0;
}

static bool proven_job_begin_submit(proven_job_sys_t *sys) {
    proven_size_t state = atomic_load_explicit(&sys->admission_state, memory_order_acquire);
    for (;;) {
        if ((state & PROVEN_JOB_ADMISSION_CLOSED) != 0) return false;
        if ((state & PROVEN_JOB_ADMISSION_ACTIVE_MASK) == PROVEN_JOB_ADMISSION_ACTIVE_MASK) return false;
        proven_size_t next = state + 1u;
        if (atomic_compare_exchange_weak_explicit(&sys->admission_state, &state, next, memory_order_acq_rel, memory_order_acquire)) {
            return true;
        }
    }
}

static void proven_job_end_submit(proven_job_sys_t *sys) {
    (void)atomic_fetch_sub_explicit(&sys->admission_state, 1u, memory_order_release);
}

static bool proven_job_close_admission(proven_job_sys_t *sys) {
    proven_size_t old_state = atomic_fetch_or_explicit(
        &sys->admission_state, PROVEN_JOB_ADMISSION_CLOSED,
        memory_order_acq_rel);
    while ((atomic_load_explicit(&sys->admission_state, memory_order_acquire) & PROVEN_JOB_ADMISSION_ACTIVE_MASK) != 0) {
        proven_sys_thread_yield();
    }
    return (old_state & PROVEN_JOB_ADMISSION_CLOSED) == 0;
}

static void proven_job_post_work(proven_job_sys_t *sys) {
    if (!proven_sys_semaphore_post(&sys->work_ready)) {
        proven_panic("proven_job: failed to wake a worker");
    }
}

static void proven_job_close_and_wake(proven_job_sys_t *sys) {
    if (!proven_job_close_admission(sys)) return;
    for (proven_size_t i = 0; i < sys->num_threads; ++i) {
        proven_job_post_work(sys);
    }
}

bool proven_job_execute_one(proven_job_sys_t *sys) {
    if (!sys) return false;
    proven_job_cell_t* cell;
    proven_size_t pos = atomic_load_explicit(&sys->queue.dequeue_pos, memory_order_relaxed);
    
    for (;;) {
        cell = &sys->queue.buffer[pos & sys->queue.buffer_mask];
        proven_size_t seq = atomic_load_explicit(&cell->sequence, memory_order_acquire);
        proven_ptrdiff_t dif = (proven_ptrdiff_t)seq - (proven_ptrdiff_t)(pos + 1);
        
        if (dif == 0) {
            if (atomic_compare_exchange_weak_explicit(&sys->queue.dequeue_pos, &pos, pos + 1, memory_order_relaxed, memory_order_relaxed)) {
                break;
            }
        } else if (dif < 0) {
            return false; // Queue is entirely empty
        } else {
            pos = atomic_load_explicit(&sys->queue.dequeue_pos, memory_order_relaxed);
        }
    }
    
    proven_job_t task = cell->data;
    atomic_store_explicit(&cell->sequence, pos + sys->queue.buffer_mask + 1, memory_order_release);
    
    if (task.routine) {
        task.routine(task.arg);
    }
    return true;
}

static proven_size_t proven_job_active_submitters(proven_job_sys_t *sys) {
    return atomic_load_explicit(&sys->admission_state, memory_order_acquire) & PROVEN_JOB_ADMISSION_ACTIVE_MASK;
}

static void* worker_main(void* arg) {
    proven_job_sys_t *sys = (proven_job_sys_t*)arg;
    for (;;) {
        if (!proven_sys_semaphore_wait(&sys->work_ready)) {
            proven_panic("proven_job: failed to park a worker");
            return NULL;
        }
        if (proven_job_execute_one(sys)) continue;

        /*
         * A permit may be stale when an external caller executed the job first.
         * During close, an in-flight submitter may also have claimed a slot but
         * not published it yet. Admission close waits for all such submitters
         * before posting the final worker permits, so closed + no active
         * submitter + one final empty read is the only safe exit condition.
         */
        if (proven_job_is_closed(sys) &&
            proven_job_active_submitters(sys) == 0 &&
            !proven_job_execute_one(sys)) {
            break;
        }
    }

    return NULL;
}

proven_err_t proven_job_system_init(proven_allocator_t alloc, proven_size_t num_workers, proven_size_t max_queue_capacity, proven_job_sys_t **out_sys) {
    if (!out_sys) return PROVEN_ERR_INVALID_ARG;
    *out_sys = NULL;
    
    // Capacity implicitly required to be a power of 2 for fast wrapping via bitwise manipulation
    if (max_queue_capacity < 2 || (max_queue_capacity & (max_queue_capacity - 1)) != 0 || num_workers == 0) {
        return PROVEN_ERR_INVALID_ARG;
    }
    
    if (!proven_alloc_is_valid(alloc)) return PROVEN_ERR_INVALID_ARG;
    
    proven_result_mem_mut_t s_res = alloc.alloc_fn(alloc.ctx, sizeof(proven_job_sys_t), alignof(proven_job_sys_t));
    if (!proven_is_ok(s_res.err)) return s_res.err;
    
    proven_job_sys_t *sys = (proven_job_sys_t*)s_res.value.ptr;
    
    proven_size_t q_bytes;
    if (PROVEN_CKD_MUL(&q_bytes, sizeof(proven_job_cell_t), max_queue_capacity)) {
        alloc.free_fn(alloc.ctx, sys);
        return PROVEN_ERR_OVERFLOW;
    }
    proven_result_mem_mut_t q_res = alloc.alloc_fn(alloc.ctx, q_bytes, 64);
    if (!proven_is_ok(q_res.err)) {
        alloc.free_fn(alloc.ctx, sys);
        return q_res.err;
    }
    
    proven_size_t t_bytes;
    if (PROVEN_CKD_MUL(&t_bytes, sizeof(proven_sys_thread_t), num_workers)) {
        alloc.free_fn(alloc.ctx, q_res.value.ptr);
        alloc.free_fn(alloc.ctx, sys);
        return PROVEN_ERR_OVERFLOW;
    }
    proven_result_mem_mut_t t_res = alloc.alloc_fn(alloc.ctx, t_bytes, 8);
    if (!proven_is_ok(t_res.err)) {
        alloc.free_fn(alloc.ctx, q_res.value.ptr);
        alloc.free_fn(alloc.ctx, sys);
        return t_res.err;
    }

    sys->alloc = alloc;
    sys->queue.buffer_mask = max_queue_capacity - 1;
    sys->queue.buffer = (proven_job_cell_t*)q_res.value.ptr;
    
    for (proven_size_t i = 0; i < max_queue_capacity; ++i) {
        atomic_init(&sys->queue.buffer[i].sequence, i);
    }
    
    atomic_init(&sys->queue.enqueue_pos, 0);
    atomic_init(&sys->queue.dequeue_pos, 0);
    atomic_init(&sys->admission_state, 0);
    
    sys->num_threads = 0;
    sys->threads = (proven_sys_thread_t*)t_res.value.ptr;

    if (!proven_sys_semaphore_init(&sys->work_ready)) {
        alloc.free_fn(alloc.ctx, sys->threads);
        alloc.free_fn(alloc.ctx, sys->queue.buffer);
        alloc.free_fn(alloc.ctx, sys);
        return PROVEN_ERR_IO;
    }
    
    for (proven_size_t i = 0; i < num_workers; ++i) {
        sys->threads[i] = proven_sys_thread_create(worker_main, sys);
        if (!sys->threads[i].internal) {
            proven_job_close_and_wake(sys);
            for (proven_size_t j = 0; j < sys->num_threads; ++j) {
                proven_sys_thread_join(sys->threads[j]);
            }
            proven_sys_semaphore_destroy(&sys->work_ready);
            alloc.free_fn(alloc.ctx, sys->threads);
            alloc.free_fn(alloc.ctx, sys->queue.buffer);
            alloc.free_fn(alloc.ctx, sys);
            return PROVEN_ERR_IO;
        }
        sys->num_threads += 1;
    }
    
    *out_sys = sys;
    return PROVEN_OK;
}

bool proven_job_submit(proven_job_sys_t *sys, void (*routine)(void*), void* arg) {
    if (!sys) return false;
    if (!proven_job_begin_submit(sys)) return false;
    
    bool committed = false;
    proven_job_cell_t* cell;
    proven_size_t pos = atomic_load_explicit(&sys->queue.enqueue_pos, memory_order_relaxed);
    
    for (;;) {
        cell = &sys->queue.buffer[pos & sys->queue.buffer_mask];
        proven_size_t seq = atomic_load_explicit(&cell->sequence, memory_order_acquire);
        proven_ptrdiff_t dif = (proven_ptrdiff_t)seq - (proven_ptrdiff_t)pos;
        
        if (dif == 0) {
            if (atomic_compare_exchange_weak_explicit(&sys->queue.enqueue_pos, &pos, pos + 1, memory_order_relaxed, memory_order_relaxed)) {
                break; // Claimed successfully
            }
        } else if (dif < 0) {
            proven_job_end_submit(sys);
            return false; // Queue is entirely full
        } else {
            pos = atomic_load_explicit(&sys->queue.enqueue_pos, memory_order_relaxed);
        }
    }
    
    cell->data.routine = routine;
    cell->data.arg = arg;
    atomic_store_explicit(&cell->sequence, pos + 1, memory_order_release); // Commit to workers
    committed = true;
    proven_job_post_work(sys);
    proven_job_end_submit(sys);
    return committed;
}

void proven_job_system_close(proven_job_sys_t *sys) {
    if (!sys) return;
    proven_job_close_and_wake(sys);
}

void proven_job_system_destroy(proven_job_sys_t *sys) {
    if (!sys) return;
    proven_job_close_and_wake(sys);
    
    for (proven_size_t i = 0; i < sys->num_threads; ++i) {
        proven_sys_thread_join(sys->threads[i]);
    }
    proven_sys_semaphore_destroy(&sys->work_ready);
    
    if (sys->alloc.alloc_fn) {
        sys->alloc.free_fn(sys->alloc.ctx, sys->queue.buffer);
        sys->alloc.free_fn(sys->alloc.ctx, sys->threads);
        sys->alloc.free_fn(sys->alloc.ctx, sys);
    }
}
