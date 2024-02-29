/** @file alert_queue.h
 * AlertQueue functions
 */

#ifndef	RAZORBACK_ALERTQUEUE_H
#define	RAZORBACK_ALERTQUEUE_H

#include <razorback/types.h>
#include <razorback/messages.h>

/** Initializes the alert queue
 * @param p_iFlags queue flags.
 * @return true if ok, false if error
 */
extern bool AlertQueue_Initialize (int p_iFlags);

/** Terminates the alert queue
 */
extern void AlertQueue_Terminate (void);

/** Gets a message from the alert queue
 * @param p_pMessage the message
 * @return true if ok, false if error or timeout (errno==EAGAIN if timeout)
 */
extern struct MessageAlert * AlertQueue_Get (void);

/** Puts a message into the alert queue
 * @param p_pMessage the message
 * @return true if ok, false if error
 */
extern bool AlertQueue_Put (struct MessageAlert *p_pMessage);


#endif // RAZORBACK_ALERTQUEUE_H
