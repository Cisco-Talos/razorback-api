#include "config.h"
#include <razorback/debug.h>
#include <razorback/block_pool.h>
#include <razorback/log.h>
#include <razorback/hash.h>
#include <razorback/uuids.h>
#include <razorback/ntlv.h>
#include <razorback/metadata.h>
#include <razorback/block_id.h>
#include <razorback/event.h>

#include "block_pool_private.h"
#include "fantasia.h"
#include <sys/mman.h>
#include <magic.h>
#include <string.h>

struct BlockPoolItem_ListNode 
{
    struct BlockPoolItem *pItem;
    struct BlockPoolItem_ListNode *pNext;
};

static struct BlockPoolItem_ListNode *sg_pHead = NULL;
static struct BlockPoolItem_ListNode *sg_pTail = NULL;
static pthread_mutex_t sg_mListMutex = PTHREAD_MUTEX_INITIALIZER;
static bool sg_bInitDone=false;

bool
BlockPool_Init(struct RazorbackContext *p_pContext)
{
    if (sg_bInitDone) 
        return true;

    sg_bInitDone = true;
    return true;
}

SO_PUBLIC struct BlockPoolItem *
BlockPool_CreateItem (struct RazorbackContext *p_pContext)
{
    struct BlockPoolItem *l_pItem;
    struct BlockPoolItem_ListNode *l_pNode;

    if ((l_pItem = calloc(1, sizeof(struct BlockPoolItem))) == NULL)
    {
        rzb_log(LOG_ERR, "%s: Failed to allocate new item", __func__);
        return NULL;
    }

    if ((l_pItem->pEvent = Event_Create()) == NULL)
    {
        rzb_log(LOG_ERR, "%s: Failed to allocate new event", __func__);
        return NULL;
    }

    l_pItem->pEvent->pBlock->pPoolItem = l_pItem;

    pthread_mutex_init(&l_pItem->mutex, NULL);

    l_pItem->iStatus = BLOCK_POOL_STATUS_COLLECTING;

    uuid_copy(l_pItem->pEvent->pId->uuidNuggetId, p_pContext->uuidNuggetId);
    uuid_copy(l_pItem->pEvent->uuidApplicationType, p_pContext->uuidApplicationType);
    if ((l_pNode = calloc(1, sizeof(struct BlockPoolItem_ListNode))) == NULL)
    {
        rzb_log(LOG_ERR, "%s: Failed to allocate new list item", __func__);
        BlockPool_DestroyItem(l_pItem);
        return NULL;
    }
    pthread_mutex_lock(&sg_mListMutex);

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
    uuid_t l_pUuid;
    if (!UUID_Get_UUID(p_sName, UUID_TYPE_DATA_TYPE, l_pUuid))
    {
        rzb_log(LOG_ERR, "%s: Invalid data type name", __func__);
        return false;
    }
    uuid_copy(p_pItem->pEvent->pBlock->pId->uuidDataType, l_pUuid); 
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
        rzb_log(LOG_ERR, "%s: failed: item not collecting", __func__);
        pthread_mutex_unlock(&p_pItem->mutex);
        return false;
    }
    if ((l_pData = calloc(1, sizeof(struct BlockPoolData))) == NULL) 
    {
        rzb_log(LOG_ERR, "%s: failed to allocate data time", __func__);
        pthread_mutex_unlock(&p_pItem->mutex);
        return false;
    }
    p_pItem->pEvent->pBlock->pId->iLength += p_iLength;
    l_pData->iLength = p_iLength;
    l_pData->iFlags = p_iFlags;
    l_pData->pData = p_pData;
    Hash_Update(p_pItem->pEvent->pBlock->pId->pHash, p_pData, p_iLength);

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
    //const char *l_sMagicFull;

    if (p_pItem->iStatus != BLOCK_POOL_STATUS_COLLECTING)
    {
        rzb_log(LOG_ERR, "%s: failed: item not collecting", __func__);
        return false;
    }
    if (!Hash_Finalize(p_pItem->pEvent->pBlock->pId->pHash))
    {
        rzb_log(LOG_ERR, "%s: Failed to finalize hash", __func__);
        return false;
    }

    if (uuid_is_null(p_pItem->pEvent->pBlock->pId->uuidDataType) == 1 && p_pItem->pDataHead != NULL)
    {
        Magic_process(p_pItem);
    }

    p_pItem->iStatus = BLOCK_POOL_STATUS_FINALIZED;
    return true;
}
void
BlockPool_DestroyItemDataList(struct BlockPoolItem *p_pItem) 
{
    struct BlockPoolData *l_pData;
    while (p_pItem->pDataHead != NULL)
    {
        l_pData = p_pItem->pDataHead;
        p_pItem->pDataHead = p_pItem->pDataHead->pNext;
        switch (l_pData->iFlags)
        {
        case BLOCK_POOL_DATA_FLAG_MMAPED:
            munmap(l_pData->pData, l_pData->iLength);
            break;
        case BLOCK_POOL_DATA_FLAG_MALLOCD:
            free(l_pData->pData);
            break;
        case BLOCK_POOL_DATA_FLAG_MANAGED:
            break;
        default:
            rzb_log(LOG_ERR, "%s: Failed to free block data", __func__);
            break;
        }
        free(l_pData);
    }
}

