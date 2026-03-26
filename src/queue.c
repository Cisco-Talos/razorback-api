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

#include "config.h"
#include <razorback/debug.h>
#include <razorback/queue.h>
#include <razorback/log.h>
#include <razorback/messages.h>
#include <razorback/list.h>
#include <razorback/lock.h>
#include <razorback/thread.h>
#include <stdio.h>
#include <string.h>
#ifdef _MSC_VER
#include "bobins.h"
#else //_MSC_VER
#include <strings.h>
#endif //_MSC_VER
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <amqp.h>
#include <amqp_tcp_socket.h>
#include <amqp_ssl_socket.h>
#include <amqp_framing.h>
#include "runtime_config.h"
#include "messages/core.h"
#include "telemetry.h"
#define MBUF_SIZE 1024
#define AMQP_CHAN_ID 1

struct StompMessage {
    char * sVerb;
    List_t *headers;
    uint8_t *pBody;
    size_t bodyLength;
};

struct _AMQP_Socket {
    amqp_socket_t *pSocket;         ///< The socket to the broker
    amqp_connection_state_t pConn;  ///< The connection to the broker
    amqp_bytes_t pQueueName;        ///< The name of the queue
    bool bChannelOpen;              ///< Is the channel open
};

static bool Queue_Reconnect(struct Queue *queue, int p_iSide, const char *cause);
static bool Queue_Connect_ReadSocket(struct Queue *queue);
static bool Queue_Connect_WriteSocket(struct Queue *queue);
static const char *Queue_MessageFamily(uint32_t messageType);
static const char *Queue_ExchangeKind(const struct Queue *queue);
static struct RazorbackContext *Queue_TryGetCurrentContext(void);
static char sg_messageTypeHeaderName[] = "rzb-msg-type";
static char sg_messageVersionHeaderName[] = "rzb-msg-ver";

static const char *
Queue_MessageFamily(uint32_t messageType)
{
    switch (messageType & 0xF0000000U) {
    case MESSAGE_GROUP_C_AND_C:
        return "cnc";
    case MESSAGE_GROUP_CACHE:
    case MESSAGE_GROUP_SUBMIT:
        return "submission";
    default:
        return "other";
    }
}

static const char *
Queue_ExchangeKind(const struct Queue *queue)
{
    if (queue == NULL)
        return "other";

    return queue->bTopic ? "topic" : "default";
}

static struct RazorbackContext *
Queue_TryGetCurrentContext(void)
{
    Thread_t *thread;
    struct RazorbackContext *context;

    thread = Thread_GetCurrent();
    if (thread == NULL)
        return NULL;

    context = Thread_GetContext(thread);
    Thread_Destroy(thread);
    return context;
}

/** Log an AMQP RPC reply and return true when it represents an error. */
static bool
AMQP_error(amqp_rpc_reply_t x, char const *context) {
    switch (x.reply_type) {
        case AMQP_RESPONSE_NORMAL:
            return false;

        case AMQP_RESPONSE_NONE:
            rzb_log(LOG_ERR, LOG_C_QUEUE, "%s: missing RPC reply type!", context);
            break;

        case AMQP_RESPONSE_LIBRARY_EXCEPTION:
            rzb_log(LOG_ERR, LOG_C_QUEUE, "%s: %s", context, amqp_error_string2(x.library_error));
            break;

        case AMQP_RESPONSE_SERVER_EXCEPTION:
            switch (x.reply.id) {
                case AMQP_CONNECTION_CLOSE_METHOD: {
                    amqp_connection_close_t *m =
                            (amqp_connection_close_t *)x.reply.decoded;
                    rzb_log(LOG_ERR, LOG_C_QUEUE, "%s: server connection error %uh, message: %.*s",
                            context, m->reply_code, (int)m->reply_text.len,
                            (char *)m->reply_text.bytes);
                    break;
                }
                case AMQP_CHANNEL_CLOSE_METHOD: {
                    amqp_channel_close_t *m = (amqp_channel_close_t *)x.reply.decoded;
                    rzb_log(LOG_ERR, LOG_C_QUEUE, "%s: server channel error %uh, message: %.*s",
                            context, m->reply_code, (int)m->reply_text.len,
                            (char *)m->reply_text.bytes);
                    break;
                }
                default:
                    rzb_log(LOG_ERR, LOG_C_QUEUE, "%s: unknown server error, method id 0x%08X",
                            context, x.reply.id);
                    break;
            }
            break;
    }

    return true;
}

/** Process a pending broker method frame and report whether the connection remains usable. */
static bool
AMQP_Handle_Frame(AMQP_Socket_t *socket, amqp_frame_t *frame, const char *context)
{
    ASSERT(socket != NULL);
    ASSERT(frame != NULL);
    ASSERT(context != NULL);
    if (socket == NULL || frame == NULL || context == NULL) {
        rzb_log(LOG_ERR, LOG_C_QUEUE, "%s: invalid frame handler arguments", __func__);
        return false;
    }

    if (frame->frame_type != AMQP_FRAME_METHOD)
        return true;

    switch (frame->payload.method.id) {
        case AMQP_BASIC_ACK_METHOD:
            return true;

        case AMQP_BASIC_RETURN_METHOD: {
            amqp_basic_return_t *ret = (amqp_basic_return_t *)frame->payload.method.decoded;
            amqp_message_t message;
            amqp_rpc_reply_t reply;

            rzb_log(LOG_ERR, LOG_C_QUEUE,
                    "%s: broker returned unroutable message %uh via exchange %.*s routing key %.*s",
                    context, ret->reply_code, (int)ret->exchange.len,
                    (char *)ret->exchange.bytes, (int)ret->routing_key.len,
                    (char *)ret->routing_key.bytes);

            reply = amqp_read_message(socket->pConn, frame->channel, &message, 0);
            if (reply.reply_type == AMQP_RESPONSE_NORMAL) {
                amqp_destroy_message(&message);
            } else {
                AMQP_error(reply, context);
            }
            return true;
        }

        case AMQP_BASIC_CANCEL_METHOD:
        case AMQP_CHANNEL_CLOSE_METHOD:
        case AMQP_CONNECTION_CLOSE_METHOD: {
            AMQP_error((amqp_rpc_reply_t){
                           .reply_type = AMQP_RESPONSE_SERVER_EXCEPTION,
                           .reply = {
                               .id = frame->payload.method.id,
                               .decoded = frame->payload.method.decoded
                           }
                       },
                       context);
            return false;
        }

        default:
            rzb_log(LOG_DEBUG, LOG_C_QUEUE,
                    "%s: ignoring unexpected AMQP method frame 0x%08X",
                    context, frame->payload.method.id);
            return true;
    }
}

