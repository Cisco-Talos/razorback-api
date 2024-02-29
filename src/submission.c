#include "config.h"
#include <razorback/debug.h>
#include <razorback/submission.h>
#include <razorback/log.h>
#include <razorback/thread.h>
#include <razorback/api.h>
#include <razorback/block.h>
#include <razorback/cache.h>
#include <razorback/queue.h>
#include <razorback/input_queue.h>
#include <razorback/block_id.h>
#include <razorback/request_queue.h>
#include <razorback/response_queue.h>

#include "block_pool_private.h"
#include "submission_private.h"
#include "local_cache.h"

#include <semaphore.h>
#include <string.h>

void Submission_GlobalCache_RequestThread(struct Thread *p_pThread);
int Submission_GlobalCache_RequestHandler(struct BlockPoolItem *p_pItem);
void Submission_GlobalCache_ResponseThread(struct Thread *p_pThread);
int Submission_GlobalCache_ResponseHandler(struct BlockPoolItem *p_pItem);
void Submission_SubmitThread(struct Thread *p_pThread);
int Submission_SubmitHandler(struct BlockPoolItem *p_pItem);

static sem_t sg_sRequestSem;
static sem_t sg_sResponseSem; // Temp Simulation of GC req->resp
static sem_t sg_sSubmitSem;
static struct Queue *sg_pResponseQueue;
static struct RazorbackContext *sg_pContext;
static bool sg_bInitDone=false;

struct CacheResult
{
    uint32_t iSfFlags;
    uint32_t iEntFlags;
    struct BlockId blockId;
};

void
Submission_Init(struct RazorbackContext *p_pContext)
{
    if (sg_bInitDone) 
        return;
    sg_pContext = p_pContext;
    BlockPool_Init(p_pContext);

    memset(&sg_sRequestSem,0,sizeof(sem_t));
    memset(&sg_sResponseSem,0,sizeof(sem_t)); // Temp
    memset(&sg_sSubmitSem,0,sizeof(sem_t));
    
    InputQueue_Initialize(QUEUE_FLAG_SEND);
    RequestQueue_Initialize(QUEUE_FLAG_SEND);
    sg_pResponseQueue = ResponseQueue_Initialize(p_pContext->uuidNuggetId, QUEUE_FLAG_RECV);

    sem_init(&sg_sRequestSem, 0,0);
    sem_init(&sg_sResponseSem, 0,0); //Temp
    sem_init(&sg_sSubmitSem, 0,0);
    Thread_Launch(Submission_GlobalCache_RequestThread, NULL, (char *)"Submission GC Request Thread", p_pContext);
    Thread_Launch(Submission_GlobalCache_ResponseThread, NULL, (char *)"Submission GC Response Thread", p_pContext);
    Thread_Launch(Submission_SubmitThread, NULL, (char *)"Submission Submit Thread", p_pContext);
    sg_bInitDone = true;


}

SO_PUBLIC bool 
Submission_Submit(struct BlockPoolItem *p_pItem, int p_iFlags, uint32_t *p_pSf_Flags, uint32_t *p_pEnt_Flags)
{
	Lookup_Result result;
	uint32_t sfflags = 0;
	uint32_t entflags = 0;
    
	if (p_pSf_Flags == NULL || p_pEnt_Flags == NULL) {
		rzb_log(LOG_ERR, "Submission_Submit: NULL pointer arguments to function");
		//Probably need more stuff here
		return false;
	}
	
    // Set up the data type.
	pthread_mutex_lock(&p_pItem->mutex);
    if (uuid_is_null(p_pItem->bidBlock.uuidDataType) == 1)
    {
        rzb_log(LOG_INFO, "Submission_SubmitHandler: Submission with null data type dropped.");
        if (p_pItem->submittedCallback != NULL)
            p_pItem->submittedCallback(p_pItem);

        pthread_mutex_unlock(&p_pItem->mutex);
        BlockPool_DestroyItem(p_pItem);
        return false;
    }

    result = checkLocalEntry(p_pItem->bidBlock.pHash->pData, p_pItem->bidBlock.pHash->iSize,
			                 &sfflags, &entflags, BADHASH);
    if (result != R_FOUND)
    {
        result = checkLocalEntry(p_pItem->bidBlock.pHash->pData, p_pItem->bidBlock.pHash->iSize, 
					   &sfflags, &entflags, GOODHASH);
    }
	if (result == R_FOUND) {
        rzb_log(LOG_INFO, "Local Cache Hit - SF: 0x%08x, ENT: 0x%08x", sfflags, entflags); 
        BlockPool_DestroyItemDataList(p_pItem); // We don't need the data any more.
        BlockPool_SetStatus(p_pItem, BLOCK_POOL_STATUS_SUBMIT_DATA);
        BlockPool_SetFlags(p_pItem, p_iFlags | BLOCK_POOL_FLAG_EVENT_ONLY);
        sem_post(&sg_sSubmitSem);
	}
	else 
	{
        BlockPool_SetStatus(p_pItem, BLOCK_POOL_STATUS_CHECK_GLOBAL_CACHE);
        BlockPool_SetFlags(p_pItem, p_iFlags);
        sem_post(&sg_sRequestSem);
    }

    pthread_mutex_unlock(&p_pItem->mutex);
    *p_pSf_Flags = sfflags;
    *p_pEnt_Flags = entflags;
    return true;
}

