#include "config.h"

#include <razorback/debug.h>
#include <razorback/messages.h>
#include <razorback/log.h>
#include <razorback/event.h>

#include "messages/core.h"
#include "binary_buffer.h"

#include <string.h>

static void Log_Destroy (struct Message *msg);
static bool Log_Deserialize_Binary(struct Message *message);
static bool Log_Deserialize(struct Message *message, int mode);
static bool Log_Serialize_Binary(struct Message *message);
static bool Log_Serialize(struct Message *message, int mode);


SO_PUBLIC struct Message *
MessageLog_Initialize (
                         const uuid_t p_uuidNuggetId,
                         uint8_t p_iPriority,
                         char *p_sMessage,
                         struct EventId *p_pEventId)
{
    struct Message *msg;
    struct MessageLogSubmission *message;
    ASSERT (p_sMessage != NULL);

    if (p_sMessage == NULL)
        return NULL;

    if ((msg = Message_Create(MESSAGE_TYPE_LOG, MESSAGE_VERSION_1, sizeof(struct MessageAlert))) == NULL)
        return NULL;

    message = msg->message;

    if (p_pEventId != NULL)
    {
        if ((message->pEventId = EventId_Clone(p_pEventId)) == NULL)
        {
            rzb_log(LOG_ERR, "%s: Failed to clone event id.", __func__);
            Log_Destroy(msg);
            return NULL;
        }
    }

    message->iPriority = p_iPriority;
    uuid_copy (message->uuidNuggetId, p_uuidNuggetId);
    message->sMessage = (uint8_t *)p_sMessage;
    msg->destroy = Log_Destroy;
    msg->deserialize=Log_Deserialize;
    msg->serialize=Log_Serialize;
    return msg;
}

void 
MessageLogSubmission_Setup(struct Message *msg)
{
    msg->destroy = Log_Destroy;
    msg->deserialize=Log_Deserialize;
    msg->serialize=Log_Serialize;
}

static void 
Log_Destroy (struct Message *msg)
{
    struct MessageLogSubmission *message;
    ASSERT (msg != NULL);
    if (msg  == NULL)
        return;
    message = msg->message; 

    // destroy any malloc'd components
    if (message->pEventId != NULL)
        EventId_Destroy (message->pEventId);

    Message_Destroy(msg);
}
static bool
Log_Deserialize_Binary(struct Message *message)
{
    struct BinaryBuffer *buffer;
    struct MessageLogSubmission *submit;
    uint8_t l_iHas = 0;

    ASSERT(message != NULL);
    if (message == NULL)
        return false;
    
    if ((buffer = BinaryBuffer_CreateFromMessage(message)) == NULL)
        return false;
    
    submit = message->message;

    if (!BinaryBuffer_Get_UUID (buffer, submit->uuidNuggetId))
    {
        buffer->pBuffer = NULL;
        BinaryBuffer_Destroy (buffer);
        rzb_log (LOG_ERR,
                 "%s: failed due to failure of BinaryBuffer_Get_UUID", __func__);
        return false;
    }
    if (!BinaryBuffer_Get_uint8_t (buffer, &submit->iPriority))
    {
        buffer->pBuffer = NULL;
        rzb_log (LOG_ERR,
                 "%s: failed due failure of BinaryBuffer_Get_uint8_t", __func__);
        BinaryBuffer_Destroy (buffer);
        return false;
    }
    if (!BinaryBuffer_Get_uint8_t (buffer, &l_iHas))
    {
        buffer->pBuffer = NULL;
        rzb_log (LOG_ERR,
                 "%s: failed due failure of BinaryBuffer_Get_uint8_t", __func__);
        BinaryBuffer_Destroy (buffer);
        return false;
    }

    if (l_iHas == 1)
    {
        if (!BinaryBuffer_Get_EventId (buffer, &submit->pEventId))
        {
            buffer->pBuffer = NULL;
            BinaryBuffer_Destroy (buffer);
            rzb_log (LOG_ERR,
                     "%s: failed due to failure of BinaryBuffer_Get_EventId", __func__);
            return false;
        }
    }
    else
        submit->pEventId = NULL;

    if ((submit->sMessage = BinaryBuffer_Get_String(buffer)) == NULL)
    {
        buffer->pBuffer = NULL;
        BinaryBuffer_Destroy (buffer);
        rzb_log (LOG_ERR,
                 "%s: failed due to failure of BinaryBuffer_Get_String", __func__);
        return false;
    }
    buffer->pBuffer = NULL;
    BinaryBuffer_Destroy (buffer);

    return true;
}