/** Drain any queued broker frames so heartbeat, return, and close events are surfaced promptly. */
static bool
AMQP_Service_Connection(AMQP_Socket_t *socket, const char *context)
{
    amqp_frame_t frame;
    int status;
    struct timeval tval;

    ASSERT(socket != NULL);
    ASSERT(context != NULL);
    if (socket == NULL || context == NULL) {
        rzb_log(LOG_ERR, LOG_C_QUEUE, "%s: invalid connection service arguments", __func__);
        return false;
    }

    tval.tv_sec = 0;
    tval.tv_usec = 0;

    do {
        status = amqp_simple_wait_frame_noblock(socket->pConn, &frame, &tval);
        if (status == AMQP_STATUS_TIMEOUT)
            return true;
        if (status != AMQP_STATUS_OK) {
            rzb_log(LOG_ERR, LOG_C_QUEUE, "%s: %s", context,
                    amqp_error_string2(status));
            return false;
        }
        if (!AMQP_Handle_Frame(socket, &frame, context))
            return false;
    } while (amqp_frames_enqueued(socket->pConn));

    return true;
}

/** Reject the current delivery when local processing fails before the caller can handle it. */
static bool
Queue_Reject_Delivery(struct Queue *queue, uint64_t delivery_tag, bool requeue,
                      const char *context)
{
    int amqpErr;

    ASSERT(queue != NULL);
    ASSERT(context != NULL);
    if (queue == NULL || context == NULL) {
        rzb_log(LOG_ERR, LOG_C_QUEUE, "%s: invalid reject arguments", __func__);
        return false;
    }
    if (queue->pReadSocket == NULL) {
        rzb_log(LOG_ERR, LOG_C_QUEUE, "%s: read socket missing while rejecting delivery", context);
        return false;
    }

    amqpErr = amqp_basic_reject(queue->pReadSocket->pConn, AMQP_CHAN_ID,
                                delivery_tag,
                                requeue ? 1 : 0);
    if (amqpErr < 0) {
        rzb_log(LOG_ERR, LOG_C_QUEUE, "%s: failed to reject delivery: %s",
                context, amqp_error_string2(amqpErr));
        Queue_Reconnect(queue, QUEUE_FLAG_RECV, NULL);
        return false;
    }

    return true;
}

/** Acknowledge a broker delivery by tag without taking ownership of queue locking. */
static bool
Queue_Acknowledge_Delivery(struct Queue *queue, uint64_t delivery_tag,
                           const char *context)
{
    int amqpErr;

    ASSERT(queue != NULL);
    ASSERT(context != NULL);
    if (queue == NULL || context == NULL) {
        rzb_log(LOG_ERR, LOG_C_QUEUE, "%s: invalid ack arguments", __func__);
        return false;
    }
    if (queue->pReadSocket == NULL) {
        rzb_log(LOG_ERR, LOG_C_QUEUE, "%s: read socket missing while acknowledging delivery",
                context);
        return false;
    }

    amqpErr = amqp_basic_ack(queue->pReadSocket->pConn, AMQP_CHAN_ID,
                             delivery_tag, 0);
    if (amqpErr < 0) {
        rzb_log(LOG_ERR, LOG_C_QUEUE, "%s: failed to ack delivery: %s",
                context, amqp_error_string2(amqpErr));
        Queue_Reconnect(queue, QUEUE_FLAG_RECV, NULL);
        return false;
    }

    return true;
}

/** Close an AMQP channel/connection pair and free the wrapper socket state. */
static void
AMQP_Socket_Close (AMQP_Socket_t *socket)
{
    // TODO - Handle errors
    if (socket == NULL)
        return;

    if (socket->pQueueName.bytes != NULL) {
        amqp_bytes_free(socket->pQueueName);
    }

    if (socket->bChannelOpen && socket->pConn != NULL) {
        amqp_channel_close(socket->pConn, AMQP_CHAN_ID, AMQP_REPLY_SUCCESS);
        socket->bChannelOpen = false;
    }

    if (socket->pConn != NULL) {
        amqp_connection_close(socket->pConn, AMQP_REPLY_SUCCESS);
        amqp_destroy_connection(socket->pConn);
    }
    free(socket);
}

