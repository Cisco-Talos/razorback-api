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
#include <razorback/list.h>
#include <razorback/uuids.h>
#include <razorback/json_buffer.h>


#include "messages/core.h"
#include "messages/cnc/core.h"


#include <string.h>

static bool ConfigUpdate_Deserialize(struct Message *message);
static void ConfigUpdate_Destroy (struct Message *message);
static bool ConfigUpdate_Serialize(struct Message *message);

static struct MessageHandler handler = {
    MESSAGE_TYPE_CONFIG_UPDATE,
    ConfigUpdate_Serialize,
    ConfigUpdate_Deserialize,
    ConfigUpdate_Destroy
};

// core.h
void 
Message_CnC_ConfigUpdate_Init(void) {
    Message_Register_Handler(&handler);
}

SO_PUBLIC struct Message *
MessageConfigurationUpdate_Initialize (
                                       const uuid_t p_uuidSourceNugget,
                                       const uuid_t p_uuidDestNugget)
{
    struct MessageConfigurationUpdate *message;
    struct Message *msg;
    List_t *list;


    msg = Message_Create_Directed(
        MESSAGE_TYPE_CONFIG_UPDATE,
        MESSAGE_VERSION_1,
        sizeof(struct MessageConfigurationUpdate),
        p_uuidSourceNugget,
        p_uuidDestNugget
    );
    if (msg == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to create message", __func__);
        return NULL;
    }
    message = msg->message;

    if ((list = UUID_Get_List(UUID_TYPE_NTLV_TYPE)) == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to get NTLV Types", __func__);
        return false;
    }
    if ((message->ntlvTypes= List_Clone(list)) == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to clone NTLV Types", __func__);
        return false;
    }
    if ((list = UUID_Get_List(UUID_TYPE_NTLV_NAME)) == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to get NTLV Names", __func__);
        return false;
    }
    if ((message->ntlvNames= List_Clone(list)) == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to clone NTLV Names", __func__);
        return false;
    }
    if ((list = UUID_Get_List(UUID_TYPE_DATA_TYPE)) == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to get Data Types", __func__);
        return false;
    }
    if ((message->dataTypes= List_Clone(list)) == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to clone Data Types", __func__);
        return false;
    }

    
    msg->destroy = ConfigUpdate_Destroy;
    msg->deserialize = ConfigUpdate_Deserialize;
    msg->serialize = ConfigUpdate_Serialize;
    return msg;
}

static void
ConfigUpdate_Destroy (struct Message *msg) {
    struct MessageConfigurationUpdate *message;
    ASSERT(msg != NULL);
    if (msg == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: msg is NULL", __func__);
        return;
    }
    message = msg->message;
    if (message->ntlvTypes != NULL) {
        List_Destroy(message->ntlvTypes);
    }
    if (message->ntlvNames != NULL) {
        List_Destroy(message->ntlvNames);
    }
    if (message->dataTypes != NULL) {
        List_Destroy(message->dataTypes);
    }

    Message_Destroy(msg);
}

static bool
ConfigUpdate_Deserialize(struct Message *message)
{
    struct MessageConfigurationUpdate *configUpdate;
    json_object *msg;
    //uint32_t size; 
    ASSERT(message != NULL);
    if (message == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: message is NULL", __func__);
        return false;
    }

    if ((message->message = calloc(1,sizeof(struct MessageConfigurationUpdate))) == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to allocate message", __func__);
        return false;
    }

    if ((msg = json_tokener_parse((char *)message->serialized)) == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to parse JSON", __func__);
        return false;
    }
    
    configUpdate = message->message;
    if (!JsonBuffer_Get_UUIDList(msg, "NTLV_Types", &configUpdate->ntlvTypes)) {
        json_object_put(msg);
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to get NTLV Types", __func__);
        return false;
    }
    if (!JsonBuffer_Get_UUIDList(msg, "NTLV_Names", &configUpdate->ntlvNames)) {
        json_object_put(msg);
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to get NTLV Names", __func__);
        return false;
    }
    if (!JsonBuffer_Get_UUIDList(msg, "Data_Types", &configUpdate->dataTypes)) {
        json_object_put(msg);
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to get Data Types", __func__);
        return false;
    }
    json_object_put(msg);
    return true;
}

static bool
ConfigUpdate_Serialize(struct Message *message) {
    struct MessageConfigurationUpdate *configUpdate;
    const char * wire;
    json_object *msg;
    ASSERT(message != NULL);
    if (message == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: message is NULL", __func__);
        return false;
    }
    
    if ((msg = json_object_new_object()) == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to create JSON object", __func__);
        return false;
    }
   
    configUpdate = message->message;

    if (!JsonBuffer_Put_UUIDList(msg, "NTLV_Types", configUpdate->ntlvTypes)) {
        json_object_put(msg);
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to get NTLV Types", __func__);
        return false;
    }
    if (!JsonBuffer_Put_UUIDList(msg, "NTLV_Names", configUpdate->ntlvNames)) {
        json_object_put(msg);
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to get NTLV Names", __func__);
        return false;
    }
    if (!JsonBuffer_Put_UUIDList(msg, "Data_Types", configUpdate->dataTypes)) {
        json_object_put(msg);
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to get Data Types", __func__);
        return false;
    }
    wire = json_object_to_json_string(msg);
    message->length = strlen(wire);
    if ((message->serialized = calloc(message->length + 1, sizeof(uint8_t))) == NULL) {
        json_object_put(msg);
        return false;
    }
    strcpy((char *) message->serialized, wire);
    json_object_put(msg);

    return true;
}