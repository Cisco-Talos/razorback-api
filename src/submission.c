#include "config.h"
#include <razorback/debug.h>
#include <razorback/submission.h>
#include <razorback/lock.h>
#include <razorback/log.h>
#include <razorback/thread.h>
#include <razorback/api.h>
#include <razorback/block.h>
#include <razorback/block_id.h>
#include <razorback/cache.h>
#include <razorback/queue.h>
#include <razorback/block_id.h>
#include <razorback/thread_pool.h>
#include <razorback/response_queue.h>

#include "block_pool_private.h"
#include "submission_private.h"
#include "local_cache.h"
#include "connected_entity_private.h"
#include "transfer/core.h"

#include <string.h>

struct ThreadPool *requestThreadPool= NULL;
struct ThreadPool *responseThreadPool= NULL;
struct ThreadPool *submissionThreadPool = NULL;

void Submission_GlobalCache_RequestThread(struct Thread *p_pThread);
int Submission_GlobalCache_RequestHandler(struct BlockPoolItem *p_pItem, void *);
void Submission_GlobalCache_ResponseThread(struct Thread *p_pThread);
int Submission_GlobalCache_ResponseHandler(struct BlockPoolItem *p_pItem, void *);
void Submission_SubmitThread(struct Thread *p_pThread);
int Submission_SubmitHandler(struct BlockPoolItem *p_pItem, void *);

static struct Semaphore * sg_sRequestSem;
static struct Semaphore * sg_sSubmitSem;

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

    sg_sRequestSem = Semaphore_Create(false,0);
    sg_sSubmitSem = Semaphore_Create(false ,0);

    requestThreadPool = ThreadPool_Create(1,100, p_pContext, "GC Request Thread (%d)", Submission_GlobalCache_RequestThread);
    responseThreadPool = ThreadPool_Create(1,100, p_pContext, "GC Response Thread (%d)", Submission_GlobalCache_ResponseThread);
    submissionThreadPool = ThreadPool_Create(1,100, p_pContext, "Submission Thread (%d)", Submission_SubmitThread);

    sg_bInitDone = true;

    return true;
}

SO_PUBLIC int
Submission_Submit(struct BlockPoolItem *p_pItem, int p_iFlags, uint32_t *p_pSf_Flags, uint32_t *p_pEnt_Flags)
{
	Lookup_Result result;
	uint32_t sfflags = 0;
	uint32_t entflags = 0;
	Mutex_Lock(p_pItem->mutex);

    if ( (p_pItem->pEvent->pBlock->pParentId != NULL ) &&
            BlockId_IsEqual(p_pItem->pEvent->pBlock->pId, p_pItem->pEvent->pBlock->pParentId) )
    {
        // You shall not pass!!!
        rzb_log(LOG_ERR, "%s: Block submission listing its self as parent dropped.", __func__);
        Mutex_Unlock(p_pItem->mutex);
        BlockPool_DestroyItem(p_pItem);
        return RZB_SUBMISSION_ERROR;

    }
    
	if (p_pSf_Flags == NULL || p_pEnt_Flags == NULL) {
		rzb_log(LOG_ERR, "%s: NULL pointer arguments to function", __func__);
        Mutex_Unlock(p_pItem->mutex);
        BlockPool_DestroyItem(p_pItem);
		return RZB_SUBMISSION_ERROR;
	}
	
    // Set up the data type.
    if (uuid_is_null(p_pItem->pEvent->pBlock->pId->uuidDataType) == 1)
    {
        rzb_log(LOG_INFO, "%s: Submission with null data type dropped.", __func__);
        if (p_pItem->submittedCallback != NULL)
            p_pItem->submittedCallback(p_pItem);

        Mutex_Unlock(p_pItem->mutex);
        BlockPool_DestroyItem(p_pItem);
        return RZB_SUBMISSION_NO_TYPE;
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
        Semaphore_Post(sg_sSubmitSem);
	}
	else 
	{
        BlockPool_SetStatus(p_pItem, BLOCK_POOL_STATUS_CHECK_GLOBAL_CACHE);
        BlockPool_SetFlags(p_pItem, p_iFlags);
        Semaphore_Post(sg_sRequestSem);
    }

    Mutex_Unlock(p_pItem->mutex);
    *p_pSf_Flags = sfflags;
    *p_pEnt_Flags = entflags;
    return RZB_SUBMISSION_OK;
}

int
Submission_GlobalCache_RequestHandler(struct BlockPoolItem *p_pItem, void *userData)
{
    struct Message *message;
    struct Thread *thread = Thread_GetCurrent();
    if (BlockPool_GetStatus(p_pItem) == BLOCK_POOL_STATUS_CHECK_GLOBAL_CACHE)
    {
        BlockPool_SetStatus(p_pItem, BLOCK_POOL_STATUS_CHECKING_GLOBAL_CACHE);

        if (( message = MessageCacheReq_Initialize(
                    sg_pContext->uuidNuggetId, p_pItem->pEvent->pBlock->pId)) == NULL)
        {
            rzb_log(LOG_ERR, "%s: Failed to initialize cache request", __func__);
            Thread_Destroy(thread);
            return LIST_EACH_OK;
        }
        Queue_Put((struct Queue *)thread->pUserData, message);
        message->destroy(message);
    }

    Thread_Destroy(thread);
    return LIST_EACH_OK;
}

