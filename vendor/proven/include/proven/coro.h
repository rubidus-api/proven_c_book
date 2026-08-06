#ifndef PROVEN_CORO_H
#define PROVEN_CORO_H

#include "proven/types.h"

/**
 * @file coro.h
 * @brief Low-overhead Stackless Coroutine Engine
 * 
 * Based on Duff's Device allowing asynchronous 
 * state-machine execution without OS Thread context switching overhead.
 * 
 * @note Macro constraints: PROVEN_CORO_YIELD and PROVEN_CORO_AWAIT MUST NOT 
 *       be used more than once on the same source line, as they rely on 
 *       __LINE__ for state labels.
 */

/**
 * @brief Coroutine state tracker. Only 4 bytes (proven_i32) per Coroutine.
 * Functions utilizing this MUST return proven_i32 (0 for yield/retry, 1 for done).
 */
typedef struct {
    proven_i32 state;
} proven_coro_t;

/**
 * @brief Initializes a coroutine structure. Must be unconditionally called once.
 */
#define PROVEN_CORO_INIT(c) do { (c)->state = 0; } while(0)

/**
 * @brief Start the coroutine evaluation block.
 */
#define PROVEN_CORO_BEGIN(c) switch((c)->state) { case 0:

/**
 * @brief Voluntarily pause execution. 
 * Resolving back to caller, resuming exactly here upon next call.
 */
#define PROVEN_CORO_YIELD(c) \
    do { \
        (c)->state = __LINE__; \
        return 0; \
        case __LINE__:; \
    } while (0)

/**
 * @brief Continuously pause execution until condition is truthy.
 */
#define PROVEN_CORO_AWAIT(c, cond) \
    do { \
        (c)->state = __LINE__; \
        case __LINE__: \
        if (!(cond)) return 0; \
    } while (0)

/**
 * @brief Terminates the execution block and marks the coroutine context as permanently completed.
 */
#define PROVEN_CORO_END(c) \
    } \
    (c)->state = -1; \
    return 1

/**
 * @brief Checks if a coroutine has finished executing its entire block.
 */
#define PROVEN_CORO_IS_DONE(c) ((c)->state == -1)

#endif /* PROVEN_CORO_H */
