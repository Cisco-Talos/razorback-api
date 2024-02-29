#include "config.h"

#include <razorback/debug.h>
#include <razorback/messages.h>
#include <razorback/log.h>

#include "messages/core.h"
#include "messages/cnc/core.h"
#include "binary_buffer.h"

SO_PUBLIC struct Message *
MessageBye_Initialize (
                       const uuid_t p_uuidSourceNugget)
{
    struct Message *msg;
    if ((msg = Message_CncBcastCreate(MESSAGE_TYPE_BYE, MESSAGE_VERSION_1, 0, p_uuidSourceNugget)) == NULL)
        return NULL;

    msg->destroy = Message_Destroy;
    msg->deserialize = Message_CnC_Deserialize_Empty;
    msg->serialize = Message_CnC_Serialize_Empty;
    return msg;
}

void 
Message_CnC_Bye_Setup(struct Message *msg)
{
    msg->destroy = Message_Destroy;
    msg->deserialize = Message_CnC_Deserialize_Empty;
    msg->serialize = Message_CnC_Serialize_Empty;
}
