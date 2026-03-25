/*
 * Copyright (c) 2011-2026 Cisco Systems, Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *  MA 02110-1301, USA.
 */

/** @file queue.h
 * Queue structures and functions
 * IMPLEMENTS:
 * STOMP PROTOCOL
 * Creative Commons Attribution v2.5
 * http://stomp.codehaus.org/Protocol
 */

#ifndef RAZORBACK_QUEUE_H
#define RAZORBACK_QUEUE_H
#include <razorback/visibility.h>
#include <razorback/types.h>
#include <razorback/socket.h>
#include <razorback/timer.h>
#ifdef __cplusplus
extern "C" {
#endif

#define COMMAND_QUEUE "COMMAND"
#define REQUEST_QUEUE "REQUEST"
#define INPUT_QUEUE "INPUT"
#define JUDGMENT_QUEUE "JUDGMENT"

typedef struct _AMQP_Socket AMQP_Socket_t;

/** Queue
 * an attachment to a distributed message queue
 */
struct Queue
{
    AMQP_Socket_t *pReadSocket;     ///< Socket for consuming messages
    AMQP_Socket_t *pWriteSocket;    ///< Socket for sending messages
    char *sName;                    ///< Queue name
    int iFlags;                     ///< Flags (read/write/etc)
    Mutex_t *mReadMutex;            ///< Read Lock
    Mutex_t *mWriteMutex;           ///< Write lock
    const char *sHostname;          ///< Broker hostname
    const char *sVhost;             ///< Broker virtual host
    uint32_t iPort;                 ///< Broker port
    const char *sUser;              ///< Broker username
    const char *sPassword;          ///< Broker password
    bool bUseSSL;                   ///< Use SSL connection to broker
    uint32_t iPrefetch;             ///< Prefetch count
    bool bTopic;                    ///< Is this a topic (vs queue)
    struct Timer *pWriteHeartbeat;  ///< Write heartbeat timer
    bool bShuttingDown;             ///< Is queue teardown in progress
};

#define QUEUE_FLAG_SEND 0x01
#define QUEUE_FLAG_RECV 0x02
#define QUEUE_FLAG_EXTERNAL_MODE 0x04

/**
 * Initializes the queue.
 * This method connects to the message broker provided in the API configuration file.
 * @param p_sQueueName the name of the queue.
 * @param p_bTopic true if this is a topic, false if a queue.
 * @param p_iFlags Flags for the connection (read/write mode, etc).
 * @return a pointer to a new Queue Struct or NULL on error.
 */
SO_PUBLIC extern struct Queue * Queue_Create(
    const char * p_sQueueName,
    bool p_bTopic,
    int p_iFlags
);

/**
 * Initializes the queue.
 * This method allows connections to other external message brokers if required.
 * @param p_sQueueName the name of the queue.
 * @param p_bTopic true if this is a topic, false if a queue.
 * @param p_iFlags Flags for the connection (read/write mode, etc).
 * @param p_sHost Host name of the broker.
 * @param p_iPort Port number to connect to.
 * @param p_sUser User name to connect to the broker with.
 * @param p_sPassword Password to connect to the broker with.
 * @param p_sVhost Virtual host to connect to on the broker.
 * @param p_bUseSSL Enable SSL for the broker connection.
 * @param p_iPrefetch Prefetch value.
 * @return a pointer to a new Queue Struct or NULL on error.
 */
SO_PUBLIC extern struct Queue * Queue_Create_With_Host(
    const char * p_sQueueName,
    bool p_bTopic,
    int p_iFlags,
    const char * p_sHost,
    uint32_t p_iPort,
    const char * p_sUser,
    const char * p_sPassword,
    const char * p_sVhost,
    bool p_bUseSSL,
    uint32_t p_iPrefetch
);

/**
 * Terminates the queue.
 * @param p_pQ the queue to terminate.
 * @return No return value.
 */
SO_PUBLIC extern void Queue_Terminate(struct Queue *p_pQ);

/**
 * Gets a message from the queue.
 * @param queue the queue.
 * @return A message struct, NULL on error.
 */
SO_PUBLIC extern struct Message * Queue_Get(struct Queue *queue);

/**
 * Gets a message from the queue with configurable ack behavior.
 * @param queue the queue.
 * @param autoAck true to ack before returning, false to leave the delivery pending.
 * @param timeoutMilliseconds timeout for the receive poll. A value of 0 blocks indefinitely.
 * @return A message struct, NULL on error or timeout (errno==EAGAIN on timeout).
 */
SO_PUBLIC extern struct Message * Queue_Get_Ex(
    struct Queue *queue,
    bool autoAck,
    uint32_t timeoutMilliseconds
);

/**
 * Acknowledge a message previously received with deferred ack enabled.
 * @param queue the queue.
 * @param message the received message.
 * @return true on success, false on failure.
 */
SO_PUBLIC extern bool Queue_Ack_Message(struct Queue *queue, struct Message *message);

/**
 * Reject a message previously received with deferred ack enabled.
 * @param queue the queue.
 * @param message the received message.
 * @param requeue true to request broker redelivery, false to discard.
 * @return true on success, false on failure.
 */
SO_PUBLIC extern bool Queue_Reject_Message(
    struct Queue *queue,
    struct Message *message,
    bool requeue
);

/**
 * Settle a message previously received with deferred ack enabled.
 * @param queue the queue.
 * @param message the received message.
 * @param ackMessage true to ack the delivery, false to reject it.
 * @param requeueMessage when rejecting, true to request broker redelivery.
 * @return true on success, false on failure.
 */
SO_PUBLIC extern bool Queue_Settle_Message(
    struct Queue *queue,
    struct Message *message,
    bool ackMessage,
    bool requeueMessage
);

/**
 * Sends a message to the queue.
 * @param queue the queue.
 * @param message the message to send.
 * @return true if ok, false if error or timeout (errno==EAGAIN if timeout).
 */
SO_PUBLIC extern bool Queue_Put(struct Queue *queue, struct Message *message);

/**
 * Send a message to the queue while overriding the default destination.
 * @param queue The queue.
 * @param message The message to send.
 * @param dest Destination queue or topic name.
 * @return true if ok, false if error or timeout (errno==EAGAIN if timeout).
 */
SO_PUBLIC extern bool Queue_Put_Dest(
    struct Queue *queue,
    struct Message *message,
    const char *dest
);

/**
 * Gets a queue name from a uuid_t.
 * @param p_sLeading the leading text.
 * @param p_pId the uuid.
 * @param p_sQueueName the destination text buffer.
 * @param p_iQueueNameSize the size of the destination buffer.
 * @return No return value.
 */
SO_PUBLIC extern void Queue_GetQueueName(
    const char * p_sLeading,
    uuid_t p_pId,
    char * p_sQueueName,
    size_t p_iQueueNameSize
);
#ifdef __cplusplus
}
#endif
#endif // RAZORBACK_QUEUE_H
