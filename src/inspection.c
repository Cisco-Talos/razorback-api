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
#include <razorback/lock.h>
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
static void Inspection_Emergency_Shutdown_Thread(Thread_t *p_pThread);
void Inspection_Shutdown(struct RazorbackContext *p_pContext);
static void Inspection_Destroy_Message(void *item);
static bool Inspection_Is_Shutdown_Started(const struct RazorbackContext *p_pContext);
static bool Inspection_Is_Paused(const struct RazorbackContext *p_pContext);
static void Inspection_Start_Shutdown(struct RazorbackContext *p_pContext);
static bool Inspection_Complete_Message(struct RazorbackContext *p_pContext,
                                        struct Message *message);
static void Inspection_Drain_Completed(struct RazorbackContext *p_pContext);
static void Inspection_Destroy_Worker_Init_State(struct RazorbackContext *p_pContext);
static void Inspection_Report_Worker_Init(struct RazorbackContext *p_pContext,
                                          bool success);
static void Inspection_Request_Emergency_Shutdown(struct RazorbackContext *p_pContext,
                                                  const char *reason);
static const char *Inspection_Result_Label(uint8_t result);

#define INSPECTION_PENDING_POP_TIMEOUT_MS 1000U
#define INSPECTION_PAUSE_SLEEP_MS 100U
#define INSPECTION_RECEIVER_POLL_TIMEOUT_MS 1000U
#define INSPECTION_SHUTDOWN_REJECT_SLEEP_MS 100U
#define INSPECTION_SHUTDOWN_DRAIN_SLEEP_MS 50U

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

static bool
Inspection_Is_Shutdown_Started(const struct RazorbackContext *p_pContext)
{
    if (p_pContext == NULL)
        return true;

    return atomic_load(&p_pContext->inspector.shutdownStarted);
}

static bool
Inspection_Is_Paused(const struct RazorbackContext *p_pContext)
{
    if (p_pContext == NULL)
        return false;

    return atomic_load(&p_pContext->paused);
}

static void
Inspection_Start_Shutdown(struct RazorbackContext *p_pContext)
{
    if (p_pContext == NULL)
        return;

    atomic_store(&p_pContext->inspector.shutdownStarted, true);
}

static void
Inspection_Destroy_Worker_Init_State(struct RazorbackContext *p_pContext)
{
    Semaphore_t *workerInitSem;
    Mutex_t *workerInitLock;

    if (p_pContext == NULL)
        return;

    workerInitSem = p_pContext->inspector.workerInitSem;
    workerInitLock = p_pContext->inspector.workerInitLock;
    p_pContext->inspector.workerInitSem = NULL;
    p_pContext->inspector.workerInitLock = NULL;
    p_pContext->inspector.workerInitPending = 0;
    p_pContext->inspector.workerInitFailed = false;

    if (workerInitSem != NULL)
        Semaphore_Destroy(workerInitSem);
    if (workerInitLock != NULL)
        Mutex_Destroy(workerInitLock);
}

static void
Inspection_Emergency_Shutdown_Thread(Thread_t *p_pThread)
{
    struct RazorbackContext *p_pContext;

    p_pContext = Thread_GetContext(p_pThread);
    if (p_pContext == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE,
                "%s: Missing context for emergency inspection shutdown",
                __func__);
        return;
    }

    while (!Thread_IsStopped(p_pThread)) {
        if (!Semaphore_Wait(p_pContext->inspector.emergencySem))
            continue;

        if (Thread_IsStopped(p_pThread))
            break;

        Razorback_Shutdown_Context(p_pContext);
        return;
    }
}

static void
Inspection_Request_Emergency_Shutdown(struct RazorbackContext *p_pContext,
                                      const char *reason)
{
    bool shouldSignal = false;

    if (p_pContext == NULL ||
        p_pContext->inspector.emergencyLock == NULL ||
        p_pContext->inspector.emergencySem == NULL)
    {
        rzb_log(LOG_ERR, LOG_C_CORE,
                "%s: Emergency shutdown infrastructure is unavailable",
                __func__);
        return;
    }

    Inspection_Start_Shutdown(p_pContext);

    Mutex_Lock(p_pContext->inspector.emergencyLock);
    if (!p_pContext->inspector.emergencyShutdownRequested) {
        p_pContext->inspector.emergencyShutdownRequested = true;
        shouldSignal = true;
    }
    Mutex_Unlock(p_pContext->inspector.emergencyLock);

    if (!shouldSignal)
        return;

    rzb_log(LOG_ERR, LOG_C_CORE,
            "%s: Fatal inspection error, shutting down context: %s",
            __func__, (reason != NULL) ? reason : "unknown");
    Semaphore_Post(p_pContext->inspector.emergencySem);
}

