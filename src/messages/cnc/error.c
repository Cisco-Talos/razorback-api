#include "config.h"

#include <razorback/debug.h>
#include <razorback/messages.h>
#include <razorback/log.h>

#include "messages/core.h"
#include "messages/cnc/core.h"
#include "binary_buffer.h"

#include <string.h>

static bool Error_Deserialize_Binary(struct Message *message);
static bool Error_Deserialize(struct Message *message, int mode);
static void Error_Destroy (struct Message *message);
static bool Error_Serialize_Binary(struct Message *message);
static bool Error_Serialize(struct Message *message, int mode);

SO_PUBLIC struct Message *
MessageError_Initialize (
                        uint32_t p_iCode,
                        const char *p_sMessage,
                       const uuid_t p_uuidSourceNugget,
                       const uuid_t p_uuidDestNugget)
{
    struct Message *msg;
    struct MessageError *message;

    if ((msg = Message_CncCreate(p_iCode, MESSAGE_VERSION_1, sizeof(struct MessageError), p_uuidSourceNugget, p_uuidDestNugget)) == NULL)
        return NULL;
    message = msg->message;

    if ((message->sMessage =
         malloc (strlen ((const char *) p_sMessage) + 1)) == NULL)
    {
        Error_Destroy(msg);
        rzb_log (LOG_ERR,
                 "%s: failed due to lack of memory", __func__);
        return NULL;
    }
    strcpy ((char *) message->sMessage,
            (const char *) p_sMessage);
    msg->destroy = Error_Destroy;
    msg->deserialize = Error_Deserialize;
    msg->serialize = Error_Serialize;
    return msg;
}

void 
Message_CnC_Error_Setup(struct Message *msg)
{
    msg->destroy = Error_Destroy;
    msg->deserialize = Error_Deserialize;
    msg->serialize = Error_Serialize;
}

static void
Error_Destroy (struct Message *msg)
{
    struct MessageError *message;
    ASSERT (msg != NULL);
    if (msg == NULL)
        return;

    message = msg->message;

    if (message->sMessage != NULL)
        free(message->sMessage);

    free(message);
}

bool
Error_Deserialize_Binary(struct Message *message)
{
    struct BinaryBuffer *buffer;
    struct MessageError *error;
    
    ASSERT(message != NULL);
    if (message == NULL)
        return false;
    
    if ((buffer = BinaryBuffer_CreateFromMessage(message)) == NULL)
        return false;
    
    error = message->message;

    if ((error->sMessage = BinaryBuffer_Get_String (buffer)) == NULL)
    {
        buffer->pBuffer = NULL;
        BinaryBuffer_Destroy (buffer);
        rzb_log (LOG_ERR,
                 "%s: ( TERM ) failed due to failure of BinaryBuffer_Get_String", __func__);
        return false;
    }
    buffer->pBuffer = NULL;
    BinaryBuffer_Destroy (buffer);
    return true;
}


static bool
Error_Deserialize(struct Message *message, int mode)
{
    ASSERT(message != NULL);
    if ( message == NULL )
        return false;

    if ((message->message = calloc(1,sizeof(struct MessageError))) == NULL)
        return false;

    switch (mode)
    {
    case MESSAGE_MODE_BIN:
        return Error_Deserialize_Binary(message);
    case MESSAGE_MODE_JSON:
        break;
    default:
        rzb_log(LOG_ERR, "%s: Invalid deserialization mode", __func__);
        return false;
    }
    return false;
}

static bool
Error_Serialize_Binary(struct Message *message)
{
    struct MessageError *error;
    struct BinaryBuffer *buffer;

    ASSERT(message != NULL);
    if (message == NULL)
        return false;

    error = message->message;

    message->length = strlen((const char *)error->sMessage) +1;

    if ((buffer = BinaryBuffer_Create(message->length)) == NULL)
        return false;

    if (!BinaryBuffer_Put_String
        (buffer, error->sMessage))
    {
        BinaryBuffer_Destroy (buffer);
        rzb_log (LOG_ERR,
                 "%s: ( TERM ) failed due to failure of BinaryBuffer_Put_String", __func__);
        return false;
    }



    message->serialized = buffer->pBuffer;
    buffer->pBuffer = NULL;
    BinaryBuffer_Destroy(buffer);
    return true;
}


static bool
Error_Serialize(struct Message *message, int mode)
{
    ASSERT(message != NULL);
    if ( message == NULL )
        return false;

    switch (mode)
    {
    case MESSAGE_MODE_BIN:
        return Error_Serialize_Binary(message);
    case MESSAGE_MODE_JSON:
        break;
    default:
        rzb_log(LOG_ERR, "%s: Invalid deserialization mode", __func__);
        return false;
    }
    return false;
}
