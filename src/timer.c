/*
 * Copyright (c) 2011-2026 Cisco Systems, Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *  MA 02110-1301, USA.
 */

#include "config.h"

#include <razorback/debug.h>
#include <razorback/lock.h>
#include <razorback/log.h>
#include <razorback/thread.h>
#include <razorback/timer.h>

#include <errno.h>
#include <time.h>

#include "runtime_config.h"
#ifdef _MSC_VER
#else //_MSC_VER
#include <unistd.h>
#endif //_MSC_VER

#define TIMER_POLL_INTERVAL_MS 100U

struct Timer
{
    Thread_t *thread;          /* Worker thread that drives the timer callback. */
    Mutex_t *mutex;            /* Timer state lock. */
    uint32_t interval;         /* Timer interval in seconds. */
    void *userData;            /* User data provided to the event function. */
    void (*function)(void *);  /* Function to call when the timer expires. */
    bool bStopRequested;       /* Has timer shutdown been requested? */
    bool bDestroyDeferred;     /* Will the worker free the timer on exit? */
    bool bCleanupDone;         /* Have the timer resources already been released? */
    uint32_t destroyWaiters;   /* Number of external destroy callers waiting on join. */
};

static void Timer_Main(Thread_t *thread);
static bool Timer_ShouldStop(struct Timer *timer);
static void Timer_SleepMilliseconds(uint32_t milliseconds);

SO_PUBLIC struct Timer *
Timer_Create(uint32_t interval, void (*handler)(void *), void *userData)
{
    struct Timer *ret;

    ASSERT(handler != NULL);
    if (handler == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: handler is NULL", __func__);
        return NULL;
    }
    if (interval == 0U) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: interval must be greater than zero", __func__);
        return NULL;
    }

    if ((ret = (struct Timer *)calloc(1, sizeof(struct Timer))) == NULL)
        return NULL;

    ret->function = handler;
    ret->userData = userData;
    ret->interval = interval;
    ret->bStopRequested = false;
    ret->bDestroyDeferred = false;
    ret->bCleanupDone = false;
    ret->destroyWaiters = 0;

    ret->mutex = Mutex_Create(MUTEX_MODE_NORMAL);
    if (ret->mutex == NULL) {
        free(ret);
        return NULL;
    }

    ret->thread = Thread_Launch(Timer_Main, ret, "Timer", NULL);
    if (ret->thread == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE,
                "%s: Failed to create timer thread for interval %u seconds (%u/%u tracked threads active)",
                __func__, interval, Thread_getCount(), Config_getThreadLimit());
        Mutex_Destroy(ret->mutex);
        free(ret);
        return NULL;
    }

    return ret;
}

SO_PUBLIC void
Timer_Destroy(struct Timer *timer)
{
    Thread_t *current;
    Thread_t *timerThread;
    bool cleanupNeeded = false;

    ASSERT(timer != NULL);
    if (timer == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: timer is NULL", __func__);
        return;
    }

    current = Thread_GetCurrent();

    Mutex_Lock(timer->mutex);
    timer->bStopRequested = true;
    timerThread = timer->thread;

    if (current != NULL && current == timerThread) {
        timer->bDestroyDeferred = true;
        Mutex_Unlock(timer->mutex);
        Thread_Destroy(current);
        rzb_log(LOG_ERR, LOG_C_CORE,
                "%s: Timer_Destroy called from the timer callback thread; deferring timer cleanup until callback exit",
                __func__);
        return;
    }

    if (!timer->bCleanupDone && timerThread != NULL)
        timer->destroyWaiters++;
    Mutex_Unlock(timer->mutex);

    if (current != NULL)
        Thread_Destroy(current);

    if (timerThread != NULL) {
        Thread_Join(timerThread);
    }

    Mutex_Lock(timer->mutex);
    if (timer->destroyWaiters > 0)
        timer->destroyWaiters--;
    if (!timer->bCleanupDone && timer->destroyWaiters == 0) {
        timer->bCleanupDone = true;
        timer->thread = NULL;
        cleanupNeeded = true;
    }
    Mutex_Unlock(timer->mutex);

    if (cleanupNeeded) {
        if (timerThread != NULL)
            Thread_Destroy(timerThread);
        Mutex_Destroy(timer->mutex);
        free(timer);
    }
}

static void
Timer_Main(Thread_t *thread)
{
    struct Timer *timer;
    uint64_t remaining;
    uint32_t step;

    ASSERT(thread != NULL);
    if (thread == NULL)
        return;

    timer = Thread_GetUserData(thread);
    ASSERT(timer != NULL);
    if (timer == NULL)
        return;

    while (!Timer_ShouldStop(timer)) {
        remaining = (uint64_t)timer->interval * 1000ULL;

        while (remaining > 0ULL) {
            if (Timer_ShouldStop(timer))
                return;

            step = (remaining > (uint64_t)TIMER_POLL_INTERVAL_MS) ?
                TIMER_POLL_INTERVAL_MS : (uint32_t)remaining;
            Timer_SleepMilliseconds(step);
            remaining -= step;
        }

        if (Timer_ShouldStop(timer))
            return;

        timer->function(timer->userData);
    }

    Mutex_Lock(timer->mutex);
    if (timer->bDestroyDeferred && timer->destroyWaiters == 0 && !timer->bCleanupDone) {
        timer->bCleanupDone = true;
        timer->thread = NULL;
        Mutex_Unlock(timer->mutex);
        Thread_Destroy(thread);
        Mutex_Destroy(timer->mutex);
        free(timer);
        return;
    }
    Mutex_Unlock(timer->mutex);
}

static bool
Timer_ShouldStop(struct Timer *timer)
{
    bool ret;

    ASSERT(timer != NULL);
    if (timer == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: timer is NULL", __func__);
        return true;
    }

    Mutex_Lock(timer->mutex);
    ret = timer->bStopRequested;
    Mutex_Unlock(timer->mutex);
    return ret;
}

static void
Timer_SleepMilliseconds(uint32_t milliseconds)
{
#ifdef _MSC_VER
    Sleep(milliseconds);
#else //_MSC_VER
    struct timespec req;
    struct timespec rem;

    req.tv_sec = milliseconds / 1000U;
    req.tv_nsec = (long)(milliseconds % 1000U) * 1000000L;

    while (nanosleep(&req, &rem) == -1 && errno == EINTR)
        req = rem;
#endif //_MSC_VER
}
