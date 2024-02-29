#include "config.h"
#include <razorback/debug.h>
#include <razorback/block.h>
#include <razorback/block_id.h>
#include <razorback/messages.h>
#include <razorback/hash.h>
#include <razorback/ntlv.h>
#include <razorback/log.h>
#include <razorback/event.h>
#include <razorback/uuids.h>
#include <razorback/judgment.h>

#include "runtime_config.h"

#include <string.h>

SO_PUBLIC uint32_t
MessageHeader_BinaryLength (const struct MessageHeader * p_pHeader)
{
    ASSERT (p_pHeader != NULL);

    return (uint32_t) sizeof (p_pHeader->iType) +
        (uint32_t) sizeof (p_pHeader->iLength) +
        (uint32_t) sizeof (p_pHeader->iVersion);
}

SO_PUBLIC struct MessageCacheReq *
MessageCacheReq_Initialize (const uuid_t p_uuidRequestor,
                            const struct BlockId *p_pBlockId)
{
    ASSERT (p_pBlockId != NULL);
    struct MessageCacheReq *message;
    if ((message = calloc(1,sizeof(struct MessageCacheReq))) == NULL)
        return NULL;

    // calculate header
    message->mhHeader.iType = MESSAGE_TYPE_REQ;
    message->mhHeader.iLength =
        MessageHeader_BinaryLength (&message->mhHeader) +
        (uint32_t) sizeof (message->uuidRequestor) +
        BlockId_BinaryLength (p_pBlockId);
    message->mhHeader.iVersion = MESSAGE_VERSION_1;

    // fill in rest of message
    uuid_copy (message->uuidRequestor, p_uuidRequestor);
    if ((message->pId = BlockId_Clone (p_pBlockId)) == NULL)
    {
        rzb_log (LOG_ERR,
                 "%s: failed due to failure of BlockId_Clone", __func__);
        MessageCacheReq_Destroy(message);
        return NULL;
    }

    // done
    return message;
}

SO_PUBLIC void
MessageCacheReq_Destroy (struct MessageCacheReq *p_pMessage)
{
    ASSERT (p_pMessage != NULL);

    // destroy any malloc'd components
    if (p_pMessage->pId != NULL)
        BlockId_Destroy (p_pMessage->pId);

    free(p_pMessage);
}

SO_PUBLIC struct MessageCacheResp*
MessageCacheResp_Initialize (
                             const struct BlockId *p_pBlockId,
                             uint32_t p_iSfFlags, uint32_t p_iEntFlags)
{
    ASSERT (p_pBlockId != NULL);

    struct MessageCacheResp *message;
    if ((message = calloc(1, sizeof(struct MessageCacheResp))) == NULL)
        return NULL;

    // calculate header
    message->mhHeader.iType = MESSAGE_TYPE_RESP;
    message->mhHeader.iLength =
        MessageHeader_BinaryLength (&message->mhHeader) +
        BlockId_BinaryLength (p_pBlockId) +
        (sizeof(uint32_t)*2);
    message->mhHeader.iVersion = MESSAGE_VERSION_1;

    // fill in rest of message
    if ((message->pId = BlockId_Clone (p_pBlockId)) == NULL)
    {
        rzb_log (LOG_ERR,
                 "%s: failed due to failure of BlockId_Clone", __func__);
        MessageCacheResp_Destroy(message);
        return NULL;
    }
    message->iSfFlags = p_iSfFlags;
    message->iEntFlags = p_iEntFlags;
    
    return message;
}

SO_PUBLIC void
MessageCacheResp_Destroy (struct MessageCacheResp *message)
{
    ASSERT (message != NULL);

    // destroy any malloc'd components
    if (message->pId != NULL)
        BlockId_Destroy (message->pId);

    free(message);
}

SO_PUBLIC struct MessageBlockSubmission*
MessageBlockSubmission_Initialize (
                                   struct Event *p_pEvent,
                                   uint32_t p_iReason)
{
    ASSERT (p_pEvent != NULL);
    struct MessageBlockSubmission *message;

    if ((message = calloc(1,sizeof(struct MessageBlockSubmission))) == NULL)
        return NULL;

    message->pEvent = p_pEvent;

    // calculate header
	if (p_pEvent->pBlock->isStored != 1)
		message->mhHeader.iType = MESSAGE_TYPE_BLOCK;
	else 
		message->mhHeader.iType = MESSAGE_TYPE_TICKET;
    message->mhHeader.iLength =
        MessageHeader_BinaryLength (&message->mhHeader) +
        Event_BinaryLength (message->pEvent) +
        (uint32_t) sizeof (message->iReason);
    
    message->mhHeader.iVersion = MESSAGE_VERSION_1;

    message->iReason = p_iReason;

    return message;
}

SO_PUBLIC void
MessageBlockSubmission_Destroy (struct MessageBlockSubmission *message)
{
    ASSERT (message != NULL);

    // destroy any malloc'd components
    if (message->pEvent != NULL)
        Event_Destroy (message->pEvent);
    
    free(message);
}

