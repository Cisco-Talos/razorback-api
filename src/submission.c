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
#include <razorback/thread_pool.h>

#include "block_pool_private.h"
#include "submission_private.h"
#include "local_cache.h"

#include <semaphore.h>
#include <string.h>

struct ThreadPool *requestThreadPool= NULL;
struct ThreadPool *responseThreadPool= NULL;
struct ThreadPool *submissionThreadPool = NULL;

void Submission_GlobalCache_RequestThread(struct Thread *p_pThread);
int Submission_GlobalCache_RequestHandler(struct BlockPoolItem *p_pItem);
void Submission_GlobalCache_ResponseThread(struct Thread *p_pThread);
int Submission_GlobalCache_ResponseHandler(struct BlockPoolItem *p_pItem);
void Submission_SubmitThread(struct Thread *p_pThread);
int Submission_SubmitHandler(struct BlockPoolItem *p_pItem);

static sem_t sg_sRequestSem;
static sem_t sg_sSubmitSem;

static struct RazorbackContext *sg_pContext;
static bool sg_bInitDone=false;

struct CacheResult
{
    uint32_t iSfFlags;
    uint32_t iEntFlags;
    struct BlockId *pId;
};

bool
Submission_Init(struct RazorbackContext *p_pContext)
{
    
    if (sg_bInitDone) 
        return true;

    sg_pContext = p_pContext;
    if (!BlockPool_Init(p_pContext))
    {
        rzb_log(LOG_ERR, "%s: Failed initialize block pool", __func__);
        return false;
    }


    memset(&sg_sRequestSem,0,sizeof(sem_t));
    memset(&sg_sSubmitSem,0,sizeof(sem_t));
    sem_init(&sg_sRequestSem, 0,0);
    sem_init(&sg_sSubmitSem, 0,0);

    requestThreadPool = ThreadPool_Create(1,100, p_pContext, "GC Request Thread (%d)", Submission_GlobalCache_RequestThread);
    responseThreadPool = ThreadPool_Create(1,100, p_pContext, "GC Response Thread (%d)", Submission_GlobalCache_ResponseThread);
    submissionThreadPool = ThreadPool_Create(1,100, p_pContext, "Submission Thread (%d)", Submission_SubmitThread);

    sg_bInitDone = true;

    return true;
}

