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
#include <razorback/messages.h>
#include <razorback/api.h>
#include <razorback/uuids.h>
#include <razorback/log.h>
#include <razorback/list.h>
#include <razorback/thread.h>
#include <razorback/thread_pool.h>
#include <razorback/event.h>
#include <razorback/ntlv.h>
#include <razorback/block.h>
#include <razorback/queue.h>
#include <razorback/inspector_queue.h>
#include <razorback/judgment.h>
#include "judgment_private.h"
#include "command_and_control.h"
#include "transfer/core.h"
#include "connected_entity_private.h"
#include "runtime_config.h"
#include "telemetry.h"
#include <errno.h>
static void Inspection_Process_Thread(Thread_t *p_pThread);
static void Inspection_Receiver_Thread(Thread_t *p_pThread);
static void Inspection_Process_Message(Thread_t *p_pThread,
                                       struct RazorbackContext *p_pContext,
                                       struct Queue *p_pQueue,
                                       struct Message *message,
                                       void *threadData);
void Inspection_Shutdown(struct RazorbackContext *p_pContext);
static void Inspection_Destroy_Message(void *item);
static bool Inspection_Complete_Message(struct RazorbackContext *p_pContext,
                                        struct Message *message);
static void Inspection_Drain_Completed(struct RazorbackContext *p_pContext);
static const char *Inspection_Result_Label(uint8_t result);

static const char *
Inspection_Result_Label(uint8_t result)
{
    switch (result) {
    case JUDGMENT_REASON_DONE:
        return "done";
    case JUDGMENT_REASON_ALERT:
        return "alert";
    case JUDGMENT_REASON_DEFERRED:
        return "deferred";
    case JUDGMENT_REASON_ERROR:
    default:
        return "error";
    }
}

bool
Inspection_Launch(struct RazorbackContext *p_pContext, uint32_t initThreads, uint32_t maxThreads)
{
    if ((p_pContext->inspector.inspectionQueue =
             InspectorQueue_Initialize(p_pContext->uuidApplicationType,
                                       QUEUE_FLAG_RECV)) == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to connect to MQ - Inspector Queue",
                __func__);
        return false;
    }

    p_pContext->inspector.pendingMessages = List_Create(
        LIST_MODE_QUEUE,
        NULL,
        NULL,
        Inspection_Destroy_Message,
        NULL,
        NULL,
        NULL
    );
    if (p_pContext->inspector.pendingMessages == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to create pending inspection queue",
                __func__);
        Inspection_Shutdown(p_pContext);
        return false;
    }

    p_pContext->inspector.completedMessages = List_Create(
        LIST_MODE_GENERIC,
        NULL,
        NULL,
        Inspection_Destroy_Message,
        NULL,
        NULL,
        NULL
    );
    if (p_pContext->inspector.completedMessages == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to create completed inspection queue",
                __func__);
        Inspection_Shutdown(p_pContext);
        return false;
    }

    p_pContext->inspector.threadPool = ThreadPool_Create(
        ((initThreads == 0) ? Config_getInspThreadsInit() : initThreads),
        ((maxThreads == 0) ? Config_getInspThreadsMax() : maxThreads),
        p_pContext,
        "Inspection Worker %i",
        Inspection_Process_Thread
    );
    if (p_pContext->inspector.threadPool == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to launch inspection workers", __func__);
        Inspection_Shutdown(p_pContext);
        return false;
    }

    p_pContext->inspector.receiverThread = Thread_Launch(
        Inspection_Receiver_Thread,
        NULL,
        "Inspection Receiver",
        p_pContext
    );
    if (p_pContext->inspector.receiverThread == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to launch inspection receiver", __func__);
        Inspection_Shutdown(p_pContext);
        return false;
    }
    return true;
}

static void
Inspection_Destroy_Message(void *item)
{
    struct Message *message = item;

    if (message == NULL)
        return;

    if (message->destroy != NULL)
        message->destroy(message);
    else
        Message_Destroy(message);
}

static bool
Inspection_Complete_Message(struct RazorbackContext *p_pContext,
                            struct Message *message)
{
    if (!List_Push(p_pContext->inspector.completedMessages, message)) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to queue completed inspection message",
                __func__);
        Inspection_Destroy_Message(message);
        return false;
    }

    return true;
}

static void
Inspection_Drain_Completed(struct RazorbackContext *p_pContext)
{
    struct Message *message;

    while ((message = List_Pop(p_pContext->inspector.completedMessages)) != NULL) {
        if (!Queue_Ack_Message(p_pContext->inspector.inspectionQueue, message)) {
            rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to ack completed inspection message",
                    __func__);
            Telemetry_RecordInspectionError("result_ack");
        }
        Inspection_Destroy_Message(message);
    }
}