SO_PUBLIC struct MessageJudgmentSubmission *
MessageJudgmentSubmission_Initialize (
                                      uint8_t p_iReason,
                                      struct Judgment *p_pJudgment)
{
    ASSERT (p_pJudgment != NULL);
    struct MessageJudgmentSubmission *message;
    if ((message = calloc(1, sizeof(struct MessageJudgmentSubmission))) == NULL)
        return NULL;

    message->pJudgment = p_pJudgment;

    // calculate header
    message->mhHeader.iType = MESSAGE_TYPE_JUDGMENT;
    message->mhHeader.iLength =
        MessageHeader_BinaryLength (&message->mhHeader) +
        (uint32_t) sizeof (message->iReason) +
        Judgment_BinaryLength(message->pJudgment);
    message->mhHeader.iVersion = MESSAGE_VERSION_1;

    message->iReason = p_iReason;

    return message;
}

SO_PUBLIC void
MessageJudgmentSubmission_Destroy (struct MessageJudgmentSubmission
                                   *message)
{
    ASSERT (message != NULL);
    
    if (message->pJudgment != NULL)
        Judgment_Destroy(message->pJudgment);

    free(message);
}

SO_PUBLIC struct MessageInspectionSubmission *
MessageInspectionSubmission_Initialize (
                                        const struct Event *p_pEvent,
                                        uint32_t p_iReason)
{
    ASSERT (p_pEvent != NULL);
    struct MessageInspectionSubmission *message;
    if ((message = calloc(1, sizeof(struct MessageInspectionSubmission))) == NULL)
        return NULL;
	
    if ((message->pBlock = Block_Clone(p_pEvent->pBlock)) == NULL)
    {
        rzb_log(LOG_ERR, "%s: Failed to clone block", __func__);
        MessageInspectionSubmission_Destroy(message);
        return NULL;
    }

	if (p_pEvent->pBlock->isStored == 1)
        message->mhHeader.iType = MESSAGE_TYPE_TICKET;
    else
        message->mhHeader.iType = MESSAGE_TYPE_INSPECTION;
        
    // calculate header
    message->mhHeader.iLength =
        MessageHeader_BinaryLength (&message->mhHeader) +
        Block_BinaryLength (message->pBlock) +
        NTLVList_Size (p_pEvent->pMetaDataList) +
        sizeof(struct EventId) + //Source nugget ID
        (uint32_t) sizeof (message->iReason);


    message->mhHeader.iVersion = MESSAGE_VERSION_1;

    message->iReason = p_iReason;
    if ((message->eventId = EventId_Clone(p_pEvent->pId)) == NULL)
    {
        rzb_log(LOG_ERR, "%s: Failed to clone event id", __func__);
        MessageInspectionSubmission_Destroy(message);
        return NULL;
    }
    if ((message->pEventMetadata = NTLVList_Create()) == NULL)
    {
        rzb_log(LOG_ERR, "%s: Failed to create metadata list", __func__);
        MessageInspectionSubmission_Destroy(message);
        return NULL;
    }
    NTLVList_Copy (message->pEventMetadata, p_pEvent->pMetaDataList);

    return message;
}

SO_PUBLIC void
MessageInspectionSubmission_Destroy (struct MessageInspectionSubmission
                                     *p_pMessage)
{
    ASSERT (p_pMessage != NULL);

    // destroy any malloc'd components
    if (p_pMessage->pBlock != NULL)
        Block_Destroy (p_pMessage->pBlock);
    if (p_pMessage->eventId != NULL)
        EventId_Destroy(p_pMessage->eventId);
    if (p_pMessage->pEventMetadata != NULL)
        NTLVList_Destroy(p_pMessage->pEventMetadata);
    free(p_pMessage);
}

SO_PUBLIC struct MessageAlert *
MessageAlert_Initialize (
                         const struct Block *p_pBlock,
                         uint32_t p_iDisposition,
                         const uuid_t p_uuidInspectorId)
{
    ASSERT (p_pBlock != NULL);
    struct MessageAlert *message;
    if (( message = calloc(1,sizeof(struct MessageAlert))) == NULL)
        return NULL;

    if ((message->pBlock= Block_Clone(p_pBlock)) == NULL)
    {
        rzb_log(LOG_ERR, "%s: Failed to clone message", __func__);
        MessageAlert_Destroy(message);
        return NULL;
    }
    // fill in rest of message
    // calculate header
    message->mhHeader.iType = MESSAGE_TYPE_ALERT;
    message->mhHeader.iLength =
        MessageHeader_BinaryLength (&message->mhHeader) +
        Block_BinaryLength (message->pBlock) +
        (uint32_t) sizeof (message->iDisposition) +
        (uint32_t) sizeof (message->uuidInspectorId);
    message->mhHeader.iVersion = MESSAGE_VERSION_1;

    message->iDisposition = p_iDisposition;
    uuid_copy (message->uuidInspectorId, p_uuidInspectorId);

    return message;
}

SO_PUBLIC void
MessageAlert_Destroy (struct MessageAlert *p_pMessage)
{
    ASSERT (p_pMessage != NULL);

    // destroy any malloc'd components
    Block_Destroy (p_pMessage->pBlock);

    free(p_pMessage);
}

