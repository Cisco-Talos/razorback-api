#include "config.h"

#include <razorback/debug.h>
#include <razorback/messages.h>
#include <razorback/log.h>

#include "messages/core.h"
#include "messages/cnc/core.h"
#include "binary_buffer.h"

static bool RegistrationRequest_Deserialize_Binary(struct Message *message);
static bool RegistrationRequest_Deserialize(struct Message *message, int mode);
static bool RegistrationRequest_Serialize_Binary(struct Message *message);
static bool RegistrationRequest_Serialize(struct Message *message, int mode);
static void RegistrationRequest_Destroy (struct Message *message);

SO_PUBLIC struct Message *
MessageRegistrationRequest_Initialize (
                                       const uuid_t p_uuidSourceNugget,
                                       const uuid_t p_uuidNuggetType,
                                       const uuid_t p_uuidApplicationType,
                                       uint32_t p_iDataTypeCount,
                                       uuid_t * p_pDataTypeList)
{
    uint32_t l_iIndex;
    struct Message *msg;
    struct MessageRegistrationRequest *message;

    if ((msg = Message_CncBcastCreate(MESSAGE_TYPE_REG_REQ, MESSAGE_VERSION_1, sizeof(struct MessageRegistrationRequest), p_uuidSourceNugget)) == NULL)
        return NULL;
    message = msg->message;

    uuid_copy (message->uuidNuggetType, p_uuidNuggetType);
    uuid_copy (message->uuidApplicationType, p_uuidApplicationType);
    message->iDataTypeCount = p_iDataTypeCount;
    if (p_iDataTypeCount > 0)
    {
        if ((message->pDataTypeList =
             malloc (sizeof (uuid_t) * p_iDataTypeCount)) == NULL)
        {
            rzb_log (LOG_ERR,
                     "%s: failed due to lack of memory", __func__);
            RegistrationRequest_Destroy(msg);
            return NULL;
        }
    } 
    else
        message->pDataTypeList = NULL;

    for (l_iIndex = 0; l_iIndex < message->iDataTypeCount; l_iIndex++)
        uuid_copy (message->pDataTypeList[l_iIndex],
                   p_pDataTypeList[l_iIndex]);

    msg->destroy = RegistrationRequest_Destroy;
    msg->deserialize = RegistrationRequest_Deserialize;
    msg->serialize = RegistrationRequest_Serialize;
    return msg;

}

void
Message_CnC_RegReq_Setup(struct Message *msg)
{
    msg->destroy = RegistrationRequest_Destroy;
    msg->deserialize = RegistrationRequest_Deserialize;
    msg->serialize = RegistrationRequest_Serialize;
}

static void
RegistrationRequest_Destroy (struct Message *msg)
{
    struct MessageRegistrationRequest *message;

    ASSERT (msg != NULL);
    if (msg == NULL)
        return;
    message = msg->message;

	if(message->pDataTypeList != NULL)
	    free (message->pDataTypeList);

    Message_Destroy(msg);
}


static bool
RegistrationRequest_Deserialize_Binary(struct Message *message)
{
    struct BinaryBuffer *buffer;
    struct MessageRegistrationRequest *regReq;

    ASSERT(message != NULL);
    if (message == NULL)
        return false;
    
    if ((buffer = BinaryBuffer_CreateFromMessage(message)) == NULL)
        return false;
    
    regReq = message->message;

    if (!BinaryBuffer_Get_UUID (buffer, regReq->uuidNuggetType))
    {
        buffer->pBuffer = NULL;
        BinaryBuffer_Destroy (buffer);
        rzb_log (LOG_ERR,
                 "%s: failed due to failure of BinaryBuffer_Get_UUID", __func__);
        return false;
    }
    if (!BinaryBuffer_Get_UUID
        (buffer, regReq->uuidApplicationType))
    {
        buffer->pBuffer = NULL;
        BinaryBuffer_Destroy (buffer);
        rzb_log (LOG_ERR,
                 "%s: failed due to failure of BinaryBuffer_Get_UUID", __func__);
        return false;
    }
    if (!BinaryBuffer_Get_uint32_t
        (buffer, &regReq->iDataTypeCount))
    {
        buffer->pBuffer = NULL;
        BinaryBuffer_Destroy (buffer);
        rzb_log (LOG_ERR,
                 "%s: failed due to failure of BinaryBuffer_Get_uint32_t", __func__);
        return false;
    }
    if (regReq->iDataTypeCount != 0){
        if((regReq->pDataTypeList = (uuid_t *) malloc (sizeof (uuid_t) * regReq->iDataTypeCount)) == NULL)
        {
            buffer->pBuffer = NULL;
            BinaryBuffer_Destroy (buffer);
            rzb_log (LOG_ERR, "%s: failed due to lack of memory", __func__);
            return false;
        }
    
        for (size_t i = 0; i < regReq->iDataTypeCount; i++)
            if (!BinaryBuffer_Get_UUID (buffer, regReq->pDataTypeList[i]))
            {
                free (regReq->pDataTypeList);
                buffer->pBuffer = NULL;
                BinaryBuffer_Destroy (buffer);
                rzb_log (LOG_ERR, "%s: failed due to failure of BinaryBuffer_Get_UUID", __func__);
                return false;
            }
    } 
    else
        regReq->pDataTypeList = NULL;

    buffer->pBuffer = NULL;
    BinaryBuffer_Destroy(buffer);
    return true;
}


