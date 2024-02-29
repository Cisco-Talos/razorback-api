/** @file queue.h
 * Queue structures and functions
 * IMPLEMENTS:
 * STOMP PROTOCOL
 * Creative Commons Attribution v2.5
 * http://stomp.codehaus.org/Protocol
 */

#ifndef	RAZORBACK_QUEUE_H
#define	RAZORBACK_QUEUE_H
#include <razorback/types.h>
#include <razorback/socket.h>
#include <razorback/binary_buffer.h>
#include <pthread.h>

/** Queue
 * an attachment to a distributed message queue
 */
struct Queue
{
    struct Socket *pReadSocket;      ///< the socket for this queue
    struct Socket *pWriteSocket;      ///< the socket for this queue
    char *sName;                ///< The Queue name
    int iFlags;              ///< flags
    pthread_mutex_t mReadMutex; ///< The Read Lock
    pthread_mutex_t mWriteMutex; ///< The write lock
};

#define QUEUE_FLAG_SEND 0x01
#define QUEUE_FLAG_RECV 0x02

/** Initializes the queue
 * @param *p_sQueueName the name of the queue
 * @param *p_bSubscribe whether you need to read from the queue
 * @return a pointer to a new Queue Struct or NULL on error.
 */
extern struct Queue *Queue_Create (const uint8_t * p_sQueueName,
                                       int p_iFlags);

/** Terminates the queue
 * @param p_pQ the queue to terminate
 */
extern void Queue_Terminate (struct Queue *p_pQ);

/** Gets a binary buffer from the queue
 * @param p_pQ the queue
 * @param p_pBuffer the binary buffer
 * @return true if ok, false if error or timeout (errno==EAGAIN if timeout)
 */
extern struct BinaryBuffer *Queue_Get (struct Queue *p_pQ);

/** Puts a binary buffer into the queue
 * @param p_pQueue the queue
 * @param p_pBuffer the binary buffer
 * @return true if ok, false if error or timeout (errno==EAGAIN if timeout)
 */
extern bool Queue_Put (struct Queue *p_pQueue, struct BinaryBuffer *p_pBuffer);

/** Gets a queue name from a uuid_t
 * @param p_sLeading the leading text
 * @param p_pId the uuid
 * @param p_sQueueName the destination text
 */
extern void Queue_GetQueueName (const uint8_t * p_sLeading,
                                uuid_t p_pId, uint8_t * p_sQueueName);

#endif // RAZORBACK_QUEUE_H