SO_PUBLIC struct MessageLogSubmission *
MessageLog_Initialize (
                         const uuid_t p_uuidNuggetId,
                         uint8_t p_iPriority,
                         char *p_sMessage,
                         struct EventId *p_pEventId)
{
    ASSERT (p_sMessage != NULL);
    struct MessageLogSubmission *message;

    if ((message = calloc(1,sizeof(struct MessageLogSubmission))) == NULL)
        return NULL;


    if (p_pEventId != NULL)
    {
        if ((message->pEventId = EventId_Clone(p_pEventId)) == NULL)
        {
            rzb_log(LOG_ERR, "%s: Failed to clone event id.", __func__);
            MessageLog_Destroy(message);
            return NULL;
        }
    }

    // fill in rest of message
    // calculate header
    message->mhHeader.iType = MESSAGE_TYPE_LOG;
    message->mhHeader.iLength =
        MessageHeader_BinaryLength (&message->mhHeader) +
        (uint32_t) sizeof (message->iPriority) +
        (uint32_t) sizeof (message->uuidNuggetId) +
        strlen(p_sMessage) +1 +
        sizeof(uint8_t); // The event id flag.
    
    if (p_pEventId != NULL)
        message->mhHeader.iLength += sizeof(struct EventId);

    message->mhHeader.iVersion = MESSAGE_VERSION_1;

    message->iPriority = p_iPriority;
    uuid_copy (message->uuidNuggetId, p_uuidNuggetId);
    message->sMessage = (uint8_t *)p_sMessage;

    return message;
}

SO_PUBLIC void
MessageLog_Destroy (struct MessageLogSubmission *p_pMessage)
{
    ASSERT (p_pMessage != NULL);

    // destroy any malloc'd components
    if (p_pMessage->pEventId != NULL)
        EventId_Destroy (p_pMessage->pEventId);

    free(p_pMessage);
}

static uint32_t
CcMessageHeader_BinaryLength (struct CcMessageHeader *p_pHeader)
{
    ASSERT (p_pHeader != NULL);

    return MessageHeader_BinaryLength (&p_pHeader->mhHeader) +
        (uint32_t) sizeof (p_pHeader->uuidSourceNugget) +
        (uint32_t) sizeof (p_pHeader->uuidDestNugget);
}

SO_PUBLIC struct MessageHello *
MessageHello_Initialize (
                         const uuid_t p_uuidSourceNugget,
                         const uuid_t p_uuidNuggetType,
                         const uuid_t p_uuidApplicationType)
{
    struct MessageHello *message;
    if ((message = calloc(1, sizeof(struct MessageHello))) == NULL)
        return NULL;

    message->ccmhHeader.mhHeader.iType = MESSAGE_TYPE_HELLO;
    message->ccmhHeader.mhHeader.iLength =
        CcMessageHeader_BinaryLength (&message->ccmhHeader) +
        (uint32_t) sizeof (message->uuidApplicationType) +
        (uint32_t) sizeof (message->uuidNuggetType);
    message->ccmhHeader.mhHeader.iVersion = MESSAGE_VERSION_1;
    uuid_copy (message->ccmhHeader.uuidSourceNugget, p_uuidSourceNugget);
    uuid_clear (message->ccmhHeader.uuidDestNugget);
    uuid_copy (message->uuidNuggetType, p_uuidNuggetType);
    uuid_copy (message->uuidApplicationType, p_uuidApplicationType);

    return message;
}

SO_PUBLIC void
MessageHello_Destroy(struct MessageHello *message)
{
    free(message);
}

SO_PUBLIC struct MessageRegistrationRequest *
MessageRegistrationRequest_Initialize (
                                       const uuid_t p_uuidSourceNugget,
                                       const uuid_t p_uuidNuggetType,
                                       const uuid_t p_uuidApplicationType,
                                       uint32_t p_iDataTypeCount,
                                       uuid_t * p_pDataTypeList)
{
    uint32_t l_iIndex;
    struct MessageRegistrationRequest *message;

    if ((message = calloc(1,sizeof(struct MessageRegistrationRequest))) == NULL)
        return NULL;

    message->ccmhMessageHeader.mhHeader.iType = MESSAGE_TYPE_REG_REQ;
    message->ccmhMessageHeader.mhHeader.iLength =
        CcMessageHeader_BinaryLength (&message->ccmhMessageHeader) +
        (uint32_t) sizeof (message->uuidNuggetType) +
        (uint32_t) sizeof (message->uuidApplicationType) +
        (uint32_t) sizeof (uint32_t) +
        (uint32_t) sizeof (uuid_t) * p_iDataTypeCount;
    message->ccmhMessageHeader.mhHeader.iVersion = MESSAGE_VERSION_1;
    uuid_copy (message->ccmhMessageHeader.uuidSourceNugget,
               p_uuidSourceNugget);
    uuid_clear (message->ccmhMessageHeader.uuidDestNugget);  // Broadcast Message
    uuid_copy (message->uuidNuggetType, p_uuidNuggetType);
    uuid_copy (message->uuidApplicationType, p_uuidApplicationType);
    message->iDataTypeCount = p_iDataTypeCount;
    if (p_iDataTypeCount > 0)
    {
        if ((message->pDataTypeList =
             malloc (sizeof (uuid_t) * p_iDataTypeCount)) == NULL)
        {
            rzb_log (LOG_ERR,
                     "%s: failed due to lack of memory", __func__);
            MessageRegistrationRequest_Destroy(message);
            return NULL;
        }
    } 
    else
        message->pDataTypeList = NULL;

    for (l_iIndex = 0; l_iIndex < message->iDataTypeCount; l_iIndex++)
        uuid_copy (message->pDataTypeList[l_iIndex],
                   p_pDataTypeList[l_iIndex]);

    return message;
}

