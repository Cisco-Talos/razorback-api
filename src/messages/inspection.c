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
#include <razorback/block.h>
#include <razorback/event.h>
#include <razorback/json_buffer.h>
#include <razorback/list.h>


#include "messages/core.h"

#include <string.h>

static void InspectionSubmission_Destroy (struct Message *message);
static bool InspectionSubmission_Deserialize(struct Message *message);
static bool InspectionSubmission_Serialize(struct Message *message);

static struct MessageHandler handler = {
    MESSAGE_TYPE_INSPECTION,
    InspectionSubmission_Serialize,
    InspectionSubmission_Deserialize,
    InspectionSubmission_Destroy
};

//core.h
void
MessageInspectionSubmission_Init(void)
{
    Message_Register_Handler(&handler);
}

SO_PUBLIC struct Message *
MessageInspectionSubmission_Initialize (
                                        const struct Event *p_pEvent,
                                        uint32_t p_iReason,
                                        uint32_t localityCount,
                                        uint8_t *localities)
{
    struct Message * msg;
    struct MessageInspectionSubmission *message;

    ASSERT (p_pEvent != NULL);
    if (p_pEvent == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Event is NULL", __func__);
        return NULL;
    }

    if ((msg = Message_Create(MESSAGE_TYPE_INSPECTION, MESSAGE_VERSION_1, sizeof(struct MessageInspectionSubmission))) == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to create message", __func__);
        return NULL;
    }
    message = msg->message;

    if ((message->pBlock = Block_Clone(p_pEvent->pBlock)) == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to clone block", __func__);
        InspectionSubmission_Destroy(msg);
        return NULL;
    }

    message->iReason = p_iReason;
    if ((message->eventId = EventId_Clone(p_pEvent->pId)) == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to clone event id", __func__);
        InspectionSubmission_Destroy(msg);
        return NULL;
    }
    message->pEventMetadata = List_Clone(p_pEvent->pMetaDataList);
    if (message->pEventMetadata == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to clone metadata list", __func__);
        InspectionSubmission_Destroy(msg);
        return NULL;
    }
    message->localityCount = localityCount;
    if (localityCount > 0) {
        if ((message->localityList = calloc(localityCount, sizeof(uint8_t))) == NULL) {
            rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to clone locality list", __func__);
            InspectionSubmission_Destroy(msg);
            return NULL;
        }
        memcpy(message->localityList, localities, localityCount);
    }
    msg->destroy=InspectionSubmission_Destroy;
    msg->deserialize=InspectionSubmission_Deserialize;
    msg->serialize=InspectionSubmission_Serialize;

    return msg;
}

static void
InspectionSubmission_Destroy (struct Message
                                     *message)
{
    struct MessageInspectionSubmission *msg;
    ASSERT (message != NULL);
    if (message == NULL)
        return;

    msg = (struct MessageInspectionSubmission *)message->message;
    if (msg != NULL) {
        // destroy any malloc'd components
        if (msg->pBlock != NULL) {
            Block_Destroy(msg->pBlock);
        }
        if (msg->eventId != NULL) {
            EventId_Destroy(msg->eventId);
        }
        if (msg->pEventMetadata != NULL) {
            List_Destroy(msg->pEventMetadata);
        }
        if (msg->localityList != NULL) {
            free(msg->localityList);
        }
    }
    Message_Destroy(message);
}

static bool
InspectionSubmission_Deserialize(struct Message *message)
{
    struct MessageInspectionSubmission *submit;
    json_object *msg;

    ASSERT(message != NULL);
    if (message == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: message is NULL", __func__);
        return false;
    }

    if ((message->message = calloc(1,sizeof(struct MessageInspectionSubmission))) == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to allocate message", __func__);
        return false;
    }

    
    if ((msg = json_tokener_parse((char *)message->serialized)) == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to parse JSON", __func__);
        return false;
    }
    
    submit = message->message;

    if (!JsonBuffer_Get_uint32_t(msg, "Reason", &submit->iReason)) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to get Reason", __func__);
        json_object_put(msg);
        return false;
    }
    if (!JsonBuffer_Get_EventId(msg, "Event_ID", &submit->eventId)) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to get Event_ID", __func__);
        json_object_put(msg);
        return false;
    }
    if (!JsonBuffer_Get_NTLVList(msg, "Event_Metadata", &submit->pEventMetadata)) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to get Event_Metadata", __func__);
        json_object_put(msg);
        return false;
    }

    if (!JsonBuffer_Get_Block(msg, "Block", &submit->pBlock)) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to get Block", __func__);
        json_object_put(msg);
        return false;
    }
    if (!JsonBuffer_Get_uint8List(msg, "Avaliable_Localities", &submit->localityList, &submit->localityCount)) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to get Avaliable_Localities", __func__);
        json_object_put(msg);
        return false;
    }
    json_object_put(msg);
    return true;
}

static bool
InspectionSubmission_Serialize(struct Message *message)
{
    struct MessageInspectionSubmission *submit;
    json_object *msg;
    const char * wire;

    ASSERT(message != NULL);
    if (message == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: message is NULL", __func__);
        return false;
    }

    submit = message->message;

    if ((msg = json_object_new_object()) == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to create JSON object", __func__);
        return false;
    }


    if (!JsonBuffer_Put_uint32_t(msg, "Reason", submit->iReason)) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to put Reason", __func__);
        json_object_put(msg);
        return false;
    }
    if (!JsonBuffer_Put_EventId(msg, "Event_ID", submit->eventId)) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to put Event_ID", __func__);
        json_object_put(msg);
        return false;
    }
    if (!JsonBuffer_Put_NTLVList(msg, "Event_Metadata", submit->pEventMetadata)) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to put Event_Metadata", __func__);
        json_object_put(msg);
        return false;
    }

    if (!JsonBuffer_Put_Block(msg, "Block", submit->pBlock)) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to put Block", __func__);
        json_object_put(msg);
        return false;
    }
    if (!JsonBuffer_Put_uint8List(msg, "Avaliable_Localities", submit->localityList, submit->localityCount)) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to put Avaliable_Localities", __func__);
        json_object_put(msg);
        return false;
    }

    wire = json_object_to_json_string(msg);
    message->length = strlen(wire);
    if ((message->serialized = calloc(message->length + 1, sizeof(uint8_t))) == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to allocate serialized message", __func__);
        json_object_put(msg);
        return false;
    }
    strcpy((char *) message->serialized, wire);
    json_object_put(msg);
    return true;
}