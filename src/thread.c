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
#include <razorback/types.h>
#include <razorback/thread.h>
#include <razorback/ntlv.h>
#include <razorback/list.h>
#include <razorback/lock.h>
#include <razorback/log.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "runtime_config.h"


static List_t *sg_threadList;

#ifdef _MSC_VER
static volatile int initialized = 0;


static void initThreading_win32(void);
#else //_MSC_VER

#include <pthread.h>
#include <signal.h>


struct _Thread
{
#ifdef _MSC_VER
    HANDLE hThread;
#endif //_MSC_VER
    rzb_thread_t iThread;               ///< pthread Thread info.
    Mutex_t * mMutex;                   ///< mutex protecting this struct
    bool bRunning;                      ///< true if executing, false if not:  must be managed explicitly by thread function
    void *pUserData;                    ///< Additional info for the thread
    const char *sName;                  ///< The thread name
    struct RazorbackContext *pContext;  ///< The Thread Context
    void (*mainFunction) (Thread_t *);  ///< Thread Main Function
    bool bShutdown;                     ///< Shutdown Flag
    int refs;                           ///< Reference count
    //void (*interrupt)(Thread_t *);    ///< Cancellation handler for a blocking function.
};


static pthread_attr_t g_attr;
static pthread_once_t g_once_control = PTHREAD_ONCE_INIT;

static void initThreading_pthreads(void);


static void handler(int sig);
#endif //_MSC_VER
static void Thread_Lock(void *);
static void Thread_Unlock(void *);
static void Thread_LogLaunchFailure(const char *threadName, const char *reason);

static void
initThreading (void)
{
    sg_threadList = List_Create(LIST_MODE_GENERIC,
            Thread_Cmp,
            Thread_KeyCmp,
            NULL, NULL,
            Thread_Lock,
            Thread_Unlock);
    List_SetLimit(sg_threadList, Config_getThreadLimit());
#ifdef _MSC_VER
    initThreading_win32();
#else
    initThreading_pthreads();
#endif
}

static void
Thread_LogLaunchFailure(const char *threadName, const char *reason)
{
    uint32_t count;
    uint32_t limit;

    count = (sg_threadList == NULL) ? 0U : List_Length(sg_threadList);
    limit = Config_getThreadLimit();

    if (limit > 0U && count >= limit) {
        rzb_log(LOG_ERR, LOG_C_CORE,
                "%s: Failed to launch thread '%s': Global.MaxThreads limit reached (%u/%u). %s",
                __func__,
                (threadName != NULL) ? threadName : "(unnamed)",
                count, limit,
                (reason != NULL) ? reason : "No additional detail");
        return;
    }

    rzb_log(LOG_ERR, LOG_C_CORE,
            "%s: Failed to launch thread '%s' (%u/%u active). %s",
            __func__,
            (threadName != NULL) ? threadName : "(unnamed)",
            count, limit,
            (reason != NULL) ? reason : "No additional detail");
}

#ifdef _MSC_VER
static DWORD WINAPI
Thread_MainWrapper (void *arg)
#else
static void *
Thread_MainWrapper (void *arg)
#endif
{
    Thread_t *l_pThread = (Thread_t *) arg;

    Thread_Lock (l_pThread);
    l_pThread->bRunning = true;
    Thread_Unlock (l_pThread);

    l_pThread->mainFunction (l_pThread);

    Thread_Lock (l_pThread);
    l_pThread->bRunning = false;
    Thread_Unlock (l_pThread);

    // Don't destroy the thread structure, it may still be referenced
    // We should probably ref count this struct
    Thread_Destroy(l_pThread);

#ifdef _MSC_VER
    return 0;
#else //_MSC_VER
    return NULL; // Implicit pthread_exit()
#endif //_MSC_VER
}

