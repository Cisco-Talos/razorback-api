#include "config.h"

#include <razorback/debug.h>
#include <razorback/thread.h>
#include <razorback/ntlv.h>
#include <razorback/list.h>
#include <razorback/log.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <signal.h>
#include "runtime_config.h"


static struct List *sg_threadList;

static pthread_attr_t g_attr;
static pthread_once_t g_once_control = PTHREAD_ONCE_INIT;



static void handler(int sig)
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
    sg_threadList = List_Create(LIST_MODE_GENERIC, 
            Thread_Cmp, 
            Thread_KeyCmp, 
            NULL, NULL, NULL, NULL);
}

static void *
Thread_MainWrapper (void *arg)
{
    struct Thread *l_pThread = (struct Thread *) arg;
    List_Lock(sg_threadList);
    pthread_mutex_lock (&l_pThread->mMutex);

    l_pThread->bRunning = true;

    pthread_mutex_unlock (&l_pThread->mMutex);
    List_Unlock(sg_threadList);

    l_pThread->mainFunction (l_pThread);

    List_Lock(sg_threadList);
    pthread_mutex_lock (&l_pThread->mMutex);

    l_pThread->bRunning = false;

    pthread_mutex_unlock (&l_pThread->mMutex);
    List_Unlock(sg_threadList);

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

    pthread_once (&g_once_control, initThreading);
    // Racy 
    if (sg_threadList->length == Config_getThreadLimit ())
        return NULL;

    // allocate memory for thread structure
    l_pThread = calloc (1, sizeof (struct Thread));
    if (l_pThread == NULL)
    {
        rzb_log (LOG_ERR,
                 "%s: Failed to launch thread in Thread_Launch due to out of memory for Thread", __func__);
        return NULL;
    }

    // initialize running indicator
    l_pThread->bRunning = false;
    l_pThread->pContext = p_pContext;
    l_pThread->pUserData = p_pUserData;
    l_pThread->sName = p_sName;
    l_pThread->bShutdown = false;

    l_pThread->mainFunction = p_fpFunction;
    // initialize running mutex
    pthread_mutex_init (&l_pThread->mMutex, NULL);

    // start thread, check for error
    if (pthread_create
        (&l_pThread->iThread, &g_attr, Thread_MainWrapper, l_pThread) != 0)
    {
        free (l_pThread);
        rzb_log (LOG_ERR,
                 "%s: Failed to launch thread in Thread_Launch due to pthread_create error (%i)", __func__,
                 errno);
        return NULL;
    }
    List_Push(sg_threadList, l_pThread);

    // done
    return l_pThread;
}

SO_PUBLIC void
Thread_Destroy (struct Thread *p_pThread)
{
    List_Remove(sg_threadList, p_pThread);
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
    struct Thread *l_pRet = NULL;
    l_tCurrent = pthread_self();
    l_pRet = (struct Thread *)List_Find(sg_threadList, &l_tCurrent);

    return l_pRet;
}
SO_PUBLIC uint32_t
Thread_getCount (void)
{
    uint32_t num;
    num = sg_threadList->length;
    return num;
}

SO_PUBLIC int 
Thread_KeyCmp(void *a, void *id)
{
    struct Thread * cA = (struct Thread *)a;
    pthread_t cId = *(pthread_t *)id;
    if (cId == cA->iThread)
        return 0;
    else
        return -1;

}

SO_PUBLIC int 
Thread_Cmp(void *a, void *b)
{
    struct Thread * cA = (struct Thread *)a;
    struct Thread * cB = (struct Thread *)b;
    if (a==b)
        return 0;
    if (cA->iThread == cB->iThread)
        return 0;
    else 
        return -1;
}

