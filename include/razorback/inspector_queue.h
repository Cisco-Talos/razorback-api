/** @file inspector_queue.h
 * InspectorQueue functions
 */

#ifndef	RAZORBACK_INSPECTORQUEUE_H
#define	RAZORBACK_INSPECTORQUEUE_H

#include <razorback/types.h>
#include <razorback/messages.h>

/** Initializes the inspector queue
 * @param p_iFlags flags
 * @param p_pApplicationType the application type
 * @return a pointer to the Queue object or NULL on an error.
 */
extern struct Queue * InspectorQueue_Initialize (uuid_t p_pApplicationType,
                                       int p_iFlags);

/** Terminates the inspector queue
 */
extern void InspectorQueue_Terminate (uuid_t p_pApplicationType);

/** Gets a message from the inspector queue
 * @param p_pQueue the queue to read the message from.
 * @param p_pMessage the message
 * @return true if ok, false if error or timeout (errno==EAGAIN if timeout)
 */
extern bool InspectorQueue_Get (struct Queue *p_pQueue, struct MessageInspectionSubmission
                                *p_pMessage);

/** Puts a message into the inspector queue
 * @param p_pQueue the queue to write the message to.
 * @param p_pMessage the message
 * @return true if ok, false if error
 */
extern bool InspectorQueue_Put (struct Queue *p_pQueue, struct MessageInspectionSubmission
                                *p_pMessage);

/// @}

#endif // RAZORBACK_INSPECTORQUEUE_H