/** Open and authenticate a single broker socket for either the read or write side. */
static AMQP_Socket_t *
Queue_Connect_Socket(
    const char * address,
    uint32_t port,
    const char * username,
    const char * password,
    const char * vHost,
    bool useSSL
) {
    int amqpErr;
    AMQP_Socket_t *socket;
    struct timeval tval;
    tval.tv_sec = 5;
    tval.tv_usec = 0;

    ASSERT (address != NULL);
    ASSERT (username != NULL);
    ASSERT (password != NULL);
    if (address == NULL || username == NULL || password == NULL) {
        rzb_log(LOG_ERR, LOG_C_QUEUE, "%s: address, username, and password must all be non-NULL", __func__);
        return NULL;
    }

    if ((socket = calloc(1, sizeof(AMQP_Socket_t))) == NULL) {
        rzb_log(LOG_ERR, LOG_C_QUEUE, "%s: Error allocating AMQP socket struct", __func__ );
        return NULL;
    }
    socket->bChannelOpen = false;

    socket->pConn = amqp_new_connection();
    if (useSSL) {
        socket->pSocket = amqp_ssl_socket_new(socket->pConn);
        if (socket->pSocket != NULL &&
            amqp_ssl_socket_set_cacert(socket->pSocket,
                                       "/etc/ssl/certs/ca-certificates.crt") != AMQP_STATUS_OK) {
            rzb_log(LOG_ERR, LOG_C_QUEUE, "%s: Error configuring CA certificate bundle", __func__);
            amqp_destroy_connection(socket->pConn);
            free(socket);
            return NULL;
        }
    } else {
        socket->pSocket = amqp_tcp_socket_new(socket->pConn);

    }
    if (!socket->pSocket) {
        rzb_log(LOG_ERR, LOG_C_QUEUE, "%s: Error creating socket", __func__ );
        amqp_destroy_connection(socket->pConn);
        free(socket);
        return NULL;
    }
    if ((amqpErr = amqp_socket_open_noblock(socket->pSocket, address, port, &tval)) < 0) {
        rzb_log(LOG_ERR, LOG_C_QUEUE, "%s: Error opening TCP socket %s:%u - %s", __func__, address, port, amqp_error_string2(amqpErr));
        amqp_destroy_connection(socket->pConn);
        free(socket);
        return NULL;
    }
    if (AMQP_error(amqp_login(socket->pConn, vHost, 0, AMQP_DEFAULT_FRAME_SIZE, 10, AMQP_SASL_METHOD_PLAIN, username, password), __func__)) {
        rzb_log(LOG_ERR, LOG_C_QUEUE, "%s: Error logging in", __func__ );
        AMQP_Socket_Close(socket);
        return NULL;
    }

    amqp_channel_open(socket->pConn, AMQP_CHAN_ID);
    if(AMQP_error(amqp_get_rpc_reply(socket->pConn), __func__)) {
        rzb_log(LOG_ERR, LOG_C_QUEUE, "%s: Error opening channel", __func__ );
        AMQP_Socket_Close(socket);
        return NULL;
    }
    socket->bChannelOpen = true;


    // done
    return socket;
}

/** Declare and bind the consumer queue, then start AMQP consumption on the read channel. */
static bool
Queue_BeginReading (struct Queue *p_pQ)
{
    amqp_bytes_t decQueuename;
    const char *exchange;
    int autoDelete;

    ASSERT (p_pQ != NULL);
    if (p_pQ == NULL) {
        rzb_log(LOG_ERR, LOG_C_QUEUE, "%s: queue is NULL", __func__);
        return false;
    }

    exchange = p_pQ->bTopic ? "amq.topic" : "amq.direct";
    autoDelete = p_pQ->bTopic ? 1 : 0;

    if (p_pQ->bTopic) {
       decQueuename = amqp_empty_bytes;
    } else {
        decQueuename = amqp_cstring_bytes(p_pQ->sName);
    }
    amqp_queue_declare_ok_t *r = amqp_queue_declare(
        p_pQ->pReadSocket->pConn,
        AMQP_CHAN_ID,
        decQueuename,
        0,
        1,
        0,
        autoDelete,
        amqp_empty_table
    );
    if (AMQP_error(amqp_get_rpc_reply(p_pQ->pReadSocket->pConn), __func__)) {
        rzb_log(LOG_ERR, LOG_C_QUEUE, "%s: Failed to declare queue", __func__);
        return false;
    }
    p_pQ->pReadSocket->pQueueName = amqp_bytes_malloc_dup(r->queue);

    if (p_pQ->pReadSocket->pQueueName.bytes == NULL) {
        rzb_log(LOG_ERR, LOG_C_QUEUE, "%s: Out of memory while copying queue name", __func__);
        return false;
    }
    if (p_pQ->bTopic) {
        amqp_queue_bind(
            p_pQ->pReadSocket->pConn,
            AMQP_CHAN_ID,
            p_pQ->pReadSocket->pQueueName,
            amqp_cstring_bytes(exchange),
            amqp_cstring_bytes(p_pQ->sName),
            amqp_empty_table
        );
        if (AMQP_error(amqp_get_rpc_reply(p_pQ->pReadSocket->pConn), __func__)) {
            rzb_log(LOG_ERR, LOG_C_QUEUE, "%s: Failed to bind to topic exchange", __func__);
            return false;
        }
    }

    if (p_pQ->iPrefetch > 0) {
        amqp_basic_qos(
            p_pQ->pReadSocket->pConn,
            AMQP_CHAN_ID,
            0,
            p_pQ->iPrefetch,
            0
        );
        if (AMQP_error(amqp_get_rpc_reply(p_pQ->pReadSocket->pConn), __func__)) {
            rzb_log(LOG_ERR, LOG_C_QUEUE, "%s: Failed to set prefetch to %u",
                    __func__, p_pQ->iPrefetch);
            return false;
        }
    }

    amqp_basic_consume(
        p_pQ->pReadSocket->pConn,
        AMQP_CHAN_ID,
        p_pQ->pReadSocket->pQueueName,
        amqp_empty_bytes,
        0,
        0,
        0,
        amqp_empty_table
    );

    if (AMQP_error(amqp_get_rpc_reply(p_pQ->pReadSocket->pConn), __func__ )) {
        rzb_log(LOG_ERR, LOG_C_QUEUE, "%s: Failed to start consuming from queue", __func__);
        return false;
    }

    return true;
}

/** Stop the read side of a queue connection. */
static bool
Queue_EndReading (struct Queue *p_pQ)
{

    ASSERT (p_pQ != NULL);
    if (p_pQ == NULL) {
        rzb_log(LOG_ERR, LOG_C_QUEUE, "%s: queue is NULL", __func__);
        return false;
    }

    return true;
}

/** Timer callback that services broker heartbeats and reconnects dead write sockets. */
static void
AMQP_Heartbeat(void *p_arg)
{
    struct Queue *queue = (struct Queue *)p_arg;

    Mutex_Lock(queue->mWriteMutex);
    if (queue->bShuttingDown) {
        Mutex_Unlock(queue->mWriteMutex);
        return;
    }

    if (queue->pWriteSocket == NULL) {
        rzb_log(LOG_ERR, LOG_C_QUEUE, "%s: Write socket missing during heartbeat, reconnecting", __func__);
        Queue_Reconnect(queue, QUEUE_FLAG_SEND, "heartbeat");
        Mutex_Unlock(queue->mWriteMutex);
        return;
    }

    if (!AMQP_Service_Connection(queue->pWriteSocket, __func__)) {
        rzb_log(LOG_ERR, LOG_C_QUEUE, "%s: Heartbeat detected dead broker connection, reconnecting", __func__);
        Queue_Reconnect(queue, QUEUE_FLAG_SEND, "heartbeat");
    }
    Mutex_Unlock(queue->mWriteMutex);

}

