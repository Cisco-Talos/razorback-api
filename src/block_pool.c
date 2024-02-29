#include "config.h"
#include <razorback/debug.h>
#include <razorback/block_pool.h>
#include <razorback/log.h>
#include <razorback/hash.h>
#include <razorback/uuids.h>
#include <razorback/ntlv.h>
#include <razorback/block_id.h>

#include "block_pool_private.h"

#include <sys/mman.h>

struct BlockPoolItem_ListNode 
{
    struct BlockPoolItem *pItem;
    struct BlockPoolItem_ListNode *pNext;
};

static struct BlockPoolItem_ListNode *sg_pHead = NULL;
static struct BlockPoolItem_ListNode *sg_pTail = NULL;
static pthread_mutex_t sg_mListMutex = PTHREAD_MUTEX_INITIALIZER;
    
SO_PUBLIC struct BlockPoolItem *
BlockPool_CreateItem (struct RazorbackContext *p_pContext)
{
    struct BlockPoolItem *l_pItem;
    struct BlockPoolItem_ListNode *l_pNode;
    pthread_mutex_lock(&sg_mListMutex);

    if ((l_pItem = calloc(1, sizeof(struct BlockPoolItem))) == NULL)
    {
        rzb_log(LOG_ERR, "BlockPool_CreateItem: Failed to allocate new item");
        pthread_mutex_unlock(&sg_mListMutex);
        return NULL;
    }
    pthread_mutex_init(&l_pItem->mutex, NULL);
    l_pItem->iStatus = BLOCK_POOL_STATUS_COLLECTING;
    if ((l_pItem->pHash = Hash_Create()) == NULL)
    {
        rzb_log(LOG_ERR, "BlockPool_CreateItem: Failed to allocate item hash");
        BlockPool_DestroyItem(l_pItem);
        pthread_mutex_unlock(&sg_mListMutex);
        return NULL;
    }

    if ((l_pItem->pMetaDataList = NTLVList_Create()) == NULL)
    {
        rzb_log(LOG_ERR, "BlockPool_CreateItem: Failed to allocate item meta data");
        BlockPool_DestroyItem(l_pItem);
        pthread_mutex_unlock(&sg_mListMutex);
        return NULL;
    }
    
    uuid_copy(l_pItem->uuidNuggetId, p_pContext->uuidNuggetId);
    uuid_copy(l_pItem->uuidApplicationType, p_pContext->uuidApplicationType);
    if ((l_pNode = calloc(1, sizeof(struct BlockPoolItem_ListNode))) == NULL)
    {
        rzb_log(LOG_ERR, "BlockPool_CreateItem: Failed to allocate new list item");
        BlockPool_DestroyItem(l_pItem);
        pthread_mutex_unlock(&sg_mListMutex);
        return NULL;
    }

    l_pNode->pItem = l_pItem;
    if (sg_pHead == NULL)
    {
        sg_pHead = l_pNode;
        sg_pTail = l_pNode;
    }
    else
    {
        sg_pTail->pNext = l_pNode;
        sg_pTail = l_pNode;
    }

    pthread_mutex_unlock(&sg_mListMutex);
    return l_pItem;
}

SO_PUBLIC bool 
BlockPool_SetItemDataType(struct BlockPoolItem *p_pItem, char * p_sName)
{
    uuid_t *l_pUuid;
    if ((l_pUuid = UUID_Get_UUID(p_sName, UUID_TYPE_DATA_TYPE)) == NULL)
    {
        rzb_log(LOG_ERR, "BlockPool_SetItemDataType: Invalid data type name");
        return false;
    }
    uuid_copy(p_pItem->uuidDataType, *l_pUuid); 
    return true;
}

SO_PUBLIC bool 
BlockPool_AddData (struct BlockPoolItem *p_pItem, uint8_t * p_pData,
                               uint32_t p_iLength, int p_iFlags)
{
    ASSERT (p_pItem != NULL);
    ASSERT (p_pData != NULL);
    ASSERT (p_iLength > 0);
    ASSERT (p_pItem->iStatus == BLOCK_POOL_STATUS_COLLECTING);

    struct BlockPoolData *l_pData;
    pthread_mutex_lock(&p_pItem->mutex);
    if (p_pItem->iStatus != BLOCK_POOL_STATUS_COLLECTING)
    {
        rzb_log(LOG_ERR, "BlockPool_AddData: failed: item not collecting");
        pthread_mutex_unlock(&p_pItem->mutex);
        return false;
    }
    if ((l_pData = calloc(1, sizeof(struct BlockPoolData))) == NULL) 
    {
        rzb_log(LOG_ERR, "BlockPool_AddData: failed to allocate data time");
        pthread_mutex_unlock(&p_pItem->mutex);
        return false;
    }
    p_pItem->iLength += p_iLength;
    l_pData->iLength = p_iLength;
    l_pData->iFlags = p_iFlags;
    l_pData->pData = p_pData;
    Hash_Update(p_pItem->pHash, p_pData, p_iLength);

    if (p_pItem->pDataHead == NULL)
    {
        p_pItem->pDataHead = l_pData;
        p_pItem->pDataTail = l_pData;
    }
    else
    {
        p_pItem->pDataTail->pNext = l_pData;
        p_pItem->pDataTail = l_pData;
    }

    pthread_mutex_unlock(&p_pItem->mutex);
    return true;
}

