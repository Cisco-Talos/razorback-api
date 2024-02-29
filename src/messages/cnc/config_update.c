#include "config.h"

#include <razorback/debug.h>
#include <razorback/messages.h>
#include <razorback/log.h>
#include <razorback/list.h>
#include <razorback/uuids.h>


#include "messages/core.h"
#include "messages/cnc/core.h"

#include "binary_buffer.h"

#include <string.h>

static bool ConfigUpdate_Deserialize_Binary(struct Message *message);
static bool ConfigUpdate_Deserialize(struct Message *message, int mode);
static void ConfigUpdate_Destroy (struct Message *message);
static bool ConfigUpdate_Serialize_Binary(struct Message *message);
static bool ConfigUpdate_Serialize(struct Message *message, int mode);

struct UUID_ListStatus
{
    uuid_t *curUuid;
    char *curString;
};

static int
UUID_MessageAddNameSize(void *vItem, void *vCount)
{
    struct UUIDListNode *item = (struct UUIDListNode *)vItem;
    uint32_t * count = (uint32_t*)vCount;
    *count = *count + strlen(item->sName) +1;
    return LIST_EACH_OK;
}

static int
UUID_MessageAddData(void *vItem, void *vStatus)
{
    struct UUIDListNode *item = (struct UUIDListNode *)vItem;
    struct UUID_ListStatus *status = (struct UUID_ListStatus *)vStatus;

    memcpy(status->curString, item->sName, strlen(item->sName)+1);
    status->curString += strlen(item->sName)+1;
    uuid_copy(*status->curUuid, item->uuid);
    status->curUuid++;
    return LIST_EACH_OK;
}

static bool
UUID_ListToConfigUpdate(int type, 
        uint32_t *count, uint32_t *nameSize, char **names, uuid_t **uuids)
{
    struct List *list;
    struct UUID_ListStatus listStatus;
    if ((list = UUID_Get_List(type)) == NULL)
        return false;
    List_Lock(list);
    *count = list->length;
    *nameSize = 0;
    if (list->length == 0)
    {
        List_Unlock(list);
        return true;
    }
    // Work out how big the strings are.
    List_ForEach(list, UUID_MessageAddNameSize, nameSize);
    if ((*names = calloc(*nameSize, sizeof(char))) == NULL)
    {
        List_Unlock(list);
        return false;
    }
    if ((*uuids = calloc(list->length, sizeof(uuid_t))) == NULL)
    {
        List_Unlock(list);
        return false;
    }
    listStatus.curUuid =*uuids;
    listStatus.curString = *names;
    List_ForEach(list, UUID_MessageAddData, &listStatus);

    List_Unlock(list);
    return true;
}

SO_PUBLIC struct Message *
MessageConfigurationUpdate_Initialize (
                                       const uuid_t p_uuidSourceNugget,
                                       const uuid_t p_uuidDestNugget)
{
    struct MessageConfigurationUpdate *message;
    struct Message *msg;

    if ((msg = Message_CncCreate(MESSAGE_TYPE_CONFIG_UPDATE, MESSAGE_VERSION_1, sizeof(struct MessageConfigurationUpdate), p_uuidSourceNugget, p_uuidDestNugget)) == NULL)
        return NULL;
    message = msg->message;

    UUID_ListToConfigUpdate(UUID_TYPE_NTLV_TYPE, 
            &message->ntlvTypesCount,
            &message->ntlvTypesNamesSize,
            &message->ntlvTypesNames,
            &message->ntlvTypesUuids);
    UUID_ListToConfigUpdate(UUID_TYPE_NTLV_NAME, 
            &message->ntlvNamesCount,
            &message->ntlvNamesNamesSize,
            &message->ntlvNamesNames,
            &message->ntlvNamesUuids);
    UUID_ListToConfigUpdate(UUID_TYPE_DATA_TYPE, 
            &message->dataTypesCount,
            &message->dataTypesNamesSize,
            &message->dataTypesNames,
            &message->dataTypesUuids);
    msg->destroy = ConfigUpdate_Destroy;
    msg->deserialize = ConfigUpdate_Deserialize;
    msg->serialize = ConfigUpdate_Serialize;
    return msg;
}

void 
Message_CnC_ConfigUpdate_Setup(struct Message *msg)
{
    msg->destroy = ConfigUpdate_Destroy;
    msg->deserialize = ConfigUpdate_Deserialize;
    msg->serialize = ConfigUpdate_Serialize;
}

