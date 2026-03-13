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


static bool Terminate_Deserialize(struct Message *message);
static bool Terminate_Serialize(struct Message *message);
static void Terminate_Destroy (struct Message *message);
static struct MessageHandler handler = {
    MESSAGE_TYPE_TERM,
    Terminate_Serialize,
    Terminate_Deserialize,
    Terminate_Destroy
};

// core.h
void 
Message_CnC_Term_Init(void)
{
    Message_Register_Handler(&handler);
}

SO_PUBLIC struct Message *
MessageTerminate_Initialize (
                             const uuid_t p_uuidSourceNugget,
                             const uuid_t p_uuidDestNugget,
                             const uint8_t * p_sTerminateReason)
{
    struct Message *msg;
    struct MessageTerminate *message;
    ASSERT (p_sTerminateReason != NULL);
    if (p_sTerminateReason == NULL) {
        rzb_log (LOG_ERR, LOG_C_CORE,
                 "%s: failed due to NULL p_sTerminateReason", __func__);
        return NULL;
    }

    msg = Message_Create_Directed(
        MESSAGE_TYPE_TERM,
        MESSAGE_VERSION_1,
        sizeof(struct MessageTerminate),
        p_uuidSourceNugget,
        p_uuidDestNugget
    );
    if (msg == NULL) {
        rzb_log (LOG_ERR, LOG_C_CORE,
                 "%s: failed due to failure of Message_Create_Directed", __func__);
        return NULL;
    }
    message = msg->message;

    if ((message->sTerminateReason =
                 malloc(strlen((const char *) p_sTerminateReason) + 1)) == NULL) {
        Terminate_Destroy(msg);
        rzb_log(LOG_ERR, LOG_C_CORE,
                "%s: failed due to lack of memory", __func__);
        return NULL;
    }
    strcpy((char *) message->sTerminateReason,(const char *) p_sTerminateReason);

    msg->destroy = Terminate_Destroy;
    msg->deserialize = Terminate_Deserialize;
    msg->serialize = Terminate_Serialize;
    return msg;
}

void 
Message_CnC_Term_Setup(struct Message *msg)
{
    msg->destroy=Terminate_Destroy;
    msg->deserialize=Terminate_Deserialize;
    msg->serialize=Terminate_Serialize;
}

static void
Terminate_Destroy (struct Message *msg)
{
    struct MessageTerminate *message;
    ASSERT (msg != NULL);
    if (msg == NULL) {
        rzb_log (LOG_ERR, LOG_C_CORE,
                 "%s: failed due to NULL msg", __func__);
        return;
    }
    message = msg->message;
    if (message != NULL) {
        if (message->sTerminateReason != NULL) {
            free(message->sTerminateReason);
        }
    }

    Message_Destroy(msg);
}


static bool
Terminate_Deserialize(struct Message *message)
{
    struct MessageTerminate *term;
    json_object *msg;
    
    ASSERT(message != NULL);
    if (message == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE,
                "%s: failed due to NULL message", __func__);
        return false;
    }

    if ((message->message = calloc(1,sizeof(struct MessageTerminate))) == NULL) {
        rzb_log (LOG_ERR, LOG_C_CORE,
                 "%s: failed due to lack of memory", __func__);
        return false;
    }

    
    if ((msg = json_tokener_parse((char *)message->serialized)) == NULL) {
        rzb_log (LOG_ERR, LOG_C_CORE,
                 "%s: failed due to failure of json_tokener_parse", __func__);
        return false;
    }

    term = message->message;

    if ((term->sTerminateReason = (uint8_t *) JsonBuffer_Get_String(msg, "Reason")) == NULL) {
        json_object_put(msg);
        rzb_log(LOG_ERR, LOG_C_CORE,
                "%s: failed due to failure of JsonBuffer_Get_String", __func__);
        return false;
    }
    json_object_put(msg);
    return true;
}

static bool
Terminate_Serialize(struct Message *message)
{
    struct MessageTerminate *term;
    json_object *msg;
    const char * wire;

    ASSERT(message != NULL);
    if (message == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE,
                "%s: failed due to NULL message", __func__);
        return false;
    }

    term = message->message;

    if ((msg = json_object_new_object()) == NULL) {
        rzb_log (LOG_ERR, LOG_C_CORE,
                 "%s: failed due to failure of json_object_new_object", __func__);
        return false;
    }


    if (!JsonBuffer_Put_String
            (msg, "Reason", (char *) term->sTerminateReason)) {
        json_object_put(msg);
        rzb_log(LOG_ERR, LOG_C_CORE,
                "%s: failed due to failure of JsonBuffer_Put_String", __func__);
        return false;
    }

    wire = json_object_to_json_string(msg);
    message->length = strlen(wire);
    if ((message->serialized = calloc(message->length + 1, sizeof(uint8_t))) == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE,
                "%s: failed to allocate serialized message", __func__);
        json_object_put(msg);
        return false;
    }
    strcpy((char *)message->serialized, wire); 
    json_object_put(msg);

    return true;
}