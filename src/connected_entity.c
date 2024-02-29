#include "config.h"

#include <razorback/debug.h>
#include <razorback/connected_entity.h>
#include <razorback/log.h>
#include <razorback/thread.h>

#include <signal.h>
#include <errno.h>

#include "connected_entity_private.h"
#include "runtime_config.h"

struct ConnectedEntityEntry
{
    uint32_t iState;            ///< state of the entity
    uuid_t uuidNuggetId;        ///< identifying uuid of the entity
    uuid_t uuidNuggetType;      ///< identifying uuid of the entity
    uuid_t uuidApplicationType; ///< identifying uuid of the entity
    time_t tTimeOfLastHello;    ///< time-stamp of last hello received
    struct ConnectedEntityEntry *pNext; ///< linked list support
};

struct ConnectedEntityList
{
    struct ConnectedEntityEntry *pHead; ///< the head of the list
    pthread_mutex_t mLock;      ///< The list lock
    uint32_t iCount;            ///< The number of items in the list.
    timer_t tTimer;             ///< The prune timer
};

static struct ConnectedEntityList *sg_pList;
static void ConnectedEntityList_Prune (union sigval val);

bool
ConnectedEntityList_Start (void)
{
    ASSERT (sg_pList == NULL);

    struct sigevent *l_pProps;
    struct itimerspec l_itsTimerSpec;

    if ((sg_pList = calloc (1, sizeof (struct ConnectedEntityList))) == NULL)
    {
        rzb_log (LOG_ERR,
                 "ConnectedEntityList_Start: Failed due to calloc failure");
        return false;
    }
    // set head to NULL
    sg_pList->pHead = NULL;

    pthread_mutex_init (&sg_pList->mLock, NULL);

    // Setup the prune timer to run HelloTime/2
    l_itsTimerSpec.it_value.tv_sec = Config_getHelloTime () / 2;
    l_itsTimerSpec.it_value.tv_nsec = 1;
    l_itsTimerSpec.it_interval.tv_sec = Config_getHelloTime () / 2;
    l_itsTimerSpec.it_interval.tv_nsec = 1;
    if ((l_pProps = calloc (1, sizeof (struct sigevent))) == NULL)
    {
        rzb_log (LOG_ERR,
                 "ConnectedEntityList_Start: Failed to malloc timer properties");
        return false;
    }
    l_pProps->sigev_notify = SIGEV_THREAD;
    l_pProps->sigev_value.sival_ptr = NULL;
    l_pProps->sigev_notify_function = &ConnectedEntityList_Prune;
    if (timer_create (CLOCK_REALTIME, l_pProps, &sg_pList->tTimer) == -1)
    {
        rzb_perror
            ("ConnectedEntityList_Start: Failed call to timer_create: %s");
        free(l_pProps);
        return false;
    }
    if (timer_settime (sg_pList->tTimer, 0, &l_itsTimerSpec, NULL) == -1)
    {
        rzb_perror ("ConnectedEntityList_Start: Failed to arm timer: %s");
        free(l_pProps);
        return false;
    }
    free(l_pProps);
    return true;

    // Thread
}

void
ConnectedEntityList_Stop (void)
{
    struct ConnectedEntityList *l_pList;
    pthread_mutex_lock (&sg_pList->mLock);
    l_pList = sg_pList;
    sg_pList = NULL;
    pthread_mutex_unlock (&l_pList->mLock);

    // temporary storage
    struct ConnectedEntityEntry *l_pTemp;

    // remove all entries
    while (sg_pList->pHead != NULL)
    {
        l_pTemp = l_pList->pHead;
        l_pList->pHead = l_pTemp->pNext;
        free (l_pTemp);
    }

    pthread_mutex_destroy (&l_pList->mLock);
    free (l_pList);
}

/** Return a _LOCKED_ entry or NULL
 */
static struct ConnectedEntityEntry *
ConnectedEntityList_GetEntry (uuid_t p_uuidNuggetId,
                              uuid_t p_uuidNuggetType,
                              uuid_t p_uuidApplicationType)
{
    struct ConnectedEntityEntry *l_pEntry;

    // find him
    for (l_pEntry = sg_pList->pHead; l_pEntry != NULL;
         l_pEntry = l_pEntry->pNext)
    {
        if ((uuid_compare (l_pEntry->uuidNuggetId, p_uuidNuggetId) == 0) &&
            (uuid_compare (l_pEntry->uuidNuggetType, p_uuidNuggetType) == 0)
            &&
            (uuid_compare
             (l_pEntry->uuidApplicationType, p_uuidApplicationType) == 0))
            break;
    }
    return l_pEntry;
}

/** Return a _LOCKED_ entry
 */
static struct ConnectedEntityEntry *
ConnectedEntityList_GetEntryOrCreate (uuid_t p_uuidNuggetId,
                                      uuid_t p_uuidNuggetType,
                                      uuid_t p_uuidApplicationType)
{
    struct ConnectedEntityEntry *l_pEntry;
    l_pEntry =
        ConnectedEntityList_GetEntry (p_uuidNuggetId, p_uuidNuggetType,
                                      p_uuidApplicationType);



    // No Entry - Create One
    if (l_pEntry == NULL)
    {
        if ((l_pEntry =
             calloc (1, sizeof (struct ConnectedEntityEntry))) == NULL)
        {
            pthread_mutex_unlock (&sg_pList->mLock);
            return false;
        }

        uuid_copy (l_pEntry->uuidNuggetId, p_uuidNuggetId);
        uuid_copy (l_pEntry->uuidNuggetType, p_uuidNuggetType);
        uuid_copy (l_pEntry->uuidApplicationType, p_uuidApplicationType);
        l_pEntry->pNext = sg_pList->pHead;
        sg_pList->pHead = l_pEntry;
        sg_pList->iCount++;
    }
    return l_pEntry;
}

