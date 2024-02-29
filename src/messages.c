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

SO_PUBLIC bool
MessageCacheReq_Initialize (struct MessageCacheReq *p_pMessage,
                            const uuid_t p_uuidRequestor,
                            const struct BlockId *p_pBlockId)
{
    ASSERT (p_pMessage != NULL);
    ASSERT (p_pBlockId != NULL);

    // calculate header
    p_pMessage->mhHeader.iType = MESSAGE_TYPE_REQ;
    p_pMessage->mhHeader.iLength =
        MessageHeader_BinaryLength (&p_pMessage->mhHeader) +
        (uint32_t) sizeof (p_pMessage->uuidRequestor) +
        BlockId_BinaryLength (p_pBlockId);
    p_pMessage->mhHeader.iVersion = MESSAGE_VERSION_1;

    // fill in rest of message
    uuid_copy (p_pMessage->uuidRequestor, p_uuidRequestor);
    if ((p_pMessage->pId = BlockId_Clone (p_pBlockId)) == NULL)
    {
        rzb_log (LOG_ERR,
                 "%s: failed due to failure of BlockId_Clone", __func__);
        return false;
    }

    // done
    return true;
}

SO_PUBLIC void
MessageCacheReq_Destroy (struct MessageCacheReq *p_pMessage)
{
    ASSERT (p_pMessage != NULL);

    // destroy any malloc'd components
    BlockId_Destroy (p_pMessage->pId);

    // done
}

SO_PUBLIC bool
MessageCacheResp_Initialize (struct MessageCacheResp *p_pMessage,
                             const struct BlockId *p_pBlockId,
                             uint32_t p_iSfFlags, uint32_t p_iEntFlags)
{
    ASSERT (p_pMessage != NULL);
    ASSERT (p_pBlockId != NULL);

    // calculate header
    p_pMessage->mhHeader.iType = MESSAGE_TYPE_RESP;
    p_pMessage->mhHeader.iLength =
        MessageHeader_BinaryLength (&p_pMessage->mhHeader) +
        BlockId_BinaryLength (p_pBlockId) +
        (sizeof(uint32_t)*2);
    p_pMessage->mhHeader.iVersion = MESSAGE_VERSION_1;

    // fill in rest of message
    if ((p_pMessage->pId = BlockId_Clone (p_pBlockId)) == NULL)
    {
        rzb_log (LOG_ERR,
                 "%s: failed due to failure of BlockId_Clone", __func__);
        return false;
    }
    p_pMessage->iSfFlags = p_iSfFlags;
    p_pMessage->iEntFlags = p_iEntFlags;
    
    // done
    return true;
}

SO_PUBLIC void
MessageCacheResp_Destroy (struct MessageCacheResp *p_pMessage)
{
    ASSERT (p_pMessage != NULL);

    // destroy any malloc'd components
    BlockId_Destroy (p_pMessage->pId);

    // done
}

SO_PUBLIC bool
MessageBlockSubmission_Initialize (struct MessageBlockSubmission *p_pMessage,
                                   struct Event *p_pEvent,
                                   uint32_t p_iReason)
{
    ASSERT (p_pMessage != NULL);
    ASSERT (p_pEvent != NULL);

    p_pMessage->pEvent = p_pEvent;

    // calculate header
	if (p_pEvent->pBlock->isStored != 1)
		p_pMessage->mhHeader.iType = MESSAGE_TYPE_BLOCK;
	else 
		p_pMessage->mhHeader.iType = MESSAGE_TYPE_TICKET;
    p_pMessage->mhHeader.iLength =
        MessageHeader_BinaryLength (&p_pMessage->mhHeader) +
        Event_BinaryLength (p_pMessage->pEvent) +
        (uint32_t) sizeof (p_pMessage->iReason);
    p_pMessage->mhHeader.iVersion = MESSAGE_VERSION_1;

    p_pMessage->iReason = p_iReason;
    return true;
}

