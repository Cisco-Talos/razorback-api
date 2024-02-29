/** @file input_queue.h
 * InputQueue functions
 */

#ifndef	RAZORBACK_INPUT_QUEUE_H
#define	RAZORBACK_INPUT_QUEUE_H

#include <razorback/types.h>
#include <razorback/queue.h>
#include <razorback/messages.h>

/** Initializes the input queue
 * @param p_iFlags Flags
 * @return true if ok, false if error
 */
extern struct Queue * InputQueue_Initialize (int p_iFlags);

/** Terminates the input queue
 */
extern void InputQueue_Terminate (struct Queue *);

/** Gets a message from the input queue
 * @param p_pMessage the message
 * @return true if ok, false if error or timeout (errno==EAGAIN if timeout)
 */
extern struct MessageBlockSubmission * InputQueue_Get (struct Queue *);

/** Puts a message into the input queue
 * @param p_pMessage the message
 * @return true if ok, false if error
 */
extern bool InputQueue_Put (struct Queue *, struct MessageBlockSubmission *p_pMessage);


#endif // RAZORBACK_INPUT_QUEUE_H
