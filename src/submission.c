/*
 * Copyright (c) 2011-2026 Cisco Systems, Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *  MA 02110-1301, USA.
 */

#include "config.h"
#include <razorback/debug.h>
#include <razorback/submission.h>
#include <razorback/lock.h>
#include <razorback/log.h>
#include <razorback/thread.h>
#include <razorback/api.h>
#include <razorback/block.h>
#include <razorback/block_id.h>
#include <razorback/queue.h>
#include <razorback/block_id.h>
#include <razorback/thread_pool.h>
#include <razorback/response_queue.h>

#include "block_pool_private.h"
#include "submission_private.h"
#include "local_cache.h"
#include "connected_entity_private.h"
#include "transfer/core.h"
#include "runtime_config.h"
#include "telemetry.h"
#include <string.h>

struct ThreadPool *requestThreadPool= NULL;
struct ThreadPool *responseThreadPool= NULL;
struct ThreadPool *submissionThreadPool = NULL;

void Submission_GlobalCache_RequestThread(Thread_t *p_pThread);
void Submission_GlobalCache_ResponseThread(Thread_t *p_pThread);
int Submission_GlobalCache_ResponseHandler(struct BlockPoolItem *p_pItem, void *);
void Submission_SubmitThread(Thread_t *p_pThread);

static List_t *requestQueue = NULL;
static List_t *submitQueue = NULL;
static List_t *requestTiming = NULL;
static Mutex_t *requestTimingLock = NULL;

static struct RazorbackContext *sg_pContext = NULL;
static bool sg_bInitDone=false;

struct CacheLookupTiming
{
    struct BlockPoolItem *item;
    double startedAt;
};

struct CacheResult
{
    uint32_t iSfFlags;
    uint32_t iEntFlags;
    struct BlockId *pId;
    uint32_t matchedCount;
};

static int Submission_CacheLookupTiming_KeyCmp(void *a, const void *key);
static int Submission_CacheLookupTiming_Cmp(void *a, void *b);
static void Submission_CacheLookupTiming_Destroy(void *item);
static void Submission_RecordCacheLookupStart(struct BlockPoolItem *item);
static void Submission_RecordCacheLookupWait(struct BlockPoolItem *item);
static const char *Submission_Reason_Label(uint32_t reason);

static int
Submission_CacheLookupTiming_KeyCmp(void *a, const void *key)
{
    struct CacheLookupTiming *entry = a;
    const struct BlockPoolItem *item = key;

    if (entry == NULL || item == NULL)
        return -1;

    return (entry->item == item) ? 0 : -1;
}

static int
Submission_CacheLookupTiming_Cmp(void *a, void *b)
{
    struct CacheLookupTiming *left = a;
    struct CacheLookupTiming *right = b;

    if (left == NULL || right == NULL)
        return -1;

    return (left->item == right->item) ? 0 : -1;
}

static void
Submission_CacheLookupTiming_Destroy(void *item)
{
    free(item);
}

static void
Submission_RecordCacheLookupStart(struct BlockPoolItem *item)
{
    struct CacheLookupTiming *entry;
    struct CacheLookupTiming *existing;

    if (item == NULL || requestTiming == NULL || requestTimingLock == NULL)
        return;

    if ((entry = calloc(1, sizeof(struct CacheLookupTiming))) == NULL)
        return;

    entry->item = item;
    entry->startedAt = Telemetry_GetMonotonicTimeSeconds();

    Mutex_Lock(requestTimingLock);
    existing = List_Find(requestTiming, item);
    if (existing != NULL) {
        List_Remove(requestTiming, existing);
        free(existing);
    }
    if (!List_Push(requestTiming, entry)) {
        free(entry);
    }
    Mutex_Unlock(requestTimingLock);
}

static void
Submission_RecordCacheLookupWait(struct BlockPoolItem *item)
{
    struct CacheLookupTiming *entry;
    double duration;

    if (item == NULL || requestTiming == NULL || requestTimingLock == NULL)
        return;

    Mutex_Lock(requestTimingLock);
    entry = List_Find(requestTiming, item);
    if (entry == NULL) {
        Mutex_Unlock(requestTimingLock);
        return;
    }

    duration = Telemetry_GetMonotonicTimeSeconds() - entry->startedAt;
    List_Remove(requestTiming, entry);
    free(entry);
    Mutex_Unlock(requestTimingLock);

    Telemetry_RecordCacheLookupWait(duration);
}