/** Establish the queue's read-side AMQP connection if it is configured and missing. */
static bool
Queue_Connect_ReadSocket(struct Queue *queue)
{
    if (queue->bShuttingDown)
        return false;

    if ((queue->iFlags & QUEUE_FLAG_RECV) != QUEUE_FLAG_RECV ||
        queue->pReadSocket != NULL)
        return true;

    queue->pReadSocket = Queue_Connect_Socket(queue->sHostname, queue->iPort,
                                              queue->sUser, queue->sPassword,
                                              queue->sVhost, queue->bUseSSL);
    if (queue->pReadSocket == NULL) {
        rzb_log(LOG_ERR, LOG_C_STOMP,
                "%s: failed due to failure of Queue_Connect_Socket (Read)",
                __func__);
        return false;
    }
    if (!Queue_BeginReading(queue)) {
        rzb_log(LOG_ERR, LOG_C_STOMP,
                "%s: failed due to failure of Queue_BeginReading", __func__);
        AMQP_Socket_Close(queue->pReadSocket);
        queue->pReadSocket = NULL;
        return false;
    }

    return true;
}

/** Establish the queue's write-side AMQP connection and heartbeat timer if needed. */
static bool
Queue_Connect_WriteSocket(struct Queue *queue)
{
    if (queue->bShuttingDown)
        return false;

    if ((queue->iFlags & QUEUE_FLAG_SEND) != QUEUE_FLAG_SEND ||
        queue->pWriteSocket != NULL)
        return true;

    queue->pWriteSocket = Queue_Connect_Socket(queue->sHostname, queue->iPort,
                                               queue->sUser, queue->sPassword,
                                               queue->sVhost, queue->bUseSSL);
    if (queue->pWriteSocket == NULL) {
        rzb_log(LOG_ERR, LOG_C_STOMP,
                "%s: failed due to failure of Queue_Connect_Socket (Write)",
                __func__);
        return false;
    }
    if (queue->pWriteHeartbeat == NULL) {
        queue->pWriteHeartbeat = Timer_Create(2, AMQP_Heartbeat, queue);
        if (queue->pWriteHeartbeat == NULL) {
            rzb_log(LOG_ERR, LOG_C_QUEUE, "%s: Failed to create write heartbeat timer", __func__);
            AMQP_Socket_Close(queue->pWriteSocket);
            queue->pWriteSocket = NULL;
            return false;
        }
    }

    return true;
}

/** Connect whichever AMQP sides are enabled for the queue. */
static bool
Queue_Connect(struct Queue *queue)
{
    if (!Queue_Connect_ReadSocket(queue))
        return false;
    if (!Queue_Connect_WriteSocket(queue))
        return false;

    return true;
}

/** Reconnect only the requested AMQP side or sides of a queue. */
static bool
Queue_Reconnect(struct Queue *queue, int p_iSide, const char *cause)
{
    struct RazorbackContext *context = NULL;

    if (queue->bShuttingDown)
        return false;

    if ((p_iSide & QUEUE_FLAG_SEND) == QUEUE_FLAG_SEND) {
        context = Queue_TryGetCurrentContext();
        Telemetry_RecordOutboundReconnect(cause, context);
    }

    if ((p_iSide & QUEUE_FLAG_RECV) == QUEUE_FLAG_RECV &&
        queue->pReadSocket != NULL)
    {
        if (queue->pReadSocket != NULL)
        {
            AMQP_Socket_Close(queue->pReadSocket);
            queue->pReadSocket = NULL;
        }
    }

    if ((p_iSide & QUEUE_FLAG_SEND) == QUEUE_FLAG_SEND &&
            queue->pWriteSocket != NULL)
    {
        if (queue->pWriteSocket != NULL)
        {
            AMQP_Socket_Close (queue->pWriteSocket);
            queue->pWriteSocket = NULL;
        }
    }


    if ((p_iSide & QUEUE_FLAG_RECV) == QUEUE_FLAG_RECV &&
        !Queue_Connect_ReadSocket(queue))
        return false;
    if ((p_iSide & QUEUE_FLAG_SEND) == QUEUE_FLAG_SEND &&
        !Queue_Connect_WriteSocket(queue))
        return false;

    return true;
}

SO_PUBLIC struct Queue *
Queue_Create (const char * p_sQueueName, bool p_bTopic, int p_iFlags)
{
    return Queue_Create_With_Host(
        p_sQueueName,
        p_bTopic,
        p_iFlags,
        Config_getMqHost(),
        Config_getMqPort(),
        Config_getMqUser(),
        Config_getMqPassword(),
        Config_getMqVhost(),
        Config_getMqSSL(),
        Config_getMqPrefetch()
    );
}

SO_PUBLIC struct Queue *
Queue_Create_With_Host (const char * p_sQueueName,
                        bool p_bTopic,
                        int p_iFlags,
                        const char * p_sHost,
                        uint32_t p_iPort,
                        const char * p_sUser,
                        const char * p_sPassword,
                        const char * p_sVhost,
                        bool p_bUseSSL,
                        uint32_t p_iPrefetch
)
{
    struct Queue *l_pQueue = NULL;

    ASSERT (p_sQueueName != NULL);
    if (p_sQueueName == NULL) {
        rzb_log (LOG_ERR,LOG_C_STOMP, "%s: queue name is NULL", __func__);
        return NULL;
    }

    if ((l_pQueue = (struct Queue *)calloc (1, sizeof (struct Queue))) == NULL)
    {
        rzb_log (LOG_ERR,LOG_C_STOMP, "%s: Failed to alloc new queue", __func__);
        return NULL;
    }
    l_pQueue->sHostname = p_sHost;
    l_pQueue->iPort = p_iPort;
    l_pQueue->sUser = p_sUser;
    l_pQueue->sPassword = p_sPassword;
    l_pQueue->sVhost = p_sVhost;
    l_pQueue->bUseSSL = p_bUseSSL;
    l_pQueue->iPrefetch = p_iPrefetch;
    l_pQueue->bTopic = p_bTopic;

    if ((l_pQueue->sName = strdup(p_sQueueName)) == NULL)
    {
        rzb_log (LOG_ERR,LOG_C_STOMP, "%s: Failed to alloc new queue name", __func__);
        goto error;
    }
    if ((l_pQueue->mReadMutex = Mutex_Create(MUTEX_MODE_NORMAL)) == NULL)
    {
        goto error;
    }
    if ((l_pQueue->mWriteMutex = Mutex_Create(MUTEX_MODE_NORMAL)) == NULL)
    {
        goto error;
    }
    l_pQueue->iFlags = p_iFlags;
    if (!Queue_Connect(l_pQueue))
    {
        rzb_log (LOG_ERR,LOG_C_STOMP,
                 "%s: failed due to failure of Queue_Connect", __func__);
        Queue_Terminate(l_pQueue);
        return NULL;
    }
    return l_pQueue;

error:
    if (l_pQueue != NULL)
    {
        if (l_pQueue->mReadMutex != NULL)
            Mutex_Destroy(l_pQueue->mReadMutex);
        if (l_pQueue->mWriteMutex != NULL)
            Mutex_Destroy(l_pQueue->mWriteMutex);
        free(l_pQueue->sName);
        free(l_pQueue);
    }
    return NULL;
}

