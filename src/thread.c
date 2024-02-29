#include "config.h"

#include <razorback/debug.h>
#include <razorback/thread.h>
#include <razorback/ntlv.h>
#include <razorback/log.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <signal.h>
#include "runtime_config.h"

struct ThreadListItem
{
    struct Thread *pThread;
    struct ThreadListItem *pNext;
};

static struct ThreadListItem *g_pThreadListHead;
static struct ThreadListItem *g_pThreadListTail;
static uint32_t g_iThreadCount = 0;

static pthread_mutex_t g_listMutex = PTHREAD_MUTEX_INITIALIZER;

static pthread_attr_t g_attr;
static pthread_once_t g_once_control = PTHREAD_ONCE_INIT;

void handler(int sig)
{
    rzb_log(LOG_DEBUG, "Thread Got Signal");
    return;
}
static void
initThreading (void)
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
        rzb_log(LOG_ERR, "%s: Failed to install signal handler", __func__);
}

static void *
Thread_MainWrapper (void *arg)
{
    struct Thread *l_pThread = (struct Thread *) arg;

    pthread_mutex_lock (&g_listMutex);
    pthread_mutex_lock (&l_pThread->mMutex);

    l_pThread->bRunning = true;

    pthread_mutex_unlock (&l_pThread->mMutex);
    pthread_mutex_unlock (&g_listMutex);

    l_pThread->mainFunction (l_pThread);

    pthread_mutex_lock (&g_listMutex);
    pthread_mutex_lock (&l_pThread->mMutex);

    l_pThread->bRunning = false;

    pthread_mutex_unlock (&l_pThread->mMutex);
    pthread_mutex_unlock (&g_listMutex);

    // Don't destroy the thread structure, it may still be referenced
    // We should probably ref count this struct
    //Thread_Destroy(l_pThread);
    //
    return NULL; // Implicit pthread_exit()
}

SO_PUBLIC struct Thread *
Thread_Launch (void (*p_fpFunction) (struct Thread *), void *p_pUserData, 
        char *p_sName, struct RazorbackContext *p_pContext)
{
    ASSERT (p_fpFunction != NULL);


    struct Thread *l_pThread;
    struct ThreadListItem *l_pThreadListItem;

    if (g_iThreadCount == Config_getThreadLimit ())
        return NULL;

    pthread_once (&g_once_control, initThreading);

    // allocate memory for thread structure
    l_pThread = calloc (1, sizeof (struct Thread));
    if (l_pThread == NULL)
    {
        rzb_log (LOG_ERR,
                 "%s: Failed to launch thread in Thread_Launch due to out of memory for Thread", __func__);
        return NULL;
    };

    // initialize running indicator
    l_pThread->bRunning = false;
    l_pThread->pContext = p_pContext;
    l_pThread->pUserData = p_pUserData;
    l_pThread->sName = p_sName;
    l_pThread->bShutdown = false;

    // allocate memory for thread list structure
    l_pThreadListItem = calloc (1, sizeof (struct ThreadListItem));
    if (l_pThreadListItem == NULL)
    {
        rzb_log (LOG_ERR,
                 "%s: Failed to launch thread in Thread_Launch due to out of memory for thread list item", __func__);
        free (l_pThread);
        return NULL;
    }
    l_pThreadListItem->pThread = l_pThread;

    l_pThread->mainFunction = p_fpFunction;
    // start thread, check for error
    if (pthread_create
        (&l_pThread->iThread, &g_attr, Thread_MainWrapper, l_pThread) != 0)
    {
        free (l_pThread);
        free (l_pThreadListItem);
        rzb_log (LOG_ERR,
                 "%s: Failed to launch thread in Thread_Launch due to pthread_create error (%i)", __func__,
                 errno);
        return NULL;
    };

    pthread_mutex_lock (&g_listMutex);
    if (g_pThreadListHead == NULL)
    {
        g_pThreadListHead = l_pThreadListItem;
        g_pThreadListTail = l_pThreadListItem;
    }
    else
    {
        g_pThreadListTail->pNext = l_pThreadListItem;
        g_pThreadListTail = l_pThreadListItem;
    
    }
    g_iThreadCount++;
    pthread_mutex_unlock (&g_listMutex);

    // initialize running mutex
    pthread_mutex_init (&l_pThread->mMutex, NULL);

    // done
    return l_pThread;
}

SO_PUBLIC void
Thread_Destroy (struct Thread *p_pThread)
{
    struct ThreadListItem *l_pPrev;
    struct ThreadListItem *l_pCur;
    l_pPrev = NULL;
    l_pCur = g_pThreadListHead;

    pthread_mutex_lock (&g_listMutex);
    while (l_pCur != NULL)
    {
        if (l_pCur->pThread == p_pThread)
        {
            if (l_pPrev == NULL)
            {
                g_pThreadListHead = NULL;
                g_pThreadListTail = NULL;
            }
            else
            {
                if (l_pCur == g_pThreadListTail)
                    g_pThreadListTail = l_pPrev;

                l_pPrev->pNext = l_pCur->pNext;
            }
            free (l_pCur);
            break;
        }
        l_pPrev = l_pCur;
        l_pCur = l_pCur->pNext;
    }
    pthread_mutex_unlock (&g_listMutex);

    // destroy running mutex
    pthread_mutex_destroy (&p_pThread->mMutex);

    free (p_pThread);
}