static bool
RegistrationRequest_Deserialize(struct Message *message, int mode)
{
    ASSERT(message != NULL);
    if ( message == NULL )
        return false;

    if ((message->message = calloc(1,sizeof(struct MessageRegistrationRequest))) == NULL)
        return false;

    switch (mode)
    {
    case MESSAGE_MODE_BIN:
        return RegistrationRequest_Deserialize_Binary(message);
    case MESSAGE_MODE_JSON:
        break;
    default:
        rzb_log(LOG_ERR, "%s: Invalid deserialization mode", __func__);
        return false;
    }
    return false;
}
static bool
RegistrationRequest_Serialize_Binary(struct Message *message)
{
    struct MessageRegistrationRequest *regReq;
    struct BinaryBuffer *buffer;

    ASSERT(message != NULL);
    if (message == NULL)
        return false;

    regReq = message->message;

    message->length = sizeof(uint32_t) +
        (sizeof(uuid_t) * (regReq->iDataTypeCount + 2));

    if ((buffer = BinaryBuffer_Create(message->length)) == NULL)
        return false;


    if (!BinaryBuffer_Put_UUID
        (buffer, regReq->uuidNuggetType))
    {
        BinaryBuffer_Destroy (buffer);
        rzb_log (LOG_ERR,
                 "%s: failed due to failure of BinaryBuffer_Put_UUID", __func__);
        return false;
    }
    if (!BinaryBuffer_Put_UUID
        (buffer, regReq->uuidApplicationType))
    {
        BinaryBuffer_Destroy (buffer);
        rzb_log (LOG_ERR,
                 "%s: failed due to failure of BinaryBuffer_Put_UUID", __func__);
        return false;
    }
    if (!BinaryBuffer_Put_uint32_t
        (buffer, regReq->iDataTypeCount))
    {
        BinaryBuffer_Destroy (buffer);
        rzb_log (LOG_ERR,
                 "%s: failed due to failure of BinaryBuffer_Put_uint32_t", __func__);
        return false;
    }
    for (size_t i = 0; i < regReq->iDataTypeCount;
         i++)
    {
        if (!BinaryBuffer_Put_UUID(buffer, regReq->pDataTypeList[i]))
        {
            BinaryBuffer_Destroy (buffer);
            rzb_log (LOG_ERR,
                     "%s: failed due to failure of BinaryBuffer_Put_UUID", __func__);
            return false;
        }
    }


    message->serialized = buffer->pBuffer;
    buffer->pBuffer = NULL;
    BinaryBuffer_Destroy(buffer);
    return true;
}


static bool
RegistrationRequest_Serialize(struct Message *message, int mode)
{
    ASSERT(message != NULL);
    if ( message == NULL )
        return false;

    switch (mode)
    {
    case MESSAGE_MODE_BIN:
        return RegistrationRequest_Serialize_Binary(message);
    case MESSAGE_MODE_JSON:
        break;
    default:
        rzb_log(LOG_ERR, "%s: Invalid deserialization mode", __func__);
        return false;
    }
    return false;
}
