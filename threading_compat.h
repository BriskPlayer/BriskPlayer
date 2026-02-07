/*
 * BriskPlayer - Blazing fast audio player.
 * Copyright (C) 2025 Zach Bacon
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

////////////////////////////////////////////////////////////////////////////////
// Threading Compatibility Layer
//
// This header provides a consistent threading interface across:
// - C23 with native <threads.h> support
// - C11/C17 with MSVC/MinGW Windows fallback
//
// Usage:
//   #include "threading_compat.h"
//
//   cp_mutex_t my_mutex;
//   cp_mutex_init(&my_mutex);
//   cp_mutex_lock(&my_mutex);
//   // ... critical section ...
//   cp_mutex_unlock(&my_mutex);
//   cp_mutex_destroy(&my_mutex);
//
////////////////////////////////////////////////////////////////////////////////

#ifndef THREADING_COMPAT_H
#define THREADING_COMPAT_H

// Detect C23 threads.h availability
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
    #define HAVE_C23_THREADING 1
#elif defined(__has_include)
    #if __has_include(<threads.h>)
        #include <threads.h>
        #ifdef __STDC_NO_THREADS__
            #define HAVE_C23_THREADING 0
        #else
            #define HAVE_C23_THREADING 1
        #endif
    #else
        #define HAVE_C23_THREADING 0
    #endif
#else
    #define HAVE_C23_THREADING 0
#endif

////////////////////////////////////////////////////////////////////////////////
// C23 Threading Support
////////////////////////////////////////////////////////////////////////////////

#if HAVE_C23_THREADING

#include <threads.h>
#include <stdatomic.h>

// Mutex type aliases
typedef mtx_t cp_mutex_t;
typedef cnd_t cp_condition_t;

// Mutex operations
#define cp_mutex_init(mtx)      mtx_init(mtx, mtx_plain)
#define cp_mutex_lock(mtx)      mtx_lock(mtx)
#define cp_mutex_trylock(mtx)   mtx_trylock(mtx)
#define cp_mutex_unlock(mtx)    mtx_unlock(mtx)
#define cp_mutex_destroy(mtx)   mtx_destroy(mtx)

// Condition variable operations
#define cp_condition_init(cnd)              cnd_init(cnd)
#define cp_condition_signal(cnd)            cnd_signal(cnd)
#define cp_condition_broadcast(cnd)         cnd_broadcast(cnd)
#define cp_condition_wait(cnd, mtx)         cnd_wait(cnd, mtx)
#define cp_condition_timedwait(cnd, mtx, t) cnd_timedwait(cnd, mtx, t)
#define cp_condition_destroy(cnd)           cnd_destroy(cnd)

// Result codes
#define CP_THREAD_SUCCESS   thrd_success
#define CP_THREAD_TIMEDOUT  thrd_timedout
#define CP_THREAD_BUSY      thrd_busy
#define CP_THREAD_ERROR     thrd_error
#define CP_THREAD_NOMEM     thrd_nomem

////////////////////////////////////////////////////////////////////////////////
// Windows Fallback (CRITICAL_SECTION / Events)
////////////////////////////////////////////////////////////////////////////////

#else

// Ensure Windows headers are available
#ifndef NOMINMAX
    #define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

// Mutex type - uses CRITICAL_SECTION for better performance than Mutex objects
typedef CRITICAL_SECTION cp_mutex_t;

// Also provide mtx_t alias for C23-style code
typedef CRITICAL_SECTION mtx_t;
#define mtx_plain 0
#define thrd_success 0

// Condition variable type - Windows Vista+ has native CONDITION_VARIABLE
#if _WIN32_WINNT >= 0x0600
typedef CONDITION_VARIABLE cp_condition_t;
#else
// For XP compatibility, use a semaphore-based approach (defined below)
// The struct is forward-declared here; implementation follows.
typedef struct {
    HANDLE semaphore;
    HANDLE waiters_done;
    CRITICAL_SECTION lock;
    volatile LONG waiters_count;
    volatile BOOL was_broadcast;
} cp_condition_t;
#endif

// Result codes
#define CP_THREAD_SUCCESS   0
#define CP_THREAD_TIMEDOUT  1
#define CP_THREAD_BUSY      2
#define CP_THREAD_ERROR     3
#define CP_THREAD_NOMEM     4

// Mutex operations
static inline int cp_mutex_init(cp_mutex_t* mtx) {
    if (!mtx) return CP_THREAD_ERROR;
    InitializeCriticalSection(mtx);
    return CP_THREAD_SUCCESS;
}

static inline int cp_mutex_lock(cp_mutex_t* mtx) {
    if (!mtx) return CP_THREAD_ERROR;
    EnterCriticalSection(mtx);
    return CP_THREAD_SUCCESS;
}

static inline int cp_mutex_trylock(cp_mutex_t* mtx) {
    if (!mtx) return CP_THREAD_ERROR;
    return TryEnterCriticalSection(mtx) ? CP_THREAD_SUCCESS : CP_THREAD_BUSY;
}

static inline int cp_mutex_unlock(cp_mutex_t* mtx) {
    if (!mtx) return CP_THREAD_ERROR;
    LeaveCriticalSection(mtx);
    return CP_THREAD_SUCCESS;
}

static inline void cp_mutex_destroy(cp_mutex_t* mtx) {
    if (mtx) DeleteCriticalSection(mtx);
}

// C23-compatible mtx_* function aliases
static inline int mtx_init(mtx_t* mtx, int type) {
    (void)type;
    return cp_mutex_init(mtx);
}

static inline int mtx_lock(mtx_t* mtx) {
    return cp_mutex_lock(mtx);
}

static inline int mtx_unlock(mtx_t* mtx) {
    return cp_mutex_unlock(mtx);
}

static inline void mtx_destroy(mtx_t* mtx) {
    cp_mutex_destroy(mtx);
}

// Condition variable operations
#if _WIN32_WINNT >= 0x0600

// Windows Vista+ native condition variables
static inline int cp_condition_init(cp_condition_t* cnd) {
    if (!cnd) return CP_THREAD_ERROR;
    InitializeConditionVariable(cnd);
    return CP_THREAD_SUCCESS;
}

static inline int cp_condition_signal(cp_condition_t* cnd) {
    if (!cnd) return CP_THREAD_ERROR;
    WakeConditionVariable(cnd);
    return CP_THREAD_SUCCESS;
}

static inline int cp_condition_broadcast(cp_condition_t* cnd) {
    if (!cnd) return CP_THREAD_ERROR;
    WakeAllConditionVariable(cnd);
    return CP_THREAD_SUCCESS;
}

static inline int cp_condition_wait(cp_condition_t* cnd, cp_mutex_t* mtx) {
    if (!cnd || !mtx) return CP_THREAD_ERROR;
    return SleepConditionVariableCS(cnd, mtx, INFINITE) ? CP_THREAD_SUCCESS : CP_THREAD_ERROR;
}

static inline int cp_condition_timedwait(cp_condition_t* cnd, cp_mutex_t* mtx, DWORD timeout_ms) {
    if (!cnd || !mtx) return CP_THREAD_ERROR;
    if (SleepConditionVariableCS(cnd, mtx, timeout_ms))
        return CP_THREAD_SUCCESS;
    return (GetLastError() == ERROR_TIMEOUT) ? CP_THREAD_TIMEDOUT : CP_THREAD_ERROR;
}

static inline void cp_condition_destroy(cp_condition_t* cnd) {
    (void)cnd; // No cleanup needed for CONDITION_VARIABLE
}

#else

// Windows XP fallback using events
// Uses a semaphore-based approach to avoid the classic lost-wakeup race
// that plagues event-based condition variable emulations.

static inline int cp_condition_init(cp_condition_t* cnd) {
    if (!cnd) return CP_THREAD_ERROR;
    cnd->semaphore = CreateSemaphore(NULL, 0, 0x7FFFFFFF, NULL);
    cnd->waiters_done = CreateEvent(NULL, FALSE, FALSE, NULL); // Auto-reset
    InitializeCriticalSection(&cnd->lock);
    cnd->waiters_count = 0;
    cnd->was_broadcast = FALSE;
    return (cnd->semaphore && cnd->waiters_done) ? CP_THREAD_SUCCESS : CP_THREAD_ERROR;
}

static inline int cp_condition_signal(cp_condition_t* cnd) {
    if (!cnd) return CP_THREAD_ERROR;
    EnterCriticalSection(&cnd->lock);
    BOOL have_waiters = (cnd->waiters_count > 0);
    LeaveCriticalSection(&cnd->lock);
    if (have_waiters) {
        ReleaseSemaphore(cnd->semaphore, 1, NULL);
    }
    return CP_THREAD_SUCCESS;
}

static inline int cp_condition_broadcast(cp_condition_t* cnd) {
    if (!cnd) return CP_THREAD_ERROR;
    EnterCriticalSection(&cnd->lock);
    BOOL have_waiters = (cnd->waiters_count > 0);
    if (have_waiters) {
        cnd->was_broadcast = TRUE;
        ReleaseSemaphore(cnd->semaphore, cnd->waiters_count, NULL);
        LeaveCriticalSection(&cnd->lock);
        // Wait for all waiters to acquire the semaphore
        WaitForSingleObject(cnd->waiters_done, INFINITE);
        cnd->was_broadcast = FALSE;
    } else {
        LeaveCriticalSection(&cnd->lock);
    }
    return CP_THREAD_SUCCESS;
}

static inline int cp_condition_wait(cp_condition_t* cnd, cp_mutex_t* mtx) {
    if (!cnd || !mtx) return CP_THREAD_ERROR;

    EnterCriticalSection(&cnd->lock);
    cnd->waiters_count++;
    LeaveCriticalSection(&cnd->lock);

    // Atomically release the mutex and wait on the semaphore
    LeaveCriticalSection(mtx);
    WaitForSingleObject(cnd->semaphore, INFINITE);

    // Check if we're the last waiter after a broadcast
    EnterCriticalSection(&cnd->lock);
    cnd->waiters_count--;
    BOOL last_waiter = (cnd->was_broadcast && cnd->waiters_count == 0);
    LeaveCriticalSection(&cnd->lock);

    if (last_waiter) {
        SetEvent(cnd->waiters_done);
    }

    EnterCriticalSection(mtx);
    return CP_THREAD_SUCCESS;
}

static inline int cp_condition_timedwait(cp_condition_t* cnd, cp_mutex_t* mtx, DWORD timeout_ms) {
    if (!cnd || !mtx) return CP_THREAD_ERROR;

    EnterCriticalSection(&cnd->lock);
    cnd->waiters_count++;
    LeaveCriticalSection(&cnd->lock);

    LeaveCriticalSection(mtx);
    DWORD result = WaitForSingleObject(cnd->semaphore, timeout_ms);

    EnterCriticalSection(&cnd->lock);
    cnd->waiters_count--;
    BOOL last_waiter = (cnd->was_broadcast && cnd->waiters_count == 0);
    LeaveCriticalSection(&cnd->lock);

    if (last_waiter) {
        SetEvent(cnd->waiters_done);
    }

    EnterCriticalSection(mtx);

    if (result == WAIT_OBJECT_0) return CP_THREAD_SUCCESS;
    if (result == WAIT_TIMEOUT) return CP_THREAD_TIMEDOUT;
    return CP_THREAD_ERROR;
}

static inline void cp_condition_destroy(cp_condition_t* cnd) {
    if (cnd) {
        if (cnd->semaphore) {
            CloseHandle(cnd->semaphore);
            cnd->semaphore = NULL;
        }
        if (cnd->waiters_done) {
            CloseHandle(cnd->waiters_done);
            cnd->waiters_done = NULL;
        }
        DeleteCriticalSection(&cnd->lock);
    }
}

#endif // _WIN32_WINNT >= 0x0600

#endif // HAVE_C23_THREADING

////////////////////////////////////////////////////////////////////////////////
// Convenience macros for scoped locking (RAII-style for C)
////////////////////////////////////////////////////////////////////////////////

// Use with caution - requires proper scope management
// WARNING: Do NOT use break, continue, return, or goto inside the guarded block.
// These will bypass the unlock, causing a deadlock.
#define CP_MUTEX_LOCK_GUARD(mtx) \
    for (int _lock_guard_done = (cp_mutex_lock(mtx), 0); \
         !_lock_guard_done; \
         _lock_guard_done = (cp_mutex_unlock(mtx), 1))

#endif // THREADING_COMPAT_H