static bool
Log_Deserialize(struct Message *message, int mode)
{
    ASSERT(message != NULL);
    if ( message == NULL )
        return false;

    if ((message->message = calloc(1,sizeof(struct MessageLogSubmission))) == NULL)
        return false;

    switch (mode)
    {
    case MESSAGE_MODE_BIN:
        return Log_Deserialize_Binary(message);
    case MESSAGE_MODE_JSON:
        break;
    default:
        rzb_log(LOG_ERR, "%s: Invalid deserialization mode", __func__);
        return false;
    }
    return false;
}


static bool
Log_Serialize_Binary(struct Message *message)
{
    struct MessageLogSubmission *submit;
    struct BinaryBuffer *buffer;

    ASSERT(message != NULL);
    if (message == NULL)
        return false;

    submit = message->message;

    message->length = (uint32_t) sizeof (submit->iPriority) +
                (uint32_t) sizeof (submit->uuidNuggetId) +
                strlen((char *)submit->sMessage) +1 +
                sizeof(uint8_t); // The event id flag.

    if (submit->pEventId != NULL)
        message->length += sizeof(struct EventId);


    if ((buffer = BinaryBuffer_Create(message->length)) == NULL)
        return false;

    if (!BinaryBuffer_Put_UUID (buffer, submit->uuidNuggetId))
    {
        BinaryBuffer_Destroy (buffer);
        rzb_log (LOG_ERR,
                 "%s: failed due to failure of BinaryBuffer_Put_UUID", __func__);
        return false;
    }
    if (!BinaryBuffer_Put_uint8_t (buffer, submit->iPriority))
    {
        rzb_log (LOG_ERR,
                 "%s: failed due failure of BinaryBuffer_Put_uint8_t", __func__);
        return false;
    }

    if (submit->pEventId == NULL)
    {
        if (!BinaryBuffer_Put_uint8_t (buffer, 0))
        {
            rzb_log (LOG_ERR,
                     "%s: failed due failure of BinaryBuffer_Put_uint8_t", __func__);
            return false;
        }
    }
    else
    {
        if (!BinaryBuffer_Put_uint8_t (buffer, 1))
        {
            rzb_log (LOG_ERR,
                     "%s: failed due failure of BinaryBuffer_Put_uint8_t", __func__);
            return false;
        }
        if (!BinaryBuffer_Put_EventId (buffer, submit->pEventId))
        {
            BinaryBuffer_Destroy (buffer);
            rzb_log (LOG_ERR,
                     "%s: failed due to failure of BinaryBuffer_Put_BlockId", __func__);
            return false;
        }
    }
    if (!BinaryBuffer_Put_String (buffer, submit->sMessage))
    {
        BinaryBuffer_Destroy (buffer);
        rzb_log (LOG_ERR,
                 "%s: failed due to failure of BinaryBuffer_Put_String", __func__);
        return false;
    }

    message->serialized = buffer->pBuffer;
    buffer->pBuffer = NULL;
    BinaryBuffer_Destroy(buffer);
    return true;
}


static bool
Log_Serialize(struct Message *message, int mode)
{
    ASSERT(message != NULL);
    if ( message == NULL )
        return false;

    switch (mode)
    {
    case MESSAGE_MODE_BIN:
        return Log_Serialize_Binary(message);
    case MESSAGE_MODE_JSON:
        break;
    default:
        rzb_log(LOG_ERR, "%s: Invalid deserialization mode", __func__);
        return false;
    }
    return false;
}