static void
ConfigUpdate_Destroy (struct Message *msg)
{
    struct MessageConfigurationUpdate *message;
    ASSERT(msg != NULL);
    if (msg == NULL)
        return;
    message = msg->message;
    if (message->ntlvTypesUuids != NULL)
        free(message->ntlvTypesUuids);
    if (message->ntlvTypesNames != NULL)
        free(message->ntlvTypesNames);
    if (message->ntlvNamesUuids != NULL)
        free(message->ntlvNamesUuids);
    if (message->ntlvNamesNames != NULL)
        free(message->ntlvNamesNames);
    if (message->dataTypesUuids != NULL)
        free(message->dataTypesUuids);
    if (message->dataTypesNames != NULL)
        free(message->dataTypesNames);

    Message_Destroy(msg);
}


static bool
ConfigUpdate_Deserialize_Binary(struct Message *message)
{
    struct BinaryBuffer *buffer;
    struct MessageConfigurationUpdate *configUpdate;

    ASSERT(message != NULL);
    if (message == NULL)
        return false;
    
    if ((buffer = BinaryBuffer_CreateFromMessage(message)) == NULL)
        return false;
    
    configUpdate = message->message;

    if (!BinaryBuffer_Get_uint32_t(buffer, &configUpdate->ntlvTypesCount))
    {
        buffer->pBuffer = NULL;
        BinaryBuffer_Destroy (buffer);
        rzb_log(LOG_ERR, "%s: Failed to get NTLV Type Count", __func__);
        return false;
    }
    if (!BinaryBuffer_Get_uint32_t(buffer, &configUpdate->ntlvTypesNamesSize))
    {
        buffer->pBuffer = NULL;
        BinaryBuffer_Destroy (buffer);
        rzb_log(LOG_ERR, "%s: Failed to get NTLV Type name size", __func__);
        return false;
    }
    if (configUpdate->ntlvTypesCount > 0)
    {
        configUpdate->ntlvTypesUuids = calloc(configUpdate->ntlvTypesCount, sizeof(uuid_t));
        configUpdate->ntlvTypesNames = calloc(configUpdate->ntlvTypesNamesSize, sizeof(char));
        if ((configUpdate->ntlvTypesUuids == NULL ) ||
                (configUpdate->ntlvTypesNames == NULL ) )
        {
            buffer->pBuffer = NULL;
            BinaryBuffer_Destroy (buffer);
            rzb_log(LOG_ERR, "%s: failed to allocate income message structures", __func__);
            return false;
        }
        if (!BinaryBuffer_Get_ByteArray(buffer, configUpdate->ntlvTypesCount * sizeof(uuid_t), (uint8_t*)configUpdate->ntlvTypesUuids))
        {
            buffer->pBuffer = NULL;
            BinaryBuffer_Destroy (buffer);
            rzb_log(LOG_ERR, "%s: failed to read Types uuids", __func__);
            return false;
        }

        if (!BinaryBuffer_Get_ByteArray(buffer, configUpdate->ntlvTypesNamesSize * sizeof(char), (uint8_t*)configUpdate->ntlvTypesNames))
        {
            buffer->pBuffer = NULL;
            BinaryBuffer_Destroy (buffer);
            rzb_log(LOG_ERR, "%s: failed to read Types names", __func__);
            return false;
        }
    }
    if (!BinaryBuffer_Get_uint32_t(buffer, &configUpdate->ntlvNamesCount))
    {
        buffer->pBuffer = NULL;
        BinaryBuffer_Destroy (buffer);
        rzb_log(LOG_ERR, "%s: Failed to put NTLV Name Count", __func__);
        return false;
    }
    if (!BinaryBuffer_Get_uint32_t(buffer, &configUpdate->ntlvNamesNamesSize))
    {
        buffer->pBuffer = NULL;
        BinaryBuffer_Destroy (buffer);
        rzb_log(LOG_ERR, "%s: Failed to put NTLV Name Names size", __func__);
        return false;
    }
    if (configUpdate->ntlvNamesCount > 0)
    {
        configUpdate->ntlvNamesUuids = calloc(configUpdate->ntlvNamesCount, sizeof(uuid_t));
        configUpdate->ntlvNamesNames = calloc(configUpdate->ntlvNamesNamesSize, sizeof(char));
        if ((configUpdate->ntlvNamesUuids == NULL ) ||
                (configUpdate->ntlvNamesNames == NULL ) )
        {
            buffer->pBuffer = NULL;
            BinaryBuffer_Destroy (buffer);
            rzb_log(LOG_ERR, "%s: failed to allocate income message structures", __func__);
            return false;
        }
        if (!BinaryBuffer_Get_ByteArray(buffer, configUpdate->ntlvNamesCount * sizeof(uuid_t), (uint8_t*)configUpdate->ntlvNamesUuids))
        {
            buffer->pBuffer = NULL;
            BinaryBuffer_Destroy (buffer);
            rzb_log(LOG_ERR, "%s: failed to read Name uuids", __func__);
            return false;
        }

        if (!BinaryBuffer_Get_ByteArray(buffer, configUpdate->ntlvNamesNamesSize * sizeof(char), (uint8_t*)configUpdate->ntlvNamesNames))
        {
            buffer->pBuffer = NULL;
            BinaryBuffer_Destroy (buffer);
            rzb_log(LOG_ERR, "%s: failed to read Name names", __func__);
            return false;
        }
    }
    if (!BinaryBuffer_Get_uint32_t(buffer, &configUpdate->dataTypesCount))
    {
        buffer->pBuffer = NULL;
        BinaryBuffer_Destroy (buffer);
        rzb_log(LOG_ERR, "%s: Failed to put NTLV Name Count", __func__);
        return false;
    }
    if (!BinaryBuffer_Get_uint32_t(buffer, &configUpdate->dataTypesNamesSize))
    {
        buffer->pBuffer = NULL;
        BinaryBuffer_Destroy (buffer);
        rzb_log(LOG_ERR, "%s: Failed to put NTLV Name Names size", __func__);
        return false;
    }
    if (configUpdate->dataTypesCount > 0)
    {
        configUpdate->dataTypesUuids = calloc(configUpdate->dataTypesCount, sizeof(uuid_t));
        configUpdate->dataTypesNames = calloc(configUpdate->dataTypesNamesSize, sizeof(char));
        if ((configUpdate->dataTypesUuids == NULL ) ||
                (configUpdate->dataTypesNames == NULL ) )
        {
            buffer->pBuffer = NULL;
            BinaryBuffer_Destroy (buffer);
            rzb_log(LOG_ERR, "%s: failed to allocate income message structures", __func__);
            return false;
        }
        if (!BinaryBuffer_Get_ByteArray(buffer, configUpdate->dataTypesCount * sizeof(uuid_t), (uint8_t*)configUpdate->dataTypesUuids))
        {
            buffer->pBuffer = NULL;
            BinaryBuffer_Destroy (buffer);
            rzb_log(LOG_ERR, "%s: failed to read Name uuids", __func__);
            return false;
        }

        if (!BinaryBuffer_Get_ByteArray(buffer, configUpdate->dataTypesNamesSize * sizeof(char), (uint8_t*)configUpdate->dataTypesNames))
        {
            buffer->pBuffer = NULL;
            BinaryBuffer_Destroy (buffer);
            rzb_log(LOG_ERR, "%s: failed to read Name names", __func__);
            return false;
        }
    }
    buffer->pBuffer = NULL;
    BinaryBuffer_Destroy (buffer);

    return true;
}