SO_PUBLIC void
MessageRegistrationRequest_Destroy (struct MessageRegistrationRequest
                                    *p_pMessage)
{
    ASSERT (p_pMessage != NULL);
	if(p_pMessage->pDataTypeList != NULL)
	    free (p_pMessage->pDataTypeList);

    free(p_pMessage);
}

SO_PUBLIC struct MessageRegistrationResponse *
MessageRegistrationResponse_Initialize (
                                        const uuid_t p_uuidSourceNugget,
                                        const uuid_t p_uuidDestNugget)
{
    struct MessageRegistrationResponse *message;
    if ((message = calloc(1, sizeof(struct MessageRegistrationResponse))) == NULL)
        return NULL;

    message->ccmhMessageHeader.mhHeader.iType = MESSAGE_TYPE_REG_RESP;
    message->ccmhMessageHeader.mhHeader.iLength =
        CcMessageHeader_BinaryLength (&message->ccmhMessageHeader);
    message->ccmhMessageHeader.mhHeader.iVersion = MESSAGE_VERSION_1;
    uuid_copy (message->ccmhMessageHeader.uuidSourceNugget,
               p_uuidSourceNugget);
    uuid_copy (message->ccmhMessageHeader.uuidDestNugget,
               p_uuidDestNugget);

    return message;
}

SO_PUBLIC void
MessageRegistrationResponse_Destroy (struct MessageRegistrationResponse
                                    *p_pMessage)
{
    ASSERT (p_pMessage != NULL);

    free(p_pMessage);
}

