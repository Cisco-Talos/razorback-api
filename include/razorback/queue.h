/** @file queue.h
 * Queue structures and functions
 * IMPLEMENTS:
 * STOMP PROTOCOL
 * Creative Commons Attribution v2.5
 * http://stomp.codehaus.org/Protocol
 */

#ifndef	RAZORBACK_QUEUE_H
#define	RAZORBACK_QUEUE_H
#include <razorback/visibility.h>
#include <razorback/types.h>
#include <razorback/socket.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COMMAND_QUEUE "/topic/COMMAND"
#define REQUEST_QUEUE "/queue/REQUEST"
#define LOG_QUEUE "/queue/LOG"
#define INPUT_QUEUE "/queue/INPUT"
#define JUDGMENT_QUEUE "/queue/JUDGMENT"

/** Queue
 * an attachment to a distributed message queue
 */
struct Queue
{
    struct Socket *pReadSocket;    ///< Read socket
    struct Socket *pWriteSocket;   ///< Write socket
    char *sName;                   ///< Queue name
    int iFlags;                    ///< Flags (read/write/etc)
    int mode;                      ///< Message processing mode (Binary/JSON)
    Mutex_t *mReadMutex;      ///< Read Lock
    Mutex_t *mWriteMutex;     ///< Write lock
    char *sHostname;               ///< Broker hostname
    uint32_t iPort;                ///< Broker port
    char *sUser;                   ///< Broker username
    char *sPassword;               ///< Broker password
    bool bUseSSL;                  ///< Use SSL connection to broker
    char *sSubscriptionId;         ///< The subscription ID
};

#define QUEUE_FLAG_SEND 0x01
#define QUEUE_FLAG_RECV 0x02
#define QUEUE_FLAG_EXTERNAL_MODE 0x04

/** Initializes the queue
 * This method connects to the message broker provided in the API configuration file.
 * @param *p_sQueueName the name of the queue
 * @param *p_iFlags Flags for the connection (read/write mode, etc)
 * @param mode Message processing mode (BINARY/JSON/etc)
 * @return a pointer to a new Queue Struct or NULL on error.
 */
SO_PUBLIC extern struct Queue *Queue_Create (const char * p_sQueueName,
                                       int p_iFlags, int mode);
/** Initializes the queue
 * This method allows connections to other external message brokers if required.
 * @param *p_sQueueName the name of the queue
 * @param *p_iFlags Flags for the connection (read/write mode, etc)
 * @param mode Message processing mode (BINARY/JSON/etc)
 * @param *p_sHost Host name of the broker
 * @param p_iPort Port number to connect to
 * @param *p_sUser User name to connect to the broker with
 * @param *p_sPassword Password to connect to the broker with
 * @param p_bUseSSL Enable SSL for the broker connection
 * @return a pointer to a new Queue Struct or NULL on error.
 */
SO_PUBLIC extern struct Queue *Queue_Create_With_Host (
    const char * p_sQueueName,
    int p_iFlags,
    int mode,
    const char * p_sHost,
    uint32_t p_iPort,
    const char * p_sUser,
    const char * p_sPassword,
    bool p_bUseSSL
    );

/** Terminates the queue
 * @param p_pQ the queue to terminate
 */
SO_PUBLIC extern void Queue_Terminate (struct Queue *p_pQ);

/** Gets a message from the queue
 * @param queue the queue
 * @return A message struct, NULL on error
 */
SO_PUBLIC extern struct Message *Queue_Get (struct Queue *queue);

/** Sends a message to the queue
 * @param queue the queue
 * @param *message the message to send
 * @return true if ok, false if error or timeout (errno==EAGAIN if timeout)
 */
SO_PUBLIC extern bool Queue_Put (struct Queue *queue, struct Message *message);

/** Sends a message to the queue overriding the default destination
 * @param queue the queue
 * @param *message the message to send
 * @param *dest the destination queue/topic name
 * @return true if ok, false if error or timeout (errno==EAGAIN if timeout)
 */SO_PUBLIC extern bool Queue_Put_Dest (struct Queue *queue, struct Message *message, char *dest);

/** Gets a queue name from a uuid_t
 * @param p_sLeading the leading text
 * @param p_pId the uuid
 * @param p_sQueueName the destination text
 */
SO_PUBLIC extern void Queue_GetQueueName (const char * p_sLeading,
                                uuid_t p_pId, char * p_sQueueName);
#ifdef __cplusplus
}
#endif
#endif // RAZORBACK_QUEUE_H