int
Submission_GlobalCache_RequestHandler(struct BlockPoolItem *p_pItem)
{
    struct MessageCacheReq l_mcrMessage;
    struct l_pContext;
    if (BlockPool_GetStatus(p_pItem) == BLOCK_POOL_STATUS_CHECK_GLOBAL_CACHE)
    {
        BlockPool_SetStatus(p_pItem, BLOCK_POOL_STATUS_CHECKING_GLOBAL_CACHE);
        memset(&l_mcrMessage, 0, sizeof(struct MessageCacheReq));

        if (!MessageCacheReq_Initialize(&l_mcrMessage, 
                    sg_pContext->uuidNuggetId, &p_pItem->bidBlock)) 
        {
            rzb_log(LOG_ERR, "Submission_GlobalCache_RequestHandler: Failed to initialize cache request");
            return BLOCK_POOL_KEEP;
        }
        RequestQueue_Put(&l_mcrMessage);
    }
    return BLOCK_POOL_KEEP;
}

void 
Submission_GlobalCache_RequestThread(struct Thread *p_pThread)
{
    struct timespec l_tsTimeOut;
    l_tsTimeOut.tv_sec=1;
    while (true)
    {
        //if (sem_timedwait(&sg_sRequestSem, &l_tsTimeOut) == -1)
        //    continue;
        sem_wait(&sg_sRequestSem);
        
        BlockPool_ForEachItem(Submission_GlobalCache_RequestHandler);

    }
}

int
Submission_GlobalCache_ResponseHandler(struct BlockPoolItem *p_pItem)
{
	Lookup_Result result;
    struct Thread *l_pThread;
    struct CacheResult *l_pRes;

    // Pull the data out the thread.
    l_pThread = Thread_GetCurrent();
    l_pRes = (struct CacheResult *)l_pThread->pUserData;

    if (BlockPool_GetStatus(p_pItem) == BLOCK_POOL_STATUS_CHECKING_GLOBAL_CACHE)
    {
        if (BlockId_IsEqual(&p_pItem->bidBlock, &l_pRes->blockId))
        {
			if (l_pRes->iSfFlags & SF_FLAG_BAD) {
				
				if ((result = addLocalEntry (p_pItem->bidBlock.pHash->pData, p_pItem->bidBlock.pHash->iSize, 
							                 l_pRes->iSfFlags & (SF_FLAG_CANHAZ ^ SF_FLAG_ALL), l_pRes->iEntFlags, BADHASH)) != R_SUCCESS) 
				{
					rzb_log(LOG_ERR, "Submission_GlobalCache_ResponseHandler: Could not add to bad cache");
				}
			}
			else 
            {
				if ((result = addLocalEntry (p_pItem->bidBlock.pHash->pData, p_pItem->bidBlock.pHash->iSize, 
								             l_pRes->iSfFlags & (SF_FLAG_CANHAZ ^ SF_FLAG_ALL), l_pRes->iEntFlags, GOODHASH)) != R_SUCCESS)
				{
					rzb_log(LOG_ERR, "Submission_GlobalCache_ResponseHandler: Could not add to good cache");
				}
			}
            rzb_log(LOG_DEBUG, "Got flags SF: 0x%08x, ENT: 0x%08x", l_pRes->iSfFlags, l_pRes->iEntFlags); 
            BlockPool_SetStatus (p_pItem, BLOCK_POOL_STATUS_SUBMIT_DATA);
            if ((l_pRes->iSfFlags & SF_FLAG_CANHAZ) != SF_FLAG_CANHAZ)
                BlockPool_SetFlags(p_pItem, p_pItem->iStatus | BLOCK_POOL_FLAG_EVENT_ONLY);

            sem_post(&sg_sSubmitSem);
        }
    }
    return BLOCK_POOL_KEEP;
}

