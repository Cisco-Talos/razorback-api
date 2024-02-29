/** @file response_queue.h
 * ResponseQueue functions
 */

#ifndef	RAZORBACK_RESPONSEQUEUE_H
#define	RAZORBACK_RESPONSEQUEUE_H

#include <razorback/types.h>
#include <razorback/messages.h>
/** Initializes the response queue
 * @param p_pCollectorId the queue to add it to
 * @param p_iFlags falgs
 * @return a pointer to the queue or null on error.
 */
extern struct Queue * ResponseQueue_Initialize (uuid_t p_pCollectorId, int p_iFlags);

/** Terminates the response queue
 */
extern void ResponseQueue_Terminate (uuid_t p_pCollectorId);

/** Gets a message from the response queue
 * @param p_pMessage the message
 * @param p_pCollectorId the queue to add it to
 * @return true if ok, false if error or timeout (errno==EAGAIN if timeout)
 */
extern bool ResponseQueue_Get (struct Queue *p_pQueue, struct MessageCacheResp *p_pMessage);

/** Puts a message into the response queue
 * @param p_pMessage the message
 * @param p_pCollectorId the queue to add it to
 * @return true if ok, false if error
 */
extern bool ResponseQueue_Put (struct MessageCacheResp *p_pMessage,
                               uuid_t p_pCollectorId);


#endif // RAZORBACK_RESPONSEQUEUE_H
