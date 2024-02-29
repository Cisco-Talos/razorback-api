#include "config.h"
#include <razorback/debug.h>
#include <razorback/block.h>
#include <razorback/block_id.h>
#include <razorback/ntlv.h>
#include <razorback/log.h>

#include <string.h>
#include <stdio.h>


SO_PUBLIC struct Block *
Block_Create (void)
{
    struct Block * l_pBlock;
    if ((l_pBlock = calloc (1, sizeof (struct Block))) == NULL )
    {
        rzb_log(LOG_ERR, "Block_Create: Failed to allocate memory for new Block");
        return NULL;
    }
    if ((l_pBlock->pMetaDataList = NTLVList_Create ()) == NULL) 
    {
        rzb_log(LOG_ERR, "Block_Create: Failed to create metadata list");
        free(l_pBlock);
        return NULL;
    }
    return l_pBlock;
}

SO_PUBLIC void
Block_Destroy (struct Block *p_pBlock)
{

    BlockId_Destroy (&p_pBlock->bidId);

    if (p_pBlock->bidParent != NULL)
    {
        BlockId_Destroy (p_pBlock->bidParent);
        free (p_pBlock->bidParent);
    }

    if (p_pBlock->pData != NULL)
        free (p_pBlock->pData);
    
    NTLVList_Destroy (p_pBlock->pMetaDataList);
    free(p_pBlock);
}

SO_PUBLIC bool
Block_Copy (struct Block *p_pDestination, const struct Block *p_pSource)
{
    ASSERT (p_pDestination != NULL);
    ASSERT (p_pSource != NULL);

    if (!BlockId_Copy (&p_pDestination->bidId, &p_pSource->bidId))
    {
        rzb_log (LOG_ERR, "Block_Copy failed due to failure of BlockId_Copy (BID)");
        return false;
    };

    if (p_pSource->bidParent == NULL)
        p_pDestination->bidParent = NULL;
    else
    {
        if ((p_pDestination->bidParent =
             malloc (sizeof (struct BlockId))) == NULL)
        {
            Block_Destroy (p_pDestination);
            rzb_log (LOG_ERR, "Block_Copy failed due to lack of memory");
            return false;
        }

        if (!BlockId_Copy (p_pDestination->bidParent, p_pSource->bidParent))
        {
            Block_Destroy (p_pDestination);
            rzb_log (LOG_ERR,
                     "Block_Copy failed due to failure of BlockId_Copy (Parent)");
            return false;
        }

    }

    if (p_pSource->pData == NULL)
        p_pDestination->pData = NULL;
    else
    {
        if ((p_pDestination->pData =
             malloc (p_pSource->bidId.iLength)) == NULL)
        {
            Block_Destroy (p_pDestination);
            rzb_log (LOG_ERR, "Block_Copy failed due to lack of memory");
            return false;
        }
        memcpy (p_pDestination->pData, p_pSource->pData,
                p_pSource->bidId.iLength);

    }

    if (!NTLVList_Copy
        (p_pDestination->pMetaDataList, p_pSource->pMetaDataList))
    {
        Block_Destroy (p_pDestination);
        rzb_log (LOG_ERR,
                 "Block_Copy failed due to failure of NTLVList_Copy");
        return false;
    }

    return true;
}

SO_PUBLIC uint32_t
Block_BinaryLength (struct Block * p_pBlock)
{
    uint32_t l_iSize = 0;
    l_iSize += BlockId_BinaryLength (&p_pBlock->bidId);
    l_iSize += sizeof (uint8_t);    // 0 or 1: 1 if parent ID present
    if (p_pBlock->bidParent != NULL)
        l_iSize += BlockId_BinaryLength (p_pBlock->bidParent);
    l_iSize += NTLVList_Size (p_pBlock->pMetaDataList);
    l_iSize += sizeof (uint8_t);    // 0 or 1: 1 if data present
    if (p_pBlock->pData != NULL || p_pBlock->pPoolItem !=NULL)
        l_iSize += p_pBlock->bidId.iLength;

    return l_iSize;
}
