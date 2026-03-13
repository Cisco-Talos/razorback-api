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
#include <razorback/json_buffer.h>

#include "messages/core.h"
#include "messages/cnc/core.h"

#include <string.h>

static bool ConfigAck_Deserialize(struct Message *message);
static bool ConfigAck_Serialize(struct Message *message);

static struct MessageHandler handler = {
    MESSAGE_TYPE_CONFIG_ACK,
    ConfigAck_Serialize,
    ConfigAck_Deserialize,
    Message_Destroy
};

// core.h
void
Message_CnC_ConfigAck_Init(void) {
    Message_Register_Handler(&handler);
}

SO_PUBLIC struct Message *
MessageConfigurationAck_Initialize (
        const uuid_t p_uuidSourceNugget,
        const uuid_t p_uuidDestNugget,
        const uuid_t p_uuidNuggetType,
        const uuid_t p_uuidApplicationType
        ) {
    struct Message *msg;
    struct MessageConfigurationAck * message;

    msg = Message_Create_Directed(
        MESSAGE_TYPE_CONFIG_ACK,
        MESSAGE_VERSION_1,
        sizeof(struct MessageConfigurationAck),
        p_uuidSourceNugget,
        p_uuidDestNugget
    );
    if (msg == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: failed to create message", __func__);
        return NULL;
    }
    message = msg->message;

    uuid_copy (message->uuidNuggetType, p_uuidNuggetType);
    uuid_copy (message->uuidApplicationType, p_uuidApplicationType);
    msg->destroy = Message_Destroy;
    msg->deserialize = ConfigAck_Deserialize;
    msg->serialize = ConfigAck_Serialize;
    return msg;
}


static bool
ConfigAck_Deserialize(struct Message *message) {
    struct MessageConfigurationAck *configAck;
    json_object *msg;

    ASSERT(message != NULL);
    if (message == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: message is NULL", __func__);
        return false;
    }

    if ((message->message = calloc(1,sizeof(struct MessageConfigurationAck))) == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: failed to allocate message", __func__);
        return false;
    }

    if ((msg = json_tokener_parse((char *)message->serialized)) == NULL) {
        rzb_log (LOG_ERR, LOG_C_CORE,
                 "%s: failed due to failure of json_tokener_parse", __func__);
        return false;
    }

    configAck = message->message;

    if (!JsonBuffer_Get_UUID(msg, "Nugget_Type", configAck->uuidNuggetType)) {
        json_object_put(msg);
        rzb_log(LOG_ERR, LOG_C_CORE,
                "%s: failed due to failure of JsonBuffer_Get_UUID", __func__);
        return false;
    }
    if (!JsonBuffer_Get_UUID(msg, "App_Type", configAck->uuidApplicationType)) {
        json_object_put(msg);
        rzb_log(LOG_ERR, LOG_C_CORE,
                "%s: failed due to failure of JsonBuffer_Get_UUID", __func__);
        return false;
    }
    json_object_put(msg);
    return true;
}

static bool
ConfigAck_Serialize(struct Message *message) {
    struct MessageConfigurationAck *configAck;
    json_object *msg;
    const char * wire;

    ASSERT(message != NULL);
    if (message == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: message is NULL", __func__);
        return false;
    }

    configAck = message->message;

    if ((msg = json_object_new_object()) == NULL) {
        rzb_log (LOG_ERR, LOG_C_CORE,
                 "%s: failed due to failure of json_object_new_object", __func__);
        return false;
    }


    if (!JsonBuffer_Put_UUID
            (msg, "Nugget_Type", configAck->uuidNuggetType)) {
        json_object_put(msg);
        rzb_log(LOG_ERR, LOG_C_CORE,
                "%s: failed due to failure of JsonBuffer_Put_UUID ( Nug Type )", __func__);
        return false;
    }

    if (!JsonBuffer_Put_UUID
            (msg, "App_Type", configAck->uuidApplicationType)) {
        json_object_put(msg);
        rzb_log(LOG_ERR, LOG_C_CORE,
                "%s: failed due to failure of JsonBuffer_Put_UUID ( App Type) ", __func__);
        return false;
    }
    wire = json_object_to_json_string(msg);
    message->length = strlen(wire);
    if ((message->serialized = calloc(message->length + 1, sizeof(uint8_t))) == NULL) {
        json_object_put(msg);
        return false;
    }
    strcpy((char *)message->serialized, wire);
    json_object_put(msg);

    return true;
}