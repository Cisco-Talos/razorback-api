/** @file judgment_queue.h
 * JudgmentQueue functions
 */

#ifndef	RAZORBACK_JUDGMENTQUEUE_H
#define	RAZORBACK_JUDGMENTQUEUE_H

#include <razorback/types.h>
#include <razorback/messages.h>


/** Initializes the judgment queue
 * @param p_pDispatcherId the id of the dispatcher
 * @param p_iFlags flags
 * @return a pointer to the queue or null on error.
 */
extern struct Queue * JudgmentQueue_Initialize (uuid_t p_pDispatcherId,
                                      int p_iFlags);

/** Terminates the judgment queue
 */
extern void JudgmentQueue_Terminate (uuid_t p_pDispatcherId);

/** Gets a message from the judgment queue
 * @param p_pQueue the queue to get the message form
 * @param p_pMessage the message
 * @return true if ok, false if error or timeout (errno==EAGAIN if timeout)
 */
extern bool JudgmentQueue_Get (struct Queue *p_pQueue, struct MessageJudgmentSubmission *p_pMessage);

/** Puts a message into the judgment queue
 * @param p_pMessage the message
 * @param p_pDispatcherId the id of the dispatcher
 * @return true if ok, false if error
 */
extern bool JudgmentQueue_Put (struct MessageJudgmentSubmission
                               *p_pMessage, uuid_t p_pDispatcherId);


#endif // RAZORBACK_JUDGMENTQUEUE_H
