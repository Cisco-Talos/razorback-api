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

static bool Queue_Reconnect(struct Queue *queue, int p_iSide);

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

static void
AMQP_Socket_Close (AMQP_Socket_t *socket)
{
    // TODO - Handle errors
    if (socket == NULL)
        return;

    if (socket->pQueueName.bytes != NULL) {
        amqp_bytes_free(socket->pQueueName);
    }

    if (socket->bChannelOpen) {
        amqp_channel_close(socket->pConn, AMQP_CHAN_ID, AMQP_REPLY_SUCCESS);
        socket->bChannelOpen = false;
    }

    amqp_connection_close(socket->pConn, AMQP_REPLY_SUCCESS);
    amqp_destroy_connection(socket->pConn);
    free(socket);
}

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
        amqp_ssl_socket_set_cacert(socket->pSocket, "/etc/ssl/certs/ca-certificates.crt");
    } else {
        socket->pSocket = amqp_tcp_socket_new(socket->pConn);

    }
    if (!socket->pSocket) {
        rzb_log(LOG_ERR, LOG_C_QUEUE, "%s: Error creating socket", __func__ );
        free(socket);
        return NULL;
    }
    if ((amqpErr = amqp_socket_open_noblock(socket->pSocket, address, port, &tval)) < 0) {
        rzb_log(LOG_ERR, LOG_C_QUEUE, "%s: Error opening TCP socket %s:%u - %s", __func__, address, port, amqp_error_string2(amqpErr));
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


    // done
    return socket;
}

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

static void
AMQP_Heartbeat(void *p_arg)
{
    struct Queue *queue = (struct Queue *)p_arg;
    amqp_frame_t frame;
    int status;
    struct timeval tval;
    tval.tv_sec = 0;
    tval.tv_usec = 0;

    Mutex_Lock(queue->mWriteMutex);
    if (queue->pWriteSocket == NULL) {
        rzb_log(LOG_ERR, LOG_C_QUEUE, "%s: Write socket missing during heartbeat, reconnecting", __func__);
        Queue_Reconnect(queue, QUEUE_FLAG_SEND);
        Mutex_Unlock(queue->mWriteMutex);
        return;
    }

    status = amqp_simple_wait_frame_noblock(queue->pWriteSocket->pConn, &frame, &tval);
    if (status != AMQP_STATUS_OK && status != AMQP_STATUS_TIMEOUT) {
        rzb_log(LOG_ERR, LOG_C_QUEUE, "%s: Heartbeat failed (%s), reconnecting", __func__, amqp_error_string2(status));
        Queue_Reconnect(queue, QUEUE_FLAG_SEND);
    }
    Mutex_Unlock(queue->mWriteMutex);

}

static bool
Queue_Connect(struct Queue *queue)
{

    if (((queue->iFlags & QUEUE_FLAG_RECV) == QUEUE_FLAG_RECV) && (queue->pReadSocket == NULL))
    {
        if ((queue->pReadSocket =
             Queue_Connect_Socket(queue->sHostname, queue->iPort,
                  queue->sUser, queue->sPassword, queue->sVhost, queue->bUseSSL)) == NULL)

        {
            rzb_log (LOG_ERR,LOG_C_STOMP,
                     "%s: failed due to failure of Queue_Connect_Socket (Read)", __func__);
            return false;
        }
        if (!Queue_BeginReading (queue))
        {
            rzb_log (LOG_ERR,LOG_C_STOMP,
                     "%s: failed due to failure of Queue_BeginReading", __func__);
            return false;
        }
    }

    if (((queue->iFlags & QUEUE_FLAG_SEND) == QUEUE_FLAG_SEND) && (queue->pWriteSocket == NULL))
    {
        if ((queue->pWriteSocket =
                    Queue_Connect_Socket(queue->sHostname, queue->iPort,
                    queue->sUser, queue->sPassword, queue->sVhost, queue->bUseSSL)) == NULL)
        {
            rzb_log (LOG_ERR,LOG_C_STOMP,
                     "%s: failed due to failure of Queue_Connect_Socket (Write)", __func__);
            return false;
        }
        if (queue->pWriteHeartbeat == NULL)
            queue->pWriteHeartbeat = Timer_Create(2, AMQP_Heartbeat, queue);
    }
    return true;
}