SO_PUBLIC void
Queue_Terminate (struct Queue *p_pQ)
{
    struct Timer *heartbeat = NULL;

    ASSERT (p_pQ != NULL);
    if (p_pQ == NULL) {
        rzb_log(LOG_ERR, LOG_C_QUEUE, "%s: queue is NULL", __func__);
        return;
    }

    Mutex_Lock (p_pQ->mReadMutex);
    Mutex_Lock (p_pQ->mWriteMutex);
    p_pQ->bShuttingDown = true;
    heartbeat = p_pQ->pWriteHeartbeat;
    p_pQ->pWriteHeartbeat = NULL;
    Mutex_Unlock (p_pQ->mWriteMutex);

    if (heartbeat != NULL)
        Timer_Destroy(heartbeat);

    Mutex_Lock (p_pQ->mWriteMutex);

    if ((p_pQ->iFlags & QUEUE_FLAG_RECV) == QUEUE_FLAG_RECV &&
            p_pQ->pReadSocket != NULL)
    {
        Queue_EndReading (p_pQ);
        AMQP_Socket_Close (p_pQ->pReadSocket);
        p_pQ->pReadSocket = NULL;
    }
    if ((p_pQ->iFlags & QUEUE_FLAG_SEND) == QUEUE_FLAG_SEND &&
            p_pQ->pWriteSocket != NULL)
    {
        AMQP_Socket_Close (p_pQ->pWriteSocket);
        p_pQ->pWriteSocket = NULL;
    }


    Mutex_Unlock (p_pQ->mReadMutex);
    Mutex_Unlock (p_pQ->mWriteMutex);
    Mutex_Destroy (p_pQ->mReadMutex);
    Mutex_Destroy (p_pQ->mWriteMutex);
    free(p_pQ->sName);
    free(p_pQ);
}

