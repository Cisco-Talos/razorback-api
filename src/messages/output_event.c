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

static void OutputEvent_Destroy (struct Message *message);
static bool OutputEvent_Deserialize(struct Message *message);
static bool OutputEvent_Serialize(struct Message *message);

static struct MessageHandler handler = {
    MESSAGE_TYPE_OUTPUT_EVENT,
    OutputEvent_Serialize,
    OutputEvent_Deserialize,
    OutputEvent_Destroy
};

//core.h
void
MessageOutputEvent_Init(void)
{
    Message_Register_Handler(&handler);
}

SO_PUBLIC struct Message *
MessageOutputEvent_Initialize (
                                   struct Event *event,
                                   struct Nugget *nugget)
{
    struct Message *msg;
    struct MessageOutputEvent *message;
    ASSERT (event != NULL);
    ASSERT (nugget != NULL);
    if (event == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: event is NULL", __func__);
        return NULL;
    }
    if (nugget == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: nugget is NULL", __func__);
        return NULL;
    }

    if ((msg = Message_Create(MESSAGE_TYPE_OUTPUT_EVENT, MESSAGE_VERSION_1, sizeof(struct MessageOutputEvent))) == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Message_Create failed", __func__);
        return NULL;
    }
    message = msg->message;
    message->event = event;
    message->nugget = nugget;

    msg->destroy = OutputEvent_Destroy;
    msg->deserialize=OutputEvent_Deserialize;
    msg->serialize=OutputEvent_Serialize;

    return msg;
}

static void
OutputEvent_Destroy (struct Message *message)
{
    struct MessageOutputEvent *msg;

    ASSERT (message != NULL);
    if (message == NULL)
        return;
    msg = message->message;
    if (msg != NULL) {
        // destroy any malloc'd components
        if (msg->event != NULL) {
            Event_Destroy(msg->event);
        }

        if (msg->nugget != NULL) {
            Nugget_Destroy(msg->nugget);
        }
    }
    Message_Destroy(message);
}

static bool
OutputEvent_Deserialize(struct Message *message)
{
    struct MessageOutputEvent *event;
    json_object *msg;

    ASSERT(message != NULL);
    if (message == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: message is NULL", __func__);
        return false;
    }

    if ((message->message = calloc(1,sizeof(struct MessageOutputEvent))) == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: calloc failed", __func__);
        return false;
    }

    if ((msg = json_tokener_parse((char *)message->serialized)) == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: json_tokener_parse failed", __func__);
        return false;
    }

    event = message->message;

    if (!JsonBuffer_Get_Nugget(msg, "Nugget", &event->nugget)) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: JsonBuffer_Get_Nugget failed", __func__);
        json_object_put(msg);
        return false;
    }
    if (!JsonBuffer_Get_Event(msg, "Event", &event->event)) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: JsonBuffer_Get_Event failed", __func__);
        json_object_put(msg);
        return false;
    }

    json_object_put(msg);
    return true;
}

static bool
OutputEvent_Serialize(struct Message *message)
{
    struct MessageOutputEvent *event;
    json_object *msg;
    const char * wire;

    ASSERT(message != NULL);
    if (message == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: message is NULL", __func__);
        return false;
    }

    event = message->message;

    if ((msg = json_object_new_object()) == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: json_object_new_object failed", __func__);
        return false;
    }

    if (!JsonBuffer_Put_Nugget(msg, "Nugget", event->nugget))
    {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: JsonBuffer_Put_Nugget failed", __func__);
        json_object_put(msg);
        return false;
    }
    if (!JsonBuffer_Put_Event(msg, "Event", event->event))
    {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: JsonBuffer_Put_Event failed", __func__);
        json_object_put(msg);
        return false;
    }

    wire = json_object_to_json_string(msg);
    message->length=strlen(wire);
    if ((message->serialized = calloc(message->length+1, sizeof(uint8_t))) == NULL)
    {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: failed to allocate serialized message", __func__);
        json_object_put(msg);
        return false;
    }
    strcpy((char *)message->serialized, wire);
    json_object_put(msg);

    return true;
}