static void
Inspection_Receiver_Thread(Thread_t *p_pThread)
{
    struct RazorbackContext *l_pContext;
    struct Message *message;
    struct Queue *l_pQueue;
    struct TelemetrySpan *receiveSpan;
    bool receiveSuccess;
    const char *receiveError;

    l_pContext = Thread_GetContext(p_pThread);
    l_pQueue = l_pContext->inspector.inspectionQueue;
    if (l_pQueue == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Inspection queue is NULL", __func__);
        return;
    }
    rzb_log(LOG_DEBUG, LOG_C_CORE, "%s: Inspection receiver thread launched", __func__);

    while (!Thread_IsStopped(p_pThread)) {
        Inspection_Drain_Completed(l_pContext);

        if ((message = Queue_Get_Ex(l_pQueue, false, 1000)) == NULL) {
            if (errno == EAGAIN || errno == EINTR)
                continue;
            rzb_log(LOG_ERR, LOG_C_CORE,
                    "%s: Dropped block due to failure of Queue_Get_Ex()",
                    __func__);
            continue;
        }

        receiveSpan = Telemetry_StartMessageProcessSpan(l_pQueue, message);
        receiveSuccess = false;
        receiveError = NULL;

        if (!Telemetry_UpdateContext(&message->telemetryContext)) {
            rzb_log(LOG_ERR, LOG_C_CORE,
                    "%s: Failed to capture telemetry context for deferred inspection message",
                    __func__);
        }

        if (Thread_IsStopped(p_pThread)) {
            Queue_Reject_Message(l_pQueue, message, true);
            Telemetry_RecordShutdownRequeuedInspection();
            Telemetry_EndSpan(receiveSpan, false, "inspection receiver stopping");
            Inspection_Destroy_Message(message);
            break;
        }

        if (!List_Push(l_pContext->inspector.pendingMessages, message)) {
            rzb_log(LOG_ERR, LOG_C_CORE,
                    "%s: Failed to queue message for inspection workers", __func__);
            Queue_Reject_Message(l_pQueue, message, true);
            Telemetry_EndSpan(receiveSpan, false, "failed to queue inspection message");
            Inspection_Destroy_Message(message);
        } else {
            receiveSuccess = true;
            Telemetry_EndSpan(receiveSpan, receiveSuccess, receiveError);
        }
    }

    Inspection_Drain_Completed(l_pContext);
    rzb_log(LOG_DEBUG, LOG_C_CORE, "%s: Inspection receiver thread exiting", __func__);
}

