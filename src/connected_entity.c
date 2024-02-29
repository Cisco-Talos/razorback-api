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
    struct ConnectedEntity entity;
    struct ConnectedEntityEntry *pNext;
};

struct ConnectedEntityList
{
    struct ConnectedEntityEntry *pHead; ///< the head of the list
    pthread_mutex_t mLock;      ///< The list lock
    uint32_t iCount;            ///< The number of items in the list.
    timer_t tTimer;             ///< The prune timer
};

struct ConnectedEntityHook
{
    void (*entityRemoved) (uuid_t uuidNuggetId, uuid_t uuidNuggetType, uuid_t uuidApplicationType, uint32_t remainingCount);
    struct ConnectedEntityHook *pNext;
};

static struct ConnectedEntityList *sg_pList = NULL;
static struct ConnectedEntityHook *sg_pHookHead = NULL;

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
                 "%s: Failed due to calloc failure", __func__);
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
                 "%s: Failed to malloc timer properties", __func__);
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
    if (sg_pList == NULL)
       return;

    timer_delete(sg_pList->tTimer);
    
    pthread_mutex_lock (&sg_pList->mLock);
    l_pList = sg_pList;
    sg_pList = NULL;
    pthread_mutex_unlock (&l_pList->mLock);

    // temporary storage
    struct ConnectedEntityEntry *l_pTemp;

    // remove all entries
    while (l_pList->pHead != NULL)
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
static struct ConnectedEntity *
ConnectedEntityList_GetEntity (uuid_t p_uuidNuggetId)
{
    struct ConnectedEntityEntry *l_pEntry;

    // find him
    for (l_pEntry = sg_pList->pHead; l_pEntry != NULL;
         l_pEntry = l_pEntry->pNext)
    {
        if ((uuid_compare (l_pEntry->entity.uuidNuggetId, p_uuidNuggetId) == 0))
            break;
    }
    if (l_pEntry == NULL)
        return NULL;
    else
        return &l_pEntry->entity;
}

SO_PUBLIC bool
ConnectedEntityList_CreateEntity (uuid_t p_uuidNuggetId,
                                      uuid_t p_uuidNuggetType,
                                      uuid_t p_uuidApplicationType)
{
    struct ConnectedEntity *l_pEntity;
    struct ConnectedEntityEntry *l_pEntry;
    pthread_mutex_lock (&sg_pList->mLock);
    l_pEntity =
        ConnectedEntityList_GetEntity (p_uuidNuggetId);

    // No Entry - Create One
    if (l_pEntity == NULL)
    {
        if ((l_pEntry =
             calloc (1, sizeof (struct ConnectedEntityEntry))) == NULL)
        {
            pthread_mutex_unlock (&sg_pList->mLock);
            return false;
        }

        uuid_copy (l_pEntry->entity.uuidNuggetId, p_uuidNuggetId);
        uuid_copy (l_pEntry->entity.uuidNuggetType, p_uuidNuggetType);
        uuid_copy (l_pEntry->entity.uuidApplicationType, p_uuidApplicationType);
        l_pEntry->pNext = sg_pList->pHead;
        sg_pList->pHead = l_pEntry;
        sg_pList->iCount++;
    }
    pthread_mutex_unlock (&sg_pList->mLock);
    return true;
}

SO_PUBLIC bool
ConnectedEntityList_ChangeState (uuid_t p_uuidNuggetId,
                                 uint32_t p_iState)
{
    ASSERT (sg_pList != NULL);
    pthread_mutex_lock (&sg_pList->mLock);

    // temporary storage
    struct ConnectedEntity *l_pEntity;

    if ((l_pEntity =
         ConnectedEntityList_GetEntity (p_uuidNuggetId)) == NULL)
    {
        rzb_log (LOG_ERR,
                 "%s: Failed due to failure of _GetEntry()", __func__);
        pthread_mutex_unlock (&sg_pList->mLock);
        return false;
    }

    l_pEntity->iState = p_iState;
    pthread_mutex_unlock (&sg_pList->mLock);

    return true;
}