SO_PUBLIC void
MessageBlockSubmission_Destroy (struct MessageBlockSubmission *p_pMessage)
{
    ASSERT (p_pMessage != NULL);

    // destroy any malloc'd components
    if (p_pMessage->pEvent != NULL)
        Event_Destroy (p_pMessage->pEvent);
    // done
}

SO_PUBLIC bool
MessageJudgmentSubmission_Initialize (struct MessageJudgmentSubmission
                                      *p_pMessage,
                                      const struct Block *p_pBlock,
                                      struct EventId *p_pEventId,
                                      const uuid_t p_uuidInspectorId,
                                      const uuid_t p_uuidApplicationId,
                                      uint8_t p_iReason, uint8_t p_iPriority,
                                      uint32_t p_iSfFlags, uint32_t p_iEntFlags,
                                      struct NTLVList *p_pMetadata)
{
    ASSERT (p_pMessage != NULL);
    ASSERT (p_pBlock != NULL);

    if ((p_pMessage->pBlock = Block_Clone(p_pBlock)) == NULL)
        return false;

    // calculate header
    p_pMessage->mhHeader.iType = MESSAGE_TYPE_JUDGMENT;
    p_pMessage->mhHeader.iLength =
        MessageHeader_BinaryLength (&p_pMessage->mhHeader) +
        Block_BinaryLength (p_pMessage->pBlock) +
        sizeof (struct EventId) +
        (uint32_t) sizeof (p_pMessage->iReason) +
        (uint32_t) sizeof (p_pMessage->iPriority) +
        (uint32_t) sizeof (p_pMessage->iSfFlags) +
        (uint32_t) sizeof (p_pMessage->iEntFlags) +
        (uint32_t) sizeof (p_pMessage->uuidInspectorId) +
        (uint32_t) sizeof (p_pMessage->uuidApplicationId) +
        NTLVList_Size(p_pMetadata);
    p_pMessage->mhHeader.iVersion = MESSAGE_VERSION_1;

    p_pMessage->iReason = p_iReason;
    p_pMessage->iPriority = p_iPriority;
    p_pMessage->iSfFlags = p_iSfFlags;
    p_pMessage->iEntFlags = p_iEntFlags;
    uuid_copy (p_pMessage->uuidInspectorId, p_uuidInspectorId);
    uuid_copy (p_pMessage->uuidApplicationId, p_uuidApplicationId);
    if ((p_pMessage->pEventId = EventId_Clone(p_pEventId)) == NULL)
    {
        MessageJudgmentSubmission_Destroy(p_pMessage);
        return false;
    }
    if ((p_pMessage->alertMetadata = NTLVList_Create()) == NULL)
    {
        MessageJudgmentSubmission_Destroy(p_pMessage);
        return false;
    }

    NTLVList_Copy(p_pMessage->alertMetadata, p_pMetadata);

    return true;
}

SO_PUBLIC void
MessageJudgmentSubmission_Destroy (struct MessageJudgmentSubmission
                                   *p_pMessage)
{
    ASSERT (p_pMessage != NULL);
    
    if (p_pMessage->alertMetadata != NULL)
        NTLVList_Destroy(p_pMessage->alertMetadata);

    // destroy any malloc'd components
    if (p_pMessage->pBlock != NULL)
        Block_Destroy (p_pMessage->pBlock);

    if (p_pMessage->pEventId != NULL)
        EventId_Destroy(p_pMessage->pEventId);

    // done
}

