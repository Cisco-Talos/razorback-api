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
#include <razorback/block_id.h>
#include <razorback/json_buffer.h>

#include "messages/core.h"

#include <string.h>

static void CacheReq_Destroy (struct Message *message);
static bool CacheReq_Deserialize(struct Message *message);
static bool CacheReq_Serialize(struct Message *message);

static struct MessageHandler handler = {
    MESSAGE_TYPE_REQ,
    CacheReq_Serialize,
    CacheReq_Deserialize,
    CacheReq_Destroy
};

// core.h
void
MessageCacheReq_Init(void) {
    Message_Register_Handler(&handler);
}

SO_PUBLIC struct Message *
MessageCacheReq_Initialize (const uuid_t p_uuidRequestor,
                            const struct BlockId *p_pBlockId) {
    struct Message *msg;
    struct MessageCacheReq *message;

    ASSERT (p_pBlockId != NULL);

    if (p_pBlockId == NULL) {
        rzb_log (LOG_ERR, LOG_C_CORE,
                 "%s: failed due to NULL p_pBlockId", __func__);
        return NULL;
    }

    if ((msg = Message_Create(MESSAGE_TYPE_REQ, MESSAGE_VERSION_1, sizeof(struct MessageCacheReq))) == NULL) {
        rzb_log (LOG_ERR, LOG_C_CORE,
                 "%s: failed due to failure of Message_Create", __func__);
        return NULL;
    }

    message = msg->message;

    // fill in rest of message
    uuid_copy (message->uuidRequestor, p_uuidRequestor);
    if ((message->pId = BlockId_Clone(p_pBlockId)) == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE,
                "%s: failed due to failure of BlockId_Clone", __func__);
        CacheReq_Destroy(msg);
        return NULL;
    }
    msg->destroy=CacheReq_Destroy;
    msg->deserialize=CacheReq_Deserialize;
    msg->serialize=CacheReq_Serialize;

    // done
    return msg;
}

static void
CacheReq_Destroy (struct Message *message) {
    struct MessageCacheReq *msg;
    ASSERT (message != NULL);
    if (message == NULL) {
        rzb_log (LOG_ERR, LOG_C_CORE,
                 "%s: failed due to NULL message", __func__);
        return;
    }
    msg = message->message;

    // destroy any malloc'd components
    if (msg != NULL) {
        if (msg->pId != NULL) {
            BlockId_Destroy(msg->pId);
        }
    }

    Message_Destroy(message);
}

static bool
CacheReq_Deserialize(struct Message *message) {
    struct MessageCacheReq *submit;
    json_object *msg;

    ASSERT(message != NULL);
    if (message == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE,
                "%s: failed due to NULL message", __func__);
        return false;
    }

    if ((message->message = calloc(1,sizeof(struct MessageCacheReq))) == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE,
                "%s: failed due to failure of calloc", __func__);
        return false;
    }

    if ((msg = json_tokener_parse((char *)message->serialized)) == NULL) {
        rzb_log (LOG_ERR, LOG_C_CORE,
                 "%s: failed due to failure of json_tokener_parse", __func__);
        return false;
    }

    submit = message->message;

    if (!JsonBuffer_Get_UUID(msg, "Requestor", submit->uuidRequestor)) {
        json_object_put(msg);
        rzb_log(LOG_ERR, LOG_C_CORE,
                "%s: failed due to failure of JsonBuffer_Get_UUID", __func__);
        return false;
    }
    if (!JsonBuffer_Get_BlockId(msg, "Block_ID", &submit->pId)) {
        json_object_put(msg);
        rzb_log(LOG_ERR, LOG_C_CORE,
                "%s: failed due to failure of JsonBuffer_GetBlockId", __func__);
        return false;
    }
    json_object_put(msg);

    return true;
}


static bool
CacheReq_Serialize(struct Message *message)
{
    struct MessageCacheReq *submit;
    json_object *msg;
    const char * wire;

    ASSERT(message != NULL);
    if (message == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE,
                "%s: failed due to NULL message", __func__);
        return false;
    }

    submit = message->message;

    if ((msg = json_object_new_object()) == NULL) {
        rzb_log (LOG_ERR, LOG_C_CORE,
                 "%s: failed due to failure of json_object_new_object", __func__);
        return false;
    }

    if (!JsonBuffer_Put_UUID(msg, "Requestor", submit->uuidRequestor)) {
        json_object_put(msg);
        rzb_log(LOG_ERR, LOG_C_CORE,
                "%s: failed due to failure of JsonBuffer_Put_UUID", __func__);
        return false;
    }

    if (!JsonBuffer_Put_BlockId(msg, "Block_ID", submit->pId)) {
        json_object_put(msg);
        rzb_log(LOG_ERR, LOG_C_CORE,
                "%s: failed due to failure of JsonBuffer_Put_BlockId", __func__);
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