static const char *
Submission_Reason_Label(uint32_t reason)
{
    switch (reason) {
    case SUBMISSION_REASON_EVENT:
        return "event";
    case SUBMISSION_REASON_REQUESTED:
        return "requested";
    default:
        return "unknown";
    }
}

bool
Submission_Init(struct RazorbackContext *p_pContext)
{

    if (sg_bInitDone)
        return true;

    sg_pContext = p_pContext;
    if (!BlockPool_Init(p_pContext))
    {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed initialize block pool", __func__);
        return false;
    }

    requestQueue = List_Create(LIST_MODE_QUEUE,
            NULL, // Cmp
            NULL, // KeyCmp
            NULL, // Destroy
            NULL, // Clone
            NULL, // lock
            NULL); // Unlock
    submitQueue = List_Create(LIST_MODE_QUEUE,
            NULL, // Cmp
            NULL, // KeyCmp
            NULL, // Destroy
            NULL, // Clone
            NULL, // lock
            NULL); // Unlock
    requestTiming = List_Create(LIST_MODE_GENERIC,
            Submission_CacheLookupTiming_Cmp,
            Submission_CacheLookupTiming_KeyCmp,
            Submission_CacheLookupTiming_Destroy,
            NULL, // Clone
            NULL, // lock
            NULL); // Unlock
    requestTimingLock = Mutex_Create(MUTEX_MODE_NORMAL);
    if (requestQueue == NULL || submitQueue == NULL ||
        requestTiming == NULL || requestTimingLock == NULL)
    {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to initialize submission queues",
                __func__);
        return false;
    }

    requestThreadPool = ThreadPool_Create(
            Config_getSubGcReqThreadsInit(),
            Config_getSubGcReqThreadsMax(),
            p_pContext,
            "GC Request Thread (%d)",
            Submission_GlobalCache_RequestThread);

    responseThreadPool = ThreadPool_Create(
            Config_getSubGcRespThreadsInit(),
            Config_getSubGcRespThreadsMax(),
            p_pContext,
            "GC Response Thread (%d)",
            Submission_GlobalCache_ResponseThread);

    submissionThreadPool = ThreadPool_Create(
            Config_getSubTransferThreadsInit(),
            Config_getSubTransferThreadsMax(),
            p_pContext,
            "Submission Transfer Thread (%d)",
            Submission_SubmitThread);

    sg_bInitDone = true;

    return true;
}

size_t
Submission_GetSubmitQueueDepth(void)
{
    return (submitQueue == NULL) ? 0 : List_Length(submitQueue);
}