SO_PUBLIC struct MessageConfigurationUpdate *
MessageConfigurationUpdate_Initialize (
                                       const uuid_t p_uuidSourceNugget,
                                       const uuid_t p_uuidDestNugget)
{
    struct MessageConfigurationUpdate *message;
    struct UUIDList *list;
    struct UUIDListNode *item;
    char *curString;
    uuid_t *curUuid;
    if ((message = calloc(1,sizeof(struct MessageConfigurationUpdate))) == NULL)
        return NULL;

    message->ccmhMessageHeader.mhHeader.iType = MESSAGE_TYPE_CONFIG_UPDATE;
    message->ccmhMessageHeader.mhHeader.iLength =
        CcMessageHeader_BinaryLength (&message->ccmhMessageHeader) +
        (sizeof(uint32_t) *6); // 3 counts, 3 sizes

    message->ccmhMessageHeader.mhHeader.iVersion = MESSAGE_VERSION_1;
    uuid_copy (message->ccmhMessageHeader.uuidSourceNugget,
               p_uuidSourceNugget);
    uuid_copy (message->ccmhMessageHeader.uuidDestNugget,
               p_uuidDestNugget);
    
    if ((list = UUID_Get_List(UUID_TYPE_NTLV_TYPE)) == NULL)
    {
        MessageConfigurationUpdate_Destroy(message);
        return NULL;
    }
    pthread_mutex_lock(&list->mutex);
    message->ccmhMessageHeader.mhHeader.iLength += (list->count * 16);
    message->ntlvTypesCount = list->count;
    message->ntlvTypesNamesSize = 0;
    if (list->count > 0)
    {
        item = list->pHead;
        while (item != NULL)
        {
            message->ntlvTypesNamesSize += strlen(item->sName) +1;
            item = item->pNext;
        }

        message->ccmhMessageHeader.mhHeader.iLength += message->ntlvTypesNamesSize;

        if ((message->ntlvTypesNames = calloc(message->ntlvTypesNamesSize, sizeof(char))) == NULL)
        {
            pthread_mutex_unlock(&list->mutex);
            MessageConfigurationUpdate_Destroy(message);
            return NULL;
        }
        if ((message->ntlvTypesUuids = calloc(list->count, sizeof(uuid_t))) == NULL)
        {
            pthread_mutex_unlock(&list->mutex);
            MessageConfigurationUpdate_Destroy(message);
            return NULL;
        }
        curString = message->ntlvTypesNames;
        curUuid = message->ntlvTypesUuids;
        item = list->pHead;
        while (item != NULL)
        {
            memcpy(curString, item->sName, strlen(item->sName)+1);
            curString += strlen(item->sName)+1;
            uuid_copy(*curUuid, item->uuid);
            curUuid++;
            item = item->pNext;
        }
    }
    pthread_mutex_unlock(&list->mutex);
    
    if ((list = UUID_Get_List(UUID_TYPE_NTLV_NAME)) == NULL)
    {
        MessageConfigurationUpdate_Destroy(message);
        return NULL;
    }
    pthread_mutex_lock(&list->mutex);
    message->ccmhMessageHeader.mhHeader.iLength += (list->count * 16);
    message->ntlvNamesCount = list->count;
    message->ntlvNamesNamesSize = 0;
    if (list->count > 0)
    {
        item = list->pHead;
        while (item != NULL)
        {
            message->ntlvNamesNamesSize += strlen(item->sName) +1;
            item = item->pNext;
        }

        message->ccmhMessageHeader.mhHeader.iLength += message->ntlvNamesNamesSize;

        if ((message->ntlvNamesNames = calloc(message->ntlvNamesNamesSize, sizeof(char))) == NULL)
        {
            pthread_mutex_unlock(&list->mutex);
            MessageConfigurationUpdate_Destroy(message);
            return NULL;
        }
        if ((message->ntlvNamesUuids = calloc(list->count, sizeof(uuid_t))) == NULL)
        {
            pthread_mutex_unlock(&list->mutex);
            MessageConfigurationUpdate_Destroy(message);
            return NULL;
        }
        curString = message->ntlvNamesNames;
        curUuid = message->ntlvNamesUuids;
        item = list->pHead;
        while (item != NULL)
        {
            memcpy(curString, item->sName, strlen(item->sName)+1);
            curString += strlen(item->sName)+1;
            uuid_copy(*curUuid, item->uuid);
            curUuid++;
            item = item->pNext;
        }
    }
    pthread_mutex_unlock(&list->mutex);

    if ((list = UUID_Get_List(UUID_TYPE_DATA_TYPE)) == NULL)
    {
        MessageConfigurationUpdate_Destroy(message);
        return NULL;
    }
    pthread_mutex_lock(&list->mutex);
    message->ccmhMessageHeader.mhHeader.iLength += (list->count * 16);
    message->dataTypesCount = list->count;
    message->dataTypesNamesSize = 0;
    if (list->count > 0)
    {
        item = list->pHead;
        while (item != NULL)
        {
            message->dataTypesNamesSize += strlen(item->sName) +1;
            item = item->pNext;
        }

        message->ccmhMessageHeader.mhHeader.iLength += message->dataTypesNamesSize;

        if ((message->dataTypesNames = calloc(message->dataTypesNamesSize, sizeof(char))) == NULL)
        {
            pthread_mutex_unlock(&list->mutex);
            MessageConfigurationUpdate_Destroy(message);
            return NULL;
        }
        if ((message->dataTypesUuids = calloc(list->count, sizeof(uuid_t))) == NULL)
        {
            pthread_mutex_unlock(&list->mutex);
            MessageConfigurationUpdate_Destroy(message);
            return NULL;
        }
        curString = message->dataTypesNames;
        curUuid = message->dataTypesUuids;
        item = list->pHead;
        while (item != NULL)
        {
            memcpy(curString, item->sName, strlen(item->sName)+1);
            curString += strlen(item->sName)+1;
            uuid_copy(*curUuid, item->uuid);
            curUuid++;
            item = item->pNext;
        }
    }
    pthread_mutex_unlock(&list->mutex);
    return message;
}

SO_PUBLIC void
MessageConfigurationUpdate_Destroy (struct MessageConfigurationUpdate
                                    *p_pMessage)
{
    ASSERT (p_pMessage != NULL);
    if (p_pMessage->ntlvTypesUuids != NULL)
        free(p_pMessage->ntlvTypesUuids);
    if (p_pMessage->ntlvTypesNames != NULL)
        free(p_pMessage->ntlvTypesNames);
    if (p_pMessage->ntlvNamesUuids != NULL)
        free(p_pMessage->ntlvNamesUuids);
    if (p_pMessage->ntlvNamesNames != NULL)
        free(p_pMessage->ntlvNamesNames);
    if (p_pMessage->dataTypesUuids != NULL)
        free(p_pMessage->dataTypesUuids);
    if (p_pMessage->dataTypesNames != NULL)
        free(p_pMessage->dataTypesNames);

    free(p_pMessage);
}

SO_PUBLIC struct MessageConfigurationAck *
MessageConfigurationAck_Initialize (
                                    const uuid_t p_uuidSourceNugget,
                                    const uuid_t p_uuidDestNugget,
                                    const uuid_t p_uuidNuggetType,
                                    const uuid_t p_uuidApplicationType)
{
    struct MessageConfigurationAck * message;
    if ((message = calloc(1,sizeof(struct MessageConfigurationAck))) == NULL)
        return NULL;

    message->ccmhMessageHeader.mhHeader.iType = MESSAGE_TYPE_CONFIG_ACK;
    message->ccmhMessageHeader.mhHeader.iLength =
        CcMessageHeader_BinaryLength (&message->ccmhMessageHeader) +
        (uint32_t) sizeof (message->uuidApplicationType) +
        (uint32_t) sizeof (message->uuidNuggetType);
        
        
    message->ccmhMessageHeader.mhHeader.iVersion = MESSAGE_VERSION_1;
    uuid_copy (message->ccmhMessageHeader.uuidSourceNugget,
               p_uuidSourceNugget);
    uuid_copy (message->ccmhMessageHeader.uuidDestNugget,
               p_uuidDestNugget);
    uuid_copy (message->uuidNuggetType, p_uuidNuggetType);
    uuid_copy (message->uuidApplicationType, p_uuidApplicationType);
    return message;
}