SO_PUBLIC bool
Thread_IsRunning (struct Thread *p_pThread)
{
    ASSERT (p_pThread != NULL);

    // local variables
    bool l_bRunning;

    pthread_mutex_lock (&p_pThread->mMutex);

    l_bRunning = p_pThread->bRunning;

    pthread_mutex_unlock (&p_pThread->mMutex);

    return l_bRunning;
}

SO_PUBLIC bool
Thread_IsStopped (struct Thread *p_pThread)
{
    ASSERT (p_pThread != NULL);

    // local variables
    bool l_bShutdown;

    pthread_mutex_lock (&p_pThread->mMutex);

    l_bShutdown = p_pThread->bShutdown;

    pthread_mutex_unlock (&p_pThread->mMutex);

    return l_bShutdown;
}


SO_PUBLIC void
Thread_Stop (struct Thread *p_pThread)
{
    ASSERT (p_pThread != NULL);

    pthread_mutex_lock (&p_pThread->mMutex);

    p_pThread->bShutdown = true;

    pthread_mutex_unlock (&p_pThread->mMutex);

}

SO_PUBLIC void
Thread_StopAndJoin(struct Thread *thread)
{
   ASSERT (thread != NULL);
   void *ret;
   int res =0;
   Thread_Stop(thread);
   if ((res = pthread_join(thread->iThread, &ret)) != 0)
       rzb_log(LOG_ERR, "%s: Failed to join: %i", __func__, res);
   
}
SO_PUBLIC void
Thread_InterruptAndJoin(struct Thread *thread)
{
   ASSERT (thread != NULL);
   void *ret;
   int res =0;
   Thread_Stop(thread);
   pthread_kill(thread->iThread, SIGUSR1);
   if ((res = pthread_join(thread->iThread, &ret)) != 0)
       rzb_log(LOG_ERR, "%s: Failed to join: %i", __func__, res);
   
}


SO_PUBLIC struct RazorbackContext * 
Thread_GetContext(struct Thread *p_pThread)
{
    struct RazorbackContext *l_pOldContext;

    pthread_mutex_lock (&p_pThread->mMutex);

    l_pOldContext = p_pThread->pContext;
    pthread_mutex_unlock (&p_pThread->mMutex);

    return l_pOldContext;
}

SO_PUBLIC struct RazorbackContext * 
Thread_GetCurrentContext(void)
{
    struct Thread *thread;
    thread = Thread_GetCurrent();
    if (thread == NULL)
    {
        rzb_log(LOG_ERR, "%s: Failed to get current thread", __func__);
        return NULL;
    }
    return Thread_GetContext(thread);
}

SO_PUBLIC struct RazorbackContext * 
Thread_ChangeContext(struct Thread *p_pThread, struct RazorbackContext *p_pContext)
{
    struct RazorbackContext *l_pOldContext;

    pthread_mutex_lock (&p_pThread->mMutex);

    l_pOldContext = p_pThread->pContext;
    p_pThread->pContext = p_pContext;

    pthread_mutex_unlock (&p_pThread->mMutex);

    return l_pOldContext;
}

SO_PUBLIC struct Thread *
Thread_GetCurrent(void)
{
    pthread_t l_tCurrent;
    struct ThreadListItem *l_pCur;
    struct Thread *l_pRet = NULL;
    l_tCurrent = pthread_self();
    pthread_mutex_lock (&g_listMutex);
    l_pCur = g_pThreadListHead;

    while (l_pCur != NULL)
    {
        if (l_pCur->pThread->iThread == l_tCurrent)
        {
            l_pRet = l_pCur->pThread;
            break;
        }
        l_pCur = l_pCur->pNext;
    }
    pthread_mutex_unlock (&g_listMutex);

    return l_pRet;
}
SO_PUBLIC uint32_t
Thread_getCount (void)
{
    uint32_t num;
    pthread_mutex_lock (&g_listMutex);
    num = g_iThreadCount;
    pthread_mutex_unlock (&g_listMutex);

    return num;
}

/* The following code is the remainder of rzb_thread.c */
#if 0
void
dumpActiveTheads (void)
{
    THREADARGS *t;

    pthread_mutex_lock (&trackingmutex);
    for (t = active_threads; t; t = t->next)
        rzb_log (LOG_EMERG, "Thread %u %s exists\n", t->threadindex,
                 t->ta_description ? t->ta_description : "Unknown");
    pthread_mutex_unlock (&trackingmutex);
}

SO_PUBLIC HRESULT
waitForIdle (unsigned to_secs)
{
    for (; to_secs; to_secs--)
    {
        if (!getActiveTheadCount ())
            return R_SUCCESS;
        sleep (1);
    }

    if (rzb_get_log_level () == LOG_DEBUG)
        dumpActiveTheads ();
    return R_FAIL;
}
#endif