static bool
Queue_Reconnect(struct Queue *queue, int p_iSide)
{
    if (p_iSide == QUEUE_FLAG_RECV && queue->pReadSocket != NULL)
    {
        if (queue->pReadSocket != NULL)
        {
            AMQP_Socket_Close(queue->pReadSocket);
            queue->pReadSocket = NULL;
        }
    }

    if (p_iSide == QUEUE_FLAG_SEND &&
            queue->pWriteSocket != NULL)
    {
        if (queue->pWriteSocket != NULL)
        {
            AMQP_Socket_Close (queue->pWriteSocket);
            queue->pWriteSocket = NULL;
        }
    }


    return Queue_Connect(queue);
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
    l_pQueue->sHostname = (char *)p_sHost;
    l_pQueue->iPort = p_iPort;
    l_pQueue->sUser = (char *)p_sUser;
    l_pQueue->sPassword = (char*)p_sPassword;
    l_pQueue->sVhost = (char*)p_sVhost;
    l_pQueue->bUseSSL = p_bUseSSL;
    l_pQueue->iPrefetch = p_iPrefetch;
    l_pQueue->bTopic = p_bTopic;

    if ((l_pQueue->sName = (char *)calloc(strlen((char *)p_sQueueName)+1, sizeof(char))) == NULL)
    {
        rzb_log (LOG_ERR,LOG_C_STOMP, "%s: Failed to alloc new queue name", __func__);
        goto error;
    }
    strcpy(l_pQueue->sName, (char *)p_sQueueName);
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
    ASSERT (p_pQ != NULL);
    if (p_pQ == NULL) {
        rzb_log(LOG_ERR, LOG_C_QUEUE, "%s: queue is NULL", __func__);
        return;
    }

    Mutex_Lock (p_pQ->mReadMutex);
    Mutex_Lock (p_pQ->mWriteMutex);

    if ((p_pQ->iFlags & QUEUE_FLAG_RECV) == QUEUE_FLAG_RECV &&
            p_pQ->pReadSocket != NULL)
    {
        Queue_EndReading (p_pQ);
        AMQP_Socket_Close (p_pQ->pReadSocket);
    }
    if ((p_pQ->iFlags & QUEUE_FLAG_SEND) == QUEUE_FLAG_SEND &&
            p_pQ->pWriteSocket != NULL)
    {
        Timer_Destroy (p_pQ->pWriteHeartbeat);
        AMQP_Socket_Close (p_pQ->pWriteSocket);
    }


    Mutex_Unlock (p_pQ->mReadMutex);
    Mutex_Unlock (p_pQ->mWriteMutex);
    Mutex_Destroy (p_pQ->mReadMutex);
    Mutex_Destroy (p_pQ->mWriteMutex);
    free(p_pQ->sName);
    free(p_pQ);
}

