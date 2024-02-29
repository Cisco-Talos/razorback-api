#include "config.h"

#include <razorback/debug.h>
#include <razorback/messages.h>
#include <razorback/log.h>

#include "messages/core.h"
#include "messages/cnc/core.h"
#include "binary_buffer.h"

SO_PUBLIC struct Message *
MessageRunning_Initialize (
                           const uuid_t p_uuidSourceNugget,
                           const uuid_t p_uuidDestNugget)
{
    struct Message *msg;
    if ((msg = Message_CncCreate(MESSAGE_TYPE_RUNNING, MESSAGE_VERSION_1, 0, p_uuidSourceNugget, p_uuidDestNugget)) == NULL)
        return NULL;

    msg->destroy = Message_Destroy;
    msg->deserialize = Message_CnC_Deserialize_Empty;
    msg->serialize = Message_CnC_Serialize_Empty;
    return msg;
}

void
Message_CnC_Running_Setup(struct Message *msg)
{
    msg->destroy=Message_Destroy;
    msg->deserialize = Message_CnC_Deserialize_Empty;
    msg->serialize = Message_CnC_Serialize_Empty;
}

