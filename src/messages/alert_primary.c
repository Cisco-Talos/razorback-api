/*
 * Copyright (c) 2011-2026 Cisco Systems, Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *  MA 02110-1301, USA.
 */

#include "config.h"

#include <razorback/debug.h>
#include <razorback/messages.h>
#include <razorback/log.h>
#include <razorback/event.h>
#include <razorback/block.h>
#include <razorback/nugget.h>
#include <razorback/list.h>
#include <razorback/json_buffer.h>

#include "messages/core.h"

#include <string.h>

static void AlertPrimary_Destroy (struct Message *message);
static bool AlertPrimary_Deserialize(struct Message *message);
static bool AlertPrimary_Serialize(struct Message *message);

static struct MessageHandler handler = {
    MESSAGE_TYPE_ALERT_PRIMARY,
    AlertPrimary_Serialize,
    AlertPrimary_Deserialize,
    AlertPrimary_Destroy
};

// core.h
void
MessageAlertPrimary_Init(void)
{
    Message_Register_Handler(&handler);
}

SO_PUBLIC struct Message *
MessageAlertPrimary_Initialize (
                                   struct Event *event,
                                   struct Block *block,
                                   List_t *metadata,
                                   struct Nugget *nugget,
                                   struct Judgment *judgment,
                                   uint32_t new_SF_Flags, uint32_t new_Ent_Flags,
                                   uint32_t old_SF_Flags, uint32_t old_Ent_Flags)
{
    struct Message *msg;
    struct MessageAlertPrimary *message;
    ASSERT (event != NULL);
    ASSERT (block != NULL);
    ASSERT (metadata != NULL);
    ASSERT (nugget != NULL);
    ASSERT (judgment != NULL);
    if (event == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: event is NULL", __func__);
        return NULL;
    }
    if (block == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: block is NULL", __func__);
        return NULL;
    }
    if (metadata == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: metadata is NULL", __func__);
        return NULL;
    }
    if (nugget == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: nugget is NULL", __func__);
        return NULL;
    }
    if (judgment == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: judgment is NULL", __func__);
        return NULL;
    }

    if ((msg = Message_Create(MESSAGE_TYPE_ALERT_PRIMARY, MESSAGE_VERSION_1, sizeof(struct MessageAlertPrimary))) == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Message_Create failed", __func__);
        return NULL;
    }

    message = msg->message;

    message->event = event;
    message->block = block;
    message->metadata = metadata;
    message->nugget = nugget;
    message->gid = judgment->iGID;
    message->sid = judgment->iSID;
    if (judgment->sMessage)
        message->message = (char *)judgment->sMessage;

    message->seconds = judgment->iSeconds;
    message->nanosecs = judgment->iNanoSecs;
    message->priority = judgment->iPriority;
    message->SF_Flags = new_SF_Flags;
    message->Ent_Flags = new_Ent_Flags;
    message->Old_SF_Flags = old_SF_Flags;
    message->Old_Ent_Flags = old_Ent_Flags;


    msg->destroy = AlertPrimary_Destroy;
    msg->deserialize=AlertPrimary_Deserialize;
    msg->serialize=AlertPrimary_Serialize;

    return msg;
}

static void
AlertPrimary_Destroy (struct Message *message)
{
    struct MessageAlertPrimary *msg;

    ASSERT (message != NULL);
    if (message == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: message is NULL", __func__);
        return;
    }
    msg = message->message;

    if (msg != NULL) {
        // destroy any malloc'd components
        if (msg->event != NULL) {
            Event_Destroy(msg->event);
        }
        if (msg->block != NULL) {
            Block_Destroy(msg->block);
        }

        if (msg->metadata != NULL) {
            List_Destroy(msg->metadata);
        }

        if (msg->message != NULL) {
            free(msg->message);
        }

        if (msg->nugget != NULL) {
            Nugget_Destroy(msg->nugget);
        }
    }

    Message_Destroy(message);
}