SO_PUBLIC struct Message *
Queue_Get_Ex(struct Queue *queue, bool autoAck, uint32_t timeoutMilliseconds)
{
    struct Message *ret = NULL;
    struct MessageHeader *header = NULL;
    struct TelemetrySpan *receiveSpan = NULL;
    amqp_rpc_reply_t res;
    amqp_envelope_t envelope;
    struct timeval timeout;
    struct timeval *timeoutPtr = NULL;
    int headerIndex = 0;
    bool envelopeReady = false;
    bool rejectDelivery = false;
    bool requeueDelivery = false;
    bool receiveSuccess = false;
    const char *receiveError = NULL;

    ASSERT (queue);
    if (queue == NULL) {
        rzb_log(LOG_ERR, LOG_C_QUEUE, "%s: queue is NULL", __func__);
        return NULL;
    }
    Mutex_Lock (queue->mReadMutex);
    if (queue->bShuttingDown) {
        rzb_log(LOG_DEBUG, LOG_C_QUEUE,
                "%s: Skipping receive on queue '%s' because the connection is shutting down",
                __func__, queue->sName);
        Mutex_Unlock(queue->mReadMutex);
        return NULL;
    }

    if (queue->pReadSocket == NULL) {
        rzb_log(LOG_ERR, LOG_C_QUEUE, "%s: Read socket unavailable, attempting reconnect", __func__);
        if (!Queue_Reconnect(queue, QUEUE_FLAG_RECV, NULL) || queue->pReadSocket == NULL) {
            Mutex_Unlock(queue->mReadMutex);
            return NULL;
        }
    }

    amqp_maybe_release_buffers(queue->pReadSocket->pConn);
    if (timeoutMilliseconds > 0) {
        timeout.tv_sec = timeoutMilliseconds / 1000;
        timeout.tv_usec = (timeoutMilliseconds % 1000) * 1000;
        timeoutPtr = &timeout;
    }
    res = amqp_consume_message(queue->pReadSocket->pConn, &envelope, timeoutPtr, 0);
    envelopeReady = (res.reply_type == AMQP_RESPONSE_NORMAL);

    if (AMQP_RESPONSE_NORMAL != res.reply_type) {
        if (res.reply_type == AMQP_RESPONSE_LIBRARY_EXCEPTION &&
            res.library_error == AMQP_STATUS_TIMEOUT) {
            errno = EAGAIN;
            if (!AMQP_Service_Connection(queue->pReadSocket, __func__)) {
                rzb_log(LOG_ERR, LOG_C_QUEUE,
                        "%s: Error servicing read connection after timeout, reconnecting read side",
                        __func__);
                Queue_Reconnect(queue, QUEUE_FLAG_RECV, NULL);
                receiveError = "failed to service read connection";
                errno = EIO;
            }
            goto cleanup;
        }
        AMQP_error(res, __func__);
        rzb_log(LOG_ERR, LOG_C_QUEUE, "%s: Error consuming message, reconnecting read side", __func__);
        Queue_Reconnect(queue, QUEUE_FLAG_RECV, NULL);
        receiveError = "failed to consume message";
        goto cleanup;
    }
    if ((ret = (struct Message *)calloc(1,sizeof(struct Message))) == NULL)
    {
        rzb_log(LOG_ERR, LOG_C_QUEUE, "%s: Error allocating message struct", __func__);
        rejectDelivery = true;
        requeueDelivery = true;
        receiveError = "failed to allocate message container";
        goto cleanup;
    }
    if ((ret->headers = Message_Header_List_Create()) == NULL) {
        rzb_log(LOG_ERR, LOG_C_QUEUE, "%s: Error allocating message header list", __func__);
        rejectDelivery = true;
        requeueDelivery = true;
        receiveError = "failed to allocate message header list";
        goto cleanup;
    }

    ret->serialized = (uint8_t *)calloc(1, envelope.message.body.len + 1);
    if (ret->serialized == NULL) {
        rzb_log(LOG_ERR, LOG_C_QUEUE, "%s: Error allocating message body", __func__);
        rejectDelivery = true;
        requeueDelivery = true;
        receiveError = "failed to allocate message body";
        goto cleanup;
    }
    memcpy(ret->serialized, envelope.message.body.bytes, envelope.message.body.len);
    ret->length = envelope.message.body.len;
    ret->brokerDeliveryTag = envelope.delivery_tag;
    ret->brokerAckPending = false;
    if (envelope.message.properties._flags & AMQP_BASIC_HEADERS_FLAG) {
        //rzb_log(LOG_DEBUG, LOG_C_QUEUE, "%s: Message has %u headers", __func__, envelope.message.properties.headers.num_entries);
        for (headerIndex = 0; headerIndex < envelope.message.properties.headers.num_entries; headerIndex++) {
            amqp_table_entry_t entry = envelope.message.properties.headers.entries[headerIndex];
            char * name = strndup((char *)entry.key.bytes, entry.key.len);
            if (name == NULL) {
                rzb_log(LOG_ERR, LOG_C_QUEUE, "%s: Failed to duplicate message header name", __func__);
                rejectDelivery = true;
                requeueDelivery = true;
                receiveError = "failed to duplicate message header name";
                goto cleanup;
            }
            //rzb_log(LOG_DEBUG, LOG_C_QUEUE, "%s: Processing header %s - Kind: %u", __func__, name, entry.value.kind);
            if (entry.value.kind == AMQP_FIELD_KIND_UTF8) {
                //rzb_log(LOG_DEBUG, LOG_C_QUEUE, "%s: Header %s: %s", __func__, name, entry.value.value.bytes.bytes);
                if (Message_HeaderList_Add(ret->headers, name,
                                           entry.value.value.bytes.bytes) == NULL) {
                    free(name);
                    rejectDelivery = true;
                    requeueDelivery = true;
                    receiveError = "failed to copy inbound utf8 header";
                    goto cleanup;
                }
            } else if (entry.value.kind == AMQP_FIELD_KIND_BYTES) {
                // Copy header values into new strings to make sure they are null terminated
                char * value = strndup((char *)entry.value.value.bytes.bytes, entry.value.value.bytes.len);
                if (value == NULL) {
                    free(name);
                    rejectDelivery = true;
                    requeueDelivery = true;
                    receiveError = "failed to duplicate inbound byte header";
                    goto cleanup;
                }
                //rzb_log(LOG_DEBUG, LOG_C_QUEUE, "%s: Header %s: %s", __func__, name, value);
                if (Message_HeaderList_Add(ret->headers, name, value) == NULL) {
                    free(value);
                    free(name);
                    rejectDelivery = true;
                    requeueDelivery = true;
                    receiveError = "failed to copy inbound byte header";
                    goto cleanup;
                }
                free(value);
            }
            free(name);
        }
    }

    receiveSpan = Telemetry_StartQueueReceiveSpan(queue, ret);

    if ((queue->iFlags & QUEUE_FLAG_EXTERNAL_MODE) != QUEUE_FLAG_EXTERNAL_MODE) {
        if ((header = (struct MessageHeader *) List_Find(ret->headers, sg_messageTypeHeaderName)) == NULL) {
            rzb_log(LOG_ERR,LOG_C_STOMP, "%s: Message header missing - rzb-msg-type", __func__);
            rejectDelivery = true;
            receiveError = "message type header missing";
            goto cleanup;
        }
        ret->type = strtoul(header->sValue, NULL, 10);

        if ((header = (struct MessageHeader *) List_Find(ret->headers, sg_messageVersionHeaderName)) == NULL) {
            rzb_log(LOG_ERR,LOG_C_STOMP, "%s: Message header missing - rzb-msg-ver", __func__);
            rejectDelivery = true;
            receiveError = "message version header missing";
            goto cleanup;
        }
        ret->version = strtoul(header->sValue, NULL, 10);

        if (!Message_Setup(ret)) {
            rzb_log(LOG_ERR,LOG_C_STOMP, "%s: Message_Setup failed", __func__);
            rejectDelivery = true;
            receiveError = "message setup failed";
            goto cleanup;
        }
        if(!ret->deserialize(ret)) {
            rzb_log(LOG_ERR,LOG_C_STOMP, "%s: Message deserialize failed: type %u body %s", __func__, ret->type, ret->serialized);
            rejectDelivery = true;
            receiveError = "message deserialize failed";
            goto cleanup;
        }
    }

    if (autoAck) {
        if (!Queue_Acknowledge_Delivery(queue, envelope.delivery_tag, __func__)) {
            receiveError = "failed to acknowledge delivery";
        } else {
            receiveSuccess = true;
        }
    } else {
        ret->brokerAckPending = true;
        receiveSuccess = true;
    }

cleanup:
    if (envelopeReady) {
        if (rejectDelivery)
            Queue_Reject_Delivery(queue, envelope.delivery_tag, requeueDelivery, __func__);
        amqp_destroy_envelope(&envelope);
    }
    Telemetry_EndSpan(receiveSpan, receiveSuccess && !rejectDelivery, receiveError);
    Mutex_Unlock (queue->mReadMutex);

    if (rejectDelivery) {
        if (ret != NULL) {
            if (ret->destroy != NULL)
                ret->destroy(ret);
            else
                Message_Destroy(ret);
        }
        return NULL;
    }

    return ret;
}

