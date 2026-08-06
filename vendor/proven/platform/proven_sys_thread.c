#include "proven_sys_thread.h"

#if defined(_WIN32) || defined(_WIN64)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

typedef struct {
    proven_sys_thread_fn routine;
    void *arg;
} proven_win_thread_start_t;

static DWORD WINAPI proven_win_thread_trampoline(LPVOID p) {
    proven_win_thread_start_t *start = (proven_win_thread_start_t *)p;
    proven_sys_thread_fn routine = start->routine;
    void *arg = start->arg;
    HeapFree(GetProcessHeap(), 0, start);
    routine(arg);
    return 0;
}

proven_sys_thread_t proven_sys_thread_create(proven_sys_thread_fn routine, void* arg) {
    proven_win_thread_start_t *start = HeapAlloc(GetProcessHeap(), 0, sizeof(proven_win_thread_start_t));
    if (!start) return (proven_sys_thread_t){ .internal = NULL };
    
    start->routine = routine;
    start->arg = arg;

    HANDLE hThread = CreateThread(NULL, 0, proven_win_thread_trampoline, start, 0, NULL);
    if (!hThread) {
        HeapFree(GetProcessHeap(), 0, start);
        return (proven_sys_thread_t){ .internal = NULL };
    }
    
    return (proven_sys_thread_t){ .internal = (void*)hThread };
}

void proven_sys_thread_join(proven_sys_thread_t thread) {
    if (thread.internal) {
        WaitForSingleObject((HANDLE)thread.internal, INFINITE);
        CloseHandle((HANDLE)thread.internal);
    }
}

void proven_sys_thread_yield(void) {
    SwitchToThread();
}

bool proven_sys_semaphore_init(proven_sys_semaphore_t *semaphore) {
    if (!semaphore) return false;
    semaphore->internal = NULL;
    HANDLE handle = CreateSemaphoreW(NULL, 0, 0x7fffffffL, NULL);
    if (!handle) return false;
    semaphore->internal = (void *)handle;
    return true;
}

void proven_sys_semaphore_destroy(proven_sys_semaphore_t *semaphore) {
    if (!semaphore || !semaphore->internal) return;
    CloseHandle((HANDLE)semaphore->internal);
    semaphore->internal = NULL;
}

bool proven_sys_semaphore_wait(proven_sys_semaphore_t *semaphore) {
    if (!semaphore || !semaphore->internal) return false;
    return WaitForSingleObject((HANDLE)semaphore->internal, INFINITE) ==
           WAIT_OBJECT_0;
}

bool proven_sys_semaphore_post(proven_sys_semaphore_t *semaphore) {
    if (!semaphore || !semaphore->internal) return false;
    return ReleaseSemaphore((HANDLE)semaphore->internal, 1, NULL) != 0;
}

#else
#include <pthread.h>
#include <sched.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct {
    pthread_t th;
} proven_posix_thread_box_t;

typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t condition;
    size_t permits;
} proven_posix_semaphore_box_t;

proven_sys_thread_t proven_sys_thread_create(proven_sys_thread_fn routine, void* arg) {
    proven_posix_thread_box_t *box = (proven_posix_thread_box_t *)malloc(sizeof(proven_posix_thread_box_t));
    if (!box) return (proven_sys_thread_t){ .internal = NULL };

    if (pthread_create(&box->th, NULL, routine, arg) != 0) {
        free(box);
        return (proven_sys_thread_t){ .internal = NULL };
    }
    return (proven_sys_thread_t){ .internal = (void*)box };
}

void proven_sys_thread_join(proven_sys_thread_t thread) {
    if (thread.internal) {
        proven_posix_thread_box_t *box = (proven_posix_thread_box_t *)thread.internal;
        pthread_join(box->th, NULL);
        free(box);
    }
}

void proven_sys_thread_yield(void) {
    sched_yield();
}

bool proven_sys_semaphore_init(proven_sys_semaphore_t *semaphore) {
    if (!semaphore) return false;
    semaphore->internal = NULL;

    proven_posix_semaphore_box_t *box =
        (proven_posix_semaphore_box_t *)malloc(sizeof(*box));
    if (!box) return false;
    box->permits = 0;

    if (pthread_mutex_init(&box->mutex, NULL) != 0) {
        free(box);
        return false;
    }
    if (pthread_cond_init(&box->condition, NULL) != 0) {
        pthread_mutex_destroy(&box->mutex);
        free(box);
        return false;
    }

    semaphore->internal = (void *)box;
    return true;
}

void proven_sys_semaphore_destroy(proven_sys_semaphore_t *semaphore) {
    if (!semaphore || !semaphore->internal) return;
    proven_posix_semaphore_box_t *box =
        (proven_posix_semaphore_box_t *)semaphore->internal;
    pthread_cond_destroy(&box->condition);
    pthread_mutex_destroy(&box->mutex);
    free(box);
    semaphore->internal = NULL;
}

bool proven_sys_semaphore_wait(proven_sys_semaphore_t *semaphore) {
    if (!semaphore || !semaphore->internal) return false;
    proven_posix_semaphore_box_t *box =
        (proven_posix_semaphore_box_t *)semaphore->internal;

    if (pthread_mutex_lock(&box->mutex) != 0) return false;
    while (box->permits == 0) {
        if (pthread_cond_wait(&box->condition, &box->mutex) != 0) {
            pthread_mutex_unlock(&box->mutex);
            return false;
        }
    }
    box->permits -= 1;
    return pthread_mutex_unlock(&box->mutex) == 0;
}

bool proven_sys_semaphore_post(proven_sys_semaphore_t *semaphore) {
    if (!semaphore || !semaphore->internal) return false;
    proven_posix_semaphore_box_t *box =
        (proven_posix_semaphore_box_t *)semaphore->internal;

    if (pthread_mutex_lock(&box->mutex) != 0) return false;
    if (box->permits == SIZE_MAX) {
        pthread_mutex_unlock(&box->mutex);
        return false;
    }
    box->permits += 1;
    int signal_result = pthread_cond_signal(&box->condition);
    int unlock_result = pthread_mutex_unlock(&box->mutex);
    return signal_result == 0 && unlock_result == 0;
}

#endif
