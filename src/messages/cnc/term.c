#include "config.h"

#include <razorback/debug.h>
#include <razorback/messages.h>
#include <razorback/log.h>

#include "messages/core.h"
#include "messages/cnc/core.h"
#include "binary_buffer.h"

#include <string.h>


static bool Terminate_Deserialize_Binary(struct Message *message);
static bool Terminate_Deserialize(struct Message *message, int mode);
static bool Terminate_Serialize_Binary(struct Message *message);
static bool Terminate_Serialize(struct Message *message, int mode);
static void Terminate_Destroy (struct Message *message);

SO_PUBLIC struct Message *
MessageTerminate_Initialize (
                             const uuid_t p_uuidSourceNugget,
                             const uuid_t p_uuidDestNugget,
                             const uint8_t * p_sTerminateReason)
{
    struct Message *msg;
    struct MessageTerminate *message;
    ASSERT (p_sTerminateReason != NULL);
    if (p_sTerminateReason == NULL)
        return NULL;

    if ((msg = Message_CncCreate(MESSAGE_TYPE_TERM, MESSAGE_VERSION_1, sizeof(struct MessageTerminate), p_uuidSourceNugget, p_uuidDestNugget)) == NULL)
        return NULL;
    message = msg->message;

    if ((message->sTerminateReason =
         malloc (strlen ((const char *) p_sTerminateReason) + 1)) == NULL)
    {
        Terminate_Destroy(msg);
        rzb_log (LOG_ERR,
                 "%s: failed due to lack of memory", __func__);
        return NULL;
    }
    strcpy ((char *) message->sTerminateReason,
            (const char *) p_sTerminateReason);

    msg->destroy=Terminate_Destroy;
    msg->deserialize=Terminate_Deserialize;
    msg->serialize=Terminate_Serialize;
    return msg;
}

void 
Message_CnC_Term_Setup(struct Message *msg)
{
    msg->destroy=Terminate_Destroy;
    msg->deserialize=Terminate_Deserialize;
    msg->serialize=Terminate_Serialize;
}

static void
Terminate_Destroy (struct Message *msg)
{
    struct MessageTerminate *message;
    ASSERT (msg != NULL);
    if (msg == NULL)
        return;
    message = msg->message;
    if (message->sTerminateReason != NULL)
        free (message->sTerminateReason);

    Message_Destroy(msg);
}

static bool
Terminate_Deserialize_Binary(struct Message *message)
{
    struct BinaryBuffer *buffer;
    struct MessageTerminate *term;
    
    ASSERT(message != NULL);
    if (message == NULL)
        return false;
    
    if ((buffer = BinaryBuffer_CreateFromMessage(message)) == NULL)
        return false;
    
    term = message->message;

    if ((term->sTerminateReason = BinaryBuffer_Get_String (buffer)) == NULL)
    {
        buffer->pBuffer = NULL;
        BinaryBuffer_Destroy (buffer);
        rzb_log (LOG_ERR,
                 "%s: failed due to failure of BinaryBuffer_Get_String", __func__);
        return false;
    }

    buffer->pBuffer = NULL;
    BinaryBuffer_Destroy (buffer);
    return true;
}

static bool
Terminate_Deserialize(struct Message *message, int mode)
{
    ASSERT(message != NULL);
    if ( message == NULL )
        return false;

    if ((message->message = calloc(1,sizeof(struct MessageTerminate))) == NULL)
        return false;

    switch (mode)
    {
    case MESSAGE_MODE_BIN:
        return Terminate_Deserialize_Binary(message);
    case MESSAGE_MODE_JSON:
        break;
    default:
        rzb_log(LOG_ERR, "%s: Invalid deserialization mode", __func__);
        return false;
    }
    return false;
}
static bool
Terminate_Serialize_Binary(struct Message *message)
{
    struct MessageTerminate *term;
    struct BinaryBuffer *buffer;

    ASSERT(message != NULL);
    if (message == NULL)
        return false;

    term = message->message;

    message->length = strlen((const char *)term->sTerminateReason) +1;

    if ((buffer = BinaryBuffer_Create(message->length)) == NULL)
        return false;


    if (!BinaryBuffer_Put_String
        (buffer, term->sTerminateReason))
    {
        BinaryBuffer_Destroy (buffer);
        rzb_log (LOG_ERR,
                 "%s: ( TERM ) failed due to failure of BinaryBuffer_Put_String", __func__);
        return false;
    }


    message->serialized = buffer->pBuffer;
    buffer->pBuffer = NULL;
    BinaryBuffer_Destroy(buffer);
    return true;
}


static bool
Terminate_Serialize(struct Message *message, int mode)
{
    ASSERT(message != NULL);
    if ( message == NULL )
        return false;

    switch (mode)
    {
    case MESSAGE_MODE_BIN:
        return Terminate_Serialize_Binary(message);
    case MESSAGE_MODE_JSON:
        break;
    default:
        rzb_log(LOG_ERR, "%s: Invalid deserialization mode", __func__);
        return false;
    }
    return false;
}