SO_PUBLIC struct Message *
Queue_Get(struct Queue *queue)
{
    return Queue_Get_Ex(queue, true, 0);
}

SO_PUBLIC bool
Queue_Ack_Message(struct Queue *queue, struct Message *message)
{
    bool ret;

    ASSERT(queue != NULL);
    ASSERT(message != NULL);
    if (queue == NULL || message == NULL) {
        rzb_log(LOG_ERR, LOG_C_QUEUE, "%s: queue or message is NULL", __func__);
        return false;
    }

    if (!message->brokerAckPending)
        return true;

    Mutex_Lock(queue->mReadMutex);
    if (queue->bShuttingDown) {
        Mutex_Unlock(queue->mReadMutex);
        return false;
    }

    if (queue->pReadSocket == NULL) {
        rzb_log(LOG_ERR, LOG_C_QUEUE, "%s: Read socket unavailable, attempting reconnect", __func__);
        if (!Queue_Reconnect(queue, QUEUE_FLAG_RECV, NULL) || queue->pReadSocket == NULL) {
            Mutex_Unlock(queue->mReadMutex);
            return false;
        }
    }

    ret = Queue_Acknowledge_Delivery(queue, message->brokerDeliveryTag, __func__);
    Mutex_Unlock(queue->mReadMutex);
    if (ret)
        message->brokerAckPending = false;

    return ret;
}

SO_PUBLIC bool
Queue_Reject_Message(struct Queue *queue, struct Message *message, bool requeue)
{
    bool ret;

    ASSERT(queue != NULL);
    ASSERT(message != NULL);
    if (queue == NULL || message == NULL) {
        rzb_log(LOG_ERR, LOG_C_QUEUE, "%s: queue or message is NULL", __func__);
        return false;
    }

    if (!message->brokerAckPending)
        return true;

    Mutex_Lock(queue->mReadMutex);
    if (queue->bShuttingDown) {
        Mutex_Unlock(queue->mReadMutex);
        return false;
    }

    if (queue->pReadSocket == NULL) {
        rzb_log(LOG_ERR, LOG_C_QUEUE, "%s: Read socket unavailable, attempting reconnect", __func__);
        if (!Queue_Reconnect(queue, QUEUE_FLAG_RECV, NULL) || queue->pReadSocket == NULL) {
            Mutex_Unlock(queue->mReadMutex);
            return false;
        }
    }

    ret = Queue_Reject_Delivery(queue, message->brokerDeliveryTag, requeue, __func__);
    Mutex_Unlock(queue->mReadMutex);
    if (ret)
        message->brokerAckPending = false;

    return ret;
}

SO_PUBLIC bool
Queue_Settle_Message(struct Queue *queue, struct Message *message,
                     bool ackMessage, bool requeueMessage)
{
    if (ackMessage)
        return Queue_Ack_Message(queue, message);

    return Queue_Reject_Message(queue, message, requeueMessage);
}

SO_PUBLIC bool
Queue_Put (struct Queue * queue,  struct Message * message)
{
    return Queue_Put_Dest(queue, message, queue->sName);
}

struct HeaderIteratorContext {
    amqp_table_entry_t *nextEntry;
};

/** Convert one Razorback message header into the AMQP table entry format. */
static int
Message_Header_To_AMQP_TableEntry(void *h, void *c)
{
    struct HeaderIteratorContext * ctx = (struct HeaderIteratorContext *)c;
    struct MessageHeader * header = (struct MessageHeader *)h;
    ctx->nextEntry->key = amqp_cstring_bytes(header->sName);
    ctx->nextEntry->value.kind = AMQP_FIELD_KIND_UTF8;
    ctx->nextEntry->value.value.bytes = amqp_cstring_bytes(header->sValue);
    ctx->nextEntry++;
    return LIST_EACH_OK;
}

