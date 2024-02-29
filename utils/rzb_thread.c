#include "config.h"

#include <signal.h>
#include <pthread.h>
#include <stdio.h>

#include "rzb_thread.h"
#include "rzb_utils.h"

#define maxthreads 100

static volatile unsigned numthreads = 0;
static THREADARGS *active_threads = NULL;

static pthread_mutex_t trackingmutex = PTHREAD_MUTEX_INITIALIZER;

static pthread_attr_t attr;
static pthread_once_t once_control = PTHREAD_ONCE_INIT;
static unsigned thread_index;

static void initThreading(void)
{
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
}

SO_PUBLIC HRESULT threadme(rzb_thread_func_t fp, THREADARGS *threadargs, const char *description)
{
    pthread_t thread_id;
    HRESULT retval;
    int rval;

    pthread_once(&once_control, initThreading);

    do
    {
        pthread_mutex_lock(&trackingmutex);
        if (numthreads < maxthreads)
        {
            sigset_t mask;

            sigemptyset(&mask);
            sigaddset(&mask, SIGTERM);
            sigaddset(&mask, SIGQUIT);
            sigaddset(&mask, SIGINT);
            sigaddset(&mask, SIGHUP);
            sigaddset(&mask, SIGUSR1);
            sigaddset(&mask, SIGUSR2);
            sigaddset(&mask, SIGALRM);
            pthread_sigmask(SIG_SETMASK, &mask, NULL);
            threadargs->threadindex = thread_index++;
            threadargs->next = active_threads;
            active_threads = threadargs;
            rval = pthread_create(&thread_id, &attr, (void *(*)(void *))fp, threadargs);
            sigemptyset(&mask);
            pthread_sigmask(SIG_SETMASK, &mask, NULL);
            if (!rval)
            {
                numthreads++;
                retval = R_SUCCESS;
            }
            else
            {
                free(threadargs);
                printf("pthread_create() failed! (retval=%d) ", rval);
                retval = R_FAIL;
            }
        }
        else
        {
            printf("Maximum thread count of %u reached\n", numthreads);
            retval = R_BUSY;
        }
        pthread_mutex_unlock(&trackingmutex);
        if (retval == R_BUSY)
            sleep(1);
    } while (retval == R_BUSY);
    return retval;
}

SO_PUBLIC void unthreadme(THREADARGS *threadargs)
{
    THREADARGS **pt;
    pthread_mutex_lock(&trackingmutex);
    for (pt = &active_threads; *pt && *pt != threadargs; pt = &(*pt)->next);
    if (*pt)
        *pt = (*pt)->next;
    else
        fprintf(stderr, "Failed to find %p in the active thread list\n", (void *)threadargs);
    numthreads--;
    pthread_mutex_unlock(&trackingmutex);

    free(threadargs);
}

unsigned getActiveTheadCount(void)
{
    unsigned num;
    pthread_mutex_lock(&trackingmutex);
    num = numthreads;
    pthread_mutex_unlock(&trackingmutex);

    return num;
}

void dumpActiveTheads(void)
{
    THREADARGS *t;

    pthread_mutex_lock(&trackingmutex);
    for (t = active_threads; t; t = t->next)
        fprintf(stderr, "Thread %u %s exists\n", t->threadindex, t->ta_description ? t->ta_description:"Unknown");
    pthread_mutex_unlock(&trackingmutex);
}

SO_PUBLIC HRESULT waitForIdle(unsigned to_secs)
{
    for (; to_secs; to_secs--)
    {
        if (!getActiveTheadCount())
            return R_SUCCESS;
        sleep(1);
    }

    if (rzb_debug)
        dumpActiveTheads();
    return R_FAIL;
}

