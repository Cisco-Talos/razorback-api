/** @file request_queue.h
 * RequestQueue functions
 */

#ifndef	RAZORBACK_REQUESTQUEUE_H
#define	RAZORBACK_REQUESTQUEUE_H

#include <razorback/types.h>
#include <razorback/messages.h>

/** Initializes the request queue
 * @param p_pDispatcherId the queue to add it to
 * @param p_iFlags flags
 * @return a pointer to the queue or NULL on error.
 */
extern struct Queue * RequestQueue_Initialize (uuid_t p_pDispatcherId, int p_iFlags);

/** Terminates the request queue
 */
extern void RequestQueue_Terminate (uuid_t p_pDispatcherId);

/** Gets a message from the request queue
 * @param p_pMessage the message
 * @param p_pDispatcherId the queue to add it to
 * @return true if ok, false if error or timeout (errno==EAGAIN if timeout)
 */
extern bool RequestQueue_Get (struct Queue *p_pQueue, struct MessageCacheReq *p_pMessage);

/** Puts a message into the request queue
 * @param p_pMessage the message
 * @param p_pDispatcherId the queue to add it to
 * @return true if ok, false if error
 */
extern bool RequestQueue_Put (struct MessageCacheReq *p_pMessage,
                              uuid_t p_pDispatcherId);


#endif // RAZORBACK_REQUESTQUEUE_H
