#include "config.h"
#include <razorback/debug.h>
#include <razorback/block.h>
#include <razorback/block_id.h>
#include <razorback/messages.h>
#include <razorback/hash.h>
#include <razorback/ntlv.h>
#include <razorback/log.h>

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
        BlockId_BinaryLength (&p_pMessage->bidBlock);
    p_pMessage->mhHeader.iVersion = MESSAGE_VERSION_1;

    // fill in rest of message
    uuid_copy (p_pMessage->uuidRequestor, p_uuidRequestor);
    if (!BlockId_Copy (&p_pMessage->bidBlock, p_pBlockId))
    {
        rzb_log (LOG_ERR,
                 "MessageCacheReq_Initialize failed due to failure of BlockId_Copy");
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
    BlockId_Destroy (&p_pMessage->bidBlock);

    // done
}

SO_PUBLIC bool
MessageCacheResp_Initialize (struct MessageCacheResp *p_pMessage,
                             const struct BlockId *p_pBlockId,
                             uint32_t p_iCode)
{
    ASSERT (p_pMessage != NULL);
    ASSERT (p_pBlockId != NULL);

    // calculate header
    p_pMessage->mhHeader.iType = MESSAGE_TYPE_RESP;
    p_pMessage->mhHeader.iLength =
        MessageHeader_BinaryLength (&p_pMessage->mhHeader) +
        BlockId_BinaryLength (&p_pMessage->bidBlock);
    p_pMessage->mhHeader.iVersion = MESSAGE_VERSION_1;

    // fill in rest of message
    if (!BlockId_Copy (&p_pMessage->bidBlock, p_pBlockId))
    {
        rzb_log (LOG_ERR,
                 "MessageCacheResp_Initialize failed due to failure of BlockId_Copy");
        return false;
    }
    p_pMessage->iCode = p_iCode;

    // done
    return true;
}

SO_PUBLIC void
MessageCacheResp_Destroy (struct MessageCacheResp *p_pMessage)
{
    ASSERT (p_pMessage != NULL);

    // destroy any malloc'd components
    BlockId_Destroy (&p_pMessage->bidBlock);

    // done
}

SO_PUBLIC bool
MessageBlockSubmission_Initialize (struct MessageBlockSubmission *p_pMessage,
                                   struct Block *p_pBlock,
                                   uuid_t p_uuidNuggetId,
                                   uuid_t p_uuidApplicationType,
                                   uint32_t p_iReason)
{
    ASSERT (p_pMessage != NULL);
    ASSERT (p_pBlock != NULL);

    p_pMessage->pBlock = p_pBlock;

    // calculate header
    p_pMessage->mhHeader.iType = MESSAGE_TYPE_BLOCK;
    p_pMessage->mhHeader.iLength =
        MessageHeader_BinaryLength (&p_pMessage->mhHeader) +
        Block_BinaryLength (p_pMessage->pBlock) +
        (uint32_t) sizeof (p_pMessage->uuidNuggetId) +
        (uint32_t) sizeof (p_pMessage->uuidApplicationType) +
        (uint32_t) sizeof (p_pMessage->iReason) +
        NTLVList_Size (p_pBlock->pMetaDataList);
    if (p_pBlock->bidParent != NULL)
        p_pMessage->mhHeader.iLength +=
            BlockId_BinaryLength (p_pBlock->bidParent);
    if (p_pBlock->pData != NULL)
        p_pMessage->mhHeader.iLength += p_pBlock->bidId.iLength;
    p_pMessage->mhHeader.iVersion = MESSAGE_VERSION_1;

    uuid_copy (p_pMessage->uuidNuggetId, p_uuidNuggetId);
    uuid_copy (p_pMessage->uuidApplicationType, p_uuidApplicationType);
    p_pMessage->iReason = p_iReason;

    return true;
}

SO_PUBLIC void
MessageBlockSubmission_Destroy (struct MessageBlockSubmission *p_pMessage)
{
    ASSERT (p_pMessage != NULL);

    // destroy any malloc'd components
//    Block_Destroy (p_pMessage->pBlock);
    // done
}

SO_PUBLIC bool
MessageJudgmentSubmission_Initialize (struct MessageJudgmentSubmission
                                      *p_pMessage,
                                      const struct Block *p_pBlock,
                                      uint32_t p_iDisposition,
                                      const uuid_t p_uuidInspectorId,
                                      const uuid_t p_uuidApplicationId)
{
    ASSERT (p_pMessage != NULL);
    ASSERT (p_pBlock != NULL);

    if ((p_pMessage->pBlock = Block_Create()) == NULL)
        return false;

    // fill in rest of message
    if (!Block_Copy (p_pMessage->pBlock, p_pBlock))
    {
        rzb_log (LOG_ERR,
                 "MessageJudgmentSubmission_Initialize failed due to failure of Block_Copy");
        return false;
    }

    // calculate header
    p_pMessage->mhHeader.iType = MESSAGE_TYPE_JUDGMENT;
    p_pMessage->mhHeader.iLength =
        MessageHeader_BinaryLength (&p_pMessage->mhHeader) +
        Block_BinaryLength (p_pMessage->pBlock) +
        (uint32_t) sizeof (p_pMessage->iDisposition) +
        (uint32_t) sizeof (p_pMessage->uuidInspectorId) +
        (uint32_t) sizeof (p_pMessage->uuidApplicationId);
    p_pMessage->mhHeader.iVersion = MESSAGE_VERSION_1;

    p_pMessage->iDisposition = p_iDisposition;
    uuid_copy (p_pMessage->uuidInspectorId, p_uuidInspectorId);
    uuid_copy (p_pMessage->uuidApplicationId, p_uuidApplicationId);

    return true;
}

