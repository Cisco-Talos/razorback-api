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

static bool RegistrationRequest_Deserialize(struct Message *message);
static bool RegistrationRequest_Serialize(struct Message *message);
static void RegistrationRequest_Destroy (struct Message *message);
static struct MessageHandler handler = {
    MESSAGE_TYPE_REG_REQ,
    RegistrationRequest_Serialize,
    RegistrationRequest_Deserialize,
    RegistrationRequest_Destroy
};

// core.h
void 
Message_CnC_RegReq_Init(void)
{
    Message_Register_Handler(&handler);
}

SO_PUBLIC struct Message *
MessageRegistrationRequest_Initialize (
                                       const uuid_t dispatcherId,
                                       const uuid_t p_uuidSourceNugget,
                                       const uuid_t p_uuidNuggetType,
                                       const uuid_t p_uuidApplicationType,
                                       uint32_t p_iDataTypeCount,
                                       uuid_t * p_pDataTypeList)
{
    uint32_t l_iIndex;
    struct Message *msg;
    struct MessageRegistrationRequest *message;

    if ((msg = Message_Create_Directed(MESSAGE_TYPE_REG_REQ, MESSAGE_VERSION_1, sizeof(struct MessageRegistrationRequest), p_uuidSourceNugget, dispatcherId)) == NULL)
        return NULL;
    message = msg->message;

    uuid_copy (message->uuidNuggetType, p_uuidNuggetType);
    uuid_copy (message->uuidApplicationType, p_uuidApplicationType);
    message->iDataTypeCount = p_iDataTypeCount;
    if (p_iDataTypeCount > 0)
    {
        if ((message->pDataTypeList =
             malloc (sizeof (uuid_t) * p_iDataTypeCount)) == NULL)
        {
            rzb_log (LOG_ERR, LOG_C_CORE,
                     "%s: failed due to lack of memory", __func__);
            RegistrationRequest_Destroy(msg);
            return NULL;
        }
    } 
    else
        message->pDataTypeList = NULL;

    for (l_iIndex = 0; l_iIndex < message->iDataTypeCount; l_iIndex++)
        uuid_copy (message->pDataTypeList[l_iIndex],
                   p_pDataTypeList[l_iIndex]);

    msg->destroy = RegistrationRequest_Destroy;
    msg->deserialize = RegistrationRequest_Deserialize;
    msg->serialize = RegistrationRequest_Serialize;
    return msg;

}

void
Message_CnC_RegReq_Setup(struct Message *msg)
{
    msg->destroy = RegistrationRequest_Destroy;
    msg->deserialize = RegistrationRequest_Deserialize;
    msg->serialize = RegistrationRequest_Serialize;
}

static void
RegistrationRequest_Destroy (struct Message *msg) {
    struct MessageRegistrationRequest *message;

    ASSERT (msg != NULL);
    if (msg == NULL) {
        rzb_log (LOG_ERR, LOG_C_CORE,
                 "%s: called with NULL msg", __func__);
        return;
    }
    message = msg->message;
    if (message != NULL) {
        if(message->pDataTypeList != NULL) {
            free(message->pDataTypeList);
        }
    }

    Message_Destroy(msg);
}


