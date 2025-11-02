#include "config.h"

#include <razorback/debug.h>
#include <razorback/messages.h>
#include <razorback/log.h>
#include <razorback/event.h>
#include <razorback/block.h>
#include <razorback/nugget.h>
#include <razorback/json_buffer.h>

#include "messages/core.h"

#include <string.h>

static void AlertChild_Destroy (struct Message *message);
static bool AlertChild_Deserialize(struct Message *message);
static bool AlertChild_Serialize(struct Message *message);

static struct MessageHandler handler = {
    MESSAGE_TYPE_ALERT_CHILD,
    AlertChild_Serialize,
    AlertChild_Deserialize,
    AlertChild_Destroy
};

// core.h
void MessageAlertChild_Init(void)
{
    Message_Register_Handler(&handler);
}

SO_PUBLIC struct Message *
MessageAlertChild_Initialize (
                                   struct Block *block,
                                   struct Block *child,
                                   struct Nugget *nugget,
                                   uint64_t parentCount, uint64_t eventCount,
                                   uint32_t new_SF_Flags, uint32_t new_Ent_Flags,
                                   uint32_t old_SF_Flags, uint32_t old_Ent_Flags)
{
    struct Message *msg;
    struct MessageAlertChild *message;
    ASSERT (child != NULL);
    ASSERT (block != NULL);
    ASSERT (nugget != NULL);
    if (child == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: child block is NULL", __func__);
        return NULL;
    }
    if (block == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: parent block is NULL", __func__);
        return NULL;
    }
    if (nugget == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: nugget is NULL", __func__);
        return NULL;
    }

    if ((msg = Message_Create(MESSAGE_TYPE_ALERT_CHILD, MESSAGE_VERSION_1, sizeof(struct MessageAlertChild))) == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Message_Create failed", __func__);
        return NULL;
    }

    message = msg->message;

    message->child = child;
    message->block = block;
    message->nugget = nugget;
    message->SF_Flags = new_SF_Flags;
    message->Ent_Flags = new_Ent_Flags;
    message->Old_SF_Flags = old_SF_Flags;
    message->Old_Ent_Flags = old_Ent_Flags;
    message->parentCount = parentCount;
    message->eventCount = eventCount;

    msg->destroy = AlertChild_Destroy;
    msg->deserialize = AlertChild_Deserialize;
    msg->serialize = AlertChild_Serialize;

    return msg;
}

static void
AlertChild_Destroy(struct Message *message)
{
    struct MessageAlertChild *msg;

    ASSERT(message != NULL);
    if (message == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: message is NULL", __func__);
        return;
    }
    msg = message->message;

    if (msg != NULL) {
        // destroy any malloc'd components
        if (msg->child != NULL) {
            Block_Destroy(msg->child);
        }
        if (msg->block != NULL) {
            Block_Destroy(msg->block);
        }
        if (msg->nugget != NULL) {
            Nugget_Destroy(msg->nugget);
        }
    }
    Message_Destroy(message);
}

