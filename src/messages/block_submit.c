#include "config.h"

#include <razorback/debug.h>
#include <razorback/messages.h>
#include <razorback/log.h>
#include <razorback/event.h>
#include <razorback/storage.h>

#include "messages/core.h"
#include "binary_buffer.h"
static void BlockSubmission_Destroy (struct Message *message);
static bool BlockSubmission_Deserialize_Binary(struct Message *message);
static bool BlockSubmission_Deserialize(struct Message *message, int mode);
static bool BlockSubmission_Serialize_Binary(struct Message *message);
static bool BlockSubmission_Serialize(struct Message *message, int mode);

SO_PUBLIC struct Message *
MessageBlockSubmission_Initialize (
                                   struct Event *p_pEvent,
                                   uint32_t p_iReason)
{
    struct Message *msg;
    struct MessageBlockSubmission *message;
    ASSERT (p_pEvent != NULL);
    if (p_pEvent == NULL)
        return NULL;

    if ((msg = Message_Create(MESSAGE_TYPE_BLOCK, MESSAGE_VERSION_1, sizeof(struct MessageBlockSubmission))) == NULL)
        return NULL;

    message = msg->message;

    message->pEvent = p_pEvent;

    msg->destroy = BlockSubmission_Destroy;
    msg->deserialize=BlockSubmission_Deserialize;
    msg->serialize=BlockSubmission_Serialize;

    return msg;
}

void 
MessageBlockSubmission_Setup(struct Message *msg)
{
    msg->destroy = BlockSubmission_Destroy;
    msg->deserialize=BlockSubmission_Deserialize;
    msg->serialize=BlockSubmission_Serialize;
}

static void
BlockSubmission_Destroy (struct Message *message)
{
    struct MessageBlockSubmission *msg;
    
    ASSERT (message != NULL);
    if (message == NULL)
        return;
    msg = message->message;

    // destroy any malloc'd components
    if (msg->pEvent != NULL)
        Event_Destroy (msg->pEvent);

    if (msg->ticket != NULL)
        Storage_TicketDestroy(msg->ticket);

    Message_Destroy(message);
}

static bool
BlockSubmission_Deserialize_Binary(struct Message *message)
{
    struct BinaryBuffer *buffer;
    struct MessageBlockSubmission *submit;
    uint8_t haveTicket = 0;

    ASSERT(message != NULL);
    if (message == NULL)
        return false;
    
    if ((buffer = BinaryBuffer_CreateFromMessage(message)) == NULL)
        return false;
    
    submit = message->message;

    if (!BinaryBuffer_Get_uint32_t (buffer, &submit->iReason))
    {
        buffer->pBuffer = NULL;
        BinaryBuffer_Destroy (buffer);
        return false;
    }
    if (!BinaryBuffer_Get_Event (buffer, &submit->pEvent))
    {
        buffer->pBuffer = NULL;
        BinaryBuffer_Destroy (buffer);
        return false;
    }

    if (!BinaryBuffer_Get_uint8_t (buffer, &haveTicket))
    {
        buffer->pBuffer = NULL;
        BinaryBuffer_Destroy (buffer);
        return false;
    }
    if (haveTicket == 1)
    {
        if (!BinaryBuffer_Get_TransferTicket (buffer, &submit->ticket))
        {
            buffer->pBuffer = NULL;
            BinaryBuffer_Destroy (buffer);
            return false;
        }
    }
    buffer->pBuffer = NULL;
    BinaryBuffer_Destroy (buffer);

    return true;
}

static bool
BlockSubmission_Deserialize(struct Message *message, int mode)
{
    ASSERT(message != NULL);
    if ( message == NULL )
        return false;

    if ((message->message = calloc(1,sizeof(struct MessageBlockSubmission))) == NULL)
        return false;

    switch (mode)
    {
    case MESSAGE_MODE_BIN:
        return BlockSubmission_Deserialize_Binary(message);
    case MESSAGE_MODE_JSON:
        break;
    default:
        rzb_log(LOG_ERR, "%s: Invalid deserialization mode", __func__);
        return false;
    }
    return false;
}


static bool
BlockSubmission_Serialize_Binary(struct Message *message)
{
    struct MessageBlockSubmission *submit;
    struct BinaryBuffer *buffer;

    ASSERT(message != NULL);
    if (message == NULL)
        return false;

    submit = message->message;

    message->length = Event_BinaryLength (submit->pEvent) +
         sizeof(uint8_t) + // XXX: Whats this bad boy?
         (uint32_t) sizeof (submit->iReason) +
         sizeof(uint8_t); // Ticket marker
    if (submit->ticket != NULL)
        message->length+=ticketSize(submit->ticket);

    if ((buffer = BinaryBuffer_Create(message->length)) == NULL)
        return false;

    if (!BinaryBuffer_Put_uint32_t (buffer, submit->iReason))
    {
        BinaryBuffer_Destroy (buffer);
        return false;
    }

    if (!BinaryBuffer_Put_Event (buffer, submit->pEvent))
    {
        BinaryBuffer_Destroy (buffer);
        return false;
    }
    if (submit->ticket == NULL)
    {
        if (!BinaryBuffer_Put_uint8_t (buffer, 0))
        {
            BinaryBuffer_Destroy (buffer);
            return false;
        }
    }
    else
    {
        if (!BinaryBuffer_Put_uint8_t (buffer, 1))
        {
            BinaryBuffer_Destroy (buffer);
            return false;
        }
        if (!BinaryBuffer_Put_TransferTicket (buffer, submit->ticket))
        {
            BinaryBuffer_Destroy (buffer);
            return false;
        }
    }


    message->serialized = buffer->pBuffer;
    buffer->pBuffer = NULL;
    BinaryBuffer_Destroy(buffer);
    return true;
}


static bool
BlockSubmission_Serialize(struct Message *message, int mode)
{
    ASSERT(message != NULL);
    if ( message == NULL )
        return false;

    switch (mode)
    {
    case MESSAGE_MODE_BIN:
        return BlockSubmission_Serialize_Binary(message);
    case MESSAGE_MODE_JSON:
        break;
    default:
        rzb_log(LOG_ERR, "%s: Invalid deserialization mode", __func__);
        return false;
    }
    return false;
}
