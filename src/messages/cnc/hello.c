#include "config.h"

#include <razorback/debug.h>
#include <razorback/messages.h>
#include <razorback/log.h>

#include "messages/core.h"
#include "messages/cnc/core.h"
#include "binary_buffer.h"

#include <string.h>

static bool Hello_Deserialize_Binary(struct Message *message);
static bool Hello_Deserialize(struct Message *message, int mode);
static bool Hello_Serialize_Binary(struct Message *message);
static bool Hello_Serialize(struct Message *message, int mode);


static bool
Hello_Deserialize_Binary(struct Message *message)
{
    struct BinaryBuffer *buffer;
    struct MessageHello *hello;

    ASSERT(message != NULL);
    if (message == NULL)
        return false;
    
    if ((buffer = BinaryBuffer_CreateFromMessage(message)) == NULL)
        return false;
    
    hello = message->message;

    if (!BinaryBuffer_Get_UUID
        (buffer, hello->uuidNuggetType))
    {
        buffer->pBuffer = NULL;
        BinaryBuffer_Destroy (buffer);
        rzb_log (LOG_ERR,
                 "%s: failed due to failure of BinaryBuffer_Get_UUID", __func__);
        return false;
    }

    if (!BinaryBuffer_Get_UUID(buffer, hello->uuidApplicationType))
    {
        buffer->pBuffer = NULL;
        BinaryBuffer_Destroy (buffer);
        rzb_log (LOG_ERR,
                 "%s: failed due to failure of BinaryBuffer_Get_UUID", __func__);
        return false;
    }
    buffer->pBuffer = NULL;
    BinaryBuffer_Destroy (buffer);

    return true;
}

static bool
Hello_Deserialize(struct Message *message, int mode)
{
    ASSERT(message != NULL);
    if ( message == NULL )
        return false;

    if ((message->message = calloc(1,sizeof(struct MessageHello))) == NULL)
        return false;

    switch (mode)
    {
    case MESSAGE_MODE_BIN:
        return Hello_Deserialize_Binary(message);
    case MESSAGE_MODE_JSON:
        break;
    default:
        rzb_log(LOG_ERR, "%s: Invalid deserialization mode", __func__);
        return false;
    }
    return false;
}

SO_PUBLIC struct Message *
MessageHello_Initialize (
                         const uuid_t p_uuidSourceNugget,
                         const uuid_t p_uuidNuggetType,
                         const uuid_t p_uuidApplicationType)
{
    struct Message * msg;
    struct MessageHello *message;

    if ((msg = Message_CncBcastCreate(MESSAGE_TYPE_HELLO, MESSAGE_VERSION_1, sizeof(struct MessageHello), p_uuidSourceNugget)) == NULL)
        return NULL;

    message = msg->message;

    uuid_copy (message->uuidNuggetType, p_uuidNuggetType);
    uuid_copy (message->uuidApplicationType, p_uuidApplicationType);

    msg->destroy=Message_Destroy;
    msg->deserialize=Hello_Deserialize;
    msg->serialize=Hello_Serialize;

    return msg;
}
void 
Message_CnC_Hello_Setup(struct Message *msg)
{
    msg->destroy = Message_Destroy;
    msg->deserialize=Hello_Deserialize;
    msg->serialize=Hello_Serialize;
}

static bool
Hello_Serialize_Binary(struct Message *message)
{
    struct MessageHello *hello;
    struct BinaryBuffer *buffer;

    ASSERT(message != NULL);
    if (message == NULL)
        return false;

    hello = message->message;

    message->length = sizeof(uuid_t) * 2;

    if ((buffer = BinaryBuffer_Create(message->length)) == NULL)
        return false;

    if (!BinaryBuffer_Put_UUID
        (buffer, hello->uuidNuggetType))
    {
        BinaryBuffer_Destroy (buffer);
        rzb_log (LOG_ERR,
                 "%s: failed due to failure of BinaryBuffer_Put_UUID", __func__);
        return false;
    }
    if (!BinaryBuffer_Put_UUID
        (buffer, hello->uuidApplicationType))
    {
        BinaryBuffer_Destroy (buffer);
        rzb_log (LOG_ERR,
                 "%s: failed due to failure of BinaryBuffer_Put_UUID", __func__);
        return false;
    }

    message->serialized = buffer->pBuffer;
    buffer->pBuffer = NULL;
    BinaryBuffer_Destroy(buffer);
    return true;
}


static bool
Hello_Serialize(struct Message *message, int mode)
{
    ASSERT(message != NULL);
    if ( message == NULL )
        return false;

    switch (mode)
    {
    case MESSAGE_MODE_BIN:
        return Hello_Serialize_Binary(message);
    case MESSAGE_MODE_JSON:
        break;
    default:
        rzb_log(LOG_ERR, "%s: Invalid deserialization mode", __func__);
        return false;
    }
    return false;
}
