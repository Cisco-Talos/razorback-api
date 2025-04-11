#include "config.h"

#include <razorback/debug.h>
#include <razorback/messages.h>
#include <razorback/log.h>
#include <razorback/block.h>
#include <razorback/event.h>
#include <razorback/json_buffer.h>
#include <razorback/list.h>


#include "messages/core.h"

#include <string.h>

static void InspectionSubmission_Destroy (struct Message *message);
static bool InspectionSubmission_Deserialize(struct Message *message);
static bool InspectionSubmission_Serialize(struct Message *message);

static struct MessageHandler handler = {
    MESSAGE_TYPE_INSPECTION,
    InspectionSubmission_Serialize,
    InspectionSubmission_Deserialize,
    InspectionSubmission_Destroy
};

//core.h
void
MessageInspectionSubmission_Init(void)
{
    Message_Register_Handler(&handler);
}

SO_PUBLIC struct Message *
MessageInspectionSubmission_Initialize (
                                        const struct Event *p_pEvent,
                                        uint32_t p_iReason,
                                        uint32_t localityCount,
                                        uint8_t *localities)
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
    message->localityCount = localityCount;
    if (localityCount > 0)
    {
        if ((message->localityList = calloc(localityCount, sizeof(uint8_t))) == NULL)
        {
            rzb_log(LOG_ERR, "%s: Failed to clone locality list", __func__);
            InspectionSubmission_Destroy(msg);
            return NULL;
        }
        memcpy(message->localityList, localities, localityCount);
    }
    msg->destroy=InspectionSubmission_Destroy;
    msg->deserialize=InspectionSubmission_Deserialize;
    msg->serialize=InspectionSubmission_Serialize;

    return msg;
}

static void
InspectionSubmission_Destroy (struct Message
                                     *message)
{
    struct MessageInspectionSubmission *msg;
    ASSERT (message != NULL);
    if (message == NULL)
        return;

    msg = (struct MessageInspectionSubmission *)message->message;

    // destroy any malloc'd components
    if (msg->pBlock != NULL)
        Block_Destroy (msg->pBlock);
    if (msg->eventId != NULL)
        EventId_Destroy(msg->eventId);
    if (msg->pEventMetadata != NULL)
        List_Destroy(msg->pEventMetadata);
    if (msg->localityList != NULL)
        free(msg->localityList);

    Message_Destroy(message);
}

static bool
InspectionSubmission_Deserialize(struct Message *message)
{
    struct MessageInspectionSubmission *submit;
    json_object *msg;

    ASSERT(message != NULL);
    if (message == NULL)
        return false;

    if ((message->message = calloc(1,sizeof(struct MessageInspectionSubmission))) == NULL)
        return false;

    
    if ((msg = json_tokener_parse((char *)message->serialized)) == NULL)
        return false;
    
    submit = message->message;

    if (!JsonBuffer_Get_uint32_t (msg, "Reason", &submit->iReason))
    {
        json_object_put(msg);
        return false;
    }
    if (!JsonBuffer_Get_EventId (msg, "Event_ID", &submit->eventId))
    {
        json_object_put(msg);
        return false;
    }
    if (!JsonBuffer_Get_NTLVList (msg, "Event_Metadata", &submit->pEventMetadata))
    {
        json_object_put(msg);
        return false;
    }
    
    if (!JsonBuffer_Get_Block (msg, "Block", &submit->pBlock))
    {
        json_object_put(msg);
        return false;
    }
    if (!JsonBuffer_Get_uint8List (msg, "Avaliable_Localities", &submit->localityList, &submit->localityCount))
    {
        json_object_put(msg);
        return false;
    }
    json_object_put(msg);
    return true;
}

static bool
InspectionSubmission_Serialize(struct Message *message)
{
    struct MessageInspectionSubmission *submit;
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
    if (!JsonBuffer_Put_EventId (msg, "Event_ID", submit->eventId))
    {
        json_object_put(msg);
        return false;
    }
    if (!JsonBuffer_Put_NTLVList (msg, "Event_Metadata", submit->pEventMetadata))
    {
        json_object_put(msg);
        return false;
    }

    if (!JsonBuffer_Put_Block (msg, "Block", submit->pBlock))
    {
        json_object_put(msg);
        return false;
    }
    if (!JsonBuffer_Put_uint8List (msg, "Avaliable_Localities", submit->localityList, submit->localityCount))
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