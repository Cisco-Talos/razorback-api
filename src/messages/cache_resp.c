#include "config.h"

#include <razorback/debug.h>
#include <razorback/messages.h>
#include <razorback/log.h>
#include <razorback/block_id.h>
#include <razorback/json_buffer.h>


#include "messages/core.h"

#include <string.h>

static void CacheResp_Destroy (struct Message *message);
static bool CacheResp_Deserialize(struct Message *message);
static bool CacheResp_Serialize(struct Message *message);

static struct MessageHandler handler = {
    MESSAGE_TYPE_RESP,
    CacheResp_Serialize,
    CacheResp_Deserialize,
    CacheResp_Destroy
};

//core.h
void
MessageCacheResp_Init(void)
{
    Message_Register_Handler(&handler);
}

SO_PUBLIC struct Message *
MessageCacheResp_Initialize (
                             const struct BlockId *p_pBlockId,
                             uint32_t p_iSfFlags, uint32_t p_iEntFlags)
{
    struct Message *msg;
    struct MessageCacheResp *message;

    ASSERT (p_pBlockId != NULL);
    if (p_pBlockId == NULL) 
        return NULL;

    if ((msg = Message_Create(MESSAGE_TYPE_RESP, MESSAGE_VERSION_1, sizeof(struct MessageCacheResp))) == NULL)
        return NULL;

    message = msg->message;

    // fill in rest of message
    if ((message->pId = BlockId_Clone (p_pBlockId)) == NULL)
    {
        rzb_log (LOG_ERR,
                 "%s: failed due to failure of BlockId_Clone", __func__);
        CacheResp_Destroy(msg);
        return NULL;
    }
    message->iSfFlags = p_iSfFlags;
    message->iEntFlags = p_iEntFlags;
    msg->destroy=CacheResp_Destroy;
    msg->deserialize=CacheResp_Deserialize;
    msg->serialize=CacheResp_Serialize;
 
    return msg;
}

static void
CacheResp_Destroy (struct Message *message)
{
    struct MessageCacheResp *msg;
    ASSERT (message != NULL);
    if (message == NULL)
        return;

    msg = message->message;

    // destroy any malloc'd components
    if (msg->pId != NULL)
        BlockId_Destroy (msg->pId);

    Message_Destroy(message);
}


static bool
CacheResp_Deserialize(struct Message *message)
{
    struct MessageCacheResp *submit;
    json_object *msg;

    ASSERT(message != NULL);
    if (message == NULL)
        return false;

    if ((message->message = calloc(1,sizeof(struct MessageCacheResp))) == NULL)
        return false;

    if ((msg = json_tokener_parse((char *)message->serialized)) == NULL)
        return false;

    submit = message->message;

    if (!JsonBuffer_Get_BlockId (msg, "Block_ID", &submit->pId))
    {
        json_object_put(msg);
        rzb_log (LOG_ERR,
                 "%s: failed due to failure of JsonBuffer_Get_BlockId", __func__);
        return false;
    }

    if (!JsonBuffer_Get_uint32_t (msg, "SF_Flags", &submit->iSfFlags))
    {
        json_object_put(msg);
        rzb_log (LOG_ERR,
                 "%s: failed due to failure of JsonBuffer_Get_uint32_t", __func__);
        return false;
    }
    if (!JsonBuffer_Get_uint32_t (msg, "Ent_Flags", &submit->iEntFlags))
    {
        json_object_put(msg);
        rzb_log (LOG_ERR,
    
                "%s: failed due to failure of JsonBuffer_Get_uint32_t", __func__);
        return false;
    }
    json_object_put(msg);
    return true;
}


static bool
CacheResp_Serialize(struct Message *message)
{
    struct MessageCacheResp *submit;
    json_object *msg;
    const char * wire;

    ASSERT(message != NULL);
    if (message == NULL)
        return false;

    if ((msg = json_object_new_object()) == NULL)
        return false;

    submit = message->message;

    if (!JsonBuffer_Put_BlockId (msg, "Block_ID", submit->pId))
    {
        rzb_log (LOG_ERR,
                 "%s: failed due to failure of JsonBuffer_Put_BlockId", __func__);
        return false;
    }
    if (!JsonBuffer_Put_uint32_t (msg, "SF_Flags", submit->iSfFlags))
    {
        rzb_log (LOG_ERR,
                 "%s: failed due to failure of JsonBuffer_Put_uint32_t", __func__);
        return false;
    }
    if (!JsonBuffer_Put_uint32_t (msg, "Ent_Flags", submit->iEntFlags))
    {
        rzb_log (LOG_ERR,
                 "%s: failed due to failure of JsonBuffer_Put_uint32_t", __func__);
        return false;
    }
    wire = json_object_to_json_string(msg);
    message->length=strlen(wire);
    if ((message->serialized = calloc(message->length+1, sizeof(char))) == NULL)
    {
        json_object_put(msg);
        return false;
    }
    strcpy((char *)message->serialized, wire); 
    json_object_put(msg);

    return true;
}