SO_PUBLIC bool
MessageInspectionSubmission_Initialize (struct MessageInspectionSubmission
                                        *p_pMessage,
                                        const struct Event *p_pEvent,
                                        uint32_t p_iReason)
{
    ASSERT (p_pMessage != NULL);
    ASSERT (p_pEvent != NULL);
	
    if ((p_pMessage->pBlock = Block_Clone(p_pEvent->pBlock)) == NULL)
        return false;

	if (p_pEvent->pBlock->isStored == 1)
		p_pMessage->mhHeader.iType = MESSAGE_TYPE_TICKET;
    else
	p_pMessage->mhHeader.iType = MESSAGE_TYPE_INSPECTION;
        
    // calculate header
    p_pMessage->mhHeader.iLength =
        MessageHeader_BinaryLength (&p_pMessage->mhHeader) +
        Block_BinaryLength (p_pMessage->pBlock) +
        NTLVList_Size (p_pEvent->pMetaDataList) +
        sizeof(struct EventId) + //Source nugget ID
        (uint32_t) sizeof (p_pMessage->iReason);


    p_pMessage->mhHeader.iVersion = MESSAGE_VERSION_1;

    p_pMessage->iReason = p_iReason;
    if ((p_pMessage->eventId = EventId_Clone(p_pEvent->pId)) == NULL)
        return false;
    if ((p_pMessage->pEventMetadata = NTLVList_Create()) == NULL)
        return false;
    NTLVList_Copy (p_pMessage->pEventMetadata, p_pEvent->pMetaDataList);

    // done
    return true;
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
    // done
}

SO_PUBLIC bool
MessageAlert_Initialize (struct MessageAlert *p_pMessage,
                         const struct Block *p_pBlock,
                         uint32_t p_iDisposition,
                         const uuid_t p_uuidInspectorId)
{
    ASSERT (p_pMessage != NULL);
    ASSERT (p_pBlock != NULL);

    if ((p_pMessage->pBlock= Block_Clone(p_pBlock)) == NULL)
        return false;

    // fill in rest of message
    // calculate header
    p_pMessage->mhHeader.iType = MESSAGE_TYPE_ALERT;
    p_pMessage->mhHeader.iLength =
        MessageHeader_BinaryLength (&p_pMessage->mhHeader) +
        Block_BinaryLength (p_pMessage->pBlock) +
        (uint32_t) sizeof (p_pMessage->iDisposition) +
        (uint32_t) sizeof (p_pMessage->uuidInspectorId);
    p_pMessage->mhHeader.iVersion = MESSAGE_VERSION_1;

    p_pMessage->iDisposition = p_iDisposition;
    uuid_copy (p_pMessage->uuidInspectorId, p_uuidInspectorId);
    return true;
}

SO_PUBLIC void
MessageAlert_Destroy (struct MessageAlert *p_pMessage)
{
    ASSERT (p_pMessage != NULL);

    // destroy any malloc'd components
    Block_Destroy (p_pMessage->pBlock);

    // done
}

static uint32_t
CcMessageHeader_BinaryLength (struct CcMessageHeader *p_pHeader)
{
    ASSERT (p_pHeader != NULL);

    return MessageHeader_BinaryLength (&p_pHeader->mhHeader) +
        (uint32_t) sizeof (p_pHeader->uuidSourceNugget) +
        (uint32_t) sizeof (p_pHeader->uuidDestNugget);
}

SO_PUBLIC void
MessageHello_Initialize (struct MessageHello *p_pMessage,
                         const uuid_t p_uuidSourceNugget,
                         const uuid_t p_uuidNuggetType,
                         const uuid_t p_uuidApplicationType)
{
    ASSERT (p_pMessage != NULL);

    p_pMessage->ccmhHeader.mhHeader.iType = MESSAGE_TYPE_HELLO;
    p_pMessage->ccmhHeader.mhHeader.iLength =
        CcMessageHeader_BinaryLength (&p_pMessage->ccmhHeader) +
        (uint32_t) sizeof (p_pMessage->uuidApplicationType) +
        (uint32_t) sizeof (p_pMessage->uuidNuggetType);
    p_pMessage->ccmhHeader.mhHeader.iVersion = MESSAGE_VERSION_1;
    uuid_copy (p_pMessage->ccmhHeader.uuidSourceNugget, p_uuidSourceNugget);
    uuid_clear (p_pMessage->ccmhHeader.uuidDestNugget);
    uuid_copy (p_pMessage->uuidNuggetType, p_uuidNuggetType);
    uuid_copy (p_pMessage->uuidApplicationType, p_uuidApplicationType);
}

