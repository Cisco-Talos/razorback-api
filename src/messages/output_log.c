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
#include <razorback/json_buffer.h>

#include "messages/core.h"

#include <string.h>

static void OutputLog_Destroy (struct Message *message);
static bool OutputLog_Deserialize(struct Message *message);
static bool OutputLog_Serialize(struct Message *message);

static struct MessageHandler handler = {
    MESSAGE_TYPE_OUTPUT_LOG,
    OutputLog_Serialize,
    OutputLog_Deserialize,
    OutputLog_Destroy
};

//core.h
void
MessageOutputLog_Init(void)
{
    Message_Register_Handler(&handler);
}

SO_PUBLIC struct Message *
MessageOutputLog_Initialize (
                                   struct MessageLogSubmission *log,
                                   struct Nugget *nugget)
{
    struct Message *msg;
    struct MessageOutputLog *message;
    ASSERT (log != NULL);
    ASSERT (nugget != NULL);
    if (log == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: log is NULL", __func__);
        return NULL;
    }
    if (nugget == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: nugget is NULL", __func__);
        return NULL;
    }

    if ((msg = Message_Create(MESSAGE_TYPE_OUTPUT_LOG, MESSAGE_VERSION_1, sizeof(struct MessageOutputLog))) == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Message_Create failed", __func__);
        return NULL;
    }
    message = msg->message;
    if ((message->event = calloc(1,sizeof(struct Event))) == NULL)
    {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: failed to allocate event", __func__);
        Message_Destroy(msg);
        return NULL;
    }
    message->event->pId = log->pEventId;
    message->nugget = nugget;
    message->priority = log->iPriority;
    message->message = (char *)log->sMessage;

    msg->destroy = OutputLog_Destroy;
    msg->deserialize=OutputLog_Deserialize;
    msg->serialize=OutputLog_Serialize;

    return msg;
}

static void
OutputLog_Destroy (struct Message *message)
{
    struct MessageOutputLog *msg;

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
OutputLog_Deserialize(struct Message *message)
{
    struct MessageOutputLog *log;
    json_object *msg;

    ASSERT(message != NULL);
    if (message == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: message is NULL", __func__);
        return false;
    }

    if ((message->message = calloc(1, sizeof(struct MessageOutputLog))) == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: failed to allocate message", __func__);
        return false;
    }

    if ((msg = json_tokener_parse((char *) message->serialized)) == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: json_tokener_parse failed", __func__);
        return false;
    }

    log = message->message;

    if (!JsonBuffer_Get_Nugget(msg, "Nugget", &log->nugget)) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: JsonBuffer_Get_Nugget failed", __func__);
        json_object_put(msg);
        return false;
    }
    if (json_object_object_get(msg, "Event") != NULL) {
        if (!JsonBuffer_Get_Event(msg, "Event", &log->event)) {
            rzb_log(LOG_ERR, LOG_C_CORE, "%s: JsonBuffer_Get_Event failed", __func__);
            json_object_put(msg);
            return false;
        }
    }
    if (!JsonBuffer_Get_uint8_t(msg, "Priority", &log->priority)) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: JsonBuffer_Get_uint8_t (Priority) failed", __func__);
        json_object_put(msg);
        return false;
    }
    if (!JsonBuffer_Get_uint64_t(msg, "Seconds", &log->seconds)) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: JsonBuffer_Get_uint64_t (Seconds) failed", __func__);
        json_object_put(msg);
        return false;
    }
    if (!JsonBuffer_Get_uint64_t(msg, "Nano_Seconds", &log->nanosecs)) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: JsonBuffer_Get_uint64_t (Nano_Seconds) failed", __func__);
        json_object_put(msg);
        return false;
    }
    if ((log->message = JsonBuffer_Get_String(msg, "Message")) == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: JsonBuffer_Get_String (Message) failed", __func__);
        json_object_put(msg);
        return false;
    }

    json_object_put(msg);
    return true;
}

static bool
OutputLog_Serialize(struct Message *message)
{
    struct MessageOutputLog *log;
    json_object *msg;
    const char *wire;

    ASSERT(message != NULL);
    if (message == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: message is NULL", __func__);
        return false;
    }

    log = message->message;

    if ((msg = json_object_new_object()) == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: json_object_new_object failed", __func__);
        return false;
    }

    if (!JsonBuffer_Put_Nugget(msg, "Nugget", log->nugget)) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: JsonBuffer_Put_Nugget failed", __func__);
        json_object_put(msg);
        return false;
    }
    if (log->event != NULL) {
        if (!JsonBuffer_Put_Event(msg, "Event", log->event)) {
            rzb_log(LOG_ERR, LOG_C_CORE, "%s: JsonBuffer_Put_Event failed", __func__);
            json_object_put(msg);
            return false;
        }
    }
    if (!JsonBuffer_Put_uint8_t(msg, "Priority", log->priority)) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: JsonBuffer_Put_uint8_t (Priority) failed", __func__);
        json_object_put(msg);
        return false;
    }
    if (!JsonBuffer_Put_uint64_t(msg, "Seconds", log->seconds)) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: JsonBuffer_Put_uint64_t (Seconds) failed", __func__);
        json_object_put(msg);
        return false;
    }
    if (!JsonBuffer_Put_uint64_t(msg, "Nano_Seconds", log->nanosecs)) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: JsonBuffer_Put_uint64_t (Nano_Seconds) failed", __func__);
        json_object_put(msg);
        return false;
    }
    if (!JsonBuffer_Put_String(msg, "Message", log->message)) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: JsonBuffer_Put_String (Message) failed", __func__);
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