static bool
RegistrationRequest_Deserialize(struct Message *message)
{
    struct MessageRegistrationRequest *regReq;
    json_object *msg, *object, *item;
    const char *str;
	size_t i;

    ASSERT(message != NULL);
    if (message == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE,
                "%s: called with NULL message", __func__);
        return false;
    }

    if ((message->message = calloc(1,sizeof(struct MessageRegistrationRequest))) == NULL) {
        rzb_log (LOG_ERR, LOG_C_CORE,
                 "%s: failed due to lack of memory", __func__);
        return false;
    }

    if ((msg = json_tokener_parse((char *)message->serialized)) == NULL) {
        rzb_log (LOG_ERR, LOG_C_CORE,
                 "%s: failed due to failure of json_tokener_parse", __func__);
        return false;
    }
 
    regReq = message->message;

    if (!JsonBuffer_Get_UUID(msg, "Nugget_Type", regReq->uuidNuggetType)) {
        json_object_put(msg);
        rzb_log(LOG_ERR, LOG_C_CORE,
                "%s: failed due to failure of JsonBuffer_Get_UUID", __func__);
        return false;
    }
    if (!JsonBuffer_Get_UUID
            (msg, "App_Type", regReq->uuidApplicationType)) {
        json_object_put(msg);
        rzb_log(LOG_ERR, LOG_C_CORE,
                "%s: failed due to failure of JsonBuffer_Get_UUID", __func__);
        return false;
    }
    if ((object = json_object_object_get(msg, "Data_Types")) != NULL) {
        regReq->iDataTypeCount = json_object_array_length(object);
        if ((regReq->pDataTypeList = (uuid_t *) malloc(sizeof(uuid_t) * regReq->iDataTypeCount)) == NULL) {
            json_object_put(msg);
            rzb_log(LOG_ERR, LOG_C_CORE, "%s: failed due to lack of memory", __func__);
            return false;
        }

        for (i = 0; i < regReq->iDataTypeCount; i++) {
            item = json_object_array_get_idx(object, i);
            if (((str = json_object_get_string(item)) == NULL) ||
                (uuid_parse(str, regReq->pDataTypeList[i]))) {
                free(regReq->pDataTypeList);
                json_object_put(msg);
                rzb_log(LOG_ERR, LOG_C_CORE, "%s: failed due to failure of JsonBuffer_Get_UUID", __func__);
                return false;
            }

        }
    } else {
        regReq->pDataTypeList = NULL;
    }

    json_object_put(msg);
    return true;
}

static bool
RegistrationRequest_Serialize(struct Message *message)
{
    struct MessageRegistrationRequest *regReq;
    json_object *msg, *object, *item;
    const char * wire;
    char uuid[UUID_STRING_LENGTH];
	size_t i;
    ASSERT(message != NULL);
    if (message == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE,
                "%s: called with NULL message", __func__);
        return false;
    }

    regReq = message->message;

    if ((msg = json_object_new_object()) == NULL) {
        rzb_log (LOG_ERR, LOG_C_CORE,
                 "%s: failed due to failure of json_object_new_object", __func__);
        return false;
    }

    if (!JsonBuffer_Put_UUID
            (msg, "Nugget_Type", regReq->uuidNuggetType)) {
        json_object_put(msg);
        rzb_log(LOG_ERR, LOG_C_CORE,
                "%s: failed due to failure of JsonBuffer_Put_UUID", __func__);
        return false;
    }
    if (!JsonBuffer_Put_UUID
            (msg, "App_Type", regReq->uuidApplicationType)) {
        json_object_put(msg);
        rzb_log(LOG_ERR, LOG_C_CORE,
                "%s: failed due to failure of JsonBuffer_Put_UUID", __func__);
        return false;
    }
    if ((object = json_object_new_array()) == NULL) {
        json_object_put(msg);
        return false;
    }
    for (i = 0; i < regReq->iDataTypeCount; i++) {
        uuid_unparse(regReq->pDataTypeList[i], uuid);
        if ((item = json_object_new_string(uuid)) == NULL) {
            json_object_put(msg);
            return false;
        }
        json_object_array_add(object, item);
    }
    json_object_object_add(msg, "Data_Types", object);

    wire = json_object_to_json_string(msg);
    message->length = strlen(wire);
    if ((message->serialized = calloc(message->length + 1, sizeof(uint8_t))) == NULL) {
        rzb_log (LOG_ERR, LOG_C_CORE,
                 "%s: failed due to lack of memory", __func__);
        json_object_put(msg);
        return false;
    }
    strcpy((char *) message->serialized, wire);
    json_object_put(msg);


    return true;
}