void 
Submission_GlobalCache_RequestThread(struct Thread *p_pThread)
{
#if 0
    struct timespec l_tsTimeOut;
    l_tsTimeOut.tv_sec=1;
#endif
    if ((p_pThread->pUserData = Queue_Create(REQUEST_QUEUE, QUEUE_FLAG_SEND, Razorback_Get_Message_Mode())) == NULL)
        return;

    while (true)
    {
        //if (sem_timedwait(&sg_sRequestSem, &l_tsTimeOut) == -1)
        //    continue;
        Semaphore_Wait(sg_sRequestSem);
        
        BlockPool_ForEachItem(Submission_GlobalCache_RequestHandler, NULL);

    }
}

int
Submission_GlobalCache_ResponseHandler(struct BlockPoolItem *p_pItem, void * userData)
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

            Semaphore_Post(sg_sSubmitSem);
        }
    }
    Thread_Destroy(l_pThread);
    return LIST_EACH_OK;
}

void 
Submission_GlobalCache_ResponseThread(struct Thread *p_pThread)
{
    struct Message *message;
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
        if ((message = Queue_Get(queue)) == NULL)
            continue;
        l_mcrMessage = message->message;
        // Copy the item into thread local storage.
        l_pRes->pId = l_mcrMessage->pId;
        l_pRes->iSfFlags = l_mcrMessage->iSfFlags;
        l_pRes->iEntFlags = l_mcrMessage->iEntFlags;
        BlockPool_ForEachItem(Submission_GlobalCache_ResponseHandler, NULL);
        // Destroy allocated items in thread local storage.
        message->destroy(message);
    }
    free(p_pThread->pUserData);
    p_pThread->pUserData = NULL;
    // TODO: BAD
    ResponseQueue_Terminate(sg_pContext->uuidNuggetId);
}

int
Submission_SubmitHandler(struct BlockPoolItem *p_pItem, void *userData)
{
    struct Message *message;
    struct Thread *thread = Thread_GetCurrent();
    uint8_t storedLocality = 0;
    struct ConnectedEntity *dispatcher = NULL;
    bool transfered = false;
    uint32_t reason = 0;

    if (BlockPool_GetStatus(p_pItem) == BLOCK_POOL_STATUS_SUBMIT_DATA)
    {

        if ((p_pItem->iStatus & (BLOCK_POOL_FLAG_EVENT_ONLY |BLOCK_POOL_FLAG_UPDATE)) != 0)
        {
            rzb_log(LOG_INFO, "%s: Sending Event Only", __func__);
            // Break the link
            p_pItem->pEvent->pBlock->pPoolItem = NULL;
            reason = SUBMISSION_REASON_EVENT;
        }
        else
        {
            while (!transfered)
            {
                dispatcher = ConnectedEntityList_GetDispatcher();
                if (dispatcher == NULL)
                {
                    rzb_log(LOG_ERR, "%s: Failed to find usable dispatcher", __func__);
                    transfered = false;
                    break;
                }
                transfered = Transfer_Store(p_pItem, dispatcher);
                if (!transfered)
                {
                    rzb_log(LOG_ERR, "%s: Marking dispatcher unusable", __func__);
                    ConnectedEntityList_MarkDispatcherUnusable(dispatcher->uuidNuggetId);
                }
            }
            if (!transfered)
            {
                rzb_log(LOG_ERR, "%s: Failed to transfer block giving up", __func__);
                if (p_pItem->submittedCallback != NULL)
                    p_pItem->submittedCallback(p_pItem);
                Thread_Destroy(thread);
                return LIST_EACH_REMOVE;
            }
            storedLocality = dispatcher->locality;
            reason = SUBMISSION_REASON_REQUESTED;
        }

        if ((message = MessageBlockSubmission_Initialize( p_pItem->pEvent, reason, storedLocality)) == NULL)
        {
            rzb_log(LOG_ERR, "%s: Failed to create message", __func__);
            if (p_pItem->submittedCallback != NULL)
                p_pItem->submittedCallback(p_pItem);
            Thread_Destroy(thread);
            return LIST_EACH_REMOVE;
        }

        if(!Queue_Put((struct Queue *)thread->pUserData, message))
            rzb_log(LOG_ERR, "%s: Failed to put message", __func__);

        // Set the event to null so we dont destroy it.
        ((struct MessageBlockSubmission*)message->message)->pEvent = NULL;

        message->destroy(message);

        if (p_pItem->submittedCallback != NULL)
            p_pItem->submittedCallback(p_pItem);

        if ((p_pItem->iStatus & BLOCK_POOL_FLAG_MAY_REUSE) == BLOCK_POOL_FLAG_MAY_REUSE)
        {
            BlockPool_SetStatus(p_pItem, BLOCK_POOL_STATUS_FINALIZED);
            BlockPool_SetFlags(p_pItem, 0);
            BlockPool_DestroyItemDataList(p_pItem);
            p_pItem->pEvent->pBlock->pPoolItem = NULL;
            Thread_Destroy(thread);
            return LIST_EACH_OK;
        }
        else
        {
            Thread_Destroy(thread);
            return LIST_EACH_REMOVE;
        }
    }
    Thread_Destroy(thread);
    return LIST_EACH_OK;
}

void
Submission_SubmitThread(struct Thread *p_pThread)
{
#if 0
    struct timespec l_tsTimeOut;
    l_tsTimeOut.tv_sec=1;
#endif
    if ((p_pThread->pUserData = Queue_Create(INPUT_QUEUE, QUEUE_FLAG_SEND, Razorback_Get_Message_Mode())) == NULL)
        return;

    while (true)
    {
        Semaphore_Wait(sg_sSubmitSem);
        //if (sem_timedwait(&sg_sSubmitSem, &l_tsTimeOut) == -1) 
            //continue;
        
        BlockPool_ForEachItem(Submission_SubmitHandler, NULL);

    }
}