struct BlockPoolItem_ListNode *
BlockPool_RemoveFromList(struct BlockPoolItem *p_pItem)
{
    struct BlockPoolItem_ListNode *l_pCurrent;
    struct BlockPoolItem_ListNode *l_pLast;
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
        rzb_log(LOG_ERR, "%s: Item not in list", __func__);
    return l_pCurrent;
}

void
BlockPool_DestroyItemData(struct BlockPoolItem *p_pItem)
{
    if (p_pItem->pEvent != NULL)
        Event_Destroy(p_pItem->pEvent);

    BlockPool_DestroyItemDataList(p_pItem);
    pthread_mutex_destroy(&p_pItem->mutex);
    free(p_pItem);
}

SO_PUBLIC bool 
BlockPool_DestroyItem (struct BlockPoolItem *p_pItem)
{
    struct BlockPoolItem_ListNode *l_pCurrent;
    pthread_mutex_lock(&sg_mListMutex);
    pthread_mutex_lock(&p_pItem->mutex);
    l_pCurrent = BlockPool_RemoveFromList(p_pItem);
    if (l_pCurrent == NULL)
        rzb_log(LOG_ERR, "%s: Item not in list", __func__);
    else 
        free(l_pCurrent);

    pthread_mutex_unlock(&p_pItem->mutex);
    pthread_mutex_unlock(&sg_mListMutex);
    BlockPool_DestroyItemData(p_pItem);
    return true;
}

void
BlockPool_ForEachItem(int (*function) (struct BlockPoolItem *))
{
    struct BlockPoolItem_ListNode *l_pCurrent;
    struct BlockPoolItem_ListNode *l_pDeleteList = NULL;
    pthread_mutex_lock(&sg_mListMutex);
    l_pCurrent = sg_pHead;
    while (l_pCurrent != NULL)
    {
        pthread_mutex_lock(&l_pCurrent->pItem->mutex);
        if (function(l_pCurrent->pItem) == BLOCK_POOL_DESTROY) 
        {
            BlockPool_RemoveFromList(l_pCurrent->pItem);
            
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
        l_pCurrent = l_pCurrent->pNext;
    }
    pthread_mutex_unlock(&sg_mListMutex);
    while (l_pDeleteList != NULL)
    {
        l_pCurrent = l_pDeleteList;
        l_pDeleteList = l_pDeleteList->pNext;
        BlockPool_DestroyItemData(l_pCurrent->pItem);
        free(l_pCurrent);
    }
}

void 
BlockPool_SetStatus(struct BlockPoolItem *p_pItem, uint32_t p_iStatus)
{
    // Set the status bits retaining the flags bits
    p_pItem->iStatus = (p_iStatus & BLOCK_POOL_STATUS_MASK ) |
        ( p_pItem->iStatus & BLOCK_POOL_FLAG_MASK);
}
uint32_t 
BlockPool_GetStatus(struct BlockPoolItem *p_pItem)
{
    // Set the status bits retaining the flags bits
    return (p_pItem->iStatus & BLOCK_POOL_STATUS_MASK );
}
void 
BlockPool_SetFlags(struct BlockPoolItem *p_pItem, uint32_t p_iFlags)
{
    p_pItem->iStatus = ( p_iFlags & BLOCK_POOL_FLAG_MASK ) |
        ( p_pItem->iStatus & BLOCK_POOL_STATUS_MASK);
}