SO_PUBLIC struct Message *
Queue_Get (struct Queue *queue)
{
    struct Message *ret = NULL;
    struct MessageHeader *header = NULL;
    amqp_rpc_reply_t res;
    amqp_envelope_t envelope;
    int headerIndex = 0;

    ASSERT (queue);
    if (queue == NULL) {
        rzb_log(LOG_ERR, LOG_C_QUEUE, "%s: queue is NULL", __func__);
        return NULL;
    }
    Mutex_Lock (queue->mReadMutex);

    amqp_maybe_release_buffers(queue->pReadSocket->pConn);
    res = amqp_consume_message(queue->pReadSocket->pConn, &envelope, NULL, 0);

    if (AMQP_RESPONSE_NORMAL != res.reply_type) {
        // TODO - Handle errors
        rzb_log(LOG_ERR, LOG_C_QUEUE, "%s: Error consuming message (reconnecting): %d", __func__, res.reply_type);
        Queue_Reconnect(queue, QUEUE_FLAG_RECV);
        Mutex_Unlock(queue->mReadMutex);
        return NULL;
    }
    if ((ret = (struct Message *)calloc(1,sizeof(struct Message))) == NULL)
    {
        rzb_log(LOG_ERR, LOG_C_QUEUE, "%s: Error allocating message struct", __func__);
        Mutex_Unlock(queue->mReadMutex);
        return NULL;
    }
    if ((ret->headers = Message_Header_List_Create()) == NULL) {
        rzb_log(LOG_ERR, LOG_C_QUEUE, "%s: Error allocating message header list", __func__);
        free(ret);
        Mutex_Unlock(queue->mReadMutex);
        return NULL;
    }

    ret->serialized = (uint8_t *)calloc(1, envelope.message.body.len + 1);
    if (ret->serialized == NULL) {
        rzb_log(LOG_ERR, LOG_C_QUEUE, "%s: Error allocating message body", __func__);
        Message_Destroy(ret);
        Mutex_Unlock(queue->mReadMutex);
        return NULL;
    }
    memcpy(ret->serialized, envelope.message.body.bytes, envelope.message.body.len);
    ret->length = envelope.message.body.len;
    if (envelope.message.properties._flags & AMQP_BASIC_HEADERS_FLAG) {
        //rzb_log(LOG_DEBUG, LOG_C_QUEUE, "%s: Message has %u headers", __func__, envelope.message.properties.headers.num_entries);
        for (headerIndex = 0; headerIndex < envelope.message.properties.headers.num_entries; headerIndex++) {
            amqp_table_entry_t entry = envelope.message.properties.headers.entries[headerIndex];
            char * name = strndup((char *)entry.key.bytes, entry.key.len);
            //rzb_log(LOG_DEBUG, LOG_C_QUEUE, "%s: Processing header %s - Kind: %u", __func__, name, entry.value.kind);
            if (entry.value.kind == AMQP_FIELD_KIND_UTF8) {
                //rzb_log(LOG_DEBUG, LOG_C_QUEUE, "%s: Header %s: %s", __func__, name, entry.value.value.bytes.bytes);
                Message_HeaderList_Add(ret->headers, name, entry.value.value.bytes.bytes);
            } else if (entry.value.kind == AMQP_FIELD_KIND_BYTES) {
                // Copy header values into new strings to make sure they are null terminated
                char * value = strndup((char *)entry.value.value.bytes.bytes, entry.value.value.bytes.len);
                //rzb_log(LOG_DEBUG, LOG_C_QUEUE, "%s: Header %s: %s", __func__, name, value);
                Message_HeaderList_Add(ret->headers, name, value);
                free(value);
            }
            free(name);
        }
    }
    amqp_basic_ack(queue->pReadSocket->pConn, 1, envelope.delivery_tag, 0);
    amqp_destroy_envelope(&envelope);
    Mutex_Unlock (queue->mReadMutex);

    if ((queue->iFlags & QUEUE_FLAG_EXTERNAL_MODE) != QUEUE_FLAG_EXTERNAL_MODE) {
        if ((header = (struct MessageHeader *) List_Find(ret->headers, (void *) "rzb-msg-type")) == NULL) {
            rzb_log(LOG_ERR,LOG_C_STOMP, "%s: Message header missing - rzb-msg-type", __func__);
            Message_Destroy(ret);
            return NULL;
        }
        ret->type = strtoul(header->sValue, NULL, 10);

        if ((header = (struct MessageHeader *) List_Find(ret->headers, (void *) "rzb-msg-ver")) == NULL) {
            rzb_log(LOG_ERR,LOG_C_STOMP, "%s: Message header missing - rzb-msg-ver", __func__);
            Message_Destroy(ret);
            return NULL;
        }
        ret->version = strtoul(header->sValue, NULL, 10);

        if (!Message_Setup(ret)) {
            rzb_log(LOG_ERR,LOG_C_STOMP, "%s: Message_Setup failed", __func__);
            Message_Destroy(ret);
            return NULL;
        }
        if(!ret->deserialize(ret)) {
            rzb_log(LOG_ERR,LOG_C_STOMP, "%s: Message deserialize failed: type %u body %s", __func__, ret->type, ret->serialized);
            ret->destroy(ret);
            return NULL;
        }
    }

    return ret;
}

SO_PUBLIC bool
Queue_Put (struct Queue * queue,  struct Message * message)
{
    return Queue_Put_Dest(queue, message, queue->sName);
}