SO_PUBLIC void
MessageConfigurationAck_Destroy (struct MessageConfigurationAck
                                    *p_pMessage)
{
    ASSERT (p_pMessage != NULL);
    free(p_pMessage);
}

SO_PUBLIC struct MessageStatsRequest *
MessageStatsRequest_Initialize (
                                const uuid_t p_uuidSourceNugget,
                                const uuid_t p_uuidDestNugget)
{
    struct MessageStatsRequest *message;
    if ((message = calloc(1, sizeof(struct MessageStatsRequest))) == NULL)
        return NULL;

    message->ccmhMessageHeader.mhHeader.iType = MESSAGE_TYPE_STATS_REQ;
    message->ccmhMessageHeader.mhHeader.iLength =
        CcMessageHeader_BinaryLength (&message->ccmhMessageHeader);
    message->ccmhMessageHeader.mhHeader.iVersion = MESSAGE_VERSION_1;
    uuid_copy (message->ccmhMessageHeader.uuidSourceNugget,
               p_uuidSourceNugget);
    uuid_copy (message->ccmhMessageHeader.uuidDestNugget,
               p_uuidDestNugget);
    return message;
}

SO_PUBLIC void
MessageStatsRequest_Destroy (struct MessageStatsRequest
                                    *p_pMessage)
{
    ASSERT (p_pMessage != NULL);
    free(p_pMessage);
}


SO_PUBLIC struct MessageStatsResponse *
MessageStatsResponse_Initialize (
                                 const uuid_t p_uuidSourceNugget,
                                 const uuid_t p_uuidDestNugget,
                                 const struct NTLVList *p_pStatsList)
{
    struct MessageStatsResponse *message;
    if ((message = calloc(1, sizeof(struct MessageStatsRequest))) == NULL)
        return NULL;

    message->ccmhMessageHeader.mhHeader.iType = MESSAGE_TYPE_STATS_REQ;
    message->ccmhMessageHeader.mhHeader.iLength =
        CcMessageHeader_BinaryLength (&message->ccmhMessageHeader);
    message->ccmhMessageHeader.mhHeader.iVersion = MESSAGE_VERSION_1;
    uuid_copy (message->ccmhMessageHeader.uuidSourceNugget,
               p_uuidSourceNugget);
    uuid_copy (message->ccmhMessageHeader.uuidDestNugget,
               p_uuidDestNugget);
    // treat NTLV list as uninitialized
    if ((message->ntlvStatsList = NTLVList_Create()) == NULL)
    {
        MessageStatsResponse_Destroy(message);
        return NULL;
    }
    NTLVList_Copy (message->ntlvStatsList, p_pStatsList);
    return message;
}

SO_PUBLIC void
MessageStatsResponse_Destroy (struct MessageStatsResponse *p_pMessage)
{
    ASSERT (p_pMessage != NULL);
    if (p_pMessage->ntlvStatsList != NULL)
        NTLVList_Destroy (p_pMessage->ntlvStatsList);

    free(p_pMessage);
}

SO_PUBLIC struct MessagePause *
MessagePause_Initialize (
                         const uuid_t p_uuidSourceNugget,
                         const uuid_t p_uuidDestNugget)
{
    struct MessagePause *message;
    if ((message = calloc(1,sizeof (struct MessagePause))) == NULL)
        return NULL;
    message->ccmhMessageHeader.mhHeader.iType = MESSAGE_TYPE_PAUSE;
    message->ccmhMessageHeader.mhHeader.iLength =
        CcMessageHeader_BinaryLength (&message->ccmhMessageHeader);
    message->ccmhMessageHeader.mhHeader.iVersion = MESSAGE_VERSION_1;
    uuid_copy (message->ccmhMessageHeader.uuidSourceNugget,
               p_uuidSourceNugget);
    uuid_copy (message->ccmhMessageHeader.uuidDestNugget,
               p_uuidDestNugget);

    return message;
}

SO_PUBLIC void
MessagePause_Destroy (struct MessagePause *message)
{
   free(message);
}

SO_PUBLIC struct MessagePaused *
MessagePaused_Initialize (
                          const uuid_t p_uuidSourceNugget,
                          const uuid_t p_uuidDestNugget)
{
    struct MessagePaused *message;
    if ((message = calloc(1,sizeof(struct MessagePaused))) == NULL)
        return NULL;

    message->ccmhMessageHeader.mhHeader.iType = MESSAGE_TYPE_PAUSED;
    message->ccmhMessageHeader.mhHeader.iLength =
        CcMessageHeader_BinaryLength (&message->ccmhMessageHeader);
    message->ccmhMessageHeader.mhHeader.iVersion = MESSAGE_VERSION_1;
    uuid_copy (message->ccmhMessageHeader.uuidSourceNugget,
               p_uuidSourceNugget);
    uuid_copy (message->ccmhMessageHeader.uuidDestNugget,
               p_uuidDestNugget);
    return message;
}

