/** @file command_queue.h
 * CommandQueue functions
 */

#ifndef	RAZORBACK_COMMANDQUEUE_H
#define	RAZORBACK_COMMANDQUEUE_H


#include <razorback/types.h>
#include <razorback/messages.h>
#include <razorback/queue.h>


/** Initializes the command queue
 * @return true if ok, false if error
 */
extern struct Queue * CommandQueue_Initialize (int flags);
/** Terminates the command queue
 */
extern void CommandQueue_Terminate (struct Queue *);

/** Gets a message from the command queue
 * @param p_pMessage the message
 * @return true if ok, false if error or timeout (errno==EAGAIN if timeout)
 */
extern bool CommandQueue_Get (struct Queue *, union CCMessage *);

/** Puts a message into the command queue
 * @param p_pMessage the message
 * @return true if ok, false if error
 */
extern bool CommandQueue_Put (struct Queue *, union CCMessage *p_pMessage);

/// @}

#endif // RAZORBACK_COMMANDQUEUE_H
