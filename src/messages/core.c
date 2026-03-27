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
#include <razorback/telemetry.h>

#include "messages/core.h"
#include "messages/cnc/core.h"
#include "init.h"

#include <string.h>
static List_t * handlerList = NULL;
static bool Message_Add_Directed_Headers(struct Message *message, const uuid_t source, const uuid_t dest);


static int MessageHeader_KeyCmp(void *a, const void *id);
static int MessageHeader_Cmp(void *a, void *b);
static void MessageHeader_Delete(void *a);

static int MessageHandler_KeyCmp(void *a, const void *id);
static int MessageHandler_Cmp(void *a, void *b);
static void MessageHandler_Delete(void *a);

struct Message *
Message_Create(uint32_t type, uint32_t version, size_t msgSize) {
    struct Message *message;
    if ((message = calloc(1,sizeof(struct Message))) == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to alloc new message", __func__);
        return NULL;
    }
    message->type = type;
    message->version = version;
    if (msgSize > 0) {
        if ((message->message = calloc(1, msgSize)) == NULL) {
            rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to alloc new message data", __func__);
            free(message);
            return NULL;
        }
    }
    message->headers = Message_Header_List_Create();
    return message;
}

bool
Message_SetSerializedJson(struct Message *message, const char *wire,
                         int logComponent, const char *caller)
{
    uint8_t *serialized;
    size_t length;
    const char *logCaller = caller != NULL ? caller : __func__;

    ASSERT(message != NULL);
    ASSERT(wire != NULL);
    ASSERT(message == NULL || message->serialized == NULL);
    if (message == NULL || wire == NULL) {
        rzb_log(LOG_ERR, logComponent, "%s: NULL message or wire", logCaller);
        return false;
    }
    if (message->serialized != NULL) {
        rzb_log(LOG_ERR, logComponent, "%s: message already has serialized content",
                logCaller);
        return false;
    }

    length = strlen(wire);
    if ((serialized = calloc(length + 1, sizeof(uint8_t))) == NULL) {
        rzb_log(LOG_ERR, logComponent, "%s: failed to allocate serialized message",
                logCaller);
        return false;
    }

    memcpy(serialized, wire, length + 1);
    message->serialized = serialized;
    message->length = length;
    return true;
}

SO_PUBLIC void
Message_Destroy(struct Message *message)
{
    ASSERT(message != NULL);
    if (message == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: NULL message", __func__);
        return;
    }

    if (message->message != NULL) {
        free(message->message);
    }

    if (message->serialized != NULL) {
        free(message->serialized);
    }

    if (message->headers != NULL) {
        List_Destroy(message->headers);
    }
    if (message->brokerSettlement != NULL) {
        free(message->brokerSettlement);
    }
    Telemetry_ClearContext(&message->telemetryContext);
    free(message);
}

struct MessageHeader *
Message_HeaderList_Add(List_t * headers, const char *p_sName, const char *p_sValue ){
    struct MessageHeader *l_pHeader;
    ASSERT(headers != NULL);
    ASSERT(p_sName != NULL);
    ASSERT(p_sValue != NULL);
    if (headers == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: NULL header list", __func__);
        return false;
    }
    if (p_sName == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: NULL header name", __func__);
        return false;
    }
    if (p_sValue == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: NULL header value", __func__);
        return false;
    }
    if ((l_pHeader = calloc(1, sizeof(struct MessageHeader))) == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to alloc new header", __func__);
        return false;
    }
    if ((l_pHeader->sName = strdup(p_sName)) == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to alloc new header name", __func__);
        free(l_pHeader);
        return false;
    }
    if ((l_pHeader->sValue = strdup(p_sValue)) == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to alloc new header value", __func__);
        free(l_pHeader->sName);
        free(l_pHeader);
        return false;
    }
    List_Push(headers, l_pHeader);

    return l_pHeader;
}