SO_PUBLIC void
MessagePaused_Destroy (struct MessagePaused *message)
{
   free(message);
}

SO_PUBLIC struct MessageGo *
MessageGo_Initialize (
                      const uuid_t p_uuidSourceNugget,
                      const uuid_t p_uuidDestNugget)
{
    struct MessageGo *message;
    if ((message = calloc(1,sizeof(struct MessageGo))) == NULL)
        return NULL;

    message->ccmhMessageHeader.mhHeader.iType = MESSAGE_TYPE_GO;
    message->ccmhMessageHeader.mhHeader.iLength =
        CcMessageHeader_BinaryLength (&message->ccmhMessageHeader);
    message->ccmhMessageHeader.mhHeader.iVersion = MESSAGE_VERSION_1;
    uuid_copy (message->ccmhMessageHeader.uuidSourceNugget,
               p_uuidSourceNugget);
    uuid_copy (message->ccmhMessageHeader.uuidDestNugget,
               p_uuidDestNugget);

    return message;
}

SO_PUBLIC void
MessageGo_Destroy (struct MessageGo *message)
{
   free(message);
}


SO_PUBLIC struct MessageRunning *
MessageRunning_Initialize (
                           const uuid_t p_uuidSourceNugget,
                           const uuid_t p_uuidDestNugget)
{
    struct MessageRunning *message;
    if ((message = calloc(1,sizeof(struct MessageRunning))) == NULL)
        return NULL;

    message->ccmhMessageHeader.mhHeader.iType = MESSAGE_TYPE_RUNNING;
    message->ccmhMessageHeader.mhHeader.iLength =
        CcMessageHeader_BinaryLength (&message->ccmhMessageHeader);
    message->ccmhMessageHeader.mhHeader.iVersion = MESSAGE_VERSION_1;
    uuid_copy (message->ccmhMessageHeader.uuidSourceNugget,
               p_uuidSourceNugget);
    uuid_copy (message->ccmhMessageHeader.uuidDestNugget,
               p_uuidDestNugget);

    return message;
}

SO_PUBLIC void
MessageRunning_Destroy (struct MessageRunning *message)
{
   free(message);
}

SO_PUBLIC struct MessageTerminate *
MessageTerminate_Initialize (
                             const uuid_t p_uuidSourceNugget,
                             const uuid_t p_uuidDestNugget,
                             const uint8_t * p_sTerminateReason)
{
    ASSERT (p_sTerminateReason != NULL);
    struct MessageTerminate *message;
    if ((message = calloc(1,sizeof(struct MessageTerminate))) == NULL)
        return NULL;

    message->ccmhMessageHeader.mhHeader.iType = MESSAGE_TYPE_TERM;
    message->ccmhMessageHeader.mhHeader.iLength =
        CcMessageHeader_BinaryLength (&message->ccmhMessageHeader) +

        strlen((char *)p_sTerminateReason)+1;
    message->ccmhMessageHeader.mhHeader.iVersion = MESSAGE_VERSION_1;
    uuid_copy (message->ccmhMessageHeader.uuidSourceNugget,
               p_uuidSourceNugget);
    uuid_copy (message->ccmhMessageHeader.uuidDestNugget,
               p_uuidDestNugget);
    if ((message->sTerminateReason =
         malloc (strlen ((const char *) p_sTerminateReason) + 1)) == NULL)
    {
        MessageTerminate_Destroy(message);
        rzb_log (LOG_ERR,
                 "%s: failed due to lack of memory", __func__);
        return NULL;
    }
    strcpy ((char *) message->sTerminateReason,
            (const char *) p_sTerminateReason);

    return message;
}

SO_PUBLIC void
MessageTerminate_Destroy (struct MessageTerminate *message)
{
    ASSERT (message != NULL);
    if (message->sTerminateReason != NULL)
        free (message->sTerminateReason);

    free(message);
}

SO_PUBLIC struct MessageBye *
MessageBye_Initialize (
                       const uuid_t p_uuidSourceNugget,
                       const uuid_t p_uuidDestNugget)
{
    struct MessageBye *message;
    if ((message = calloc(1,sizeof(struct MessageBye))) == NULL)
        return NULL;

    message->ccmhMessageHeader.mhHeader.iType = MESSAGE_TYPE_BYE;
    message->ccmhMessageHeader.mhHeader.iLength =
        CcMessageHeader_BinaryLength (&message->ccmhMessageHeader);
    message->ccmhMessageHeader.mhHeader.iVersion = MESSAGE_VERSION_1;
    uuid_copy (message->ccmhMessageHeader.uuidSourceNugget,
               p_uuidSourceNugget);
    uuid_copy (message->ccmhMessageHeader.uuidDestNugget,
               p_uuidDestNugget);

    return message;
}

