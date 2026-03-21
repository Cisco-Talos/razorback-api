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
static void Inspection_Thread (Thread_t *p_pThread);

bool
Inspection_Launch (struct RazorbackContext *p_pContext, uint32_t initThreads, uint32_t maxThreads) {
    p_pContext->inspector.threadPool = ThreadPool_Create(
        ((initThreads == 0) ? Config_getInspThreadsInit() : initThreads),
        ((maxThreads == 0) ? Config_getInspThreadsMax() : maxThreads),
        p_pContext,
        "Inspection Thread Pool %i",
        Inspection_Thread
    );
    if (p_pContext->inspector.threadPool == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to launch thread.", __func__);
        return false;
    }
    return true;
}

static void
Inspection_Thread (Thread_t *p_pThread)
{
    struct RazorbackContext *l_pContext;
    struct Message *message;
    struct MessageInspectionSubmission *l_misMessage;
    struct Message *l_mjsMessage;
    struct Block *l_pBlock, *l_pClonedBlock;
    struct EventId *l_pEventId;
    struct Queue *l_pQueue;
    uint8_t l_iResult;
    struct Judgment *judgment;
    enum TransferStatus transfered;
    int transferTries = 0;
    struct ConnectedEntity *dispatcher = NULL;
    struct TelemetrySpan *processSpan = NULL;
    struct TelemetrySpan *inspectSpan = NULL;
    struct TelemetrySpan *runSpan = NULL;
    void *threadData = NULL;
    bool processSuccess = false;
    bool runSuccess = false;
    const char *processError = NULL;
    const char *runError = NULL;

    l_pContext = Thread_GetContext (p_pThread);
    if ((l_pQueue =
                 InspectorQueue_Initialize(l_pContext->uuidApplicationType,
                                           QUEUE_FLAG_RECV)) == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to connect to MQ - Inspector Queue",
                __func__);
        return;
    }
    rzb_log(LOG_DEBUG, LOG_C_CORE, "%s: Inspection Thread Launched", __func__);
    Thread_SetUserData(p_pThread, l_pQueue);
    if (l_pContext->inspector.hooks->initThread != NULL) {
        if (!l_pContext->inspector.hooks->initThread(&threadData)) {
            rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to init thread", __func__);
            return;
        }
    }

    while (!Thread_IsStopped(p_pThread)) {
        if ((message = Queue_Get(l_pQueue)) == NULL) {
            // timeout
            if (errno == EAGAIN || errno == EINTR)
                continue;
            // error
            rzb_log(LOG_ERR, LOG_C_CORE,
                    "%s: Dropped block due to failure of InspectorQueue_Get()",
                    __func__);
            // drop message
            continue;
        }
        processSpan = Telemetry_StartMessageProcessSpan(l_pQueue, message);
        inspectSpan = NULL;
        runSpan = NULL;
        processSuccess = false;
        processError = NULL;
        if (message->type != MESSAGE_TYPE_INSPECTION) {
            rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed dispatch message due to wrong type %u",
                    __func__, message->type);
            processError = "unexpected inspection message type";
            Telemetry_EndSpan(processSpan, false, processError);
            message->destroy(message);
            continue;
        }
        l_misMessage = message->message;
        if (l_misMessage->pBlock == NULL) {
            rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed dispatch message due to NULL block",
                    __func__);
            processError = "inspection message missing block";
            Telemetry_EndSpan(processSpan, false, processError);
            message->destroy(message);
            continue;
        }
        if (l_misMessage->pBlock->pId->pHash == NULL) {
            rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed dispatch message due to NULL Hash",
                    __func__);
            processError = "inspection message missing block hash";
            Telemetry_EndSpan(processSpan, false, processError);
            message->destroy(message);
            continue;
        }
        l_pBlock = l_misMessage->pBlock;
        l_misMessage->pBlock = NULL;
        inspectSpan = Telemetry_StartSpanWithKind("inspect block", NULL,
                                                  TELEMETRY_SPAN_KIND_CONSUMER);
        Telemetry_AddBlockAttributes(inspectSpan, l_pBlock);
        transfered = TRANSFER_FAIL_LOCAL;
        transferTries = 0;
        while (transferTries < 20) {
            dispatcher = ConnectedEntityList_GetDispatcher();
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
            else
                transferTries++;
        }
        // TODO - Return JUSDGEMENT_ERROR
        if (transfered != TRANSFER_OK) {
            rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to transfer block giving up", __func__);
            processError = "failed to fetch block from dispatcher";
            Telemetry_EndSpan(inspectSpan, false, processError);
            Telemetry_EndSpan(processSpan, false, processError);
            message->destroy(message);
            continue;
        }

        if (l_pBlock->data.pointer == NULL || l_pBlock->data.fileName == NULL) {
            rzb_log(LOG_ERR, LOG_C_CORE, "%s: No data block", __func__);
            processError = "inspection block has no local data";
            Telemetry_EndSpan(inspectSpan, false, processError);
            Telemetry_EndSpan(processSpan, false, processError);
            message->destroy(message);
            continue;
        }
        if ((l_pEventId = EventId_Clone(l_misMessage->eventId)) == NULL) {
            rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed create new event id", __func__);
            processError = "failed to clone event id";
            Telemetry_EndSpan(inspectSpan, false, processError);
            Telemetry_EndSpan(processSpan, false, processError);
            message->destroy(message);
            continue;
        }


        // Clone the block for the inspector to use.
        if ((l_pClonedBlock = Block_Clone(l_pBlock)) == NULL) {
            rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed create new block", __func__);
            processError = "failed to clone inspection block";
            Telemetry_EndSpan(inspectSpan, false, processError);
            Telemetry_EndSpan(processSpan, false, processError);
            message->destroy(message);
            continue;
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
                l_pContext->inspector.hooks->processBlock(l_pClonedBlock,
                                                          l_misMessage->eventId,
                                                          l_misMessage->pEventMetadata, threadData);
        runSuccess = (l_iResult == JUDGMENT_REASON_DONE ||
                      l_iResult == JUDGMENT_REASON_DEFERRED);
        runError = NULL;
        if (l_iResult == JUDGMENT_REASON_ERROR)
            runError = "inspector returned error judgment";
        else if (!runSuccess)
            runError = "inspector returned invalid judgment";
        Telemetry_EndSpan(runSpan, runSuccess, runError);

        message->destroy(message);
        if ((l_iResult != JUDGMENT_REASON_DONE)
                && (l_iResult != JUDGMENT_REASON_ERROR)
                && (l_iResult != JUDGMENT_REASON_DEFERRED)) {
            rzb_log(LOG_ERR, LOG_C_CORE, "%s: Bad return from inspection", __func__);
            processError = "inspector returned invalid judgment";
            Telemetry_EndSpan(inspectSpan, false, processError);
            Telemetry_EndSpan(processSpan, false, processError);
            continue;
        }


        // Lock the pause lock before submitting judgment
        Mutex_Lock(sg_mPauseLock);
        judgment = Judgment_Create(l_pEventId, l_pClonedBlock->pId);
        // Destroy the copy, we don't need it any more
        Transfer_Free(l_pClonedBlock, dispatcher);
        l_pClonedBlock->data.pointer = NULL;
        Block_Destroy(l_pClonedBlock);
        if ((l_mjsMessage = MessageJudgmentSubmission_Initialize(l_iResult, judgment)) == NULL) {
            rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to create message", __func__);
            processError = "failed to create judgment submission";
        } else {
            if (!Queue_Put(l_pContext->inspector.judgmentQueue, l_mjsMessage)) {
                rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to send judgment submission", __func__);
                processError = "failed to send judgment submission";
            } else {
                processSuccess = true;
                processError = NULL;
            }
            l_mjsMessage->destroy(l_mjsMessage);
        }
        Mutex_Unlock(sg_mPauseLock);
        Block_Destroy(l_pBlock);
        EventId_Destroy(l_pEventId);
        Telemetry_EndSpan(inspectSpan, processSuccess, processError);
        Telemetry_EndSpan(processSpan, processSuccess, processError);
    }
    if (l_pContext->inspector.hooks->cleanupThread != NULL)
        l_pContext->inspector.hooks->cleanupThread(threadData);

    rzb_log(LOG_DEBUG, LOG_C_CORE, "%s: Inspection Thread Exiting", __func__);
    return;
}