SO_PUBLIC int
Submission_Submit(struct BlockPoolItem *p_pItem, int p_iFlags, uint32_t *p_pSf_Flags, uint32_t *p_pEnt_Flags)
{
    Lookup_Result result;
    uint32_t sfflags = 0;
    uint32_t entflags = 0;
    BlockPool_Item_Lock(p_pItem);
    Telemetry_UpdateContext(&p_pItem->telemetryContext);

    if ( (p_pItem->pEvent->pBlock->pParentId != NULL ) &&
            BlockId_IsEqual(p_pItem->pEvent->pBlock->pId, p_pItem->pEvent->pBlock->pParentId) )
    {
        // You shall not pass!!!
        rzb_log(LOG_ERR,LOG_C_CORE, "%s: Block submission listing its self as parent dropped.", __func__);
        BlockPool_Item_Unlock(p_pItem);
        BlockPool_DestroyItem(p_pItem);
        return RZB_SUBMISSION_ERROR;

    }

    if (p_pSf_Flags == NULL || p_pEnt_Flags == NULL) {
        rzb_log(LOG_ERR,LOG_C_CORE, "%s: NULL pointer arguments to function", __func__);
        BlockPool_Item_Unlock(p_pItem);
        BlockPool_DestroyItem(p_pItem);
        return RZB_SUBMISSION_ERROR;
    }

    // Set up the data type.
    if (uuid_is_null(p_pItem->pEvent->pBlock->pId->uuidDataType) == 1)
    {
        rzb_log(LOG_INFO,LOG_C_CORE, "%s: Submission with null data type dropped.", __func__);
        Telemetry_RecordBlockSubmitDecision("invalid_datatype");
        if (p_pItem->submittedCallback != NULL)
            p_pItem->submittedCallback(p_pItem);

        BlockPool_Item_Unlock(p_pItem);
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
        rzb_log(LOG_INFO,LOG_C_CORE, "%s: Local Cache Hit - SF: 0x%08x, ENT: 0x%08x", __func__, sfflags, entflags);
        Telemetry_RecordBlockSubmitDecision("event_only");
        BlockPool_DestroyItemDataList(p_pItem->pDataHead); // We don't need the data any more.
        p_pItem->pDataHead = NULL;
        p_pItem->pDataTail = NULL;
        BlockPool_SetStatus(p_pItem, BLOCK_POOL_STATUS_SUBMIT_DATA);
        BlockPool_SetFlags(p_pItem, p_iFlags | BLOCK_POOL_FLAG_EVENT_ONLY);
        List_Push(submitQueue, p_pItem);
    }
    else
    {
        Telemetry_RecordBlockSubmitDecision("cache_miss");
        BlockPool_SetStatus(p_pItem, BLOCK_POOL_STATUS_CHECK_GLOBAL_CACHE);
        BlockPool_SetFlags(p_pItem, p_iFlags);
        List_Push(requestQueue, p_pItem);
        Submission_RecordCacheLookupStart(p_pItem);
    }

    BlockPool_Item_Unlock(p_pItem);
    *p_pSf_Flags = sfflags;
    *p_pEnt_Flags = entflags;
    return RZB_SUBMISSION_OK;
}

void
Submission_GlobalCache_RequestThread(Thread_t *p_pThread)
{
    struct BlockPoolItem *item = NULL;
    struct Queue *queue = NULL;
    struct Message *message = NULL;
    struct TelemetrySpan *itemSpan = NULL;
    bool itemSuccess = false;
    const char *itemError = NULL;
#if 0
    struct timespec l_tsTimeOut;
    l_tsTimeOut.tv_sec=1;
#endif
    if ((queue = Queue_Create(REQUEST_QUEUE, false, QUEUE_FLAG_SEND)) == NULL)
        return;

    while (!Thread_IsStopped(p_pThread))
    {
        item = List_Pop(requestQueue);
        if (item == NULL)
        {
            rzb_log(LOG_ERR,LOG_C_CORE, "%s: Failed to dequeue item", __func__);
            continue;
        }
        BlockPool_Item_Lock(item);
        itemSpan = Telemetry_StartSpan("request global cache", &item->telemetryContext);
        BlockPool_AddCommonTelemetryAttributes(item, itemSpan);
        itemSuccess = false;
        itemError = NULL;
        if (BlockPool_GetStatus(item) != BLOCK_POOL_STATUS_CHECK_GLOBAL_CACHE)
        {
            rzb_log(LOG_ERR,LOG_C_CORE, "%s: Item (%p) in queue with wrong status", __func__, item );
            itemError = "block pool item in wrong state for cache request";
            Telemetry_EndSpan(itemSpan, false, itemError);
            BlockPool_Item_Unlock(item);
            continue;
        }
        BlockPool_SetStatus(item, BLOCK_POOL_STATUS_CHECKING_GLOBAL_CACHE);

        if (( message = MessageCacheReq_Initialize(
                        sg_pContext->uuidNuggetId, item->pEvent->pBlock->pId)) == NULL)
        {
            rzb_log(LOG_ERR,LOG_C_CORE, "%s: Failed to initialize cache request", __func__);
            itemError = "failed to initialize cache request";
            Telemetry_EndSpan(itemSpan, false, itemError);
            BlockPool_Item_Unlock(item);
            continue;
        }
        if (!Queue_Put(queue, message))
        {
            rzb_log(LOG_ERR,LOG_C_CORE, "%s: Failed to send cache request", __func__);
            itemError = "failed to send cache request";
        }
        else
        {
            itemSuccess = true;
        }
        BlockPool_Item_Unlock(item);
        message->destroy(message);
        Telemetry_EndSpan(itemSpan, itemSuccess, itemError);
    }
}

