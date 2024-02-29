#include "config.h"
#include <razorback/debug.h>
#include <razorback/log.h>

#include "messages/cnc/core.h"
#include "messages/core.h"

static bool Message_AddCncHeader(struct Message *message, const uuid_t source, const uuid_t dest);

static bool 
Message_AddCncHeader(struct Message *message, const uuid_t source, const uuid_t dest)
{
    char uuid[UUID_STRING_LENGTH];
    uuid_unparse(source, uuid);
    Message_Add_Header(message->headers, MSG_CNC_HEADER_SOURCE, uuid);
    uuid_unparse(dest, uuid);
    Message_Add_Header(message->headers, MSG_CNC_HEADER_DEST, uuid);

    return true;
}

struct Message * 
Message_CncCreate(uint32_t type, uint32_t version, 
        size_t msgSize, const uuid_t source, const uuid_t dest)
{
    struct Message *msg;
    if ((msg = Message_Create(type, version, msgSize)) == NULL)
        return NULL;

    Message_AddCncHeader(msg, source, dest);
    return msg;
}

struct Message * 
Message_CncBcastCreate(uint32_t type, uint32_t version, 
        size_t msgSize, const uuid_t source)
{
    struct Message *msg;
    uuid_t dest;
    uuid_clear(dest);

    if ((msg = Message_Create(type, version, msgSize)) == NULL)
        return NULL;

    Message_AddCncHeader(msg, source, dest);
    return msg;
}


bool
Message_CnC_Serialize_Empty(struct Message *message, int mode)
{
    ASSERT(message != NULL);
    if ( message == NULL )
        return false;


    switch (mode)
    {
    case MESSAGE_MODE_BIN:
    case MESSAGE_MODE_JSON:
        break;
    default:
        rzb_log(LOG_ERR, "%s: Invalid serialization mode", __func__);
        return false;
    }
    if ((message->serialized = calloc(2, sizeof(char))) == NULL)
        return false;

    message->serialized[0]=' ';
    message->serialized[1]='\0';
    message->length=1;

    return true;
}

bool
Message_CnC_Deserialize_Empty(struct Message *message, int mode)
{
    ASSERT(message != NULL);
    if ( message == NULL )
        return false;


    switch (mode)
    {
    case MESSAGE_MODE_BIN:
    case MESSAGE_MODE_JSON:
        return true;
    default:
        rzb_log(LOG_ERR, "%s: Invalid serialization mode", __func__);
        return false;
    }
    return false;
}

SO_PUBLIC bool 
Message_CnC_Get_Nuggets(struct Message *message, uuid_t source, uuid_t dest)
{
    struct MessageHeader *header;

    ASSERT(message != NULL);
    if (message == NULL)
        return false;

    if ((header = List_Find(message->headers, (void *)MSG_CNC_HEADER_DEST)) == NULL)
        return false;

    if (uuid_parse(header->sValue, dest) != 0)
        return false;

    if ((header = List_Find(message->headers, (void *)MSG_CNC_HEADER_SOURCE)) == NULL)
        return false;

    if (uuid_parse(header->sValue, source) != 0)
        return false;

    return true;
}

bool 
Message_CnC_Setup(struct Message *message)
{
    switch (message->type)
    {
    case MESSAGE_TYPE_HELLO:
        Message_CnC_Hello_Setup(message);
        break;
    case MESSAGE_TYPE_REG_REQ:
        Message_CnC_RegReq_Setup(message);
        break;
    case MESSAGE_TYPE_REG_RESP:
        Message_CnC_RegResp_Setup(message);
        break;
    case MESSAGE_TYPE_CONFIG_UPDATE:
        Message_CnC_ConfigUpdate_Setup(message);
        break;
    case MESSAGE_TYPE_CONFIG_ACK:
        Message_CnC_ConfigAck_Setup(message);
        break;
    case MESSAGE_TYPE_CONFIG_ERR:
    case MESSAGE_TYPE_REG_ERR:
        Message_CnC_Error_Setup(message);
        break;
    case MESSAGE_TYPE_PAUSE:
        Message_CnC_Pause_Setup(message);
        break;
    case MESSAGE_TYPE_PAUSED:
        Message_CnC_Paused_Setup(message);
        break;
    case MESSAGE_TYPE_GO:
        Message_CnC_Go_Setup(message);
        break;
    case MESSAGE_TYPE_RUNNING:
        Message_CnC_Running_Setup(message);
        break;
    case MESSAGE_TYPE_TERM:
        Message_CnC_Term_Setup(message);
        break;
    case MESSAGE_TYPE_BYE:
        Message_CnC_Bye_Setup(message);
        break;
    case MESSAGE_TYPE_CLEAR:
        Message_CnC_CacheClear_Setup(message);
        break;
    default:
        return false;
    }
    return true;
}