void 
Submission_GlobalCache_ResponseThread(struct Thread *p_pThread)
{
    struct MessageCacheResp l_mcrMessage;
    struct CacheResult *l_pRes;
    if ((l_pRes = calloc(1, sizeof(struct CacheResult))) == NULL)
    {
        rzb_log(LOG_ERR, "Submission_GlobalCache_ResponseThread: Failed to allocate thread args");
        return;
    }
    p_pThread->pUserData = l_pRes;
    while (true)
    {
        ResponseQueue_Get(sg_pResponseQueue, &l_mcrMessage);
        // Copy the item into thread local storage.
        BlockId_Copy(&l_pRes->blockId, &l_mcrMessage.bidBlock);
        l_pRes->iSfFlags = l_mcrMessage.iSfFlags;
        l_pRes->iEntFlags = l_mcrMessage.iEntFlags;
        BlockPool_ForEachItem(Submission_GlobalCache_ResponseHandler);
        // Destroy allocated items in thread local storage.
        BlockId_Destroy(&l_pRes->blockId);
    }
    free(p_pThread->pUserData);
    p_pThread->pUserData = NULL;

}

int
Submission_SubmitHandler(struct BlockPoolItem *p_pItem)
{
    struct Block *l_pBlock;
    struct MessageBlockSubmission l_mbsSubmit;
    if (BlockPool_GetStatus(p_pItem) == BLOCK_POOL_STATUS_SUBMIT_DATA)
    {
        if ((l_pBlock = Block_Create()) == NULL) 
        {
            rzb_log(LOG_ERR, "Submission_SubmitHandler: Failed to allocate block");
            return BLOCK_POOL_DESTROY;
        }

        BlockId_Copy(&l_pBlock->bidId, &p_pItem->bidBlock);
        
        l_pBlock->bidParent = p_pItem->bidParent;
        p_pItem->bidParent = NULL;
        NTLVList_Destroy(l_pBlock->pMetaDataList);
        l_pBlock->pMetaDataList = p_pItem->pMetaDataList;
        p_pItem->pMetaDataList = NULL;

        l_pBlock->pData = NULL;
        if ((p_pItem->iStatus & (BLOCK_POOL_FLAG_EVENT_ONLY |BLOCK_POOL_FLAG_UPDATE)) != 0)
        {
            rzb_log(LOG_INFO, "Sending Event Only");
            l_pBlock->pPoolItem = NULL;
        }
        else
            l_pBlock->pPoolItem = p_pItem;

        MessageBlockSubmission_Initialize(&l_mbsSubmit, 
            l_pBlock, p_pItem->uuidNuggetId,
            p_pItem->uuidApplicationType, 
            p_pItem->iSeconds, p_pItem->iNanoSecs,
            0);
        if(!InputQueue_Put(&l_mbsSubmit))
            rzb_log(LOG_ERR, "Failed to put message");

        MessageBlockSubmission_Destroy(&l_mbsSubmit);
        l_pBlock->pPoolItem =NULL;

        if (p_pItem->submittedCallback != NULL)
            p_pItem->submittedCallback(p_pItem);

        if ((p_pItem->iStatus & BLOCK_POOL_FLAG_MAY_REUSE) == BLOCK_POOL_FLAG_MAY_REUSE)
        {
            BlockPool_SetStatus(p_pItem, BLOCK_POOL_STATUS_FINALIZED);
            BlockPool_SetFlags(p_pItem, 0);
            BlockPool_DestroyItemDataList(p_pItem);
            return BLOCK_POOL_KEEP;
        }
        else
            return BLOCK_POOL_DESTROY;
    }
    return BLOCK_POOL_KEEP;
}

void
Submission_SubmitThread(struct Thread *p_pThread)
{
    struct timespec l_tsTimeOut;
    l_tsTimeOut.tv_sec=1;
    while (true)
    {
        sem_wait(&sg_sSubmitSem);
        //if (sem_timedwait(&sg_sSubmitSem, &l_tsTimeOut) == -1) 
            //continue;
        
        BlockPool_ForEachItem(Submission_SubmitHandler);

    }
}