void * MessageHeader_Clone(void *o) {
    struct MessageHeader *orig = (struct MessageHeader *)o;
    struct MessageHeader *clone;
    ASSERT(orig != NULL);
    if (orig == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: NULL original header", __func__);
        return NULL;
    }
    if ((clone = calloc(1, sizeof(struct MessageHeader))) == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to alloc new header clone", __func__);
        return NULL;
    }
    if ((clone->sName = strdup(orig->sName)) == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to alloc new header name clone", __func__);
        free(clone);
        return NULL;
    }
    if ((clone->sValue = strdup(orig->sValue)) == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to alloc new header value clone", __func__);
        free(clone->sName);
        free(clone);
        return NULL;
    }
    return clone;
}

SO_PUBLIC bool
Message_Add_Header(struct Message *p_pMessage, const char *p_sName, const char *p_sValue) {
    struct MessageHeader *l_pHeader;
    ASSERT(p_pMessage != NULL);
    ASSERT(p_sName != NULL);
    ASSERT(p_sValue != NULL);
    if (p_pMessage == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: NULL message", __func__);
        return false;
    }
    if (p_sName == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: NULL header name", __func__);
        return false;
    }
    if (p_sValue == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: NULL header value", __func__);
        return false;
    }
    if ((l_pHeader = Message_HeaderList_Add(p_pMessage->headers, p_sName, p_sValue)) == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: (%p) Failed to add message header %s - %s", __func__, p_pMessage, p_sName, p_sValue);
        return false;
    }
    l_pHeader->pMessage = p_pMessage;
    return true;
}


SO_PUBLIC List_t *
Message_Header_List_Create(void) {
    return List_Create(LIST_MODE_GENERIC,
            MessageHeader_Cmp,
            MessageHeader_KeyCmp,
            MessageHeader_Delete,
            MessageHeader_Clone, NULL, NULL);
}

static int
MessageHeader_KeyCmp(void *a, const void *id) {
    struct MessageHeader *item = (struct MessageHeader *)a;
    const char *key = id;
    return strcmp(item->sName, key);
}

static int
MessageHeader_Cmp(void *a, void *b) {
    struct MessageHeader *iA = (struct MessageHeader *)a;
    struct MessageHeader *iB = (struct MessageHeader *)b;
    if (a == b)
        return 0;
    if ( (strcmp(iA->sName, iA->sName) == 0) ||
            ( strcmp(iA->sValue, iB->sValue) == 0 ))
    {
        return 0;
    }
    return -1;

}

static void
MessageHeader_Delete(void *a) {
    struct MessageHeader *header = a;
    ASSERT(header != NULL);
    if (header == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: NULL header", __func__);
        return;
    }
    free(header->sName);
    free(header->sValue);
    free(header);
}


bool
Message_Init() {
    handlerList = List_Create(LIST_MODE_GENERIC,
            MessageHandler_Cmp,
            MessageHandler_KeyCmp,
            MessageHandler_Delete,
            NULL, NULL, NULL);

    if (handlerList == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to create handler list", __func__);
        return false;
    }

    MessageBlockSubmission_Init();
    MessageCacheReq_Init();
    MessageCacheResp_Init();
    MessageInspectionSubmission_Init();
    MessageJudgmentSubmission_Init();
    MessageAlertPrimary_Init();
    MessageAlertChild_Init();
    MessageOutputEvent_Init();

    Message_CnC_Bye_Init();
    Message_CnC_CacheClear_Init();
    Message_CnC_ConfigAck_Init();
    Message_CnC_ConfigUpdate_Init();
    Message_CnC_Error_Init();
    Message_CnC_Go_Init();
    Message_CnC_Hello_Init();
    Message_CnC_Pause_Init();
    Message_CnC_Paused_Init();
    Message_CnC_RegReq_Init();
    Message_CnC_RegResp_Init();
    Message_CnC_Running_Init();
    Message_CnC_Term_Init();
    Message_CnC_ReReg_Init();

    return true;
}

bool
Message_Setup(struct Message *message) {
    struct MessageHandler *handler = NULL;
    ASSERT(message != NULL);
    if (message == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: NULL message", __func__);
        return false;
    }

    handler = List_Find(handlerList, &message->type);
    if (handler == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: No handler for message type %u", __func__, message->type);
        return false;
    }

    message->serialize = handler->serialize;
    message->deserialize = handler->deserialize;
    message->destroy = handler->destroy;

    return true;
}