int
Submission_GlobalCache_ResponseHandler(struct BlockPoolItem *p_pItem, void * userData)
{
    Thread_t *l_pThread;
    struct CacheResult *l_pRes;

    // Pull the data out the thread.
    l_pThread = Thread_GetCurrent();
    l_pRes = (struct CacheResult *) Thread_GetUserData(l_pThread);

    if (BlockPool_GetStatus(p_pItem) == BLOCK_POOL_STATUS_CHECKING_GLOBAL_CACHE)
    {
        if (BlockId_IsEqual(p_pItem->pEvent->pBlock->pId, l_pRes->pId))
        {
            l_pRes->matchedCount++;
            if (l_pRes->iSfFlags & SF_FLAG_BAD) {

                if (addLocalEntry (p_pItem->pEvent->pBlock->pId->pHash->pData, p_pItem->pEvent->pBlock->pId->pHash->iSize,
                                             l_pRes->iSfFlags & (SF_FLAG_CANHAZ ^ SF_FLAG_ALL), l_pRes->iEntFlags, BADHASH) != R_SUCCESS)
                {
                    rzb_log(LOG_ERR,LOG_C_CORE, "%s: Could not add to bad cache", __func__);
                }
            }
            else
            {
                if (addLocalEntry (p_pItem->pEvent->pBlock->pId->pHash->pData, p_pItem->pEvent->pBlock->pId->pHash->iSize,
                                             l_pRes->iSfFlags & (SF_FLAG_CANHAZ ^ SF_FLAG_ALL), l_pRes->iEntFlags, GOODHASH) != R_SUCCESS)
                {
                    rzb_log(LOG_ERR,LOG_C_CORE, "%s: Could not add to good cache", __func__);
                }
            }
            rzb_log(LOG_DEBUG,LOG_C_CORE, "%s: Got flags SF: 0x%08x, ENT: 0x%08x", __func__, l_pRes->iSfFlags, l_pRes->iEntFlags);
            BlockPool_SetStatus (p_pItem, BLOCK_POOL_STATUS_SUBMIT_DATA);
            if ((l_pRes->iSfFlags & SF_FLAG_CANHAZ) != SF_FLAG_CANHAZ)
            {
                BlockPool_SetFlags(p_pItem, p_pItem->iStatus | BLOCK_POOL_FLAG_EVENT_ONLY);
                Telemetry_RecordCacheResponse("event_only");
            }
            else
            {
                Telemetry_RecordCacheResponse("submit");
            }

            Submission_RecordCacheLookupWait(p_pItem);
            Telemetry_UpdateContext(&p_pItem->telemetryContext);
            List_Push(submitQueue, p_pItem);
        }
    }
    Thread_Destroy(l_pThread);
    return LIST_EACH_OK;
}

void
Submission_GlobalCache_ResponseThread(Thread_t *p_pThread)
{
    struct Message *message;
    struct MessageCacheResp *l_mcrMessage;
    struct CacheResult *l_pRes;
    struct Queue *queue;
    struct TelemetrySpan *processSpan;
    bool processSuccess;
    const char *processError;
    // TODO: Not what we think it is!!
    if ((queue = ResponseQueue_Initialize(sg_pContext->uuidNuggetId, QUEUE_FLAG_RECV)) == NULL)
        return;

    if ((l_pRes = calloc(1, sizeof(struct CacheResult))) == NULL)
    {
        rzb_log(LOG_ERR,LOG_C_CORE, "%s: Failed to allocate thread args", __func__);
        return;
    }
    Thread_SetUserData(p_pThread, l_pRes);
    while (!Thread_IsStopped(p_pThread))
    {
        if ((message = Queue_Get(queue)) == NULL)
            continue;
        processSpan = Telemetry_StartMessageProcessSpan(queue, message);
        processSuccess = false;
        processError = NULL;
        if (message->type != MESSAGE_TYPE_RESP) {
            rzb_log(LOG_ERR, LOG_C_CORE, "%s: Unexpected cache response message type %u",
                    __func__, message->type);
            processError = "unexpected cache response message type";
            Telemetry_RecordCacheResponse("invalid_type");
            Telemetry_EndSpan(processSpan, false, processError);
            message->destroy(message);
            continue;
        }
        l_mcrMessage = message->message;
        // Copy the item into thread local storage.
        l_pRes->pId = l_mcrMessage->pId;
        l_pRes->iSfFlags = l_mcrMessage->iSfFlags;
        l_pRes->iEntFlags = l_mcrMessage->iEntFlags;
        l_pRes->matchedCount = 0;
        rzb_log(LOG_DEBUG,LOG_C_CORE, "%s: Got flags SF: 0x%08x, ENT: 0x%08x", __func__, l_pRes->iSfFlags, l_pRes->iEntFlags);
        BlockPool_ForEachItem(Submission_GlobalCache_ResponseHandler, NULL);
        if (l_pRes->matchedCount == 0)
            Telemetry_RecordCacheResponse("unknown_block");
        // Destroy allocated items in thread local storage.
        processSuccess = true;
        message->destroy(message);
        Telemetry_EndSpan(processSpan, processSuccess, processError);
    }

    Thread_SetUserData(p_pThread, NULL);
    free(l_pRes);
    // TODO: BAD
    ResponseQueue_Terminate(sg_pContext->uuidNuggetId);
}

