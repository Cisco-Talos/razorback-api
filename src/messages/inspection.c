#include "config.h"

#include <razorback/debug.h>
#include <razorback/messages.h>
#include <razorback/log.h>
#include <razorback/block.h>
#include <razorback/event.h>
#include <razorback/storage.h>


#include "messages/core.h"
#include "binary_buffer.h"
static void InspectionSubmission_Destroy (struct Message *message);
static bool InspectionSubmission_Deserialize_Binary(struct Message *message);
static bool InspectionSubmission_Deserialize(struct Message *message, int mode);
static bool InspectionSubmission_Serialize_Binary(struct Message *message);
static bool InspectionSubmission_Serialize(struct Message *message, int mode);

SO_PUBLIC struct Message *
MessageInspectionSubmission_Initialize (
                                        const struct Event *p_pEvent,
                                        uint32_t p_iReason,
                                        struct TransferTicket *ticket)
{
    struct Message * msg;
    struct MessageInspectionSubmission *message;

    ASSERT (p_pEvent != NULL);
    if (p_pEvent == NULL)
        return NULL;

    if ((msg = Message_Create(MESSAGE_TYPE_INSPECTION, MESSAGE_VERSION_1, sizeof(struct MessageInspectionSubmission))) == NULL)
        return NULL;
    message = msg->message;
	
    if ((message->pBlock = Block_Clone(p_pEvent->pBlock)) == NULL)
    {
        rzb_log(LOG_ERR, "%s: Failed to clone block", __func__);
        InspectionSubmission_Destroy(msg);
        return NULL;
    }

    message->iReason = p_iReason;
    if ((message->eventId = EventId_Clone(p_pEvent->pId)) == NULL)
    {
        rzb_log(LOG_ERR, "%s: Failed to clone event id", __func__);
        InspectionSubmission_Destroy(msg);
        return NULL;
    }
    message->pEventMetadata = List_Clone (p_pEvent->pMetaDataList);
    if (message->pEventMetadata == NULL)
    {
        rzb_log(LOG_ERR, "%s: Failed to clone metadata list", __func__);
        InspectionSubmission_Destroy(msg);
        return NULL;
    }
    message->ticket = ticket;
    msg->destroy=InspectionSubmission_Destroy;
    msg->deserialize=InspectionSubmission_Deserialize;
    msg->serialize=InspectionSubmission_Serialize;

    return msg;
}
void 
MessageInspectionSubmission_Setup(struct Message *msg)
{
    msg->destroy = InspectionSubmission_Destroy;
    msg->deserialize=InspectionSubmission_Deserialize;
    msg->serialize=InspectionSubmission_Serialize;
}

static void
InspectionSubmission_Destroy (struct Message
                                     *message)
{
    struct MessageInspectionSubmission *msg;
    ASSERT (message != NULL);
    if (message == NULL)
        return;

    msg = message->message;

    // destroy any malloc'd components
    if (msg->pBlock != NULL)
        Block_Destroy (msg->pBlock);
    if (msg->eventId != NULL)
        EventId_Destroy(msg->eventId);
    if (msg->pEventMetadata != NULL)
        List_Destroy(msg->pEventMetadata);

    if (msg->ticket != NULL)
        Storage_TicketDestroy(msg->ticket);

    Message_Destroy(message);
}

static bool
InspectionSubmission_Deserialize_Binary(struct Message *message)
{
    struct BinaryBuffer *buffer;
    struct MessageInspectionSubmission *submit;
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
    if (!BinaryBuffer_Get_EventId (buffer, &submit->eventId))
    {
        buffer->pBuffer = NULL;
        BinaryBuffer_Destroy (buffer);
        return false;
    }
    if (!BinaryBuffer_Get_NTLVList (buffer, &submit->pEventMetadata))
    {
        buffer->pBuffer = NULL;
        BinaryBuffer_Destroy (buffer);
        return false;
    }
    
    if (!BinaryBuffer_Get_Block (buffer, &submit->pBlock))
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
InspectionSubmission_Deserialize(struct Message *message, int mode)
{
    ASSERT(message != NULL);
    if ( message == NULL )
        return false;

    if ((message->message = calloc(1,sizeof(struct MessageInspectionSubmission))) == NULL)
        return false;

    switch (mode)
    {
    case MESSAGE_MODE_BIN:
        return InspectionSubmission_Deserialize_Binary(message);
    case MESSAGE_MODE_JSON:
        break;
    default:
        rzb_log(LOG_ERR, "%s: Invalid deserialization mode", __func__);
        return false;
    }
    return false;
}


static bool
InspectionSubmission_Serialize_Binary(struct Message *message)
{
    struct MessageInspectionSubmission *submit;
    struct BinaryBuffer *buffer;

    ASSERT(message != NULL);
    if (message == NULL)
        return false;

    submit = message->message;

    message->length = Block_BinaryLength (submit->pBlock) +
            NTLVList_Size (submit->pEventMetadata) +
            sizeof(struct EventId) + //Source nugget ID
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
    if (!BinaryBuffer_Put_EventId (buffer, submit->eventId))
    {
        BinaryBuffer_Destroy (buffer);
        return false;
    }
    if (!BinaryBuffer_Put_NTLVList (buffer, submit->pEventMetadata))
    {
        BinaryBuffer_Destroy (buffer);
        return false;
    }

    if (!BinaryBuffer_Put_Block (buffer, submit->pBlock))
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
InspectionSubmission_Serialize(struct Message *message, int mode)
{
    ASSERT(message != NULL);
    if ( message == NULL )
        return false;

    switch (mode)
    {
    case MESSAGE_MODE_BIN:
        return InspectionSubmission_Serialize_Binary(message);
    case MESSAGE_MODE_JSON:
        break;
    default:
        rzb_log(LOG_ERR, "%s: Invalid deserialization mode", __func__);
        return false;
    }
    return false;
}
