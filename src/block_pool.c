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
#include <magic.h>
#include <string.h>
#include <time.h>

#define MAGIC_FILE ETC_DIR "/magic"

struct BlockPoolItem_ListNode 
{
    struct BlockPoolItem *pItem;
    struct BlockPoolItem_ListNode *pNext;
};

static struct BlockPoolItem_ListNode *sg_pHead = NULL;
static struct BlockPoolItem_ListNode *sg_pTail = NULL;
static pthread_mutex_t sg_mListMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t sg_mMagicMutex = PTHREAD_MUTEX_INITIALIZER;
static magic_t sg_magicCookie;
static bool sg_bInitDone=false;

void
BlockPool_Init(struct RazorbackContext *p_pContext)
{
    if (sg_bInitDone) 
        return;

    if ((sg_magicCookie = magic_open(MAGIC_NO_CHECK_CDF|MAGIC_NO_CHECK_TAR)) == NULL)
    {
        rzb_perror("Error creating magic_cookie: %s");
    }

    if (magic_load(sg_magicCookie, MAGIC_FILE) != 0)
    {
        magic_close(sg_magicCookie);
        return;
    }
    sg_bInitDone = true;
}

SO_PUBLIC struct BlockPoolItem *
BlockPool_CreateItem (struct RazorbackContext *p_pContext)
{
    struct BlockPoolItem *l_pItem;
    struct BlockPoolItem_ListNode *l_pNode;
    struct timespec l_tsTime;

    memset(&l_tsTime, 0, sizeof(struct timespec));
    if (clock_gettime(CLOCK_REALTIME, &l_tsTime) == -1)
    {
        rzb_log(LOG_ERR, "BlockPool_CreateItem: Failed to get time stamp");
        return NULL;
    }

    if ((l_pItem = calloc(1, sizeof(struct BlockPoolItem))) == NULL)
    {
        rzb_log(LOG_ERR, "BlockPool_CreateItem: Failed to allocate new item");
        return NULL;
    }
    pthread_mutex_init(&l_pItem->mutex, NULL);

    l_pItem->iStatus = BLOCK_POOL_STATUS_COLLECTING;
    l_pItem->iSeconds = l_tsTime.tv_sec;
    l_pItem->iNanoSecs = l_tsTime.tv_nsec;

    if ((l_pItem->bidBlock.pHash = Hash_Create()) == NULL)
    {
        rzb_log(LOG_ERR, "BlockPool_CreateItem: Failed to allocate item hash");
        BlockPool_DestroyItem(l_pItem);
        return NULL;
    }

    if ((l_pItem->pMetaDataList = NTLVList_Create()) == NULL)
    {
        rzb_log(LOG_ERR, "BlockPool_CreateItem: Failed to allocate item meta data");
        BlockPool_DestroyItem(l_pItem);
        return NULL;
    }
    
    uuid_copy(l_pItem->uuidNuggetId, p_pContext->uuidNuggetId);
    uuid_copy(l_pItem->uuidApplicationType, p_pContext->uuidApplicationType);
    if ((l_pNode = calloc(1, sizeof(struct BlockPoolItem_ListNode))) == NULL)
    {
        rzb_log(LOG_ERR, "BlockPool_CreateItem: Failed to allocate new list item");
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
    uuid_t *l_pUuid;
    if ((l_pUuid = UUID_Get_UUID(p_sName, UUID_TYPE_DATA_TYPE)) == NULL)
    {
        rzb_log(LOG_ERR, "BlockPool_SetItemDataType: Invalid data type name");
        return false;
    }
    uuid_copy(p_pItem->bidBlock.uuidDataType, *l_pUuid); 
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
    p_pItem->bidBlock.iLength += p_iLength;
    l_pData->iLength = p_iLength;
    l_pData->iFlags = p_iFlags;
    l_pData->pData = p_pData;
    Hash_Update(p_pItem->bidBlock.pHash, p_pData, p_iLength);

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
    const char *l_sMagicFull;
    uuid_t *l_pUuid;

    if (p_pItem->iStatus != BLOCK_POOL_STATUS_COLLECTING)
    {
        rzb_log(LOG_ERR, "BlockBool_FinalizeItem: failed: item not collecting");
        return false;
    }
    if (!Hash_Finalize(p_pItem->bidBlock.pHash))
    {
        rzb_log(LOG_ERR, "BlockPool_FinalizeItem: Failed to finalize hash");
        return false;
    }

    if (uuid_is_null(p_pItem->bidBlock.uuidDataType) == 1 && p_pItem->pDataHead != NULL)
    {
        pthread_mutex_lock(&sg_mMagicMutex);
        if ((l_sMagicFull = magic_buffer(sg_magicCookie, p_pItem->pDataHead->pData, 
                        p_pItem->pDataHead->iLength)) == NULL)
        {
            rzb_perror("Error reading file type: %s");
            pthread_mutex_unlock(&sg_mMagicMutex);
            return false;
        }
        pthread_mutex_unlock(&sg_mMagicMutex);
        if ((l_pUuid = UUID_Get_UUID(l_sMagicFull, UUID_TYPE_DATA_TYPE)) == NULL)
            return false;

        uuid_copy(p_pItem->bidBlock.uuidDataType, *l_pUuid);
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
            rzb_log(LOG_ERR, "BlockPool_DestroyItem: Failed to free block data");
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
        rzb_log(LOG_ERR, "BlockPool_RemoveFromList: Item not in list");
    return l_pCurrent;
}

void
BlockPool_DestroyItemData(struct BlockPoolItem *p_pItem)
{
    if (p_pItem->bidBlock.pHash != NULL)
        Hash_Destroy(p_pItem->bidBlock.pHash);

    if (p_pItem->pMetaDataList != NULL)
        NTLVList_Destroy(p_pItem->pMetaDataList);

    if (p_pItem->bidParent != NULL)
        BlockId_Destroy(p_pItem->bidParent);

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
        rzb_log(LOG_ERR, "BlockPool_DestroyItem: Item not in list");
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
    struct BlockPoolItem_ListNode *l_pLast;
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
        l_pLast = l_pCurrent;
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