SO_PUBLIC bool
ConnectedEntityList_ChangeState (uuid_t p_uuidNuggetId,
                                 uuid_t p_uuidNuggetType,
                                 uuid_t p_uuidApplicationType,
                                 uint32_t p_iState)
{
    ASSERT (sg_pList != NULL);
    pthread_mutex_lock (&sg_pList->mLock);

    // temporary storage
    struct ConnectedEntityEntry *l_pEntry;

    if ((l_pEntry =
         ConnectedEntityList_GetEntryOrCreate (p_uuidNuggetId,
                                               p_uuidNuggetType,
                                               p_uuidApplicationType)) ==
        NULL)
    {
        rzb_log (LOG_ERR,
                 "ConnectedEntityList_ChangeState: Failed due to failure of _GetEntry()");
        pthread_mutex_unlock (&sg_pList->mLock);
        return false;
    }

    l_pEntry->iState = p_iState;
    pthread_mutex_unlock (&sg_pList->mLock);

    return true;
}

SO_PUBLIC bool
ConnectedEntityList_Update (uuid_t p_uuidNuggetId,
                            uuid_t p_uuidNuggetType,
                            uuid_t p_uuidApplicationType)
{
    ASSERT (sg_pList != NULL);
    pthread_mutex_lock (&sg_pList->mLock);

    // temporary storage
    struct ConnectedEntityEntry *l_pEntry = NULL;
    if (sg_pList == NULL)
    {
        rzb_log (LOG_ERR,
                 "ConnectedEntityList_Update: Called before Start or after Stop");
        pthread_mutex_unlock (&sg_pList->mLock);
        return false;
    }

    if ((l_pEntry =
         ConnectedEntityList_GetEntryOrCreate (p_uuidNuggetId,
                                               p_uuidNuggetType,
                                               p_uuidApplicationType)) ==
        NULL)
    {
        rzb_log (LOG_ERR,
                 "ConnectedEntityList_Update: Failed due to failure of _GetEntry()");
        pthread_mutex_unlock (&sg_pList->mLock);
        return false;
    }

    l_pEntry->tTimeOfLastHello = time (NULL);

    pthread_mutex_unlock (&sg_pList->mLock);
    return true;
}

SO_PUBLIC uint32_t
ConnectedEntityList_GetCount (void)
{
    ASSERT (sg_pList != NULL);

    uint32_t l_iCount;
    pthread_mutex_lock (&sg_pList->mLock);
    l_iCount = sg_pList->iCount;
    pthread_mutex_unlock (&sg_pList->mLock);

    return l_iCount;
}

static void
ConnectedEntityList_Prune (union sigval val)
{
    ASSERT (sg_pList != NULL);
    // temporary storage
    struct ConnectedEntityEntry *l_pPrevious;
    struct ConnectedEntityEntry *l_pCurrent;
    struct ConnectedEntityEntry *l_pToFree;
    time_t l_tTimeNow;

    // initialize time
    l_tTimeNow = time (NULL);

    // initialize locals

    time_t l_iDeadTime = Config_getDeadTime ();
    pthread_mutex_lock (&sg_pList->mLock);
    l_pPrevious = NULL;
    l_pCurrent = sg_pList->pHead;
    while (l_pCurrent != NULL)
    {
        if ((l_pCurrent->tTimeOfLastHello > 0 ) && 
                ( l_tTimeNow - l_pCurrent->tTimeOfLastHello > l_iDeadTime))
        {
            // add to removed items list if not null
            // TODO: Fire Event

            // remove this entry
            if (l_pPrevious == NULL)
            {
                sg_pList->pHead = l_pCurrent->pNext;

            }
            else
            {
                l_pPrevious->pNext = l_pCurrent->pNext;
            }
            l_pToFree = l_pCurrent;
            l_pCurrent = l_pCurrent->pNext;

            free (l_pToFree);

        }
        else 
        {
            l_pPrevious = l_pCurrent;
            l_pCurrent = l_pCurrent->pNext;
        }
    }
    pthread_mutex_unlock (&sg_pList->mLock);
}



SO_PUBLIC bool
ConnectedEntityList_GetState (uuid_t p_uuidNuggetId,
                              uuid_t p_uuidNuggetType,
                              uuid_t p_uuidApplicationType,
                              uint32_t * p_pState)
{
    ASSERT (sg_pList != NULL);
    pthread_mutex_lock (&sg_pList->mLock);

    // temporary storage
    struct ConnectedEntityEntry *l_pEntry;
    l_pEntry =
        ConnectedEntityList_GetEntry (p_uuidNuggetId, p_uuidNuggetType,
                                      p_uuidApplicationType);
    if (l_pEntry == NULL)
    {
        pthread_mutex_unlock (&sg_pList->mLock);
        return false;
    }
    *p_pState = l_pEntry->iState;
    pthread_mutex_unlock (&sg_pList->mLock);

    return true;
}
