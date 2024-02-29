/** @file request_queue.h
 * RequestQueue functions
 */

#ifndef	RAZORBACK_REQUESTQUEUE_H
#define	RAZORBACK_REQUESTQUEUE_H

#include <razorback/types.h>
#include <razorback/messages.h>

/** Initializes the request queue
 * @param p_iFlags flags
 * @return true on success false on error.
 */
extern bool RequestQueue_Initialize (int p_iFlags);

/** Terminates the request queue
 */
extern void RequestQueue_Terminate ();

/** Gets a message from the request queue
 * @param p_pMessage the message
 * @return true if ok, false if error or timeout (errno==EAGAIN if timeout)
 */
extern bool RequestQueue_Get (struct MessageCacheReq *p_pMessage);

/** Puts a message into the request queue
 * @param p_pMessage the message
 * @return true if ok, false if error
 */
extern bool RequestQueue_Put (struct MessageCacheReq *p_pMessage);


#endif // RAZORBACK_REQUESTQUEUE_H