static bool
AlertChild_Deserialize(struct Message *message)
{
    struct MessageAlertChild *alert;
    json_object *msg;
    ASSERT(message != NULL);
    if (message == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: message is NULL", __func__);
        return false;
    }

    if ((message->message = calloc(1, sizeof(struct MessageAlertChild))) == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: calloc failed", __func__);
        return false;
    }

    if ((msg = json_tokener_parse((char *) message->serialized)) == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: json_tokener_parse failed", __func__);
        return false;
    }

    alert = message->message;

    if (!JsonBuffer_Get_Nugget(msg, "Nugget", &alert->nugget)) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: JsonBuffer_Get_Nugget failed", __func__);
        json_object_put(msg);
        return false;
    }
    if (!JsonBuffer_Get_Block(msg, "Block", &alert->block)) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: JsonBuffer_Get_Block failed", __func__);
        json_object_put(msg);
        return false;
    }
    if (!JsonBuffer_Get_Block(msg, "Child_Block", &alert->child)) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: JsonBuffer_Get_Block (child) failed", __func__);
        json_object_put(msg);
        return false;
    }
    if (!JsonBuffer_Get_uint32_t(msg, "SF_Flags", &alert->SF_Flags)) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: JsonBuffer_Get_uint32_t (SF_Flags) failed", __func__);
        json_object_put(msg);
        return false;
    }
    if (!JsonBuffer_Get_uint32_t(msg, "Ent_Flags", &alert->Ent_Flags)) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: JsonBuffer_Get_uint32_t (Ent_Flags) failed", __func__);
        json_object_put(msg);
        return false;
    }
    if (!JsonBuffer_Get_uint32_t(msg, "Old_SF_Flags", &alert->Old_SF_Flags)) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: JsonBuffer_Get_uint32_t (Old_SF_Flags) failed", __func__);
        json_object_put(msg);
        return false;
    }
    if (!JsonBuffer_Get_uint32_t(msg, "Old_Ent_Flags", &alert->Old_Ent_Flags)) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: JsonBuffer_Get_uint32_t (Old_Ent_Flags) failed", __func__);
        json_object_put(msg);
        return false;
    }
    if (!JsonBuffer_Get_uint64_t(msg, "Event_Count", &alert->eventCount)) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: JsonBuffer_Get_uint64_t (Event_Count) failed", __func__);
        json_object_put(msg);
        return false;
    }
    if (!JsonBuffer_Get_uint64_t(msg, "Parent_Count", &alert->parentCount)) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: JsonBuffer_Get_uint64_t (Parent_Count) failed", __func__);
        json_object_put(msg);
        return false;
    }

    json_object_put(msg);
    return true;
}


static bool
AlertChild_Serialize(struct Message *message)
{
    struct MessageAlertChild *alert;
    json_object *msg;
    const char *wire;

    ASSERT(message != NULL);
    if (message == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: message is NULL", __func__);
        return false;
    }

    alert = message->message;
    ASSERT(alert != NULL);
    if (alert == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: alert is NULL", __func__);
        return false;
    }

    if ((msg = json_object_new_object()) == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: json_object_new_object failed", __func__);
        return false;
    }

    if (!JsonBuffer_Put_Nugget(msg, "Nugget", alert->nugget)) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: JsonBuffer_Put_Nugget failed", __func__);
        json_object_put(msg);
        return false;
    }
    if (!JsonBuffer_Put_Block(msg, "Block", alert->block)) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: JsonBuffer_Put_Block failed", __func__);
        json_object_put(msg);
        return false;
    }
    if (!JsonBuffer_Put_Block(msg, "Child_Block", alert->child)) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: JsonBuffer_Put_Block (child) failed", __func__);
        json_object_put(msg);
        return false;
    }
    if (!JsonBuffer_Put_uint32_t(msg, "SF_Flags", alert->SF_Flags)) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: JsonBuffer_Put_uint32_t (SF_Flags) failed", __func__);
        json_object_put(msg);
        return false;
    }
    if (!JsonBuffer_Put_uint32_t(msg, "Ent_Flags", alert->Ent_Flags)) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: JsonBuffer_Put_uint32_t (Ent_Flags) failed", __func__);
        json_object_put(msg);
        return false;
    }
    if (!JsonBuffer_Put_uint32_t(msg, "Old_SF_Flags", alert->Old_SF_Flags)) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: JsonBuffer_Put_uint32_t (Old_SF_Flags) failed", __func__);
        json_object_put(msg);
        return false;
    }
    if (!JsonBuffer_Put_uint32_t(msg, "Old_Ent_Flags", alert->Old_Ent_Flags)) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: JsonBuffer_Put_uint32_t (Old_Ent_Flags) failed", __func__);
        json_object_put(msg);
        return false;
    }
    if (!JsonBuffer_Put_uint64_t(msg, "Event_Count", alert->eventCount)) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: JsonBuffer_Put_uint64_t (Event_Count) failed", __func__);
        json_object_put(msg);
        return false;
    }
    if (!JsonBuffer_Put_uint64_t(msg, "Parent_Count", alert->parentCount)) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: JsonBuffer_Put_uint64_t (Parent_Count) failed", __func__);
        json_object_put(msg);
        return false;
    }

    wire = json_object_to_json_string(msg);
    message->length = strlen(wire);
    if ((message->serialized = calloc(message->length + 1, sizeof(uint8_t))) == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: failed to allocate serialized message", __func__);
        json_object_put(msg);
        return false;
    }
    strcpy((char *) message->serialized, wire);
    json_object_put(msg);

    return true;
}
