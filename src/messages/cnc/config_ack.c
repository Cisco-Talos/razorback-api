#include "config.h"

#include <razorback/debug.h>
#include <razorback/messages.h>
#include <razorback/log.h>

#include "messages/core.h"
#include "messages/cnc/core.h"
#include "binary_buffer.h"

static bool ConfigAck_Deserialize_Binary(struct Message *message);
static bool ConfigAck_Deserialize(struct Message *message, int mode);
static bool ConfigAck_Serialize_Binary(struct Message *message);
static bool ConfigAck_Serialize(struct Message *message, int mode);

SO_PUBLIC struct Message *
MessageConfigurationAck_Initialize (
                                    const uuid_t p_uuidSourceNugget,
                                    const uuid_t p_uuidDestNugget,
                                    const uuid_t p_uuidNuggetType,
                                    const uuid_t p_uuidApplicationType)
{
    struct Message *msg;
    struct MessageConfigurationAck * message;

    if ((msg = Message_CncCreate(MESSAGE_TYPE_CONFIG_ACK, MESSAGE_VERSION_1, sizeof(struct MessageConfigurationAck), p_uuidSourceNugget, p_uuidDestNugget)) == NULL)
        return NULL;
    message = msg->message;

    uuid_copy (message->uuidNuggetType, p_uuidNuggetType);
    uuid_copy (message->uuidApplicationType, p_uuidApplicationType);
    msg->destroy = Message_Destroy;
    msg->deserialize = ConfigAck_Deserialize;
    msg->serialize = ConfigAck_Serialize;
    return msg;
}

void 
Message_CnC_ConfigAck_Setup(struct Message *msg)
{
    msg->destroy = Message_Destroy;
    msg->deserialize = ConfigAck_Deserialize;
    msg->serialize = ConfigAck_Serialize;
}

static bool
ConfigAck_Deserialize_Binary(struct Message *message)
{
    struct BinaryBuffer *buffer;
    struct MessageConfigurationAck *configAck;

    ASSERT(message != NULL);
    if (message == NULL)
        return false;
    
    if ((buffer = BinaryBuffer_CreateFromMessage(message)) == NULL)
        return false;
    
    configAck = message->message;
    if (!BinaryBuffer_Get_UUID (buffer, configAck->uuidNuggetType))
    {
        buffer->pBuffer = NULL;
        BinaryBuffer_Destroy (buffer);
        rzb_log (LOG_ERR,
                 "%s: failed due to failure of BinaryBuffer_Get_UUID", __func__);
        return false;
    }
    if (!BinaryBuffer_Get_UUID(buffer, configAck->uuidApplicationType))
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
ConfigAck_Deserialize(struct Message *message, int mode)
{
    ASSERT(message != NULL);
    if ( message == NULL )
        return false;

    if ((message->message = calloc(1,sizeof(struct MessageConfigurationAck))) == NULL)
        return false;

    switch (mode)
    {
    case MESSAGE_MODE_BIN:
        return ConfigAck_Deserialize_Binary(message);
    case MESSAGE_MODE_JSON:
        break;
    default:
        rzb_log(LOG_ERR, "%s: Invalid deserialization mode", __func__);
        return false;
    }
    return false;
}

static bool
ConfigAck_Serialize_Binary(struct Message *message)
{
    struct MessageConfigurationAck *configAck;
    struct BinaryBuffer *buffer;

    ASSERT(message != NULL);
    if (message == NULL)
        return false;

    configAck = message->message;

    message->length = sizeof(uuid_t) * 2;

    if ((buffer = BinaryBuffer_Create(message->length)) == NULL)
        return false;


    if (!BinaryBuffer_Put_UUID
        (buffer, configAck->uuidNuggetType))
    {
        BinaryBuffer_Destroy (buffer);
        rzb_log (LOG_ERR,
                 "%s: failed due to failure of BinaryBuffer_Put_UUID ( Nug Type )", __func__);
        return false;
    }

    if (!BinaryBuffer_Put_UUID
        (buffer, configAck->uuidApplicationType))
    {
        BinaryBuffer_Destroy (buffer);
        rzb_log (LOG_ERR,
                 "%s: failed due to failure of BinaryBuffer_Put_UUID ( App Type) ", __func__);
        return false;
    }


    message->serialized = buffer->pBuffer;
    buffer->pBuffer = NULL;
    BinaryBuffer_Destroy(buffer);
    return true;
}


static bool
ConfigAck_Serialize(struct Message *message, int mode)
{
    ASSERT(message != NULL);
    if ( message == NULL )
        return false;

    switch (mode)
    {
    case MESSAGE_MODE_BIN:
        return ConfigAck_Serialize_Binary(message);
    case MESSAGE_MODE_JSON:
        break;
    default:
        rzb_log(LOG_ERR, "%s: Invalid deserialization mode", __func__);
        return false;
    }
    return false;
}

