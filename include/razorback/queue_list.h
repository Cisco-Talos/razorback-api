/** @file queue_list.h
 * A list of queues.
 */
#ifndef	RAZORBACK_QUEUE_LIST_H
#define	RAZORBACK_QUEUE_LIST_H
#include <razorback/queue.h>
#include <pthread.h>

/** QueueListEntry
 * an entry in a list of queues
 */
struct QueueListEntry
{
    struct Queue *pQueue;       ///< the queue
    uuid_t uuiKey;              ///< a key for the queue
    struct QueueListEntry *pNext;   ///< the next item in the list
};

/** QueueList
 * a list of queues
 */
struct QueueList
{
    pthread_mutex_t mutex;      ///< The list lock
    struct QueueListEntry *pHead;   ///< the head of the list
};

/** Initializes a queue list
 * @param p_pList the list
 */
extern void QueueList_Initialize (struct QueueList *p_pList);

/** Destroys a queue list
 * @param p_pList the list
 */
extern void QueueList_Terminate (struct QueueList *p_pList);

/** finds a queue in a list
 * @param p_pList the list
 * @param p_pId the id of the queue to find
 * @return a pointer to the queue or null if not found
 */
extern struct Queue *QueueList_Find (struct QueueList *p_pList,
                                     const uuid_t p_pId);

/** adds a queue to a list
 * @param p_pList the list
 * @param p_pId the id of the queue to add
 * @return a pointer to the queue or null if not found
 */
extern bool QueueList_Add (struct QueueList *p_pList,
                           struct Queue *p_pQ, const uuid_t p_pId);

/** lock a queue list
 * @param p_pList the list
 */
extern void QueueList_Lock (struct QueueList *p_pList);

/** unlock a queue list
 * @param p_pList the list
 */
extern void QueueList_Unlock (struct QueueList *p_pList);

/** first entry in a queue list
 * @param p_pList the list
 */
extern struct QueueListEntry *QueueList_First (const struct QueueList
                                               *p_pList);

/** next entry in a queue list
 * @param p_pList the list
 * @param p_pCurrent the current entry
 */
extern struct QueueListEntry *QueueList_Next (const struct QueueList *p_pList,
                                              const struct QueueListEntry
                                              *p_pCurrent);

#endif