SO_PUBLIC void
MessageBye_Destroy (struct MessageBye *message)
{
    ASSERT (message != NULL);

    free(message);
}

SO_PUBLIC struct MessageCacheClear *
MessageCacheClear_Initialize (
                       const uuid_t p_uuidSourceNugget)
{
    struct MessageCacheClear *message;
    if ((message = calloc(1,sizeof(struct MessageCacheClear))) == NULL)
        return NULL;

    message->mhHeader.mhHeader.iType = MESSAGE_TYPE_CLEAR;
    message->mhHeader.mhHeader.iLength =
        CcMessageHeader_BinaryLength (&message->mhHeader);
    message->mhHeader.mhHeader.iVersion = MESSAGE_VERSION_1;
    uuid_copy (message->mhHeader.uuidSourceNugget,
               p_uuidSourceNugget);
    uuid_clear (message->mhHeader.uuidDestNugget);
    return message;
}

SO_PUBLIC void
MessageCacheClear_Destroy (struct MessageCacheClear *message)
{
    ASSERT (message != NULL);

    free(message);
}

SO_PUBLIC struct MessageError *
MessageError_Initialize (
                        uint32_t p_iCode,
                        const char *p_sMessage,
                       const uuid_t p_uuidSourceNugget,
                       const uuid_t p_uuidDestNugget)
{
    struct MessageError *message;
    if ((message = calloc(1,sizeof(struct MessageError))) == NULL)
        return NULL;

    message->ccmhHeader.mhHeader.iType = p_iCode;
    message->ccmhHeader.mhHeader.iLength =
        CcMessageHeader_BinaryLength (&message->ccmhHeader) +
        strlen(p_sMessage)+1;
    message->ccmhHeader.mhHeader.iVersion = MESSAGE_VERSION_1;
    uuid_copy (message->ccmhHeader.uuidSourceNugget,
               p_uuidSourceNugget);
    uuid_copy (message->ccmhHeader.uuidDestNugget,
               p_uuidDestNugget);

    if ((message->sMessage =
         malloc (strlen ((const char *) p_sMessage) + 1)) == NULL)
    {
        MessageError_Destroy(message);
        rzb_log (LOG_ERR,
                 "%s: failed due to lack of memory", __func__);
        return NULL;
    }
    strcpy ((char *) message->sMessage,
            (const char *) p_sMessage);
    return message;
}

SO_PUBLIC void
MessageError_Destroy (struct MessageError *message)
{
    ASSERT (message != NULL);
    if (message->sMessage != NULL)
        free(message->sMessage);

    free(message);
}

SO_PUBLIC void
MessageCC_Destroy (union CCMessage *message)
{
    ASSERT (message != NULL);

    switch (message->mchHeader->mhHeader.iType)
    {

    case MESSAGE_TYPE_HELLO:
        MessageHello_Destroy (message->mhHello);
        break;
    case MESSAGE_TYPE_REG_REQ:
        MessageRegistrationRequest_Destroy (message->mrrRegReq);
        break;
    case MESSAGE_TYPE_REG_RESP:
        MessageRegistrationResponse_Destroy (message->mrrRegResp);
        break;
    case MESSAGE_TYPE_REG_ERR:
        MessageError_Destroy (message->meError);
        break;
    case MESSAGE_TYPE_CONFIG_UPDATE:
        MessageConfigurationUpdate_Destroy(message->mcuConfigUpdate);
        break;
    case MESSAGE_TYPE_CONFIG_ACK:
        MessageConfigurationAck_Destroy (message->mcaConfigAck);
        break;
    case MESSAGE_TYPE_CONFIG_ERR:
        MessageError_Destroy (message->meError);
        break;
    case MESSAGE_TYPE_STATS_REQ:
        MessageStatsRequest_Destroy (message->msrStatsReq);
        break;
    case MESSAGE_TYPE_STATS_RESP:
        MessageStatsResponse_Destroy (message->msrStatsResp);
        break;
    case MESSAGE_TYPE_STATS_ERR:
        MessageError_Destroy (message->meError);
        break;
    case MESSAGE_TYPE_PAUSE:
        MessagePause_Destroy (message->mpPause);
        break;
    case MESSAGE_TYPE_PAUSED:
        MessagePaused_Destroy (message->mpPaused);
        break;
    case MESSAGE_TYPE_GO:
        MessageGo_Destroy (message->mgGo);
        break;
    case MESSAGE_TYPE_RUNNING:
        MessageRunning_Destroy (message->mrRunning);
        break;
    case MESSAGE_TYPE_TERM:
        MessageTerminate_Destroy (message->mtTerminate);
        break;
    case MESSAGE_TYPE_BYE:
        MessageBye_Destroy (message->mbBye);
        break;
    case MESSAGE_TYPE_CLEAR:
        MessageCacheClear_Destroy (message->mccCacheClear);
        break;

    default:
        rzb_log(LOG_ERR, "%s: Failed to destroy message unknown type", __func__);
        break;
    }
}
