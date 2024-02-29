/** @file log_queue.h
 * LogQueue functions
 */

#ifndef	RAZORBACK_LOGQUEUE_H
#define	RAZORBACK_LOGQUEUE_H

#include <razorback/types.h>
#include <razorback/messages.h>

/** Initializes the logqueue
 * @param p_iFlags flags
 * @return true on success false on error.
 */
extern bool LogQueue_Initialize (int p_iFlags);

/** Terminates the log queue
 */
extern void LogQueue_Terminate ();

/** Gets a message from the log queue
 * @param p_pMessage the message
 * @return true if ok, false if error or timeout (errno==EAGAIN if timeout)
 */
extern struct MessageLogSubmission * LogQueue_Get (void);

/** Puts a message into the log queue
 * @param p_pMessage the message
 * @return true if ok, false if error
 */
extern bool LogQueue_Put (struct MessageLogSubmission *p_pMessage);


#endif // RAZORBACK_LOGQUEUE_H
