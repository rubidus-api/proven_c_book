#ifndef PROVEN_INTERNAL_JOBSEQ_H
#define PROVEN_INTERNAL_JOBSEQ_H

#include "proven/types.h"

/*
 * The bounded queue in job.c is a Vyukov ring: each cell carries a sequence number, and a
 * producer or consumer decides what to do by comparing that number with its own position.
 * Both counters run forward for ever and wrap; only the DISTANCE between them is small.
 *
 * The comparison used to be written as
 *
 *     (proven_ptrdiff_t)seq - (proven_ptrdiff_t)pos
 *
 * which is signed overflow at the sign boundary: cast two positions that straddle it and
 * you can get PTRDIFF_MAX and PTRDIFF_MIN, whose difference does not exist in the type -
 * even though the queue distance those two numbers describe is -1. It is undefined
 * behaviour, not a wrong-but-predictable number, and a full queue at that boundary reaches
 * it with no concurrency involved at all.
 *
 * The distance is computed here in the unsigned counter type, where wrapping is defined and
 * is exactly the modular arithmetic the algorithm wants, and then classified by its high
 * bit. That bit is the sign of the modular distance: a distance in the top half of the
 * range is a small negative number written the only way an unsigned type can write one.
 *
 * This works only while the queue capacity stays strictly below half the counter range, so
 * a live distance can never be large enough to be mistaken for its own negation.
 * proven_job_system_init enforces that before it allocates anything.
 */

#define PROVEN_JOB_SEQ_SIGN_BIT ((proven_size_t)1u << (sizeof(proven_size_t) * 8u - 1u))

/** @brief The largest queue capacity the modular comparison stays unambiguous for. */
#define PROVEN_JOB_MAX_CAPACITY (PROVEN_JOB_SEQ_SIGN_BIT >> 1u)

typedef enum {
    PROVEN_JOB_CELL_READY = 0,  /**< the cell is exactly at this position: claim it */
    PROVEN_JOB_CELL_BEHIND,     /**< the cell is one lap behind: the queue is full (or empty) */
    PROVEN_JOB_CELL_AHEAD       /**< another thread moved first: re-read the position and retry */
} proven_job_cell_state_t;

/**
 * @brief Where `seq` stands relative to `pos`, in modular distance.
 *
 * One helper for both the producer and the consumer, because two copies of a comparison
 * this easy to get wrong are two chances to get it wrong differently.
 */
static inline proven_job_cell_state_t proven_job_cell_state(proven_size_t seq, proven_size_t pos) {
    proven_size_t distance = seq - pos;   /* unsigned: wrapping here is defined, and intended */
    if (distance == 0) return PROVEN_JOB_CELL_READY;
    return (distance & PROVEN_JOB_SEQ_SIGN_BIT) ? PROVEN_JOB_CELL_BEHIND : PROVEN_JOB_CELL_AHEAD;
}

#endif /* PROVEN_INTERNAL_JOBSEQ_H */
