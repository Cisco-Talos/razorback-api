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

#include "block_pool_private.h"
#include "submission_private.h"

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
bool sg_bInitDone=false;
void
Submission_Init(struct RazorbackContext *p_pContext)
{
    if (sg_bInitDone) 
        return;

    memset(&sg_sRequestSem,0,sizeof(sem_t));
    memset(&sg_sResponseSem,0,sizeof(sem_t)); // Temp
    memset(&sg_sSubmitSem,0,sizeof(sem_t));
    
    InputQueue_Initialize(QUEUE_FLAG_SEND);

    sem_init(&sg_sRequestSem, 0,0);
    sem_init(&sg_sResponseSem, 0,0); //Temp
    sem_init(&sg_sSubmitSem, 0,0);
    Thread_Launch(Submission_GlobalCache_RequestThread, NULL, (char *)"Submission GC Request Thread", p_pContext);
    Thread_Launch(Submission_GlobalCache_ResponseThread, NULL, (char *)"Submission GC Response Thread", p_pContext);
    Thread_Launch(Submission_SubmitThread, NULL, (char *)"Submission Submit Thread", p_pContext);
    sg_bInitDone = true;
}

SO_PUBLIC uint32_t
Submission_Submit(struct BlockPoolItem *p_pItem)
{
    pthread_mutex_lock(&p_pItem->mutex);
    p_pItem->iStatus = BLOCK_POOL_STATUS_CHECK_GLOBAL_CACHE;
    pthread_mutex_unlock(&p_pItem->mutex);
    sem_post(&sg_sRequestSem);
    return CACHE_NOT_FOUND;
}

int
Submission_GlobalCache_RequestHandler(struct BlockPoolItem *p_pItem)
{
    if (p_pItem->iStatus == BLOCK_POOL_STATUS_CHECK_GLOBAL_CACHE)
    {
        p_pItem->iStatus = BLOCK_POOL_STATUS_CHECKING_GLOBAL_CACHE;
        // TODO: Send request;
//        rzb_log(LOG_DEBUG, "Sending request");
        sem_post(&sg_sResponseSem); // Remove this when we add GC send.
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
    if (p_pItem->iStatus == BLOCK_POOL_STATUS_CHECKING_GLOBAL_CACHE)
    {
        // Assume MISS, and always submit
        // TODO: Add gc response check.
        p_pItem->iStatus = BLOCK_POOL_STATUS_SUBMIT_DATA;
//        rzb_log(LOG_DEBUG, "got result");
        sem_post(&sg_sSubmitSem);
    }
    return BLOCK_POOL_KEEP;
}

void 
Submission_GlobalCache_ResponseThread(struct Thread *p_pThread)
{
    struct timespec l_tsTimeOut;
    l_tsTimeOut.tv_sec=1;
    while (true)
    {
        sem_wait(&sg_sResponseSem);
//        if (sem_timedwait(&sg_sResponseSem, &l_tsTimeOut) == -1) // Replace with Queue Get
//            continue;
        
        BlockPool_ForEachItem(Submission_GlobalCache_ResponseHandler);

    }

}

int
Submission_SubmitHandler(struct BlockPoolItem *p_pItem)
{
    struct Block *l_pBlock;
    struct MessageBlockSubmission l_mbsSubmit;
    if (p_pItem->iStatus == BLOCK_POOL_STATUS_SUBMIT_DATA)
    {
        if ((l_pBlock = Block_Create()) == NULL) 
        {
            rzb_log(LOG_ERR, "Submission_SubmitHandler: Failed to allocate block");
            return BLOCK_POOL_DESTROY;
        }

        // Set up the data type.
        if (uuid_is_null(p_pItem->uuidDataType) == 1)
        {
            //TODO: Magic time
        }
        else
            uuid_copy(l_pBlock->bidId.uuidDataType, p_pItem->uuidDataType);

        l_pBlock->bidId.iLength = p_pItem->iLength;
//        rzb_log(LOG_DEBUG, "submitting");
        
        // Move the hash over
        l_pBlock->bidId.pHash = p_pItem->pHash;
        p_pItem->pHash = NULL;
        l_pBlock->bidParent = p_pItem->bidParent;
        p_pItem->bidParent = NULL;
        l_pBlock->pMetaDataList = p_pItem->pMetaDataList;
        p_pItem->pMetaDataList = NULL;

        l_pBlock->pData = NULL;
        l_pBlock->pPoolItem = p_pItem;

        MessageBlockSubmission_Initialize(&l_mbsSubmit, 
            l_pBlock, p_pItem->uuidNuggetId,
            p_pItem->uuidApplicationType, 0);
        if(!InputQueue_Put(&l_mbsSubmit))
            rzb_log(LOG_ERR, "Failed to put message");

        MessageBlockSubmission_Destroy(&l_mbsSubmit);
        l_pBlock->pPoolItem =NULL;
        p_pItem->submittedCallback(p_pItem);
        Block_Destroy(l_pBlock);
        return BLOCK_POOL_DESTROY;
        // return destroy value
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