SO_PUBLIC bool
Queue_Put_Dest (struct Queue * queue,  struct Message * message, const char *dest)
{
    amqp_bytes_t message_bytes = amqp_empty_bytes;
    char *messageType = NULL;
    char *messageVer = NULL;
    amqp_bytes_t exchange = amqp_empty_bytes;
    int amqpErr;
    size_t headerCount;
    size_t injectedHeaderCount = 0;
    unsigned int attempt;
    struct HeaderIteratorContext ctx;
    struct TelemetrySpan *sendSpan = NULL;
    struct TelemetryInjectedHeaders injectedHeaders = { 0, NULL };
    bool ret = false;
    const char *sendError = NULL;
    const char *messageFamily;
    const char *destination;
    const char *exchangeKind;
    struct RazorbackContext *context;
    size_t i;
    amqp_basic_properties_t props = {0};

    ASSERT (queue != NULL);
    ASSERT (message != NULL);
    ASSERT (dest != NULL);

    if (queue == NULL)
        return false;
    if (message == NULL)
        return false;
    if (dest == NULL)
        return false;

    context = Queue_TryGetCurrentContext();
    messageFamily = Queue_MessageFamily(message->type);
    destination = (dest[0] != '\0') ? dest : "unknown";
    exchangeKind = Queue_ExchangeKind(queue);

    Mutex_Lock (queue->mWriteMutex);
    if (queue->bShuttingDown) {
        rzb_log(LOG_DEBUG, LOG_C_QUEUE,
                "%s: Refusing to send message type %u to '%s' because the connection is shutting down",
                __func__, message->type, dest);
        sendError = "queue is shutting down";
        goto cleanup;
    }
    if (queue->pWriteSocket == NULL) {
        rzb_log(LOG_ERR, LOG_C_QUEUE, "%s: Write socket unavailable, attempting reconnect", __func__);
        if (!Queue_Reconnect(queue, QUEUE_FLAG_SEND, "publish_retry") ||
            queue->pWriteSocket == NULL) {
            sendError = "write reconnect failed";
            Telemetry_RecordOutboundMessage(message->type, "publish_error", messageFamily,
                                           destination, exchangeKind, context);
            goto cleanup;
        }
    }

    // Don't serialize the message more than once.
    if (message->serialized == NULL)
    {
        if (!message->serialize(message))
        {
            rzb_log(LOG_ERR,LOG_C_STOMP, "%s: Failed to serialize message", __func__);
            sendError = "message serialization failed";
            Telemetry_RecordOutboundMessage(message->type, "serialize_error", messageFamily,
                                           destination, exchangeKind, context);
            goto cleanup;
        }
    }

    sendSpan = Telemetry_StartQueueSendSpan(queue, message, dest, &injectedHeaders);
    injectedHeaderCount = injectedHeaders.count;

    if (asprintf(&messageType, "%u", message->type) == -1)
    {
        sendError = "failed to allocate message type header";
        goto cleanup;
    }
    if (asprintf(&messageVer, "%u", message->version) == -1)
    {
        sendError = "failed to allocate message version header";
        goto cleanup;
    }

    message_bytes.len = message->length;
    message_bytes.bytes = message->serialized;
    if (queue->bTopic) {
        // For topics the destination is the routing key
        exchange = amqp_cstring_bytes("amq.topic");
    } else {
        exchange = amqp_empty_bytes;
    }
    props._flags = AMQP_BASIC_CONTENT_TYPE_FLAG | AMQP_BASIC_DELIVERY_MODE_FLAG | AMQP_BASIC_HEADERS_FLAG;
    props.content_type = amqp_cstring_bytes("application/json");
    props.delivery_mode = 2; /* persistent delivery mode */
    headerCount = (message->headers != NULL) ? List_Length(message->headers) : 0;
    props.headers.num_entries = 2 + injectedHeaderCount + headerCount;
    props.headers.entries = (amqp_table_entry_t *)calloc(props.headers.num_entries, sizeof(amqp_table_entry_t));
    if (props.headers.entries == NULL) {
        sendError = "failed to allocate AMQP header table";
        goto cleanup;
    }
    props.headers.entries[0].key = amqp_cstring_bytes("rzb-msg-type");
    props.headers.entries[0].value.kind = AMQP_FIELD_KIND_UTF8;
    props.headers.entries[0].value.value.bytes = amqp_cstring_bytes(messageType);
    props.headers.entries[1].key = amqp_cstring_bytes("rzb-msg-ver");
    props.headers.entries[1].value.kind = AMQP_FIELD_KIND_UTF8;
    props.headers.entries[1].value.value.bytes = amqp_cstring_bytes(messageVer);
    for (i = 0; i < injectedHeaderCount; ++i) {
        props.headers.entries[2 + i].key =
            amqp_cstring_bytes(injectedHeaders.entries[i].name);
        props.headers.entries[2 + i].value.kind = AMQP_FIELD_KIND_UTF8;
        props.headers.entries[2 + i].value.value.bytes =
            amqp_cstring_bytes(injectedHeaders.entries[i].value);
    }
    ctx.nextEntry = &(props.headers.entries[2 + injectedHeaderCount]);
    if (message->headers != NULL &&
        !List_ForEach(message->headers, Message_Header_To_AMQP_TableEntry, &ctx)) {
        sendError = "failed to convert Razorback headers to AMQP headers";
        goto cleanup;
    }

    for (attempt = 0; attempt < 2; attempt++) {
        if (!AMQP_Service_Connection(queue->pWriteSocket, __func__)) {
            rzb_log(LOG_ERR, LOG_C_QUEUE,
                    "%s: Broker reported an outbound connection error, reconnecting",
                    __func__);
            sendError = "outbound broker connection error";
            Telemetry_RecordOutboundMessage(message->type, "publish_error", messageFamily,
                                           destination, exchangeKind, context);
            if (attempt == 0)
                Telemetry_RecordOutboundPublishRetry(message->type, messageFamily,
                                                    destination, exchangeKind, context);
            if (attempt == 0 &&
                Queue_Reconnect(queue, QUEUE_FLAG_SEND, "publish_retry") &&
                queue->pWriteSocket != NULL) {
                continue;
            }
            break;
        }

        amqpErr = amqp_basic_publish(
            queue->pWriteSocket->pConn,
            AMQP_CHAN_ID,
            exchange,
            amqp_cstring_bytes(dest),
            0, 0, &props, message_bytes);
        if (amqpErr == AMQP_STATUS_OK) {
            ret = true;
            sendError = NULL;
            Telemetry_RecordOutboundMessage(message->type, "published", messageFamily,
                                           destination, exchangeKind, context);
            break;
        }

        rzb_log(LOG_ERR, LOG_C_QUEUE, "%s: Failed to publish message: %s",
                __func__, amqp_error_string2(amqpErr));
        sendError = amqp_error_string2(amqpErr);
        Telemetry_RecordOutboundMessage(message->type, "publish_error", messageFamily,
                                       destination, exchangeKind, context);
        if (attempt == 0)
            Telemetry_RecordOutboundPublishRetry(message->type, messageFamily,
                                                destination, exchangeKind, context);
        if (attempt == 0 &&
            Queue_Reconnect(queue, QUEUE_FLAG_SEND, "publish_retry") &&
            queue->pWriteSocket != NULL) {
            continue;
        }
        break;
    }

cleanup:
    free(props.headers.entries);
    free(messageType);
    free(messageVer);
    Telemetry_FreeInjectedHeaders(&injectedHeaders);
    Mutex_Unlock (queue->mWriteMutex);
    Telemetry_EndSpan(sendSpan, ret, sendError);
    return ret;
}

SO_PUBLIC void
Queue_GetQueueName (const char * p_sLeading, uuid_t p_pId,
                    char * p_sQueueName, size_t p_iQueueNameSize)
{
    char l_sUUID[UUID_STRING_LENGTH];
    int written;

    ASSERT(p_sLeading != NULL);
    ASSERT(p_sQueueName != NULL);
    ASSERT(p_iQueueNameSize > 0);
    if (p_sLeading == NULL || p_sQueueName == NULL || p_iQueueNameSize == 0)
    {
        rzb_log(LOG_ERR, LOG_C_QUEUE, "%s: invalid queue name arguments", __func__);
        return;
    }

    uuid_unparse (p_pId, l_sUUID);
    written = snprintf(p_sQueueName, p_iQueueNameSize, "%s.%s", p_sLeading, l_sUUID);
    if (written < 0 || (size_t)written >= p_iQueueNameSize)
    {
        rzb_log(LOG_ERR, LOG_C_QUEUE, "%s: queue name buffer is too small", __func__);
    }
}
