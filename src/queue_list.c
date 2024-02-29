#include "config.h"
#include <razorback/debug.h>
#include <razorback/queue_list.h>
#include <razorback/log.h>
#include <string.h>

SO_PUBLIC void
QueueList_Initialize (struct QueueList *p_pList)
{
    ASSERT (p_pList != NULL);

    pthread_mutex_init (&p_pList->mutex, NULL);
    p_pList->pHead = NULL;
}

SO_PUBLIC void
QueueList_Terminate (struct QueueList *p_pList)
{
    ASSERT (p_pList != NULL);

    pthread_mutex_destroy (&p_pList->mutex);
    struct QueueListEntry *l_pEntry;

    while (p_pList->pHead != NULL)
    {
        l_pEntry = p_pList->pHead->pNext;
        free (p_pList->pHead);
        p_pList->pHead = l_pEntry;
    }
}

SO_PUBLIC struct Queue *
QueueList_Find (struct QueueList *p_pList, const uuid_t p_pId)
{
    ASSERT (p_pList != NULL);
    ASSERT (p_pId != NULL);

    struct QueueListEntry *l_pEntry;

    pthread_mutex_lock (&p_pList->mutex);

    for (l_pEntry = p_pList->pHead; l_pEntry != NULL;
         l_pEntry = l_pEntry->pNext)
        if (uuid_compare (p_pId, l_pEntry->uuiKey) == 0)
            break;

    pthread_mutex_unlock (&p_pList->mutex);

    if (l_pEntry == NULL)
        return NULL;
    return l_pEntry->pQueue;
}

SO_PUBLIC bool
QueueList_Add (struct QueueList * p_pList, struct Queue * p_pQ,
               const uuid_t p_pId)
{
    ASSERT (p_pList != NULL);
    ASSERT (p_pId != NULL);

    struct QueueListEntry *l_pEntry;

    pthread_mutex_lock (&p_pList->mutex);

    l_pEntry =
        (struct QueueListEntry *) malloc (sizeof (struct QueueListEntry));
    if (l_pEntry != NULL)
    {
        uuid_copy (l_pEntry->uuiKey, p_pId);
        l_pEntry->pQueue = p_pQ;
        l_pEntry->pNext = p_pList->pHead;
        p_pList->pHead = l_pEntry;
    }
    pthread_mutex_unlock (&p_pList->mutex);


    if (l_pEntry == NULL)
    {
        rzb_log (LOG_ERR, "%s: failed due to lack of memory", __func__);
        return false;
    }

    return true;
}

SO_PUBLIC void
QueueList_Lock (struct QueueList *p_pList)
{

    pthread_mutex_lock (&p_pList->mutex);
}

SO_PUBLIC void
QueueList_Unlock (struct QueueList *p_pList)
{
    pthread_mutex_unlock (&p_pList->mutex);
}

SO_PUBLIC struct QueueListEntry *
QueueList_First (const struct QueueList *p_pList)
{
    ASSERT (p_pList != NULL);

    return p_pList->pHead;
}

SO_PUBLIC struct QueueListEntry *
QueueList_Next (const struct QueueList *p_pList,
                const struct QueueListEntry *p_pCurrent)
{
    ASSERT (p_pList != NULL);
    ASSERT (p_pCurrent != NULL);

    return p_pCurrent->pNext;
}