SO_PUBLIC bool 
Submission_Submit(struct BlockPoolItem *p_pItem, int p_iFlags, uint32_t *p_pSf_Flags, uint32_t *p_pEnt_Flags)
{
	Lookup_Result result;
	uint32_t sfflags = 0;
	uint32_t entflags = 0;
    
	if (p_pSf_Flags == NULL || p_pEnt_Flags == NULL) {
		rzb_log(LOG_ERR, "%s: NULL pointer arguments to function", __func__);
		//Probably need more stuff here
		return false;
	}
	
    // Set up the data type.
	pthread_mutex_lock(&p_pItem->mutex);
    if (uuid_is_null(p_pItem->pEvent->pBlock->pId->uuidDataType) == 1)
    {
        rzb_log(LOG_INFO, "%s: Submission with null data type dropped.", __func__);
        if (p_pItem->submittedCallback != NULL)
            p_pItem->submittedCallback(p_pItem);

        pthread_mutex_unlock(&p_pItem->mutex);
        BlockPool_DestroyItem(p_pItem);
        return false;
    }

    result = checkLocalEntry(p_pItem->pEvent->pBlock->pId->pHash->pData, p_pItem->pEvent->pBlock->pId->pHash->iSize,
			                 &sfflags, &entflags, BADHASH);
    if (result != R_FOUND)
    {
        result = checkLocalEntry(p_pItem->pEvent->pBlock->pId->pHash->pData, p_pItem->pEvent->pBlock->pId->pHash->iSize,
					   &sfflags, &entflags, GOODHASH);
    }
	if (result == R_FOUND) {
        rzb_log(LOG_INFO, "%s: Local Cache Hit - SF: 0x%08x, ENT: 0x%08x", __func__, sfflags, entflags); 
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
    struct MessageCacheReq *message;
    struct Thread *thread = Thread_GetCurrent();
    if (BlockPool_GetStatus(p_pItem) == BLOCK_POOL_STATUS_CHECK_GLOBAL_CACHE)
    {
        BlockPool_SetStatus(p_pItem, BLOCK_POOL_STATUS_CHECKING_GLOBAL_CACHE);

        if (( message = MessageCacheReq_Initialize(
                    sg_pContext->uuidNuggetId, p_pItem->pEvent->pBlock->pId)) == NULL)
        {
            rzb_log(LOG_ERR, "%s: Failed to initialize cache request", __func__);
            return BLOCK_POOL_KEEP;
        }
        RequestQueue_Put((struct Queue *)thread->pUserData, message);
        MessageCacheReq_Destroy(message);
    }
    return BLOCK_POOL_KEEP;
}

void 
Submission_GlobalCache_RequestThread(struct Thread *p_pThread)
{
    struct timespec l_tsTimeOut;
    l_tsTimeOut.tv_sec=1;

    if ((p_pThread->pUserData = RequestQueue_Initialize(QUEUE_FLAG_SEND)) == NULL)
        return;

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
    struct Thread *l_pThread;
    struct CacheResult *l_pRes;

    // Pull the data out the thread.
    l_pThread = Thread_GetCurrent();
    l_pRes = (struct CacheResult *)l_pThread->pUserData;

    if (BlockPool_GetStatus(p_pItem) == BLOCK_POOL_STATUS_CHECKING_GLOBAL_CACHE)
    {
        if (BlockId_IsEqual(p_pItem->pEvent->pBlock->pId, l_pRes->pId))
        {
			if (l_pRes->iSfFlags & SF_FLAG_BAD) {
				
				if (addLocalEntry (p_pItem->pEvent->pBlock->pId->pHash->pData, p_pItem->pEvent->pBlock->pId->pHash->iSize, 
							                 l_pRes->iSfFlags & (SF_FLAG_CANHAZ ^ SF_FLAG_ALL), l_pRes->iEntFlags, BADHASH) != R_SUCCESS) 
				{
					rzb_log(LOG_ERR, "%s: Could not add to bad cache", __func__);
				}
			}
			else 
            {
				if (addLocalEntry (p_pItem->pEvent->pBlock->pId->pHash->pData, p_pItem->pEvent->pBlock->pId->pHash->iSize, 
								             l_pRes->iSfFlags & (SF_FLAG_CANHAZ ^ SF_FLAG_ALL), l_pRes->iEntFlags, GOODHASH) != R_SUCCESS)
				{
					rzb_log(LOG_ERR, "%s: Could not add to good cache", __func__);
				}
			}
            rzb_log(LOG_DEBUG, "%s: Got flags SF: 0x%08x, ENT: 0x%08x", __func__, l_pRes->iSfFlags, l_pRes->iEntFlags); 
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
    struct MessageCacheResp *l_mcrMessage;
    struct CacheResult *l_pRes;
    struct Queue *queue;
    // TODO: Not what we think it is!!
    if ((queue = ResponseQueue_Initialize(sg_pContext->uuidNuggetId, QUEUE_FLAG_RECV)) == NULL)
        return;

    if ((l_pRes = calloc(1, sizeof(struct CacheResult))) == NULL)
    {
        rzb_log(LOG_ERR, "%s: Failed to allocate thread args", __func__);
        return;
    }
    p_pThread->pUserData = l_pRes;
    while (true)
    {
        if ((l_mcrMessage = ResponseQueue_Get(queue)) == NULL)
            continue;
        // Copy the item into thread local storage.
        l_pRes->pId = l_mcrMessage->pId;
        l_pRes->iSfFlags = l_mcrMessage->iSfFlags;
        l_pRes->iEntFlags = l_mcrMessage->iEntFlags;
        BlockPool_ForEachItem(Submission_GlobalCache_ResponseHandler);
        // Destroy allocated items in thread local storage.
        MessageCacheResp_Destroy(l_mcrMessage);
    }
    free(p_pThread->pUserData);
    p_pThread->pUserData = NULL;
    // TODO: BAD
    ResponseQueue_Terminate(sg_pContext->uuidNuggetId);
}

int
Submission_SubmitHandler(struct BlockPoolItem *p_pItem)
{
    struct MessageBlockSubmission *l_mbsSubmit;
    struct Thread *thread = Thread_GetCurrent();
    if (BlockPool_GetStatus(p_pItem) == BLOCK_POOL_STATUS_SUBMIT_DATA)
    {

        if ((p_pItem->iStatus & (BLOCK_POOL_FLAG_EVENT_ONLY |BLOCK_POOL_FLAG_UPDATE)) != 0)
        {
            rzb_log(LOG_INFO, "%s: Sending Event Only", __func__);
            // Break the link
            p_pItem->pEvent->pBlock->pPoolItem = NULL;
        }

        if ((l_mbsSubmit = MessageBlockSubmission_Initialize( p_pItem->pEvent, 0)) == NULL)
        {
            rzb_log(LOG_ERR, "%s: Failed to create message", __func__);
            return BLOCK_POOL_DESTROY;
        }

        if(!InputQueue_Put((struct Queue *)thread->pUserData, l_mbsSubmit))
            rzb_log(LOG_ERR, "%s: Failed to put message", __func__);

        // Set the event to null so we dont destroy it.
        l_mbsSubmit->pEvent = NULL;
        MessageBlockSubmission_Destroy(l_mbsSubmit);

        if (p_pItem->submittedCallback != NULL)
            p_pItem->submittedCallback(p_pItem);

        if ((p_pItem->iStatus & BLOCK_POOL_FLAG_MAY_REUSE) == BLOCK_POOL_FLAG_MAY_REUSE)
        {
            BlockPool_SetStatus(p_pItem, BLOCK_POOL_STATUS_FINALIZED);
            BlockPool_SetFlags(p_pItem, 0);
            BlockPool_DestroyItemDataList(p_pItem);
            p_pItem->pEvent->pBlock->pPoolItem = NULL;
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
    if ((p_pThread->pUserData = InputQueue_Initialize(QUEUE_FLAG_SEND)) == NULL)
        return;

    while (true)
    {
        sem_wait(&sg_sSubmitSem);
        //if (sem_timedwait(&sg_sSubmitSem, &l_tsTimeOut) == -1) 
            //continue;
        
        BlockPool_ForEachItem(Submission_SubmitHandler);

    }
}