SO_PUBLIC bool
ConnectedEntityList_Update (uuid_t p_uuidNuggetId)
{
    ASSERT (sg_pList != NULL);
    pthread_mutex_lock (&sg_pList->mLock);

    // temporary storage
    struct ConnectedEntity *l_pEntity = NULL;
    if (sg_pList == NULL)
    {
        rzb_log (LOG_ERR,
                 "%s: Called before Start or after Stop", __func__);
        pthread_mutex_unlock (&sg_pList->mLock);
        return false;
    }

    if ((l_pEntity = ConnectedEntityList_GetEntity (p_uuidNuggetId)) == NULL)
    {
        rzb_log (LOG_ERR,
                 "%s: Failed due to failure of _GetEntry()", __func__);
        pthread_mutex_unlock (&sg_pList->mLock);
        return false;
    }

    l_pEntity->tTimeOfLastHello = time (NULL);

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
    struct ConnectedEntityEntry *l_pCountNext;
    struct ConnectedEntityHook *l_pHook;
    uint32_t l_iRemaining = 0;
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
        if ((l_pCurrent->entity.tTimeOfLastHello > 0 ) && 
                ( l_tTimeNow - l_pCurrent->entity.tTimeOfLastHello > l_iDeadTime))
        {
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
            l_iRemaining = 0;
            l_pCountNext = sg_pList->pHead;
            while (l_pCountNext != NULL)
            {
                if ((uuid_compare(l_pCountNext->entity.uuidNuggetType, l_pToFree->entity.uuidNuggetType) == 0) &&
                        (uuid_compare(l_pCountNext->entity.uuidApplicationType, l_pToFree->entity.uuidApplicationType) == 0))
                    l_iRemaining++;

                l_pCountNext = l_pCountNext->pNext;
            }

            l_pHook = sg_pHookHead;

            while (l_pHook != NULL)
            {
                l_pHook->entityRemoved(l_pToFree->entity.uuidNuggetId, l_pToFree->entity.uuidNuggetType, l_pToFree->entity.uuidApplicationType, l_iRemaining);
                l_pHook = l_pHook->pNext;
            }
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
ConnectedEntityList_GetState (uuid_t p_uuidNuggetId, uint32_t * p_pState)
{
    ASSERT (sg_pList != NULL);
    pthread_mutex_lock (&sg_pList->mLock);

    // temporary storage
    struct ConnectedEntity *l_pEntity;
    l_pEntity =
        ConnectedEntityList_GetEntity (p_uuidNuggetId);
    if (l_pEntity == NULL)
    {
        pthread_mutex_unlock (&sg_pList->mLock);
        return false;
    }
    *p_pState = l_pEntity->iState;
    pthread_mutex_unlock (&sg_pList->mLock);

    return true;
}

SO_PUBLIC bool
ConnectedEntityList_AddPruneListener(void (*entityRemoved) (uuid_t uuidNuggetId, uuid_t uuidNuggetType, uuid_t uuidApplicationType, uint32_t remainingCount))
{
    struct ConnectedEntityHook *l_pHook;
    if (sg_pList == NULL)
        return false;

    if ((l_pHook = calloc(1, sizeof(struct ConnectedEntityHook))) == NULL)
    {
        rzb_log(LOG_ERR, "%s: fail to allocate new node", __func__);
        return false;
    }
    l_pHook->entityRemoved = entityRemoved;
    pthread_mutex_lock (&sg_pList->mLock);
    if (sg_pHookHead != NULL)
        l_pHook->pNext = sg_pHookHead;
    
    sg_pHookHead = l_pHook;

    pthread_mutex_unlock (&sg_pList->mLock);
    return true;
}

SO_PUBLIC bool
ConnectedEntityList_ForEach_Entity (bool (*function) (struct ConnectedEntity *))
{
    pthread_mutex_lock (&sg_pList->mLock);
    struct ConnectedEntityEntry *l_pNode = sg_pList->pHead;
    while (l_pNode != NULL)
    {
        if (!function (&l_pNode->entity))
        {
            pthread_mutex_unlock (&sg_pList->mLock);
            return false;
        }
        l_pNode = l_pNode->pNext;
    }
    pthread_mutex_unlock (&sg_pList->mLock);
    return true;
}

