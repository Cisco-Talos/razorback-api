#include "config.h"
#include <razorback/debug.h>
#include <razorback/block.h>
#include <razorback/block_id.h>
#include <razorback/messages.h>
#include <razorback/hash.h>
#include <razorback/ntlv.h>
#include <razorback/log.h>
#include <razorback/event.h>
#include <razorback/uuids.h>
#include <razorback/judgment.h>

#include "runtime_config.h"
#include "messages_binary.h"

#include <string.h>
static void MessageAlert_Destroy (struct Message *message);




// End: Command and control





SO_PUBLIC struct Message *
MessageAlert_Initialize (
                         const struct Block *p_pBlock,
                         uint32_t p_iDisposition,
                         const uuid_t p_uuidInspectorId)
{
    struct MessageAlert *message;
    struct Message *msg;

    ASSERT (p_pBlock != NULL);
    if (p_pBlock ==  NULL)
        return NULL;

    if ((msg = Message_Create(MESSAGE_TYPE_ALERT, MESSAGE_VERSION_1, sizeof(struct MessageAlert))) == NULL)
        return NULL;
    message = msg->message;

    if ((message->pBlock= Block_Clone(p_pBlock)) == NULL)
    {
        rzb_log(LOG_ERR, "%s: Failed to clone message", __func__);
        MessageAlert_Destroy(msg);
        return NULL;
    }

    message->iDisposition = p_iDisposition;
    uuid_copy (message->uuidInspectorId, p_uuidInspectorId);

    msg->destroy = MessageAlert_Destroy;
    return msg;
}

SO_PUBLIC void
MessageAlert_Destroy (struct Message *msg)
{
    struct MessageAlert *message;
    ASSERT (msg != NULL);
    if (msg == NULL)
        return;


    message = msg->message;
    // destroy any malloc'd components
    Block_Destroy (message->pBlock);
    
    Message_Destroy(msg);
}