SO_PUBLIC bool
MessageRegistrationRequest_Initialize (struct MessageRegistrationRequest
                                       *p_pMessage,
                                       const uuid_t p_uuidSourceNugget,
                                       const uuid_t p_uuidNuggetType,
                                       const uuid_t p_uuidApplicationType,
                                       uint32_t p_iDataTypeCount,
                                       uuid_t * p_pDataTypeList)
{
    ASSERT (p_pMessage != NULL);

    uint32_t l_iIndex;
    memset(p_pMessage, 0, sizeof(struct MessageRegistrationRequest));
    p_pMessage->ccmhMessageHeader.mhHeader.iType = MESSAGE_TYPE_REG_REQ;
    p_pMessage->ccmhMessageHeader.mhHeader.iLength =
        CcMessageHeader_BinaryLength (&p_pMessage->ccmhMessageHeader) +
        (uint32_t) sizeof (p_pMessage->uuidNuggetType) +
        (uint32_t) sizeof (p_pMessage->uuidApplicationType) +
        (uint32_t) sizeof (uint32_t) +
        (uint32_t) sizeof (uuid_t) * p_iDataTypeCount;
    p_pMessage->ccmhMessageHeader.mhHeader.iVersion = MESSAGE_VERSION_1;
    uuid_copy (p_pMessage->ccmhMessageHeader.uuidSourceNugget,
               p_uuidSourceNugget);
    uuid_clear (p_pMessage->ccmhMessageHeader.uuidDestNugget);  // Broadcast Message
    uuid_copy (p_pMessage->uuidNuggetType, p_uuidNuggetType);
    uuid_copy (p_pMessage->uuidApplicationType, p_uuidApplicationType);
    p_pMessage->iDataTypeCount = p_iDataTypeCount;
    if (p_iDataTypeCount > 0)
    {
        if ((p_pMessage->pDataTypeList =
             malloc (sizeof (uuid_t) * p_iDataTypeCount)) == NULL)
        {
            rzb_log (LOG_ERR,
                     "%s: failed due to lack of memory", __func__);
            return false;
        }
    } 
    else
        p_pMessage->pDataTypeList = NULL;

    for (l_iIndex = 0; l_iIndex < p_pMessage->iDataTypeCount; l_iIndex++)
        uuid_copy (p_pMessage->pDataTypeList[l_iIndex],
                   p_pDataTypeList[l_iIndex]);
    return true;
}

SO_PUBLIC void
MessageRegistrationRequest_Destroy (struct MessageRegistrationRequest
                                    *p_pMessage)
{
    ASSERT (p_pMessage != NULL);
	if(p_pMessage->pDataTypeList != NULL)
	    free (p_pMessage->pDataTypeList);
}

SO_PUBLIC void
MessageRegistrationResponse_Initialize (struct MessageRegistrationResponse
                                        *p_pMessage,
                                        const uuid_t p_uuidSourceNugget,
                                        const uuid_t p_uuidDestNugget)
{
    ASSERT (p_pMessage != NULL);

    p_pMessage->ccmhMessageHeader.mhHeader.iType = MESSAGE_TYPE_REG_RESP;
    p_pMessage->ccmhMessageHeader.mhHeader.iLength =
        CcMessageHeader_BinaryLength (&p_pMessage->ccmhMessageHeader);
    p_pMessage->ccmhMessageHeader.mhHeader.iVersion = MESSAGE_VERSION_1;
    uuid_copy (p_pMessage->ccmhMessageHeader.uuidSourceNugget,
               p_uuidSourceNugget);
    uuid_copy (p_pMessage->ccmhMessageHeader.uuidDestNugget,
               p_uuidDestNugget);
}