SO_PUBLIC Thread_t *
Thread_Launch (void (*fpFunction) (Thread_t *), void *userData,
        const char *name, struct RazorbackContext *context)
{
    Thread_t *thread;

    ASSERT (fpFunction != NULL);
    if (fpFunction == NULL)
        return NULL;

#ifdef _MSC_VER
    if (initialized == 0)
        initThreading();

#else //_MSC_VER
    pthread_once (&g_once_control, initThreading);
#endif //_MSC_VER


    // allocate memory for thread structure
    thread = (Thread_t *)calloc (1, sizeof (Thread_t));
    if (thread == NULL)
    {
        rzb_log (LOG_ERR,LOG_C_CORE,
                 "%s: Failed to launch thread in Thread_Launch due to out of memory for Thread", __func__);
        return NULL;
    }

    // initialize running indicator
    thread->bRunning = false;
    thread->pContext = context;
    thread->pUserData = userData;
    thread->sName = name;
    thread->bShutdown = false;
    // Init ref count, once for the list, once for the caller.
    thread->refs =2;

    thread->mainFunction = fpFunction;
    // initialize running mutex
    if ((thread->mMutex = Mutex_Create(MUTEX_MODE_RECURSIVE)) == NULL)
    {
        rzb_log(LOG_ERR, LOG_C_CORE,
                "%s: Failed to launch thread '%s': unable to create thread mutex",
                __func__, (name != NULL) ? name : "(unnamed)");
        free(thread);
        return NULL;
    }

    if (!List_Push(sg_threadList, thread)) {
        Thread_LogLaunchFailure(name, "Failed to reserve a tracked thread slot");
        Mutex_Destroy(thread->mMutex);
        free(thread);
        return NULL;
    }

#ifdef _MSC_VER
    thread->hThread = CreateThread(NULL, 0, Thread_MainWrapper, thread, 0, &thread->iThread);
    if (thread->hThread == NULL)
    {
        Thread_LogLaunchFailure(name, "CreateThread failed");
        List_Remove(sg_threadList, thread);
        Mutex_Destroy(thread->mMutex);
        free(thread);
        return NULL;
    }
#else //_MSC_VER
    // start thread, check for error
    if (pthread_create
        (&thread->iThread, &g_attr, Thread_MainWrapper, thread) != 0)
    {
        Thread_LogLaunchFailure(name, "pthread_create failed");
        List_Remove(sg_threadList, thread);
        Mutex_Destroy(thread->mMutex);
        free (thread);
        return NULL;
    }
#endif //_MSC_VER

    // done
    return thread;
}

SO_PUBLIC void
Thread_Destroy (Thread_t *thread)
{
    ASSERT(thread != NULL);
    if (thread == NULL)
        return;

    Thread_Lock(thread);

    // Reference count should not drop below 1 as the list holds a ref.
    ASSERT(thread->refs >= 1);

    if (thread->refs > 1)
    {
        thread->refs--;
        Thread_Unlock(thread);
        return;
    }

    List_Remove(sg_threadList, thread);
    // destroy running mutex
    Thread_Unlock(thread);

    Mutex_Destroy (thread->mMutex);

    free (thread);
}

SO_PUBLIC bool
Thread_IsRunning (Thread_t *thread)
{
    bool l_bRunning;
    ASSERT (thread != NULL);
    if (thread == NULL)
        return false;

    Thread_Lock (thread);

    l_bRunning = thread->bRunning;

    Thread_Unlock (thread);

    return l_bRunning;
}

SO_PUBLIC bool
Thread_IsStopped (Thread_t *thread)
{
    bool l_bShutdown;

    ASSERT (thread != NULL);
    if (thread == NULL)
        return false;

    Thread_Lock (thread);

    l_bShutdown = thread->bShutdown;

    Thread_Unlock (thread);

    return l_bShutdown;
}


SO_PUBLIC void
Thread_Stop (Thread_t *thread)
{
    ASSERT (thread != NULL);
    if (thread == NULL)
        return;

    Thread_Lock (thread);

    thread->bShutdown = true;

    Thread_Unlock (thread);

}
SO_PUBLIC void
Thread_Interrupt(Thread_t *thread)
{
    ASSERT (thread != NULL);
    if (thread == NULL)
        return;

    Thread_Lock(thread);
    Thread_Stop(thread);
#ifdef _MSC_VER
    //UNIMPLEMENTED();
#else //_MSC_VER
    pthread_kill(thread->iThread, SIGUSR1);
#endif //_MSC_VER
    //thread->interrupt(thread);
    Thread_Unlock(thread);

}

SO_PUBLIC void
Thread_Join(Thread_t *thread)
{
    ASSERT(thread != NULL);
    if (thread == NULL)
        return;

#ifdef _MSC_VER
    WaitForSingleObject(thread->hThread,INFINITE);
#else //_MSC_VER
    void *ret;
    int res =0;
    if ((res = pthread_join(thread->iThread, &ret)) != 0)
        rzb_log(LOG_ERR,LOG_C_CORE, "%s: Failed to join: %i", __func__, res);
#endif //_MSC_VER
}

SO_PUBLIC void
Thread_StopAndJoin(Thread_t *thread)
{
   ASSERT (thread != NULL);
   if (thread == NULL)
       return;

   Thread_Stop(thread);
   Thread_Join(thread);
}

SO_PUBLIC void
Thread_InterruptAndJoin(Thread_t *thread)
{
    ASSERT (thread != NULL);
    if (thread == NULL)
        return;

    Thread_Stop(thread);
    Thread_Interrupt(thread);
    Thread_Join(thread);
}


SO_PUBLIC struct RazorbackContext *
Thread_GetContext(Thread_t *thread)
{
    struct RazorbackContext *l_pOldContext;

    ASSERT(thread != NULL);
    if (thread == NULL)
        return NULL;

    Thread_Lock (thread);

    l_pOldContext = thread->pContext;
    Thread_Unlock (thread);

    return l_pOldContext;
}

