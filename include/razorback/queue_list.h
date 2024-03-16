/** @file queue_list.h
 * A list of queues.
 */
#ifndef	RAZORBACK_QUEUE_LIST_H
#define	RAZORBACK_QUEUE_LIST_H
#include <razorback/visibility.h>
#include <razorback/queue.h>
#include <razorback/list.h>

#ifdef __cplusplus
extern "C" {
#endif

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
SO_PUBLIC extern List_t * QueueList_Create (void);

/** finds a queue in a list
 * @param p_pList the list
 * @param p_pId the id of the queue to find
 * @return a pointer to the queue or null if not found
 */
SO_PUBLIC extern struct Queue *QueueList_Find (List_t *p_pList,
                                     const uuid_t p_pId);

/** adds a queue to a list
 * @param p_pList the list
 * @param p_pId the id of the queue to add
 * @return a pointer to the queue or null if not found
 */
SO_PUBLIC extern bool QueueList_Add (List_t *p_pList,
                           struct Queue *p_pQ, const uuid_t p_pId);

/** Remove a queue to a list
 * @param p_pList the list
 * @param p_pId the id of the queue to add
 * @return a pointer to the queue or null if not found
 */
SO_PUBLIC extern bool QueueList_Remove (List_t *p_pList,
                           const uuid_t p_pId);

/** first entry in a queue list
 * @param p_pList the list
 */
SO_PUBLIC extern struct QueueListEntry *QueueList_First (const List_t *p_pList);

/** next entry in a queue list
 * @param p_pList the list
 * @param p_pCurrent the current entry
 */
SO_PUBLIC extern struct QueueListEntry *QueueList_Next (const List_t *p_pList,
                                              const struct QueueListEntry
                                              *p_pCurrent);
#ifdef __cplusplus
}
#endif
#endif
