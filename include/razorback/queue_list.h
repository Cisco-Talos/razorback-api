/** @file queue_list.h
 * A list of queues.
 */
#ifndef	RAZORBACK_QUEUE_LIST_H
#define	RAZORBACK_QUEUE_LIST_H
#include <razorback/queue.h>
#include <razorback/list.h>
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

/** Initializes a queue list
 * @param p_pList the list
 */
extern struct List * QueueList_Create (void);

/** finds a queue in a list
 * @param p_pList the list
 * @param p_pId the id of the queue to find
 * @return a pointer to the queue or null if not found
 */
extern struct Queue *QueueList_Find (struct List *p_pList,
                                     const uuid_t p_pId);

/** adds a queue to a list
 * @param p_pList the list
 * @param p_pId the id of the queue to add
 * @return a pointer to the queue or null if not found
 */
extern bool QueueList_Add (struct List *p_pList,
                           struct Queue *p_pQ, const uuid_t p_pId);

/** Remove a queue to a list
 * @param p_pList the list
 * @param p_pId the id of the queue to add
 * @return a pointer to the queue or null if not found
 */
extern bool QueueList_Remove (struct List *p_pList,
                           const uuid_t p_pId);

/** first entry in a queue list
 * @param p_pList the list
 */
extern struct QueueListEntry *QueueList_First (const struct List
                                               *p_pList);

/** next entry in a queue list
 * @param p_pList the list
 * @param p_pCurrent the current entry
 */
extern struct QueueListEntry *QueueList_Next (const struct List *p_pList,
                                              const struct QueueListEntry
                                              *p_pCurrent);

#endif