SO_PUBLIC bool
MessageConfigurationUpdate_Initialize (struct MessageConfigurationUpdate
                                       *p_pMessage,
                                       const uuid_t p_uuidSourceNugget,
                                       const uuid_t p_uuidDestNugget)
{
    ASSERT (p_pMessage != NULL);
    struct UUIDList *list;
    struct UUIDListNode *item;
    char *curString;
    uuid_t *curUuid;
    memset(p_pMessage, 0, sizeof(struct MessageConfigurationUpdate));
    p_pMessage->ccmhMessageHeader.mhHeader.iType = MESSAGE_TYPE_CONFIG_UPDATE;
    p_pMessage->ccmhMessageHeader.mhHeader.iLength =
        CcMessageHeader_BinaryLength (&p_pMessage->ccmhMessageHeader) +
        (sizeof(uint32_t) *4); // 2 counts, 2 sizes

    p_pMessage->ccmhMessageHeader.mhHeader.iVersion = MESSAGE_VERSION_1;
    uuid_copy (p_pMessage->ccmhMessageHeader.uuidSourceNugget,
               p_uuidSourceNugget);
    uuid_copy (p_pMessage->ccmhMessageHeader.uuidDestNugget,
               p_uuidDestNugget);
    
    if ((list = UUID_Get_List(UUID_TYPE_NTLV_TYPE)) == NULL)
        return false;
    pthread_mutex_lock(&list->mutex);
    p_pMessage->ccmhMessageHeader.mhHeader.iLength += (list->count * 16);
    p_pMessage->ntlvTypesCount = list->count;
    p_pMessage->ntlvTypesNamesSize = 0;
    if (list->count > 0)
    {
        item = list->pHead;
        while (item != NULL)
        {
            p_pMessage->ntlvTypesNamesSize += strlen(item->sName) +1;
            item = item->pNext;
        }

        p_pMessage->ccmhMessageHeader.mhHeader.iLength += p_pMessage->ntlvTypesNamesSize;

        if ((p_pMessage->ntlvTypesNames = calloc(p_pMessage->ntlvTypesNamesSize, sizeof(char))) == NULL)
        {
            pthread_mutex_unlock(&list->mutex);
            return false;
        }
        if ((p_pMessage->ntlvTypesUuids = calloc(list->count, sizeof(uuid_t))) == NULL)
        {
            pthread_mutex_unlock(&list->mutex);
            return false;
        }
        curString = p_pMessage->ntlvTypesNames;
        curUuid = p_pMessage->ntlvTypesUuids;
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
        return false;
    pthread_mutex_lock(&list->mutex);
    p_pMessage->ccmhMessageHeader.mhHeader.iLength += (list->count * 16);
    p_pMessage->ntlvNamesCount = list->count;
    p_pMessage->ntlvNamesNamesSize = 0;
    if (list->count > 0)
    {
        item = list->pHead;
        while (item != NULL)
        {
            p_pMessage->ntlvNamesNamesSize += strlen(item->sName) +1;
            item = item->pNext;
        }

        p_pMessage->ccmhMessageHeader.mhHeader.iLength += p_pMessage->ntlvNamesNamesSize;

        if ((p_pMessage->ntlvNamesNames = calloc(p_pMessage->ntlvNamesNamesSize, sizeof(char))) == NULL)
        {
            pthread_mutex_unlock(&list->mutex);
            return false;
        }
        if ((p_pMessage->ntlvNamesUuids = calloc(list->count, sizeof(uuid_t))) == NULL)
        {
            pthread_mutex_unlock(&list->mutex);
            return false;
        }
        curString = p_pMessage->ntlvNamesNames;
        curUuid = p_pMessage->ntlvNamesUuids;
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
    return true;
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

}

SO_PUBLIC void
MessageConfigurationAck_Initialize (struct MessageConfigurationAck
                                    *p_pMessage,
                                    const uuid_t p_uuidSourceNugget,
                                    const uuid_t p_uuidDestNugget,
                                    const uuid_t p_uuidNuggetType,
                                    const uuid_t p_uuidApplicationType)
{
    ASSERT (p_pMessage != NULL);

    p_pMessage->ccmhMessageHeader.mhHeader.iType = MESSAGE_TYPE_CONFIG_ACK;
    p_pMessage->ccmhMessageHeader.mhHeader.iLength =
        CcMessageHeader_BinaryLength (&p_pMessage->ccmhMessageHeader) +
        (uint32_t) sizeof (p_pMessage->uuidApplicationType) +
        (uint32_t) sizeof (p_pMessage->uuidNuggetType);
        
        
    p_pMessage->ccmhMessageHeader.mhHeader.iVersion = MESSAGE_VERSION_1;
    uuid_copy (p_pMessage->ccmhMessageHeader.uuidSourceNugget,
               p_uuidSourceNugget);
    uuid_copy (p_pMessage->ccmhMessageHeader.uuidDestNugget,
               p_uuidDestNugget);
    uuid_copy (p_pMessage->uuidNuggetType, p_uuidNuggetType);
    uuid_copy (p_pMessage->uuidApplicationType, p_uuidApplicationType);
}

SO_PUBLIC void
MessageStatsRequest_Initialize (struct MessageStatsRequest *p_pMessage,
                                const uuid_t p_uuidSourceNugget,
                                const uuid_t p_uuidDestNugget)
{
    ASSERT (p_pMessage != NULL);

    p_pMessage->ccmhMessageHeader.mhHeader.iType = MESSAGE_TYPE_STATS_REQ;
    p_pMessage->ccmhMessageHeader.mhHeader.iLength =
        CcMessageHeader_BinaryLength (&p_pMessage->ccmhMessageHeader);
    p_pMessage->ccmhMessageHeader.mhHeader.iVersion = MESSAGE_VERSION_1;
    uuid_copy (p_pMessage->ccmhMessageHeader.uuidSourceNugget,
               p_uuidSourceNugget);
    uuid_copy (p_pMessage->ccmhMessageHeader.uuidDestNugget,
               p_uuidDestNugget);
}

SO_PUBLIC void
MessageStatsResponse_Initialize (struct MessageStatsResponse *p_pMessage,
                                 const uuid_t p_uuidSourceNugget,
                                 const uuid_t p_uuidDestNugget,
                                 const struct NTLVList *p_pStatsList)
{
    ASSERT (p_pMessage != NULL);

    p_pMessage->ccmhMessageHeader.mhHeader.iType = MESSAGE_TYPE_STATS_REQ;
    p_pMessage->ccmhMessageHeader.mhHeader.iLength =
        CcMessageHeader_BinaryLength (&p_pMessage->ccmhMessageHeader);
    p_pMessage->ccmhMessageHeader.mhHeader.iVersion = MESSAGE_VERSION_1;
    uuid_copy (p_pMessage->ccmhMessageHeader.uuidSourceNugget,
               p_uuidSourceNugget);
    uuid_copy (p_pMessage->ccmhMessageHeader.uuidDestNugget,
               p_uuidDestNugget);
    // treat NTLV list as uninitialized
    if ((p_pMessage->ntlvStatsList = NTLVList_Create()) == NULL)
        return;
    NTLVList_Copy (p_pMessage->ntlvStatsList, p_pStatsList);
}

SO_PUBLIC void
MessageStatsResponse_Destroy (struct MessageStatsResponse *p_pMessage)
{
    ASSERT (p_pMessage != NULL);

    NTLVList_Destroy (p_pMessage->ntlvStatsList);
}

SO_PUBLIC void
MessagePause_Initialize (struct MessagePause *p_pMessage,
                         const uuid_t p_uuidSourceNugget,
                         const uuid_t p_uuidDestNugget)
{
    ASSERT (p_pMessage != NULL);

    p_pMessage->ccmhMessageHeader.mhHeader.iType = MESSAGE_TYPE_PAUSE;
    p_pMessage->ccmhMessageHeader.mhHeader.iLength =
        CcMessageHeader_BinaryLength (&p_pMessage->ccmhMessageHeader);
    p_pMessage->ccmhMessageHeader.mhHeader.iVersion = MESSAGE_VERSION_1;
    uuid_copy (p_pMessage->ccmhMessageHeader.uuidSourceNugget,
               p_uuidSourceNugget);
    uuid_copy (p_pMessage->ccmhMessageHeader.uuidDestNugget,
               p_uuidDestNugget);
}

SO_PUBLIC void
MessagePaused_Initialize (struct MessagePaused *p_pMessage,
                          const uuid_t p_uuidSourceNugget,
                          const uuid_t p_uuidDestNugget)
{
    ASSERT (p_pMessage != NULL);

    p_pMessage->ccmhMessageHeader.mhHeader.iType = MESSAGE_TYPE_PAUSED;
    p_pMessage->ccmhMessageHeader.mhHeader.iLength =
        CcMessageHeader_BinaryLength (&p_pMessage->ccmhMessageHeader);
    p_pMessage->ccmhMessageHeader.mhHeader.iVersion = MESSAGE_VERSION_1;
    uuid_copy (p_pMessage->ccmhMessageHeader.uuidSourceNugget,
               p_uuidSourceNugget);
    uuid_copy (p_pMessage->ccmhMessageHeader.uuidDestNugget,
               p_uuidDestNugget);
}

SO_PUBLIC void
MessageGo_Initialize (struct MessageGo *p_pMessage,
                      const uuid_t p_uuidSourceNugget,
                      const uuid_t p_uuidDestNugget)
{
    ASSERT (p_pMessage != NULL);

    p_pMessage->ccmhMessageHeader.mhHeader.iType = MESSAGE_TYPE_GO;
    p_pMessage->ccmhMessageHeader.mhHeader.iLength =
        CcMessageHeader_BinaryLength (&p_pMessage->ccmhMessageHeader);
    p_pMessage->ccmhMessageHeader.mhHeader.iVersion = MESSAGE_VERSION_1;
    uuid_copy (p_pMessage->ccmhMessageHeader.uuidSourceNugget,
               p_uuidSourceNugget);
    uuid_copy (p_pMessage->ccmhMessageHeader.uuidDestNugget,
               p_uuidDestNugget);
}

SO_PUBLIC void
MessageRunning_Initialize (struct MessageRunning *p_pMessage,
                           const uuid_t p_uuidSourceNugget,
                           const uuid_t p_uuidDestNugget)
{
    ASSERT (p_pMessage != NULL);

    p_pMessage->ccmhMessageHeader.mhHeader.iType = MESSAGE_TYPE_RUNNING;
    p_pMessage->ccmhMessageHeader.mhHeader.iLength =
        CcMessageHeader_BinaryLength (&p_pMessage->ccmhMessageHeader);
    p_pMessage->ccmhMessageHeader.mhHeader.iVersion = MESSAGE_VERSION_1;
    uuid_copy (p_pMessage->ccmhMessageHeader.uuidSourceNugget,
               p_uuidSourceNugget);
    uuid_copy (p_pMessage->ccmhMessageHeader.uuidDestNugget,
               p_uuidDestNugget);
}

SO_PUBLIC bool
MessageTerminate_Initialize (struct MessageTerminate *p_pMessage,
                             const uuid_t p_uuidSourceNugget,
                             const uuid_t p_uuidDestNugget,
                             const uint8_t * p_sTerminateReason)
{
    ASSERT (p_pMessage != NULL);
    ASSERT (p_sTerminateReason != NULL);

    p_pMessage->ccmhMessageHeader.mhHeader.iType = MESSAGE_TYPE_TERM;
    p_pMessage->ccmhMessageHeader.mhHeader.iLength =
        CcMessageHeader_BinaryLength (&p_pMessage->ccmhMessageHeader) +

        strlen((char *)p_sTerminateReason)+1;
    p_pMessage->ccmhMessageHeader.mhHeader.iVersion = MESSAGE_VERSION_1;
    uuid_copy (p_pMessage->ccmhMessageHeader.uuidSourceNugget,
               p_uuidSourceNugget);
    uuid_copy (p_pMessage->ccmhMessageHeader.uuidDestNugget,
               p_uuidDestNugget);
    if ((p_pMessage->sTerminateReason =
         malloc (strlen ((const char *) p_sTerminateReason) + 1)) == NULL)
    {
        rzb_log (LOG_ERR,
                 "%s: failed due to lack of memory", __func__);
        return false;
    }
    strcpy ((char *) p_pMessage->sTerminateReason,
            (const char *) p_sTerminateReason);

    return true;
}

SO_PUBLIC void
MessageTerminate_Destroy (struct MessageTerminate *p_pMessage)
{
    ASSERT (p_pMessage != NULL);

    free (p_pMessage->sTerminateReason);
}

SO_PUBLIC void
MessageBye_Initialize (struct MessageBye *p_pMessage,
                       const uuid_t p_uuidSourceNugget,
                       const uuid_t p_uuidDestNugget)
{
    ASSERT (p_pMessage != NULL);

    p_pMessage->ccmhMessageHeader.mhHeader.iType = MESSAGE_TYPE_BYE;
    p_pMessage->ccmhMessageHeader.mhHeader.iLength =
        CcMessageHeader_BinaryLength (&p_pMessage->ccmhMessageHeader);
    p_pMessage->ccmhMessageHeader.mhHeader.iVersion = MESSAGE_VERSION_1;
    uuid_copy (p_pMessage->ccmhMessageHeader.uuidSourceNugget,
               p_uuidSourceNugget);
    uuid_copy (p_pMessage->ccmhMessageHeader.uuidDestNugget,
               p_uuidDestNugget);
}
SO_PUBLIC void
MessageCacheClear_Initialize (struct MessageCacheClear *p_pMessage,
                       const uuid_t p_uuidSourceNugget)
{
    ASSERT (p_pMessage != NULL);

    p_pMessage->mhHeader.mhHeader.iType = MESSAGE_TYPE_CLEAR;
    p_pMessage->mhHeader.mhHeader.iLength =
        CcMessageHeader_BinaryLength (&p_pMessage->mhHeader);
    p_pMessage->mhHeader.mhHeader.iVersion = MESSAGE_VERSION_1;
    uuid_copy (p_pMessage->mhHeader.uuidSourceNugget,
               p_uuidSourceNugget);
    uuid_clear (p_pMessage->mhHeader.uuidDestNugget);

}
SO_PUBLIC void
MessageError_Initialize (struct MessageError *p_pMessage,
                        uint32_t p_iCode,
                        const char *p_sMessage,
                       const uuid_t p_uuidSourceNugget,
                       const uuid_t p_uuidDestNugget)
{
    ASSERT (p_pMessage != NULL);

    p_pMessage->ccmhHeader.mhHeader.iType = p_iCode;
    p_pMessage->ccmhHeader.mhHeader.iLength =
        CcMessageHeader_BinaryLength (&p_pMessage->ccmhHeader) +
        strlen(p_sMessage)+1;
    p_pMessage->ccmhHeader.mhHeader.iVersion = MESSAGE_VERSION_1;
    uuid_copy (p_pMessage->ccmhHeader.uuidSourceNugget,
               p_uuidSourceNugget);
    uuid_copy (p_pMessage->ccmhHeader.uuidDestNugget,
               p_uuidDestNugget);
    p_pMessage->sMessage = (uint8_t *)p_sMessage;
}

SO_PUBLIC void
MessageCC_Destroy (union CcMessageUnion *p_pMessage)
{
    ASSERT (p_pMessage != NULL);

    switch (p_pMessage->mchHeader.mhHeader.iType)
    {
    case MESSAGE_TYPE_TERM:
        MessageTerminate_Destroy ((struct MessageTerminate *) p_pMessage);
        break;
    case MESSAGE_TYPE_STATS_RESP:
        MessageStatsResponse_Destroy ((struct MessageStatsResponse *)
                                      p_pMessage);
        break;
    case MESSAGE_TYPE_REG_REQ:
        MessageRegistrationRequest_Destroy ((struct MessageRegistrationRequest
                                             *) p_pMessage);
        break;
    case MESSAGE_TYPE_CONFIG_UPDATE:
        MessageConfigurationUpdate_Destroy ((struct MessageConfigurationUpdate
                                             *) p_pMessage);
        break;
    default:
        break;
    };
}
