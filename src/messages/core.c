#include "config.h"
#include <razorback/debug.h>
#include <razorback/messages.h>
#include <razorback/log.h>
#include <razorback/list.h>

#include "messages/core.h"
#include "messages/cnc/core.h"

#include <string.h>

static int MessageHeader_KeyCmp(void *a, void *id);
static int MessageHeader_Cmp(void *a, void *b);
static void MessageHeader_Delete(void *a);

struct Message *
Message_Create(uint32_t type, uint32_t version, size_t msgSize)
{
    struct Message *message;
    if ((message = calloc(1,sizeof(struct Message))) == NULL)
        return NULL;
    message->type = type;
    message->version = version;
    if (msgSize > 0)
    {
        if ((message->message = calloc(1,msgSize)) == NULL)
            return NULL;
    }
    message->headers = Message_Header_List_Create();
    return message;
}

void
Message_Destroy(struct Message *message)
{
    ASSERT(message != NULL);
    if (message == NULL)
        return;

    if (message->message != NULL)
        free(message->message);

    if (message->serialized != NULL)
        free(message->serialized);

    List_Destroy(message->headers);
    free(message);
}

SO_PUBLIC bool
Message_Add_Header(struct List *headers, const char *p_sName, const char *p_sValue)
{
    struct MessageHeader *l_pHeader;
    if ((l_pHeader = calloc(1, sizeof (struct MessageHeader))) == NULL)
    {
        rzb_log(LOG_ERR, "%s: Failed to alloc new header", __func__);
        return false;
    }
    if ((l_pHeader->sName = calloc(1, strlen(p_sName)+1)) == NULL)
    {
        rzb_log(LOG_ERR, "%s: Failed to alloc new header name", __func__);
        free(l_pHeader);
        return false;
    }
    if ((l_pHeader->sValue = calloc(1, strlen(p_sValue)+1)) == NULL)
    {
        rzb_log(LOG_ERR, "%s: Failed to alloc new header value", __func__);
        free(l_pHeader->sName);
        free(l_pHeader);
        return false;
    }
    strcpy(l_pHeader->sName, p_sName);
    strcpy(l_pHeader->sValue, p_sValue);
    List_Push(headers, l_pHeader);
    return true;
}


SO_PUBLIC struct List *
Message_Header_List_Create(void)
{
    return List_Create(LIST_MODE_GENERIC, 
            MessageHeader_Cmp,
            MessageHeader_KeyCmp,
            MessageHeader_Delete,
            NULL);
}

static int 
MessageHeader_KeyCmp(void *a, void *id)
{
    struct MessageHeader *item = (struct MessageHeader *)a;
    char *key = id;
    return strcmp(item->sName, key);
}

static int
MessageHeader_Cmp(void *a, void *b)
{
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
MessageHeader_Delete(void *a)
{
    struct MessageHeader *header = a;
    free(header->sName);
    free(header->sValue);
    free(header);
}

bool
Message_Setup(struct Message *message)
{
    if ((message->type & MESSAGE_GROUP_C_AND_C) != 0)
        return Message_CnC_Setup(message);

    switch (message->type)
    {
    case MESSAGE_TYPE_REQ:
        MessageCacheReq_Setup(message);
        break;
    case MESSAGE_TYPE_RESP:
        MessageCacheResp_Setup(message);
        break;
    case MESSAGE_TYPE_BLOCK:
        MessageBlockSubmission_Setup(message);
        break;
    case MESSAGE_TYPE_JUDGMENT:
        MessageJudgmentSubmission_Setup(message);
        break;
    case MESSAGE_TYPE_INSPECTION:
        MessageInspectionSubmission_Setup(message);
        break;
    case MESSAGE_TYPE_LOG:
        MessageLogSubmission_Setup(message);
        break;
    default:
        return false;
    }
    return true;
}