SO_PUBLIC bool 
BlockPool_FinalizeItem (struct BlockPoolItem *p_pItem)
{
    ASSERT (p_pItem->iStatus == BLOCK_POOL_STATUS_COLLECTING);
    if (p_pItem->iStatus != BLOCK_POOL_STATUS_COLLECTING)
    {
        rzb_log(LOG_ERR, "BlockBool_FinalizeItem: failed: item not collecting");
        return false;
    }
    if (!Hash_Finalize(p_pItem->pHash))
    {
        rzb_log(LOG_ERR, "BlockPool_FinalizeItem: Failed to finalize hash");
        return false;
    }
    p_pItem->iStatus = BLOCK_POOL_STATUS_FINALIZED;
    return true;
}

SO_PUBLIC bool 
BlockPool_DestroyItem (struct BlockPoolItem *p_pItem)
{
    struct BlockPoolItem_ListNode *l_pCurrent;
    struct BlockPoolItem_ListNode *l_pLast;
    struct BlockPoolData *l_pData;
    pthread_mutex_lock(&p_pItem->mutex);
    pthread_mutex_lock(&sg_mListMutex);
    l_pCurrent = sg_pHead;
    l_pLast = NULL;
    while (l_pCurrent != NULL)
    {
        if (l_pCurrent->pItem == p_pItem)
        {
            if (l_pLast == NULL)
                sg_pHead = l_pCurrent->pNext;
            else 
                l_pLast->pNext = l_pCurrent->pNext;

            if (l_pCurrent == sg_pTail)
                sg_pTail = l_pLast;

            break;
        }
        l_pLast = l_pCurrent;
        l_pCurrent = l_pCurrent->pNext;
    }
    if (l_pCurrent == NULL)
        rzb_log(LOG_ERR, "BlockPool_DestroyItem: Item not in list");

    pthread_mutex_unlock(&sg_mListMutex);
    pthread_mutex_unlock(&p_pItem->mutex);

    if (p_pItem->pHash != NULL)
        Hash_Destroy(p_pItem->pHash);

    if (p_pItem->pMetaDataList != NULL)
        NTLVList_Destroy(p_pItem->pMetaDataList);

    if (p_pItem->bidParent != NULL)
        BlockId_Destroy(p_pItem->bidParent);

    l_pData = p_pItem->pDataHead;
    while (l_pData != NULL)
    {
        switch (l_pData->iFlags)
        {
        case BLOCK_POOL_DATA_FLAG_MMAPED:
            munmap(l_pData->pData, l_pData->iLength);
            break;
        case BLOCK_POOL_DATA_FLAG_MALLOCD:
            free(l_pData->pData);
            break;
        default:
            rzb_log(LOG_ERR, "BlockPool_DestroyItem: Failed to free block data");
            break;
        }
        l_pData = l_pData->pNext;
    }

    pthread_mutex_destroy(&p_pItem->mutex);
    free(p_pItem);
    return true;
}

void
BlockPool_ForEachItem(int (*function) (struct BlockPoolItem *))
{
    struct BlockPoolItem_ListNode *l_pCurrent;
    struct BlockPoolItem_ListNode *l_pLast;
    struct BlockPoolItem_ListNode *l_pDeleteList = NULL;
    pthread_mutex_lock(&sg_mListMutex);
    l_pCurrent = sg_pHead;
    while (l_pCurrent != NULL)
    {
        pthread_mutex_lock(&l_pCurrent->pItem->mutex);
        if (function(l_pCurrent->pItem) == BLOCK_POOL_DESTROY) 
        {
            // Add the node to the delete list
            if (l_pDeleteList == NULL)
            {
                l_pDeleteList = l_pCurrent;
                l_pCurrent->pNext = NULL;
            }
            else
            {
                l_pCurrent->pNext = l_pDeleteList;
                l_pDeleteList = l_pCurrent;
            }

        }
        pthread_mutex_unlock(&l_pCurrent->pItem->mutex);
        l_pLast = l_pCurrent;
        l_pCurrent = l_pCurrent->pNext;
    }
    pthread_mutex_unlock(&sg_mListMutex);
    while (l_pDeleteList != NULL)
    {
        l_pCurrent = l_pDeleteList;
        l_pDeleteList = l_pDeleteList->pNext;
        BlockPool_DestroyItem(l_pCurrent->pItem);
    }
}