void
Submission_SubmitThread(Thread_t *p_pThread)
{
    struct Message *message;
    uint8_t storedLocality = 0;
    struct ConnectedEntity *dispatcher = NULL;
    enum TransferStatus transfered = TRANSFER_FAIL_LOCAL;
    int transferTries = 0;
    uint32_t reason = 0;
    struct BlockPoolItem *item = NULL;
    struct Queue *queue = NULL;
    struct TelemetrySpan *itemSpan = NULL;
    bool itemSuccess = false;
    const char *itemError = NULL;
    double submitStartedAt = 0.0;
    const char *submitOutcome = "error";
    const char *reasonLabel = "unknown";

#if 0
    struct timespec l_tsTimeOut;
    l_tsTimeOut.tv_sec=1;
#endif
    if ((queue = Queue_Create(INPUT_QUEUE, false, QUEUE_FLAG_SEND)) == NULL)
        return;

    while (!Thread_IsStopped(p_pThread))
    {
        transfered = TRANSFER_FAIL_LOCAL;
        transferTries = 0;
        item = List_Pop(submitQueue);
        if (item == NULL)
        {
            rzb_log(LOG_ERR,LOG_C_CORE, "%s: Failed to dequeue item", __func__);
            continue;
        }

        BlockPool_Item_Lock(item);
        submitStartedAt = Telemetry_GetMonotonicTimeSeconds();
        submitOutcome = "error";
        reasonLabel = "unknown";
        itemSpan = Telemetry_StartSpan("submit block", &item->telemetryContext);
        BlockPool_AddCommonTelemetryAttributes(item, itemSpan);
        itemSuccess = false;
        itemError = NULL;
        if (BlockPool_GetStatus(item) != BLOCK_POOL_STATUS_SUBMIT_DATA)
        {
            rzb_log(LOG_ERR,LOG_C_CORE, "%s: Dequeued item with wrong state", __func__);
            itemError = "block pool item in wrong state for submission";
            Telemetry_EndSpan(itemSpan, false, itemError);
            Telemetry_RecordSubmitDuration(Telemetry_GetMonotonicTimeSeconds() - submitStartedAt,
                                           reasonLabel,
                                           submitOutcome);
            BlockPool_Item_Unlock(item);
            continue;
        }

        if ((item->iStatus & (BLOCK_POOL_FLAG_EVENT_ONLY |BLOCK_POOL_FLAG_UPDATE)) != 0)
        {
            rzb_log(LOG_INFO,LOG_C_CORE, "%s: Sending Event Only", __func__);
            reason = SUBMISSION_REASON_EVENT;
            reasonLabel = Submission_Reason_Label(reason);
        }
        else
        {
            while (transferTries < 20)
            {
                dispatcher = ConnectedEntityList_GetDispatcher();
                rzb_log(LOG_ERR,LOG_C_CORE, "%s: %z", __func__, dispatcher);
                if (dispatcher == NULL)
                {
                    rzb_log(LOG_ERR,LOG_C_CORE, "%s: Failed to find usable dispatcher", __func__);
                    transfered = TRANSFER_FAIL_LOCAL;
                    itemError = "failed to find usable dispatcher";
                    transferTries++;
                    break;
                }
                transfered = Transfer_Store(item, dispatcher);
                if (transfered == TRANSFER_FAIL_DISPATCHER)
                {
                    rzb_log(LOG_ERR,LOG_C_CORE, "%s: Marking dispatcher unusable", __func__);
                    ConnectedEntityList_MarkDispatcherUnusable(dispatcher->uuidNuggetId);
                }
                if (transfered == TRANSFER_OK)
                    break;
                else
                    transferTries++;
            }
            if (transfered != TRANSFER_OK)
            {
                rzb_log(LOG_ERR,LOG_C_CORE, "%s: Failed to transfer block giving up", __func__);
                itemError = "failed to transfer block";
                reasonLabel = Submission_Reason_Label(SUBMISSION_REASON_REQUESTED);
                submitOutcome = "store_failed";
                if (item->submittedCallback != NULL)
                    item->submittedCallback(item);
                Telemetry_EndSpan(itemSpan, false, itemError);
                Telemetry_RecordSubmitDuration(Telemetry_GetMonotonicTimeSeconds() - submitStartedAt,
                                               reasonLabel,
                                               submitOutcome);
                BlockPool_Item_Unlock(item);
                BlockPool_DestroyItem(item);
                continue;
            }
            storedLocality = dispatcher->locality;
            reason = SUBMISSION_REASON_REQUESTED;
            reasonLabel = Submission_Reason_Label(reason);
            rzb_log(LOG_ERR,LOG_C_CORE, "%s: %z", __func__, dispatcher);
            ConnectedEntity_Destroy(dispatcher);
        }

        if ((message = MessageBlockSubmission_Initialize( item->pEvent, reason, storedLocality)) == NULL)
        {
            rzb_log(LOG_ERR,LOG_C_CORE, "%s: Failed to create message", __func__);
            itemError = "failed to create block submission message";
            if (item->submittedCallback != NULL)
                item->submittedCallback(item);
            Telemetry_EndSpan(itemSpan, false, itemError);
            Telemetry_RecordSubmitDuration(Telemetry_GetMonotonicTimeSeconds() - submitStartedAt,
                                           reasonLabel,
                                           "send_failed");
            BlockPool_Item_Unlock(item);
            BlockPool_DestroyItem(item);
            continue;
        }

        if(!Queue_Put(queue, message))
        {
            rzb_log(LOG_ERR,LOG_C_CORE, "%s: Failed to put message", __func__);
            itemError = "failed to send block submission";
            submitOutcome = "send_failed";
        }
        else
        {
            itemSuccess = true;
            submitOutcome = "sent";
        }

        // Set the event to null so we don't destroy it.
        ((struct MessageBlockSubmission*)message->message)->pEvent = NULL;

        message->destroy(message);

        if (item->submittedCallback != NULL)
            item->submittedCallback(item);

        if ((item->iStatus & BLOCK_POOL_FLAG_MAY_REUSE) == BLOCK_POOL_FLAG_MAY_REUSE)
        {
            BlockPool_SetStatus(item, BLOCK_POOL_STATUS_FINALIZED);
            BlockPool_SetFlags(item, 0);
            BlockPool_DestroyItemDataList(item->pDataHead);
            item->pDataHead = NULL;
            item->pDataTail = NULL;
            Telemetry_ClearContext(&item->telemetryContext);
            Telemetry_EndSpan(itemSpan, itemSuccess, itemError);
            Telemetry_RecordSubmitDuration(Telemetry_GetMonotonicTimeSeconds() - submitStartedAt,
                                           reasonLabel,
                                           submitOutcome);
            BlockPool_Item_Unlock(item);
            continue;
        }
        else
        {
            Telemetry_EndSpan(itemSpan, itemSuccess, itemError);
            Telemetry_RecordSubmitDuration(Telemetry_GetMonotonicTimeSeconds() - submitStartedAt,
                                           reasonLabel,
                                           submitOutcome);
            BlockPool_Item_Unlock(item);
            BlockPool_DestroyItem(item);
            continue;
        }
    }
    Queue_Terminate(queue);
}
