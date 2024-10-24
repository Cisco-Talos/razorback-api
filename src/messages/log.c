#include "config.h"

#include <razorback/debug.h>
#include <razorback/messages.h>
#include <razorback/log.h>
#include <razorback/event.h>
#include <razorback/json_buffer.h>

#include "messages/core.h"

#include <string.h>

static void Log_Destroy (struct Message *msg);
static bool Log_Deserialize(struct Message *message);
static bool Log_Serialize(struct Message *message);

static struct MessageHandler handler = {
    MESSAGE_TYPE_LOG,
    Log_Serialize,
    Log_Deserialize,
    Log_Destroy
};

//core.h
void
MessageLogSubmission_Init(void)
{
    Message_Register_Handler(&handler);
}


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

    if ((msg = Message_Create(MESSAGE_TYPE_LOG, MESSAGE_VERSION_1, sizeof(struct MessageLogSubmission))) == NULL)
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
Log_Deserialize(struct Message *message)
{
    json_object *msg;
    struct MessageLogSubmission *submit;

    ASSERT(message != NULL);
    if (message == NULL)
        return false;

    if ((message->message = calloc(1,sizeof(struct MessageLogSubmission))) == NULL)
        return false;

    if ((msg = json_tokener_parse((char *)message->serialized)) == NULL)
        return false;
    
    submit = message->message;

    if (!JsonBuffer_Get_UUID (msg, "Nugget_ID", submit->uuidNuggetId))
    {
        json_object_put(msg);
        rzb_log (LOG_ERR,
                 "%s: failed due to failure of JsonBuffer_Get_UUID", __func__);
        return false;
    }
    if (!JsonBuffer_Get_uint8_t (msg, "Priority", &submit->iPriority))
    {
        json_object_put(msg);
        rzb_log (LOG_ERR,
                 "%s: failed due failure of JsonBuffer_Get_uint8_t", __func__);
        return false;
    }

    if (json_object_object_get(msg, "Event_ID") != NULL)
    {
        if (!JsonBuffer_Get_EventId (msg, "Event_ID", &submit->pEventId))
        {
            json_object_put(msg);
            rzb_log (LOG_ERR,
                     "%s: failed due to failure of JsonBuffer_Get_EventId", __func__);
            return false;
        }
    }
    else
        submit->pEventId = NULL;

    if ((submit->sMessage = (uint8_t *)JsonBuffer_Get_String(msg, "Message")) == NULL)
    {
        json_object_put(msg);
        rzb_log (LOG_ERR,
                 "%s: failed due to failure of JsonBuffer_Get_String", __func__);
        return false;
    }

    return true;
}

static bool
Log_Serialize(struct Message *message)
{
    struct MessageLogSubmission *submit;
    json_object *msg;
    const char * wire;

    ASSERT(message != NULL);
    if (message == NULL)
        return false;

    submit = message->message;

    if ((msg = json_object_new_object()) == NULL)
        return false;

    if (!JsonBuffer_Put_UUID (msg, "Nugget_ID", submit->uuidNuggetId))
    {
        json_object_put(msg);
        rzb_log (LOG_ERR,
                 "%s: failed due to failure of JsonBuffer_Put_UUID", __func__);
        return false;
    }
    if (!JsonBuffer_Put_uint8_t (msg, "Priority", submit->iPriority))
    {
        json_object_put(msg);
        rzb_log (LOG_ERR,
                 "%s: failed due failure of JsonBuffer_Put_uint8_t", __func__);
        return false;
    }

    if (submit->pEventId != NULL)
    {
        if (!JsonBuffer_Put_EventId (msg, "Event_ID", submit->pEventId))
        {
            json_object_put(msg);
            rzb_log (LOG_ERR,
                     "%s: failed due to failure of JsonBuffer_Put_BlockId", __func__);
            return false;
        }
    }
    if (!JsonBuffer_Put_String (msg, "Message", (char *)submit->sMessage))
    {
        json_object_put(msg);
        rzb_log (LOG_ERR,
                 "%s: failed due to failure of JsonBuffer_Put_String", __func__);
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