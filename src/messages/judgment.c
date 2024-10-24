#include "config.h"

#include <razorback/debug.h>
#include <razorback/messages.h>
#include <razorback/log.h>
#include <razorback/judgment.h>
#include <razorback/json_buffer.h>

#include "messages/core.h"

#include <string.h>

static void JudgmentSubmission_Destroy (struct Message *message);
static bool JudgmentSubmission_Deserialize(struct Message *message);
static bool JudgmentSubmission_Serialize(struct Message *message);

static struct MessageHandler handler = {
    MESSAGE_TYPE_JUDGMENT,
    JudgmentSubmission_Serialize,
    JudgmentSubmission_Deserialize,
    JudgmentSubmission_Destroy
};

//core.h
void
MessageJudgmentSubmission_Init(void)
{
    Message_Register_Handler(&handler);
}


SO_PUBLIC struct Message *
MessageJudgmentSubmission_Initialize (
                                      uint8_t p_iReason,
                                      struct Judgment *p_pJudgment)
{
    struct Message *msg;
    struct MessageJudgmentSubmission *message;

    ASSERT (p_pJudgment != NULL);
    if (p_pJudgment == NULL)
        return NULL;

    if ((msg = Message_Create(MESSAGE_TYPE_JUDGMENT, MESSAGE_VERSION_1, sizeof(struct MessageJudgmentSubmission))) == NULL)
        return NULL;

    message = msg->message;
    message->pJudgment = p_pJudgment;

    message->iReason = p_iReason;

    msg->destroy = JudgmentSubmission_Destroy;
    msg->deserialize=JudgmentSubmission_Deserialize;
    msg->serialize=JudgmentSubmission_Serialize;
    return msg;
}

static void
JudgmentSubmission_Destroy (struct Message
                                   *message)
{
    struct MessageJudgmentSubmission *msg;
    ASSERT (message != NULL);
    if (message == NULL)
        return;

    msg = message->message;
    
    if (msg->pJudgment != NULL)
        Judgment_Destroy(msg->pJudgment);
    Message_Destroy(message);
}

static bool
JudgmentSubmission_Deserialize(struct Message *message)
{
    struct MessageJudgmentSubmission *submit;
    json_object *msg;

    ASSERT(message != NULL);
    if (message == NULL)
        return false;

    if ((message->message = calloc(1,sizeof(struct MessageJudgmentSubmission))) == NULL)
        return false;

    if ((msg = json_tokener_parse((char *)message->serialized)) == NULL)
        return false;
    
    submit = message->message;

    if (!JsonBuffer_Get_uint8_t (msg, "Reason", &submit->iReason))
    {
        json_object_put(msg);
        return false;
    }
    if (!JsonBuffer_Get_Judgment (msg, "Judgment", &submit->pJudgment))
    {
        json_object_put(msg);
        return false;
    }
    json_object_put(msg);

    return true;
}

static bool
JudgmentSubmission_Serialize(struct Message *message)
{
    struct MessageJudgmentSubmission *submit;
    json_object *msg;
    const char * wire;

    ASSERT(message != NULL);
    if (message == NULL)
        return false;

    submit = message->message;

    if ((msg = json_object_new_object()) == NULL)
        return false;


    if (!JsonBuffer_Put_uint8_t (msg, "Reason", submit->iReason))
    {
        json_object_put(msg);
        return false;
    }
    if (!JsonBuffer_Put_Judgment (msg, "Judgment", submit->pJudgment))
    {
        json_object_put(msg);
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