bool Message_Register_Handler(struct MessageHandler *handler) {
    ASSERT(handler != NULL);
    if (handler == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: NULL handler", __func__);
        return false;
    }
    return List_Push(handlerList, handler);
}



static int
MessageHandler_KeyCmp(void *a, const void *id) {
    struct MessageHandler *item = (struct MessageHandler *)a;
    const uint32_t *key = id;
    return (item->type - *key);
}

static int
MessageHandler_Cmp(void *a, void *b) {
    struct MessageHandler *iA = (struct MessageHandler *)a;
    struct MessageHandler *iB = (struct MessageHandler *)b;
    if (a == b)
        return 0;
    return (iA->type - iB->type);
}

static void
MessageHandler_Delete(void *a)
{
    /* Handler storage is static; the list only owns the wrapper nodes. */
    (void)a;
}

static bool
Message_Add_Directed_Headers(struct Message *message, const uuid_t source, const uuid_t dest) {
    char uuid[UUID_STRING_LENGTH];
    ASSERT(message != NULL);
    if (message == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: NULL message", __func__);
        return false;
    }
    uuid_unparse(source, uuid);
    Message_Add_Header(message, MSG_CNC_HEADER_SOURCE, uuid);
    uuid_unparse(dest, uuid);
    Message_Add_Header(message, MSG_CNC_HEADER_DEST, uuid);

    return true;
}

SO_PUBLIC struct Message *
Message_Create_Directed(uint32_t type, uint32_t version,
        size_t msgSize, const uuid_t source, const uuid_t dest)
{
    struct Message *msg;
    if ((msg = Message_Create(type, version, msgSize)) == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to create message", __func__);
        return NULL;
    }

    if (!Message_Add_Directed_Headers(msg, source, dest)) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to add directed headers", __func__);
        Message_Destroy(msg);
        return NULL;
    }
    return msg;
}

SO_PUBLIC struct Message *
Message_Create_Broadcast(uint32_t type, uint32_t version,
        size_t msgSize, const uuid_t source) {
    struct Message *msg;
    uuid_t dest;
    uuid_clear(dest);

    if ((msg = Message_Create(type, version, msgSize)) == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to create message", __func__);
        return NULL;
    }

    if (!Message_Add_Directed_Headers(msg, source, dest)) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to add directed headers", __func__);
        Message_Destroy(msg);
        return NULL;
    }
    return msg;
}

SO_PUBLIC bool
Message_Serialize_Empty(struct Message *message) {
    ASSERT(message != NULL);
    if ( message == NULL ) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: NULL message", __func__);
        return false;
    }


    if ((message->serialized = calloc(3, sizeof(uint8_t))) == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to alloc serialized data", __func__);
        return false;
    }

    message->serialized[0]='{';
    message->serialized[1]='}';
    message->serialized[2]='\0';
    message->length=2;

    return true;
}

SO_PUBLIC bool
Message_Deserialize_Empty(struct Message *message) {
    ASSERT(message != NULL);
    if ( message == NULL ) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: NULL message", __func__);
        return false;
    }
    return true;
}

SO_PUBLIC bool
Message_Get_Nuggets(struct Message *message, uuid_t source, uuid_t dest) {
    struct MessageHeader *header;

    ASSERT(message != NULL);
    if (message == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: NULL message", __func__);
        return false;
    }

    if ((header = List_Find(message->headers, MSG_CNC_HEADER_DEST)) == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: No destination header", __func__);
        return false;
    }

    if (uuid_parse(header->sValue, dest) != 0) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to parse destination UUID", __func__);
        return false;
    }

    if ((header = List_Find(message->headers, MSG_CNC_HEADER_SOURCE)) == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: No source header", __func__);
        return false;
    }

    if (uuid_parse(header->sValue, source) != 0) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to parse source UUID", __func__);
        return false;
    }

    return true;
}
