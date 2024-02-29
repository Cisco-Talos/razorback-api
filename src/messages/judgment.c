#include "config.h"

#include <razorback/debug.h>
#include <razorback/messages.h>
#include <razorback/log.h>
#include <razorback/judgment.h>

#include "messages/core.h"
#include "binary_buffer.h"

static void JudgmentSubmission_Destroy (struct Message *message);
static bool JudgmentSubmission_Deserialize_Binary(struct Message *message);
static bool JudgmentSubmission_Deserialize(struct Message *message, int mode);
static bool JudgmentSubmission_Serialize_Binary(struct Message *message);
static bool JudgmentSubmission_Serialize(struct Message *message, int mode);


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
void 
MessageJudgmentSubmission_Setup(struct Message *msg)
{
    msg->destroy = JudgmentSubmission_Destroy;
    msg->deserialize=JudgmentSubmission_Deserialize;
    msg->serialize=JudgmentSubmission_Serialize;
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
JudgmentSubmission_Deserialize_Binary(struct Message *message)
{
    struct BinaryBuffer *buffer;
    struct MessageJudgmentSubmission *submit;

    ASSERT(message != NULL);
    if (message == NULL)
        return false;
    
    if ((buffer = BinaryBuffer_CreateFromMessage(message)) == NULL)
        return false;
    
    submit = message->message;

    if (!BinaryBuffer_Get_uint8_t (buffer, &submit->iReason))
    {
        buffer->pBuffer = NULL;
        BinaryBuffer_Destroy (buffer);
        return false;
    }
    if (!BinaryBuffer_Get_Judgment (buffer, &submit->pJudgment))
    {
        buffer->pBuffer = NULL;
        BinaryBuffer_Destroy (buffer);
        return false;
    }
    buffer->pBuffer = NULL;
    BinaryBuffer_Destroy (buffer);

    return true;
}

static bool
JudgmentSubmission_Deserialize(struct Message *message, int mode)
{
    ASSERT(message != NULL);
    if ( message == NULL )
        return false;

    if ((message->message = calloc(1,sizeof(struct MessageJudgmentSubmission))) == NULL)
        return false;

    switch (mode)
    {
    case MESSAGE_MODE_BIN:
        return JudgmentSubmission_Deserialize_Binary(message);
    case MESSAGE_MODE_JSON:
        break;
    default:
        rzb_log(LOG_ERR, "%s: Invalid deserialization mode", __func__);
        return false;
    }
    return false;
}


static bool
JudgmentSubmission_Serialize_Binary(struct Message *message)
{
    struct MessageJudgmentSubmission *submit;
    struct BinaryBuffer *buffer;

    ASSERT(message != NULL);
    if (message == NULL)
        return false;

    submit = message->message;

    message->length = (uint32_t) sizeof (submit->iReason) +
            Judgment_BinaryLength(submit->pJudgment);

    if ((buffer = BinaryBuffer_Create(message->length)) == NULL)
        return false;

    if (!BinaryBuffer_Put_uint8_t (buffer, submit->iReason))
    {
        BinaryBuffer_Destroy (buffer);
        return false;
    }
    if (!BinaryBuffer_Put_Judgment (buffer, submit->pJudgment))
    {
        BinaryBuffer_Destroy (buffer);
        return false;
    }

    message->serialized = buffer->pBuffer;
    buffer->pBuffer = NULL;
    BinaryBuffer_Destroy(buffer);
    return true;
}


static bool
JudgmentSubmission_Serialize(struct Message *message, int mode)
{
    ASSERT(message != NULL);
    if ( message == NULL )
        return false;

    switch (mode)
    {
    case MESSAGE_MODE_BIN:
        return JudgmentSubmission_Serialize_Binary(message);
    case MESSAGE_MODE_JSON:
        break;
    default:
        rzb_log(LOG_ERR, "%s: Invalid deserialization mode", __func__);
        return false;
    }
    return false;
}

