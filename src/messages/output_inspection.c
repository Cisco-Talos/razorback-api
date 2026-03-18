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
#include <razorback/block_id.h>
#include <razorback/nugget.h>
#include <razorback/json_buffer.h>

#include "messages/core.h"

#include <string.h>

static void OutputInspection_Destroy (struct Message *message);
static bool OutputInspection_Deserialize(struct Message *message);
static bool OutputInspection_Serialize(struct Message *message);

static struct MessageHandler handler = {
    MESSAGE_TYPE_OUTPUT_INSPECTION,
    OutputInspection_Serialize,
    OutputInspection_Deserialize,
    OutputInspection_Destroy
};

//core.h
void
MessageOutputInspection_Init(void)
{
    Message_Register_Handler(&handler);
}

SO_PUBLIC struct Message *
MessageOutputInspection_Initialize (
                                   struct Nugget *nugget,
                                   struct BlockId *blockId, uint8_t reason, bool final)
{
    struct Message *msg;
    struct MessageOutputInspection *message;
    struct timespec l_tsTime;
    ASSERT (blockId != NULL);
    ASSERT (nugget != NULL);
    if (blockId == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: BlockId is NULL", __func__);
        return NULL;
    }
    if (nugget == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Nugget is NULL", __func__);
        return NULL;
    }

    if ((msg = Message_Create(MESSAGE_TYPE_OUTPUT_INSPECTION, MESSAGE_VERSION_1, sizeof(struct MessageOutputInspection))) == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to create message", __func__);
        return NULL;
    }
    message = msg->message;
    message->blockId = BlockId_Clone(blockId);
    message->nugget = nugget;
    message->status = reason;
    message->final = final;

    memset(&l_tsTime, 0, sizeof(struct timespec));
    if (clock_gettime(CLOCK_REALTIME, &l_tsTime) == -1) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to get time stamp", __func__);
        return NULL;
    }
    message->seconds = l_tsTime.tv_sec;
    message->nanosecs = l_tsTime.tv_nsec;

    msg->destroy = OutputInspection_Destroy;
    msg->deserialize=OutputInspection_Deserialize;
    msg->serialize=OutputInspection_Serialize;

    return msg;
}

static void
OutputInspection_Destroy (struct Message *message)
{
    struct MessageOutputInspection *msg;

    ASSERT (message != NULL);
    if (message == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Message is NULL", __func__);
        return;
    }
    msg = message->message;
    if (msg != NULL) {
        // destroy any malloc'd components
        if (msg->blockId != NULL) {
            BlockId_Destroy(msg->blockId);
        }

        if (msg->nugget != NULL) {
            Nugget_Destroy(msg->nugget);
        }
    }
    Message_Destroy(message);
}

static bool
OutputInspection_Deserialize(struct Message *message)
{
    struct MessageOutputInspection *event;
    json_object *msg;

    ASSERT(message != NULL);
    if (message == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Message is NULL", __func__);
        return false;
    }

    if ((message->message = calloc(1,sizeof(struct MessageOutputInspection))) == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to allocate message", __func__);
        return false;
    }

    if ((msg = json_tokener_parse((char *)message->serialized)) == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to parse JSON", __func__);
        return false;
    }

    event = message->message;

    if (!JsonBuffer_Get_Nugget(msg, "Nugget", &event->nugget)) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to get Nugget", __func__);
        json_object_put(msg);
        return false;
    }
    if (!JsonBuffer_Get_BlockId(msg, "BlockId", &event->blockId)) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to get BlockId", __func__);
        json_object_put(msg);
        return false;
    }
    if (!JsonBuffer_Get_uint8_t(msg, "Status", &event->status)) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to get Status", __func__);
        json_object_put(msg);
        return false;
    }
    if (!JsonBuffer_Get_uint64_t(msg, "Seconds", &event->seconds)) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to get Seconds", __func__);
        json_object_put(msg);
        return false;
    }
    if (!JsonBuffer_Get_uint64_t(msg, "NanoSecs", &event->nanosecs)) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to get NanoSecs", __func__);
        json_object_put(msg);
        return false;
    }
    uint8_t final = 0;
    if (!JsonBuffer_Get_uint8_t(msg, "Final", &final)) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to get Final", __func__);
        json_object_put(msg);
        return false;
    }
    if (final == 0) {
        event->final = false;
    } else {
        event->final = true;
    }

    json_object_put(msg);
    return true;
}

static bool
OutputInspection_Serialize(struct Message *message)
{
    struct MessageOutputInspection *event;
    json_object *msg;
    const char *wire;

    ASSERT(message != NULL);
    if (message == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Message is NULL", __func__);
        return false;
    }

    event = message->message;

    if ((msg = json_object_new_object()) == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to create JSON object", __func__);
        return false;
    }

    if (!JsonBuffer_Put_Nugget(msg, "Nugget", event->nugget)) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to put Nugget", __func__);
        json_object_put(msg);
        return false;
    }
    if (!JsonBuffer_Put_BlockId(msg, "BlockId", event->blockId)) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to put BlockId", __func__);
        json_object_put(msg);
        return false;
    }
    if (!JsonBuffer_Put_uint8_t(msg, "Status", event->status)) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to put Status", __func__);
        json_object_put(msg);
        return false;
    }
    if (!JsonBuffer_Put_uint64_t(msg, "Seconds", event->seconds)) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to put Seconds", __func__);
        json_object_put(msg);
        return false;
    }
    if (!JsonBuffer_Put_uint64_t(msg, "NanoSecs", event->nanosecs)) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to put NanoSecs", __func__);
        json_object_put(msg);
        return false;
    }
    if (!JsonBuffer_Put_uint8_t(msg, "Final", (event->final ? 1 : 0))) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to put Final", __func__);
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