SO_PUBLIC struct RazorbackContext *
Thread_GetCurrentContext(void)
{
    Thread_t *thread;
    struct RazorbackContext *cont;
    thread = Thread_GetCurrent();
    if (thread == NULL)
    {
        rzb_log(LOG_ERR,LOG_C_CORE, "%s: Failed to get current thread", __func__);
        return NULL;
    }
    cont = Thread_GetContext(thread);
    Thread_Destroy(thread);
    return cont;
}

SO_PUBLIC struct RazorbackContext *
Thread_ChangeContext(Thread_t *thread, struct RazorbackContext *context)
{
    struct RazorbackContext *l_pOldContext;

    ASSERT(thread != NULL);
    if (thread == NULL)
        return NULL;

    Thread_Lock (thread);

    l_pOldContext = thread->pContext;
    thread->pContext = context;

    Thread_Unlock (thread);

    return l_pOldContext;
}

SO_PUBLIC rzb_thread_t
Thread_GetCurrentId(void)
{
#ifdef _MSC_VER
    return GetCurrentThreadId();
#else
    return pthread_self();
#endif
}

SO_PUBLIC Thread_t *
Thread_GetCurrent(void)
{
    Thread_t *l_pRet = NULL;
    rzb_thread_t l_tCurrent = Thread_GetCurrentId();
    l_pRet = (Thread_t *)List_Find(sg_threadList, &l_tCurrent);
    if (l_pRet == NULL)
        return NULL;

    Thread_Lock(l_pRet);
    l_pRet->refs++;
    Thread_Unlock(l_pRet);
    return l_pRet;
}

SO_PUBLIC void
Thread_Yield(void)
{
#ifdef _MSC_VER
    SwitchToThread();
#else
    sched_yield();
#endif
}

SO_PUBLIC uint32_t
Thread_getCount (void)
{
    uint32_t num;
    num = List_Length(sg_threadList);
    return num;
}

SO_PUBLIC uint32_t
Thread_getLimit(void)
{
    return Config_getThreadLimit();
}

SO_PUBLIC int
Thread_KeyCmp(void *a, const void *id)
{
    Thread_t * cA = (Thread_t *)a;
    rzb_thread_t cId = *(const rzb_thread_t *)id;

    if (cId == cA->iThread)
        return 0;
    else
        return -1;

    return -1;
}

SO_PUBLIC int
Thread_Cmp(void *a, void *b)
{
    Thread_t * cA = (Thread_t *)a;
    Thread_t * cB = (Thread_t *)b;
    if (a==b)
        return 0;
    if (cA->iThread == cB->iThread)
        return 0;
    else
        return -1;

    return -1;
}

static void
Thread_Lock(void *a)
{
    Thread_t *t = (Thread_t *)a;
    Mutex_Lock(t->mMutex);
}

static void
Thread_Unlock(void *a)
{
    Thread_t *t = (Thread_t *)a;
    Mutex_Unlock(t->mMutex);
}


#ifdef _MSC_VER

static void
initThreading_win32(void)
{
    initialized=1;
}

#else //_MSC_VER

static void
initThreading_pthreads (void)
{
    struct sigaction act;
    sigset_t mask;

    pthread_attr_init (&g_attr);
    pthread_attr_setdetachstate (&g_attr, PTHREAD_CREATE_JOINABLE);

    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    act.sa_handler = handler;
    act.sa_flags = 0;
    sigemptyset(&act.sa_mask);
    if (sigaction(SIGUSR1, &act, NULL) < 0)
        rzb_log(LOG_ERR,LOG_C_CORE, "%s: Failed to install signal handler", __func__);
}

static void handler(int sig)
{
    rzb_log(LOG_DEBUG,LOG_C_CORE, "Thread Got Signal");
    return;
}

SO_PUBLIC void
Thread_Interrupt_pthread(Thread_t *thread)
{
    pthread_kill(thread->iThread, SIGUSR1);
}

SO_PUBLIC void
Thread_SetUserData(Thread_t *thread, void *data)
{
    ASSERT(thread != NULL);
    if (thread == NULL)
        return;

    Thread_Lock(thread);
    thread->pUserData = data;
    Thread_Unlock(thread);
}

SO_PUBLIC void *
Thread_GetUserData(Thread_t *thread)
{
    void *data;
    ASSERT(thread != NULL);
    if (thread == NULL)
        return NULL;

    Thread_Lock(thread);
    data = thread->pUserData;
    Thread_Unlock(thread);
    return data;
}

SO_PUBLIC const char *
Thread_GetName(Thread_t *thread)
{
    const char *name;
    Thread_Lock(thread);
    name = thread->sName;
    Thread_Unlock(thread);
    return name;
}

#endif //_MSC_VER
