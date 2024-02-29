/** @file judgment_queue.h
 * JudgmentQueue functions
 */

#ifndef	RAZORBACK_JUDGMENTQUEUE_H
#define	RAZORBACK_JUDGMENTQUEUE_H

#include <razorback/types.h>
#include <razorback/queue.h>
#include <razorback/messages.h>


/** Initializes the judgment queue
 * @param p_iFlags flags
 * @return a pointer to the queue or null on error.
 */
extern struct Queue * JudgmentQueue_Initialize (int p_iFlags);

/** Terminates the judgment queue
 */
extern void JudgmentQueue_Terminate (struct Queue *);

/** Gets a message from the judgment queue
 * @param p_pMessage the message
 * @return true if ok, false if error or timeout (errno==EAGAIN if timeout)
 */
extern struct MessageJudgmentSubmission * JudgmentQueue_Get (struct Queue *);

/** Puts a message into the judgment queue
 * @param p_pMessage the message
 * @return true if ok, false if error
 */
extern bool JudgmentQueue_Put (struct Queue *, struct MessageJudgmentSubmission
                               *p_pMessage);


#endif // RAZORBACK_JUDGMENTQUEUE_H