static void
Inspection_Process_Message(Thread_t *p_pThread,
                           struct RazorbackContext *p_pContext,
                           struct Queue *p_pQueue,
                           struct Message *message,
                           void *threadData)
{
    struct MessageInspectionSubmission *l_misMessage = NULL;
    struct Message *l_mjsMessage = NULL;
    struct Block *l_pBlock = NULL;
    struct Block *l_pClonedBlock = NULL;
    struct EventId *l_pEventId = NULL;
    uint8_t l_iResult = JUDGMENT_REASON_ERROR;
    struct Judgment *judgment = NULL;
    enum TransferStatus transfered = TRANSFER_FAIL_LOCAL;
    int transferTries = 0;
    struct TelemetrySpan *inspectSpan = NULL;
    struct TelemetrySpan *runSpan = NULL;
    bool processSuccess = false;
    bool runSuccess = false;
    bool destroyOriginalBlock = false;
    bool pauseLocked = false;
    const char *processError = NULL;
    const char *runError = NULL;
    const char *resultReason = "error";
    const char *errorPhase = NULL;
    double inspectionStartedAt = Telemetry_GetMonotonicTimeSeconds();

    (void)p_pThread;
    (void)p_pQueue;

    if (message->type != MESSAGE_TYPE_INSPECTION) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed dispatch message due to wrong type %u",
                __func__, message->type);
        processError = "unexpected inspection message type";
        errorPhase = "validation";
        goto cleanup;
    }

    l_misMessage = message->message;
    if (l_misMessage == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed dispatch message due to NULL payload",
                __func__);
        processError = "inspection message missing payload";
        errorPhase = "validation";
        goto cleanup;
    }
    if (l_misMessage->pBlock == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed dispatch message due to NULL block",
                __func__);
        processError = "inspection message missing block";
        errorPhase = "validation";
        goto cleanup;
    }
    if (l_misMessage->pBlock->pId == NULL || l_misMessage->pBlock->pId->pHash == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed dispatch message due to NULL Hash",
                __func__);
        processError = "inspection message missing block hash";
        errorPhase = "validation";
        goto cleanup;
    }

    l_pBlock = l_misMessage->pBlock;
    inspectSpan = Telemetry_StartSpanWithKind("inspect block", &message->telemetryContext,
                                              TELEMETRY_SPAN_KIND_INTERNAL);
    Telemetry_AddBlockAttributes(inspectSpan, l_pBlock);

    while (transferTries < 20) {
        struct ConnectedEntity *dispatcher = ConnectedEntityList_GetDispatcher();

        if (dispatcher == NULL) {
            rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to find usable dispatcher", __func__);
            transferTries++;
            break;
        }

        transfered = Transfer_Fetch(l_pBlock, dispatcher);
        if (transfered == TRANSFER_FAIL_DISPATCHER) {
            rzb_log(LOG_ERR, LOG_C_CORE, "%s: Marking dispatcher unusable", __func__);
            ConnectedEntityList_MarkDispatcherUnusable(dispatcher->uuidNuggetId);
        }
        ConnectedEntity_Destroy(dispatcher);
        if (transfered == TRANSFER_OK)
            break;

        transferTries++;
    }

    if (transfered != TRANSFER_OK) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to transfer block giving up", __func__);
        processError = "failed to fetch block from dispatcher";
        errorPhase = "transfer";
        goto cleanup;
    }
    destroyOriginalBlock = true;

    if (l_pBlock->data.pointer == NULL || l_pBlock->data.fileName == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: No data block", __func__);
        processError = "inspection block has no local data";
        errorPhase = "transfer";
        goto cleanup;
    }
    if ((l_pEventId = EventId_Clone(l_misMessage->eventId)) == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed create new event id", __func__);
        processError = "failed to clone event id";
        errorPhase = "inspection";
        goto cleanup;
    }
    if ((l_pClonedBlock = Block_Clone(l_pBlock)) == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed create new block", __func__);
        processError = "failed to clone inspection block";
        errorPhase = "inspection";
        goto cleanup;
    }

    l_pClonedBlock->data.pointer = l_pBlock->data.pointer;
    l_pClonedBlock->data.file = l_pBlock->data.file;
    l_pClonedBlock->data.fileName = l_pBlock->data.fileName;
    l_pClonedBlock->data.tempFile = l_pBlock->data.tempFile;
    l_pBlock->data.pointer = NULL;
    l_pBlock->data.file = NULL;
    l_pBlock->data.fileName = NULL;

#ifdef _MSC_VER
    l_pClonedBlock->data.mfileHandle = l_pBlock->data.mfileHandle;
    l_pClonedBlock->data.mapHandle = l_pBlock->data.mapHandle;
    l_pBlock->data.mfileHandle = NULL;
    l_pBlock->data.mapHandle = NULL;
#endif

    runSpan = Telemetry_StartSpan("run inspection", NULL);
    Telemetry_AddBlockAttributes(runSpan, l_pClonedBlock);
    l_iResult =
            p_pContext->inspector.hooks->processBlock(l_pClonedBlock,
                                                      l_misMessage->eventId,
                                                      l_misMessage->pEventMetadata,
                                                      threadData);
    runSuccess = (l_iResult == JUDGMENT_REASON_DONE ||
                  l_iResult == JUDGMENT_REASON_DEFERRED);
    runError = NULL;
    if (l_iResult == JUDGMENT_REASON_ERROR)
        runError = "inspector returned error judgment";
    else if (!runSuccess)
        runError = "inspector returned invalid judgment";
    Telemetry_EndSpan(runSpan, runSuccess, runError);
    runSpan = NULL;
    resultReason = Inspection_Result_Label(l_iResult);
    if (l_iResult == JUDGMENT_REASON_ERROR || !runSuccess)
        errorPhase = "inspection";

    if ((l_iResult != JUDGMENT_REASON_DONE) &&
        (l_iResult != JUDGMENT_REASON_ERROR) &&
        (l_iResult != JUDGMENT_REASON_DEFERRED)) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Bad return from inspection", __func__);
        processError = "inspector returned invalid judgment";
        errorPhase = "inspection";
        goto cleanup;
    }

    Mutex_Lock(sg_mPauseLock);
    pauseLocked = true;
    judgment = Judgment_Create(l_pEventId, l_pClonedBlock->pId);
    if (judgment == NULL) {
        processError = "failed to create judgment";
        errorPhase = "submission";
        goto cleanup;
    }

    Transfer_Free(l_pClonedBlock, NULL);
    l_pClonedBlock->data.pointer = NULL;
    l_pClonedBlock->data.file = NULL;
    l_pClonedBlock->data.fileName = NULL;
    Block_Destroy(l_pClonedBlock);
    l_pClonedBlock = NULL;

    if ((l_mjsMessage = MessageJudgmentSubmission_Initialize(l_iResult, judgment)) == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to create message", __func__);
        processError = "failed to create judgment submission";
        errorPhase = "submission";
        judgment = NULL;
        goto cleanup;
    }
    judgment = NULL;

    if (!Queue_Put(p_pContext->inspector.judgmentQueue, l_mjsMessage)) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to send judgment submission", __func__);
        processError = "failed to send judgment submission";
        errorPhase = "submission";
        goto cleanup;
    }

    processSuccess = true;
    processError = NULL;