struct HeaderIteratorContext {
    amqp_table_entry_t *nextEntry;
};

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
Queue_Put_Dest (struct Queue * queue,  struct Message * message, char *dest)
{
    amqp_bytes_t message_bytes;
    char *messageType = NULL;
    char *messageVer = NULL;
    amqp_bytes_t exchange;
    int amqpErr;
    struct HeaderIteratorContext ctx;
    bool ret = true;
    ASSERT (queue != NULL);
    ASSERT (message != NULL);

    if (queue == NULL)
        return false;
    if (message == NULL)
        return false;

    Mutex_Lock (queue->mWriteMutex);
    if (queue->pWriteSocket == NULL) {
        rzb_log(LOG_ERR, LOG_C_QUEUE, "%s: Write socket unavailable, attempting reconnect", __func__);
        if (!Queue_Reconnect(queue, QUEUE_FLAG_SEND) || queue->pWriteSocket == NULL) {
            Mutex_Unlock(queue->mWriteMutex);
            return false;
        }
    }

    // Don't serialize the message more than once.
    if (message->serialized == NULL)
    {
        if (!message->serialize(message))
        {
            rzb_log(LOG_ERR,LOG_C_STOMP, "%s: Failed to serialize message", __func__);
            Mutex_Unlock (queue->mWriteMutex);
            return false;
        }
    }

    if (asprintf(&messageType, "%u", message->type) == -1)
    {
        Mutex_Unlock (queue->mWriteMutex);
        return false;
    }
    if (asprintf(&messageVer, "%u", message->version) == -1)
    {
        free(messageType);
        Mutex_Unlock (queue->mWriteMutex);
        return false;
    }

    message_bytes = amqp_cstring_bytes((char *)message->serialized);
    if (queue->bTopic) {
        // For topics the destination is the routing key
        exchange = amqp_cstring_bytes("amq.topic");
    } else {
        exchange = amqp_empty_bytes;
    }
    amqp_basic_properties_t props;
    props._flags = AMQP_BASIC_CONTENT_TYPE_FLAG | AMQP_BASIC_DELIVERY_MODE_FLAG | AMQP_BASIC_HEADERS_FLAG;
    props.content_type = amqp_cstring_bytes("application/json");
    props.delivery_mode = 2; /* persistent delivery mode */
    props.headers.num_entries = 2 + List_Length(message->headers);
    props.headers.entries = (amqp_table_entry_t *)calloc(props.headers.num_entries, sizeof(amqp_table_entry_t));
    props.headers.entries[0].key = amqp_cstring_bytes("rzb-msg-type");
    props.headers.entries[0].value.kind = AMQP_FIELD_KIND_UTF8;
    props.headers.entries[0].value.value.bytes = amqp_cstring_bytes(messageType);
    props.headers.entries[1].key = amqp_cstring_bytes("rzb-msg-ver");
    props.headers.entries[1].value.kind = AMQP_FIELD_KIND_UTF8;
    props.headers.entries[1].value.value.bytes = amqp_cstring_bytes(messageVer);
    ctx.nextEntry = &(props.headers.entries[2]);
    List_ForEach(message->headers, Message_Header_To_AMQP_TableEntry, &ctx);

    if (( amqpErr = amqp_basic_publish(
            queue->pWriteSocket->pConn,
            AMQP_CHAN_ID,
            exchange,
            amqp_cstring_bytes(dest),
            0, 0, &props, message_bytes)) < 0) {
        rzb_log(LOG_ERR, LOG_C_QUEUE, "%s: Failed to publish message: %s", __func__, amqp_error_string2(amqpErr));
        ret = false;
        Queue_Reconnect(queue, QUEUE_FLAG_SEND);
    }
    free(props.headers.entries);
    free(messageType);
    free(messageVer);

    Mutex_Unlock (queue->mWriteMutex);
    return ret;
}

SO_PUBLIC void
Queue_GetQueueName (const char * p_sLeading, uuid_t p_pId,
                    char * p_sQueueName)
{
    char l_sUUID[UUID_STRING_LENGTH];

    uuid_unparse (p_pId, l_sUUID);
    sprintf (p_sQueueName, "%s.%s", p_sLeading, l_sUUID);
}
