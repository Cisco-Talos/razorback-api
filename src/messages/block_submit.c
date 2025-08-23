#include "config.h"

#include <razorback/debug.h>
#include <razorback/messages.h>
#include <razorback/log.h>
#include <razorback/event.h>
#include <razorback/json_buffer.h>

#include "messages/core.h"

#include <string.h>

static void BlockSubmission_Destroy (struct Message *message);
static bool BlockSubmission_Deserialize(struct Message *message);
static bool BlockSubmission_Serialize(struct Message *message);

static struct MessageHandler handler = {
    MESSAGE_TYPE_BLOCK,
    BlockSubmission_Serialize,
    BlockSubmission_Deserialize,
    BlockSubmission_Destroy
};

// core.h
void
MessageBlockSubmission_Init(void)
{
    Message_Register_Handler(&handler);
}

SO_PUBLIC struct Message *
MessageBlockSubmission_Initialize (
                                   struct Event *p_pEvent,
                                   uint32_t p_iReason, uint8_t locality)
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
    message->iReason = p_iReason;
    message->storedLocality = locality;

    msg->destroy = BlockSubmission_Destroy;
    msg->deserialize=BlockSubmission_Deserialize;
    msg->serialize=BlockSubmission_Serialize;

    return msg;
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

    Message_Destroy(message);
}


static bool
BlockSubmission_Deserialize(struct Message *message)
{
    struct MessageBlockSubmission *submit;
    json_object *msg;

    ASSERT(message != NULL);
    if (message == NULL)
        return false;

    if ((message->message = calloc(1,sizeof(struct MessageBlockSubmission))) == NULL)
        return false;

    if ((msg = json_tokener_parse((char *)message->serialized)) == NULL)
        return false;

    submit = message->message;

    if (!JsonBuffer_Get_uint32_t (msg, "Reason", &submit->iReason))
    {
        rzb_log(LOG_ERR, "%s: Failed to get Reason from JSON buffer", __func__);
        json_object_put(msg);
        return false;
    }
    if (!JsonBuffer_Get_Event (msg, "Event", &submit->pEvent))
    {
        rzb_log(LOG_ERR, "%s: Failed to get Event from JSON buffer", __func__);
        json_object_put(msg);
        return false;
    }
    if (!JsonBuffer_Get_uint8_t (msg, "Stored_Locality", &submit->storedLocality))
    {
        rzb_log(LOG_ERR, "%s: Failed to get Stored_Locality from JSON buffer", __func__);
        json_object_put(msg);
        return false;
    }

    json_object_put(msg);
    return true;
}

static bool
BlockSubmission_Serialize(struct Message *message)
{
    struct MessageBlockSubmission *submit;
    json_object *msg;
    const char * wire;

    ASSERT(message != NULL);
    if (message == NULL)
        return false;

    submit = message->message;

    if ((msg = json_object_new_object()) == NULL)
        return false;

    if (!JsonBuffer_Put_uint32_t (msg, "Reason", submit->iReason))
    {
        json_object_put(msg);
        return false;
    }

    if (!JsonBuffer_Put_Event (msg, "Event", submit->pEvent))
    {
        json_object_put(msg);
        return false;
    }

    if (!JsonBuffer_Put_uint8_t (msg, "Stored_Locality", submit->storedLocality))
    {
        json_object_put(msg);
        return false;
    }

    wire = json_object_to_json_string(msg);
    message->length=strlen(wire);
    if ((message->serialized = calloc(message->length+1, sizeof(uint8_t))) == NULL)
    {
        json_object_put(msg);
        return false;
    }
    strcpy((char *)message->serialized, wire); 
    json_object_put(msg);

    return true;
}