static void
Inspection_Report_Worker_Init(struct RazorbackContext *p_pContext, bool success)
{
    Semaphore_t *workerInitSem = NULL;

    if (p_pContext == NULL ||
        p_pContext->inspector.workerInitLock == NULL ||
        p_pContext->inspector.workerInitSem == NULL) {
        return;
    }

    Mutex_Lock(p_pContext->inspector.workerInitLock);
    if (p_pContext->inspector.workerInitPending > 0) {
        p_pContext->inspector.workerInitPending--;
        if (!success)
            p_pContext->inspector.workerInitFailed = true;
        workerInitSem = p_pContext->inspector.workerInitSem;
    }
    Mutex_Unlock(p_pContext->inspector.workerInitLock);

    if (workerInitSem != NULL)
        Semaphore_Post(workerInitSem);
}

bool
Inspection_Launch(struct RazorbackContext *p_pContext)
{
    uint32_t workerCount;
    uint32_t workerLimit;
    uint32_t i;
    bool workerInitFailed;

    workerCount = Config_getInspThreadsInit();
    workerLimit = Config_getInspThreadsMax();

    if (workerCount == 0) {
        rzb_log(LOG_ERR, LOG_C_CORE,
                "%s: Refusing to launch inspection without at least one worker",
                __func__);
        return false;
    }

    atomic_init(&p_pContext->inspector.shutdownStarted, false);

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

    p_pContext->inspector.emergencyLock = Mutex_Create(MUTEX_MODE_NORMAL);
    if (p_pContext->inspector.emergencyLock == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to create emergency shutdown lock",
                __func__);
        Inspection_Shutdown(p_pContext);
        return false;
    }

    p_pContext->inspector.emergencySem = Semaphore_Create(false, 0);
    if (p_pContext->inspector.emergencySem == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to create emergency shutdown semaphore",
                __func__);
        Inspection_Shutdown(p_pContext);
        return false;
    }
    p_pContext->inspector.emergencyShutdownRequested = false;

    p_pContext->inspector.emergencyThread = Thread_Launch(
        Inspection_Emergency_Shutdown_Thread,
        NULL,
        "Inspection Emergency Shutdown",
        p_pContext
    );
    if (p_pContext->inspector.emergencyThread == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to launch emergency shutdown thread",
                __func__);
        Inspection_Shutdown(p_pContext);
        return false;
    }

    p_pContext->inspector.workerInitLock = Mutex_Create(MUTEX_MODE_NORMAL);
    if (p_pContext->inspector.workerInitLock == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to create inspection worker init lock",
                __func__);
        Inspection_Shutdown(p_pContext);
        return false;
    }

    p_pContext->inspector.workerInitSem = Semaphore_Create(false, 0);
    if (p_pContext->inspector.workerInitSem == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to create inspection worker init semaphore",
                __func__);
        Inspection_Shutdown(p_pContext);
        return false;
    }
    p_pContext->inspector.workerInitPending = workerCount;
    p_pContext->inspector.workerInitFailed = false;

    p_pContext->inspector.threadPool = ThreadPool_Create(
        workerCount,
        workerLimit,
        p_pContext,
        "Inspection Worker %i",
        Inspection_Process_Thread
    );
    if (p_pContext->inspector.threadPool == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to launch inspection workers", __func__);
        Inspection_Shutdown(p_pContext);
        return false;
    }

    for (i = 0; i < workerCount; ++i) {
        if (!Semaphore_Wait(p_pContext->inspector.workerInitSem)) {
            rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed waiting for inspection worker init",
                    __func__);
            Inspection_Shutdown(p_pContext);
            return false;
        }
    }

    Mutex_Lock(p_pContext->inspector.workerInitLock);
    workerInitFailed = p_pContext->inspector.workerInitFailed;
    Mutex_Unlock(p_pContext->inspector.workerInitLock);
    Inspection_Destroy_Worker_Init_State(p_pContext);

    if (workerInitFailed) {
        rzb_log(LOG_ERR, LOG_C_CORE,
                "%s: Failed to launch inspection workers due to worker init failure",
                __func__);
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
        Telemetry_RecordInspectionError("result_ack_queue");
        Inspection_Request_Emergency_Shutdown(
            p_pContext,
            "failed to queue completed inspection message for receiver-thread ack");
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

        if ((message = Queue_Get_Ex(l_pQueue, false,
                                    INSPECTION_RECEIVER_POLL_TIMEOUT_MS)) == NULL) {
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

        if (Inspection_Is_Shutdown_Started(l_pContext)) {
            Queue_Reject_Message(l_pQueue, message, true);
            Telemetry_RecordShutdownRequeuedInspection();
            Telemetry_EndSpan(receiveSpan, false, "inspection shutdown in progress");
            Inspection_Destroy_Message(message);
            Thread_Sleep(INSPECTION_SHUTDOWN_REJECT_SLEEP_MS);
            continue;
        }

        if (!Telemetry_UpdateContext(&message->telemetryContext)) {
            rzb_log(LOG_ERR, LOG_C_CORE,
                    "%s: Failed to capture telemetry context for deferred inspection message",
                    __func__);
        }

        if (Thread_IsStopped(p_pThread) || Inspection_Is_Shutdown_Started(l_pContext)) {
            Queue_Reject_Message(l_pQueue, message, true);
            Telemetry_RecordShutdownRequeuedInspection();
            Telemetry_EndSpan(receiveSpan, false,
                              Thread_IsStopped(p_pThread)
                                  ? "inspection receiver stopping"
                                  : "inspection shutdown in progress");
            Inspection_Destroy_Message(message);
            if (Thread_IsStopped(p_pThread))
                break;
            Thread_Sleep(INSPECTION_SHUTDOWN_REJECT_SLEEP_MS);
            continue;
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
    void (*cleanupThread)(void *) = NULL;

    l_pContext = Thread_GetContext(p_pThread);
    rzb_log(LOG_DEBUG, LOG_C_CORE, "%s: Inspection worker launched", __func__);
    cleanupThread = l_pContext->inspector.hooks->cleanupThread;

    if (l_pContext->inspector.hooks->initThread != NULL) {
        if (!l_pContext->inspector.hooks->initThread(&threadData)) {
            rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to init thread", __func__);
            Inspection_Report_Worker_Init(l_pContext, false);
            Inspection_Request_Emergency_Shutdown(l_pContext,
                                                  "failed to initialize inspection worker thread");
            return;
        }
    }
    Inspection_Report_Worker_Init(l_pContext, true);

    while (!Thread_IsStopped(p_pThread)) {
        if (Inspection_Is_Paused(l_pContext) &&
            !Inspection_Is_Shutdown_Started(l_pContext))
        {
            Thread_Sleep(INSPECTION_PAUSE_SLEEP_MS);
            continue;
        }

        message = List_Pop_Ex(l_pContext->inspector.pendingMessages,
                              INSPECTION_PENDING_POP_TIMEOUT_MS);
        if (message == NULL) {
            if (Thread_IsStopped(p_pThread))
                break;
            if (Inspection_Is_Shutdown_Started(l_pContext) &&
                List_Length(l_pContext->inspector.pendingMessages) == 0)
            {
                break;
            }
            continue;
        }

        Telemetry_AddInspectionInFlight(1);
        Inspection_Process_Message(p_pThread,
                                   l_pContext,
                                   l_pContext->inspector.inspectionQueue,
                                   message,
                                   threadData);
        Telemetry_AddInspectionInFlight(-1);
        if (!Inspection_Complete_Message(l_pContext, message))
            break;
    }

    if (cleanupThread != NULL)
        cleanupThread(threadData);

    rzb_log(LOG_DEBUG, LOG_C_CORE, "%s: Inspection worker exiting", __func__);
}

void
Inspection_Shutdown(struct RazorbackContext *p_pContext)
{
    Thread_t *currentThread;
    bool currentIsEmergencyThread;

    if (p_pContext == NULL)
        return;

    Inspection_Start_Shutdown(p_pContext);

    currentThread = Thread_GetCurrent();
    currentIsEmergencyThread = (currentThread != NULL &&
                                currentThread == p_pContext->inspector.emergencyThread);

    if (p_pContext->inspector.threadPool != NULL) {
        while (ThreadPool_GetAliveCount(p_pContext->inspector.threadPool) > 0)
            Thread_Sleep(INSPECTION_SHUTDOWN_DRAIN_SLEEP_MS);
    }

    if (p_pContext->inspector.receiverThread != NULL &&
        p_pContext->inspector.completedMessages != NULL) {
        while (List_Length(p_pContext->inspector.completedMessages) > 0)
            Thread_Sleep(INSPECTION_SHUTDOWN_DRAIN_SLEEP_MS);
    }

    if (p_pContext->inspector.receiverThread != NULL) {
        Thread_StopAndJoin(p_pContext->inspector.receiverThread);
        Thread_Destroy(p_pContext->inspector.receiverThread);
        p_pContext->inspector.receiverThread = NULL;
    }

    if (p_pContext->inspector.threadPool != NULL) {
        ThreadPool_Destroy(p_pContext->inspector.threadPool);
        p_pContext->inspector.threadPool = NULL;
    }

    if (p_pContext->inspector.emergencyThread != NULL) {
        if (!currentIsEmergencyThread) {
            Thread_Stop(p_pContext->inspector.emergencyThread);
            if (p_pContext->inspector.emergencySem != NULL)
                Semaphore_Post(p_pContext->inspector.emergencySem);
            Thread_Join(p_pContext->inspector.emergencyThread);
        }
        /* Dropping the context-held ref is safe even on the current thread:
         * Thread_GetCurrent() below still holds a temporary ref, and the
         * wrapper drops the final ref after the thread main function returns. */
        Thread_Destroy(p_pContext->inspector.emergencyThread);
        p_pContext->inspector.emergencyThread = NULL;
    }

    Inspection_Destroy_Worker_Init_State(p_pContext);

    if (p_pContext->inspector.emergencySem != NULL) {
        Semaphore_Destroy(p_pContext->inspector.emergencySem);
        p_pContext->inspector.emergencySem = NULL;
    }

    if (p_pContext->inspector.emergencyLock != NULL) {
        Mutex_Destroy(p_pContext->inspector.emergencyLock);
        p_pContext->inspector.emergencyLock = NULL;
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

    if (currentThread != NULL)
        Thread_Destroy(currentThread);
}