SO_PUBLIC void
MessageJudgmentSubmission_Destroy (struct MessageJudgmentSubmission
                                   *p_pMessage)
{
    ASSERT (p_pMessage != NULL);

    // destroy any malloc'd components
    Block_Destroy (p_pMessage->pBlock);

    // done
}

SO_PUBLIC bool
MessageInspectionSubmission_Initialize (struct MessageInspectionSubmission
                                        *p_pMessage,
                                        const struct Block *p_pBlock,
                                        const uuid_t p_uuidApplicationType,
                                        uint32_t p_iReason)
{
    ASSERT (p_pMessage != NULL);
    ASSERT (p_pBlock != NULL);

    if ((p_pMessage->pBlock = Block_Create()) == NULL)
        return false;

    // fill in rest of message
    if (!Block_Copy (p_pMessage->pBlock, p_pBlock))
    {
        rzb_log (LOG_ERR,
                 "MessageInspectionSubmission_Initialize failed due to failure of Block_Copy");
        return false;
    }
    // calculate header
    p_pMessage->mhHeader.iType = MESSAGE_TYPE_INSPECTION;
    p_pMessage->mhHeader.iLength =
        MessageHeader_BinaryLength (&p_pMessage->mhHeader) +
        Block_BinaryLength (p_pMessage->pBlock) +
        (uint32_t) sizeof (uuid_t) +
        (uint32_t) sizeof (uint8_t) + (uint32_t) sizeof (p_pMessage->iReason);

    p_pMessage->mhHeader.iVersion = MESSAGE_VERSION_1;

    uuid_copy (p_pMessage->uuidApplicationType, p_uuidApplicationType);
    p_pMessage->iReason = p_iReason;

    // done
    return true;
}

SO_PUBLIC void
MessageInspectionSubmission_Destroy (struct MessageInspectionSubmission
                                     *p_pMessage)
{
    ASSERT (p_pMessage != NULL);

    // destroy any malloc'd components
    Block_Destroy (p_pMessage->pBlock);
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

    if ((p_pMessage->pBlock= Block_Create()) == NULL)
        return false;

    // fill in rest of message
    if (!Block_Copy (p_pMessage->pBlock, p_pBlock))
    {
        rzb_log (LOG_ERR,
                 "MessageAlert_Initialize failed due to failure of Block_Copy");
        return false;
    }
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
    rzb_log (LOG_DEBUG, "Datatypes: %i", p_iDataTypeCount);
    if (p_iDataTypeCount > 0)
    {
        if ((p_pMessage->pDataTypeList =
             malloc (sizeof (uuid_t) * p_iDataTypeCount)) == NULL)
        {
            rzb_log (LOG_ERR,
                     "MessageRegistrationRequest_Initialize failed due to lack of memory");
            return false;
        }
    }
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
                                       const uuid_t p_uuidDestNugget,
                                       const struct NTLVList
                                       *p_ntlvConfigurationList)
{
    ASSERT (p_pMessage != NULL);
    ASSERT (p_ntlvConfigurationList != NULL);

    p_pMessage->ccmhMessageHeader.mhHeader.iType = MESSAGE_TYPE_CONFIG_UPDATE;
    p_pMessage->ccmhMessageHeader.mhHeader.iLength =
        CcMessageHeader_BinaryLength (&p_pMessage->ccmhMessageHeader) +
        NTLVList_Size(p_ntlvConfigurationList);
    p_pMessage->ccmhMessageHeader.mhHeader.iVersion = MESSAGE_VERSION_1;
    uuid_copy (p_pMessage->ccmhMessageHeader.uuidSourceNugget,
               p_uuidSourceNugget);
    uuid_copy (p_pMessage->ccmhMessageHeader.uuidDestNugget,
               p_uuidDestNugget);
    // treat NTLV list as uninitialized
    if ((p_pMessage->ntlvConfigurationList = NTLVList_Create() ) == NULL )
        return false;

    if (!NTLVList_Copy
        (p_pMessage->ntlvConfigurationList, p_ntlvConfigurationList))
    {
        rzb_log (LOG_ERR,
                 "MessageConfigurationUpdate_Initialize failed due to failure of NTLVList_Copy");
        NTLVList_Destroy(p_pMessage->ntlvConfigurationList);
        return false;
    }
    return true;
}

SO_PUBLIC void
MessageConfigurationUpdate_Destroy (struct MessageConfigurationUpdate
                                    *p_pMessage)
{
    ASSERT (p_pMessage != NULL);

    NTLVList_Destroy (p_pMessage->ntlvConfigurationList);
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
                 "MessageTerminate_Initialize failed due to lack of memory");
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
