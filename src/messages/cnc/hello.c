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
#include <razorback/uuids.h>
#include <razorback/json_buffer.h>
#include <razorback/list.h>

#include "messages/core.h"
#include "messages/cnc/core.h"

#include <string.h>

static bool Hello_Deserialize(struct Message *message);
static bool Hello_Serialize(struct Message *message);
static void Hello_Destroy (struct Message *message);

static struct MessageHandler handler = {
    MESSAGE_TYPE_HELLO,
    Hello_Serialize,
    Hello_Deserialize,
    Hello_Destroy
};

// core.h
void
Message_CnC_Hello_Init(void) {
    Message_Register_Handler(&handler);
}


static bool
Hello_Deserialize(struct Message *message) {
    struct MessageHello *hello;
    json_object *msg;
    uuid_t dispatcher;

    UUID_Get_UUID(NUGGET_TYPE_DISPATCHER, UUID_TYPE_NUGGET_TYPE, dispatcher);

    ASSERT(message != NULL);
    if (message == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: message is NULL", __func__);
        return false;
    }

    if ((message->message = calloc(1,sizeof(struct MessageHello))) == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to allocate memory for MessageHello", __func__);
        return false;
    }

    if ((msg = json_tokener_parse((char *)message->serialized)) == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: failed to parse json: %s", __func__, message->serialized);
        return false;
    }


    hello = message->message;

    if (!JsonBuffer_Get_UUID
            (msg, "Nugget_Type", hello->uuidNuggetType)) {
        json_object_put(msg);
        rzb_log(LOG_ERR, LOG_C_CORE,
                "%s: failed due to failure of JsonBuffer_Get_UUID", __func__);
        return false;
    }

    if (!JsonBuffer_Get_UUID(msg, "App_Type", hello->uuidApplicationType)) {
        json_object_put(msg);
        rzb_log(LOG_ERR, LOG_C_CORE,
                "%s: failed due to failure of JsonBuffer_Get_UUID", __func__);
        return false;
    }
    if (!JsonBuffer_Get_uint8_t(msg, "Locality", &hello->locality)) {
        json_object_put(msg);
        rzb_log(LOG_ERR, LOG_C_CORE,
                "%s: failed due to failure of JsonBuffer_Get_UUID", __func__);
        return false;
    }
    if (uuid_compare(hello->uuidNuggetType, dispatcher) == 0) {
        if (!(JsonBuffer_Get_uint8_t(msg, "Priority", &hello->priority) &&
              JsonBuffer_Get_uint8_t(msg, "Protocol", &hello->protocol) &&
              JsonBuffer_Get_uint16_t(msg, "Port", &hello->port) &&
              JsonBuffer_Get_uint32_t(msg, "Flags", &hello->flags))) {
            json_object_put(msg);
            rzb_log(LOG_ERR, LOG_C_CORE,
                    "%s: failed due to failure of JsonBuffer_Get_uint8", __func__);
            return false;
        }
        // Address List
        if (!JsonBuffer_Get_StringList(msg, "Address_List", &hello->addressList)) {
            json_object_put(msg);
            rzb_log(LOG_ERR, LOG_C_CORE,
                    "%s: failed due to failure of JsonBuffer_Get_StringList", __func__);
            return false;
        }

    }
    json_object_put(msg);

    return true;
}