static bool
ConfigUpdate_Deserialize(struct Message *message, int mode)
{
    ASSERT(message != NULL);
    if ( message == NULL )
        return false;

    if ((message->message = calloc(1,sizeof(struct MessageConfigurationUpdate))) == NULL)
        return false;

    switch (mode)
    {
    case MESSAGE_MODE_BIN:
        return ConfigUpdate_Deserialize_Binary(message);
    case MESSAGE_MODE_JSON:
        break;
    default:
        rzb_log(LOG_ERR, "%s: Invalid deserialization mode", __func__);
        return false;
    }
    return false;
}

static bool
ConfigUpdate_Serialize_Binary(struct Message *message)
{
    struct MessageConfigurationUpdate *configUpdate;
    struct BinaryBuffer *buffer;

    ASSERT(message != NULL);
    if (message == NULL)
        return false;

    configUpdate = message->message;

    message->length = (sizeof(uint32_t) * 6 )+
        (configUpdate->ntlvTypesCount * sizeof(uuid_t) ) +
        (configUpdate->ntlvTypesNamesSize * sizeof(char) ) +
        (configUpdate->ntlvNamesCount * sizeof(uuid_t) ) +
        (configUpdate->ntlvNamesNamesSize * sizeof(char) ) +
        (configUpdate->dataTypesCount * sizeof(uuid_t) ) +
        (configUpdate->dataTypesNamesSize * sizeof(char) );

    if ((buffer = BinaryBuffer_Create(message->length)) == NULL)
        return false;


    if (!BinaryBuffer_Put_uint32_t(buffer, configUpdate->ntlvTypesCount))
    {
        rzb_log(LOG_ERR, "%s: Failed to put NTLV Type Count", __func__);
        return false;
    }
    if (!BinaryBuffer_Put_uint32_t(buffer, configUpdate->ntlvTypesNamesSize))
    {
        rzb_log(LOG_ERR, "%s: Failed to put NTLV Type name size", __func__);
        return false;
    }
    if (configUpdate->ntlvTypesCount > 0)
    {
        if (!BinaryBuffer_Put_ByteArray(buffer,configUpdate->ntlvTypesCount*16,  (uint8_t *)configUpdate->ntlvTypesUuids))
        {
            rzb_log(LOG_ERR, "%s: Failed to put NTLV Type uuids", __func__);
            return false;
        }

        if (!BinaryBuffer_Put_ByteArray(buffer,configUpdate->ntlvTypesNamesSize,  (uint8_t *)configUpdate->ntlvTypesNames))
        {
            rzb_log(LOG_ERR, "%s: Failed to put NTLV Type names", __func__);
            return false;
        }
    }
    if (!BinaryBuffer_Put_uint32_t(buffer, configUpdate->ntlvNamesCount))
    {
        rzb_log(LOG_ERR, "%s: Failed to put NTLV Name Count", __func__);
        return false;
    }
    if (!BinaryBuffer_Put_uint32_t(buffer, configUpdate->ntlvNamesNamesSize))
    {
        rzb_log(LOG_ERR, "%s: Failed to put NTLV Name name size", __func__);
        return false;
    }
    if (configUpdate->ntlvNamesCount > 0)
    {
        if (!BinaryBuffer_Put_ByteArray(buffer,configUpdate->ntlvNamesCount*16,  (uint8_t *)configUpdate->ntlvNamesUuids))
        {
            rzb_log(LOG_ERR, "%s: Failed to put NTLV Name uuids", __func__);
            return false;
        }
        if (!BinaryBuffer_Put_ByteArray(buffer,configUpdate->ntlvNamesNamesSize,  (uint8_t *)configUpdate->ntlvNamesNames))
        {
            rzb_log(LOG_ERR, "%s: Failed to put NTLV Name names", __func__);
            return false;
        }
    }
    if (!BinaryBuffer_Put_uint32_t(buffer, configUpdate->dataTypesCount))
    {
        rzb_log(LOG_ERR, "%s: Failed to put NTLV Name Count", __func__);
        return false;
    }
    if (!BinaryBuffer_Put_uint32_t(buffer, configUpdate->dataTypesNamesSize))
    {
        rzb_log(LOG_ERR, "%s: Failed to put NTLV Name name size", __func__);
        return false;
    }
    if (configUpdate->dataTypesCount > 0)
    {
        if (!BinaryBuffer_Put_ByteArray(buffer,configUpdate->dataTypesCount*16,  (uint8_t *)configUpdate->dataTypesUuids))
        {
            rzb_log(LOG_ERR, "%s: Failed to put NTLV Name uuids", __func__);
            return false;
        }
        if (!BinaryBuffer_Put_ByteArray(buffer,configUpdate->dataTypesNamesSize,  (uint8_t *)configUpdate->dataTypesNames))
        {
            rzb_log(LOG_ERR, "%s: Failed to put NTLV Name names", __func__);
            return false;
        }
    }


    message->serialized = buffer->pBuffer;
    buffer->pBuffer = NULL;
    BinaryBuffer_Destroy(buffer);
    return true;
}


static bool
ConfigUpdate_Serialize(struct Message *message, int mode)
{
    ASSERT(message != NULL);
    if ( message == NULL )
        return false;

    switch (mode)
    {
    case MESSAGE_MODE_BIN:
        return ConfigUpdate_Serialize_Binary(message);
    case MESSAGE_MODE_JSON:
        break;
    default:
        rzb_log(LOG_ERR, "%s: Invalid deserialization mode", __func__);
        return false;
    }
    return false;
}