static bool
AlertPrimary_Deserialize(struct Message *message)
{
    struct MessageAlertPrimary *alert;
    json_object *msg;
    ASSERT(message != NULL);
    if ( message == NULL ) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: message is NULL", __func__);
        return false;
    }

    if ((message->message = calloc(1,sizeof(struct MessageAlertPrimary))) == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: calloc failed", __func__);
        return false;
    }

    if ((msg = json_tokener_parse((char *)message->serialized)) == NULL) {
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
    if (!JsonBuffer_Get_Event(msg, "Event", &alert->event)) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: JsonBuffer_Get_Event failed", __func__);
        json_object_put(msg);
        return false;
    }

    // Some alerts dont have messages
    alert->message = JsonBuffer_Get_String(msg, "Message");

    if (!JsonBuffer_Get_uint8_t(msg, "Priority", &alert->priority)) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: JsonBuffer_Get_uint8_t (Priority) failed", __func__);
        json_object_put(msg);
        return false;
    }
    if (!JsonBuffer_Get_uint64_t(msg, "Seconds", &alert->seconds)) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: JsonBuffer_Get_uint64_t (Seconds) failed", __func__);
        json_object_put(msg);
        return false;
    }
    if (!JsonBuffer_Get_uint64_t(msg, "Nano_Seconds", &alert->nanosecs)) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: JsonBuffer_Get_uint64_t (Nano_Seconds) failed", __func__);
        json_object_put(msg);
        return false;
    }
    if (!JsonBuffer_Get_uint32_t(msg, "GID", &alert->gid)) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: JsonBuffer_Get_uint32_t (GID) failed", __func__);
        json_object_put(msg);
        return false;
    }
    if (!JsonBuffer_Get_uint32_t(msg, "SID", &alert->sid)) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: JsonBuffer_Get_uint32_t (SID) failed", __func__);
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

    if (!JsonBuffer_Get_NTLVList(msg, "Metadata", &alert->metadata)) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: JsonBuffer_Get_NTLVList (Metadata) failed", __func__);
        json_object_put(msg);
        return false;
    }

    json_object_put(msg);
    return true;
}

static bool
AlertPrimary_Serialize(struct Message *message)
{
    struct MessageAlertPrimary *alert;
    json_object *msg;
    const char * wire;

    ASSERT(message != NULL);
    if (message == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: message is NULL", __func__);
        return false;
    }

    alert = message->message;

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
    if (!JsonBuffer_Put_Event(msg, "Event", alert->event)) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: JsonBuffer_Put_Event failed", __func__);
        json_object_put(msg);
        return false;
    }
    if (alert->message != NULL) {
        if (!JsonBuffer_Put_String(msg, "Message", alert->message)) {
            rzb_log(LOG_ERR, LOG_C_CORE, "%s: JsonBuffer_Put_String (Message) failed", __func__);
            json_object_put(msg);
            return false;
        }
    }
    if (!JsonBuffer_Put_uint8_t(msg, "Priority", alert->priority)) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: JsonBuffer_Put_uint8_t (Priority) failed", __func__);
        json_object_put(msg);
        return false;
    }
    if (!JsonBuffer_Put_uint64_t(msg, "Seconds", alert->seconds)) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: JsonBuffer_Put_uint64_t (Seconds) failed", __func__);
        json_object_put(msg);
        return false;
    }
    if (!JsonBuffer_Put_uint64_t(msg, "Nano_Seconds", alert->nanosecs)) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: JsonBuffer_Put_uint64_t (Nano_Seconds) failed", __func__);
        json_object_put(msg);
        return false;
    }
    if (!JsonBuffer_Put_uint32_t(msg, "GID", alert->gid)) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: JsonBuffer_Put_uint32_t (GID) failed", __func__);
        json_object_put(msg);
        return false;
    }
    if (!JsonBuffer_Put_uint32_t(msg, "SID", alert->sid)) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: JsonBuffer_Put_uint32_t (SID) failed", __func__);
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
    if (!JsonBuffer_Put_NTLVList(msg, "Metadata", alert->metadata)) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: JsonBuffer_Put_NTLVList (Metadata) failed", __func__);
        json_object_put(msg);
        return false;
    }


    wire = json_object_to_json_string(msg);
    if (!Message_SetSerializedJson(message, wire, LOG_C_CORE, __func__)) {
        json_object_put(msg);
        return false;
    }
    json_object_put(msg);

    return true;
}