SO_PUBLIC struct Message *
MessageHello_Initialize (struct RazorbackContext *context)
{
    struct Message * msg;
    struct MessageHello *message;
    uuid_t dispatcher;
    if (!UUID_Get_UUID(NUGGET_TYPE_DISPATCHER, UUID_TYPE_NUGGET_TYPE, dispatcher)) {
        rzb_log (LOG_ERR, LOG_C_CORE,
                 "%s: failed to get dispatcher nugget type uuid", __func__);
        return NULL;
    }

    msg = Message_Create_Broadcast(
        MESSAGE_TYPE_HELLO,
        MESSAGE_VERSION_1,
        sizeof(struct MessageHello),
        context->uuidNuggetId
    );
    if (msg == NULL) {
        rzb_log (LOG_ERR, LOG_C_CORE,
                 "%s: failed to create message", __func__);
        return NULL;
    }

    message = msg->message;

    uuid_copy (message->uuidNuggetType, context->uuidNuggetType);
    uuid_copy (message->uuidApplicationType, context->uuidApplicationType);
    message->locality = context->locality;
    if (uuid_compare(dispatcher, context->uuidNuggetType) == 0) {
        message->flags = context->dispatcher.flags;
        message->priority = context->dispatcher.priority;
        message->port = context->dispatcher.port;
        message->protocol = context->dispatcher.protocol;
        if ((message->addressList = List_Clone(context->dispatcher.addressList)) == NULL) {
            Hello_Destroy(msg);
            return NULL;
        }
    }

    msg->destroy = Hello_Destroy;
    msg->deserialize = Hello_Deserialize;
    msg->serialize = Hello_Serialize;

    return msg;
}

static void
Hello_Destroy (struct Message *msg)
{
    struct MessageHello *message;

    ASSERT (msg != NULL);
    if (msg == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: msg is NULL", __func__);
        return;
    }
    message = msg->message;
    if (message != NULL) {
        if (message->addressList != NULL) {
            List_Destroy(message->addressList);
        }
    }

    Message_Destroy(msg);
}


static bool
Hello_Serialize(struct Message *message)
{
    struct MessageHello *hello;
    json_object *msg;
    const char * wire;
    uuid_t dispatcher;
    UUID_Get_UUID(NUGGET_TYPE_DISPATCHER, UUID_TYPE_NUGGET_TYPE, dispatcher);

    ASSERT(message != NULL);
    if (message == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: message is NULL", __func__);
        return false;
    }

    hello = message->message;

    if ((msg = json_object_new_object()) == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: failed to create new json object", __func__);
        return false;
    }

    if (!JsonBuffer_Put_UUID
            (msg, "Nugget_Type", hello->uuidNuggetType)) {
        json_object_put(msg);
        rzb_log(LOG_ERR, LOG_C_CORE,
                "%s: failed due to failure of JsonBuffer_Put_UUID", __func__);
        return false;
    }
    if (!JsonBuffer_Put_UUID
            (msg, "App_Type", hello->uuidApplicationType)) {
        json_object_put(msg);
        rzb_log(LOG_ERR, LOG_C_CORE,
                "%s: failed due to failure of JsonBuffer_Put_UUID", __func__);
        return false;
    }

    if (!JsonBuffer_Put_uint8_t(msg, "Locality", hello->locality)) {
        json_object_put(msg);
        rzb_log(LOG_ERR, LOG_C_CORE,
                "%s: failed due to failure of JsonBuffer_Get_UUID", __func__);
        return false;
    }
    if (uuid_compare(hello->uuidNuggetType, dispatcher) == 0) {
        if (!(JsonBuffer_Put_uint8_t(msg, "Priority", hello->priority) &&
              JsonBuffer_Put_uint8_t(msg, "Protocol", hello->protocol) &&
              JsonBuffer_Put_uint16_t(msg, "Port", hello->port) &&
              JsonBuffer_Put_uint32_t(msg, "Flags", hello->flags))) {
            json_object_put(msg);
            rzb_log(LOG_ERR, LOG_C_CORE,
                    "%s: failed due to failure of JsonBuffer_Put_uint8", __func__);
            return false;
        }
        // Address List
        if (!JsonBuffer_Put_StringList(msg, "Address_List", hello->addressList)) {
            json_object_put(msg);
            rzb_log(LOG_ERR, LOG_C_CORE,
                    "%s: failed due to failure of JsonBuffer_Put_StringList", __func__);
            return false;
        }

    }

    wire = json_object_to_json_string(msg);
    if (!Message_SetSerializedJson(message, wire, LOG_C_CORE, __func__)) {
        json_object_put(msg);
        return false;
    }
    json_object_put(msg);

    return true;
}