cleanup:
    if (l_mjsMessage != NULL)
        l_mjsMessage->destroy(l_mjsMessage);
    if (pauseLocked)
        Mutex_Unlock(sg_mPauseLock);
    if (judgment != NULL)
        Judgment_Destroy(judgment);
    if (l_pClonedBlock != NULL) {
        Transfer_Free(l_pClonedBlock, NULL);
        l_pClonedBlock->data.pointer = NULL;
        l_pClonedBlock->data.file = NULL;
        l_pClonedBlock->data.fileName = NULL;
        Block_Destroy(l_pClonedBlock);
    }
    if (destroyOriginalBlock && l_pBlock != NULL) {
        Transfer_Free(l_pBlock, NULL);
        l_misMessage->pBlock = NULL;
        Block_Destroy(l_pBlock);
    }
    if (l_pEventId != NULL)
        EventId_Destroy(l_pEventId);
    if (runSpan != NULL)
        Telemetry_EndSpan(runSpan, false, (runError != NULL) ? runError : processError);
    if (errorPhase != NULL)
        Telemetry_RecordInspectionError(errorPhase);
    Telemetry_RecordInspectionResult(resultReason);
    Telemetry_RecordInspectionDuration(Telemetry_GetMonotonicTimeSeconds() - inspectionStartedAt,
                                       resultReason);
    if (inspectSpan != NULL)
        Telemetry_EndSpan(inspectSpan, processSuccess, processError);
}

static void
Inspection_Process_Thread(Thread_t *p_pThread)
{
    struct RazorbackContext *l_pContext;
    struct Message *message;
    void *threadData = NULL;

    l_pContext = Thread_GetContext(p_pThread);
    rzb_log(LOG_DEBUG, LOG_C_CORE, "%s: Inspection worker launched", __func__);

    if (l_pContext->inspector.hooks->initThread != NULL) {
        if (!l_pContext->inspector.hooks->initThread(&threadData)) {
            rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to init thread", __func__);
            return;
        }
    }

    while (!Thread_IsStopped(p_pThread)) {
        message = List_Pop(l_pContext->inspector.pendingMessages);
        if (message == NULL) {
            if (Thread_IsStopped(p_pThread))
                break;
            continue;
        }

        Telemetry_AddInspectionInFlight(1);
        Inspection_Process_Message(p_pThread,
                                   l_pContext,
                                   l_pContext->inspector.inspectionQueue,
                                   message,
                                   threadData);
        Telemetry_AddInspectionInFlight(-1);
        Inspection_Complete_Message(l_pContext, message);
    }

    if (l_pContext->inspector.hooks->cleanupThread != NULL)
        l_pContext->inspector.hooks->cleanupThread(threadData);

    rzb_log(LOG_DEBUG, LOG_C_CORE, "%s: Inspection worker exiting", __func__);
}

void
Inspection_Shutdown(struct RazorbackContext *p_pContext)
{
    if (p_pContext == NULL)
        return;

    if (p_pContext->inspector.receiverThread != NULL) {
        Thread_StopAndJoin(p_pContext->inspector.receiverThread);
        Thread_Destroy(p_pContext->inspector.receiverThread);
        p_pContext->inspector.receiverThread = NULL;
    }

    if (p_pContext->inspector.threadPool != NULL) {
        ThreadPool_KillWorkers(p_pContext->inspector.threadPool);
        p_pContext->inspector.threadPool = NULL;
    }

    if (p_pContext->inspector.pendingMessages != NULL) {
        List_Destroy(p_pContext->inspector.pendingMessages);
        p_pContext->inspector.pendingMessages = NULL;
    }

    if (p_pContext->inspector.completedMessages != NULL) {
        List_Destroy(p_pContext->inspector.completedMessages);
        p_pContext->inspector.completedMessages = NULL;
    }

    if (p_pContext->inspector.inspectionQueue != NULL) {
        InspectorQueue_Terminate(p_pContext->uuidApplicationType);
        p_pContext->inspector.inspectionQueue = NULL;
    }
}
