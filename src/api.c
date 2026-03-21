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
#include <razorback/list.h>
#include <razorback/log.h>
#include <razorback/thread.h>
#include <razorback/thread_pool.h>
#include <razorback/event.h>
#include <razorback/ntlv.h>
#include <razorback/block.h>
#include <razorback/queue.h>
#include <razorback/inspector_queue.h>
#include <razorback/judgment.h>

#include <errno.h>
#ifdef _MSC_VER
#include "bobins.h"
#else //_MSC_VER
#include <sys/mman.h>
#endif //_MSC_VER
#include "api_internal.h"
#include "command_and_control.h"
#include "submission_private.h"
#include "judgment_private.h"
#include "runtime_config.h"
#include "connected_entity_private.h"
#include "inspection.h"
#include "telemetry.h"

static void Razorback_Output_Thread (Thread_t *thread);
static int Context_KeyCmp(void *a, const void *b);
static int Context_Cmp(void *a, void *b);

static List_t *sg_ContextList;
void initApi(void) {
    sg_ContextList = List_Create(LIST_MODE_GENERIC,
            Context_Cmp,
            Context_KeyCmp,
            NULL, NULL, NULL, NULL);
}

static void
Razorback_Remove_Context(struct RazorbackContext *context) {
    ASSERT(context != NULL);
    if (context == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Context is NULL", __func__);
        return;
    }
    List_Remove(sg_ContextList, context);
}

void Razorback_Destroy_Context(struct RazorbackContext *context) {
    ASSERT(context != NULL);
    if (context == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Context is NULL", __func__);
        return;
    }
    if (context->inspector.dataTypeList != NULL)
        free(context->inspector.dataTypeList);
    if (context->regSem != NULL)
        Semaphore_Destroy(context->regSem);
    free(context);
}

SO_PUBLIC bool
Razorback_Init_Context (struct RazorbackContext *context) {
    uuid_t l_pUuid;

    ASSERT(context != NULL);
    if (context == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Context is NULL", __func__);
        return false;
    }

    context->locality = Config_getLocalityId();

    // Init the registration semaphore.
    if ((context->regSem = Semaphore_Create(false, 0)) == NULL) {
        return false;
    }

    List_Push(sg_ContextList, context);
    // Launch C&C for this context
    if (!CommandAndControl_Start (context)) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to start command and control", __func__);
        Razorback_Remove_Context(context);
        return false;
    }


    UUID_Get_UUID (NUGGET_TYPE_COLLECTION, UUID_TYPE_NUGGET_TYPE, l_pUuid);
    if (uuid_compare (context->uuidNuggetType, l_pUuid) == 0) {
        // XXX: This has interesting side effects -> this is not the context we
        // expect later in execution.
        if (!Submission_Init (context)) {
            rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to initialize submission api", __func__);
            Razorback_Remove_Context(context);
            return false;
        }
    }
    return true;
}

SO_PUBLIC struct RazorbackContext *
Razorback_LookupContext (uuid_t nuggetId) {
    return (struct RazorbackContext *)List_Find(sg_ContextList, nuggetId);
}

SO_PUBLIC struct RazorbackContext *
Razorback_Init_Inspection_Context (uuid_t nuggetId,
                                   uuid_t applicationType,
                                   uint32_t dataTypeCount,
                                   uuid_t * dataTypeList,
                                   struct RazorbackInspectionHooks *inspectionHooks,
                                   uint32_t initialThreads, uint32_t maxThreads
                                   ) {
    struct RazorbackContext *context;
    uuid_t uuidInspector;
    UUID_Get_UUID (NUGGET_TYPE_INSPECTION, UUID_TYPE_NUGGET_TYPE, uuidInspector);

    ASSERT(inspectionHooks != NULL);
    if (inspectionHooks == NULL){
        rzb_log (LOG_ERR, LOG_C_CORE, "%s: Inspection Hooks NULL", __func__);
        return NULL;
    }

    if ((context = (struct RazorbackContext *)calloc (1, sizeof (struct RazorbackContext))) == NULL) {
        rzb_log (LOG_ERR, LOG_C_CORE, "%s: Failed to malloc new context", __func__);
        return NULL;
    }

    uuid_copy (context->uuidNuggetId, nuggetId);
    uuid_copy (context->uuidNuggetType, uuidInspector);
    uuid_copy (context->uuidApplicationType, applicationType);
    context->iFlags = 0;
    context->inspector.dataTypeCount = dataTypeCount;
    context->inspector.dataTypeList = dataTypeList;
    context->inspector.dataTypeList = calloc(dataTypeCount, sizeof(uuid_t));
    if (context->inspector.dataTypeList == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to allocate data type list", __func__);
        Razorback_Destroy_Context(context);
        return NULL;
    }
    memcpy(context->inspector.dataTypeList, dataTypeList, dataTypeCount *sizeof(uuid_t));
    context->pCommandHooks = NULL;
    context->inspector.hooks = inspectionHooks;

    if (!Razorback_Init_Context (context)) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to initialize context", __func__);
        Razorback_Destroy_Context(context);
        return NULL;
    }

    if ((context->inspector.judgmentQueue = Queue_Create(JUDGMENT_QUEUE, false, QUEUE_FLAG_SEND)) == NULL) {
        rzb_log (LOG_ERR, LOG_C_CORE, "%s: Failed to create judgment queue", __func__);
        Razorback_Remove_Context(context);
        return NULL;
    }

    if (!Inspection_Launch (context, initialThreads, maxThreads)) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to launch inspection threads", __func__);
        Razorback_Remove_Context(context);
        return NULL;
    }
    // XXX: This has interesting side effects -> this is not the context we expect later in execution.
    if (!Submission_Init (context)) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to initialize submission api", __func__);
        Razorback_Remove_Context(context);
        return NULL;
    }

    return context;
}

SO_PUBLIC struct RazorbackContext *
Razorback_Init_Output_Context (uuid_t nuggetId,
                                   uuid_t applicationType) {
    struct RazorbackContext *context;
    uuid_t uuidOutput;
    UUID_Get_UUID (NUGGET_TYPE_OUTPUT, UUID_TYPE_NUGGET_TYPE, uuidOutput);

    if ((context = calloc (1, sizeof (struct RazorbackContext))) == NULL)
    {
        rzb_log (LOG_ERR, LOG_C_CORE, "%s: Failed to malloc new context", __func__);
        return NULL;
    }

    uuid_copy (context->uuidNuggetId, nuggetId);
    uuid_copy (context->uuidNuggetType, uuidOutput);
    uuid_copy (context->uuidApplicationType, applicationType);
    context->iFlags = 0;
    context->pCommandHooks = NULL;
    context->inspector.hooks = NULL;
    context->output.threads = List_Create(LIST_MODE_GENERIC,
            Thread_Cmp,
            Thread_KeyCmp,
            NULL, // destroy
            NULL, //Clone
            NULL, // lock
            NULL); // unlock

    if (!Razorback_Init_Context (context)) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to initialize context", __func__);
        Razorback_Destroy_Context(context);
        return NULL;
    }

    return context;
}

SO_PUBLIC struct RazorbackContext *
Razorback_Init_Collection_Context (uuid_t nuggetId,
                                   uuid_t applicationType) {
    struct RazorbackContext *context;
    uuid_t uuidCollection;
    UUID_Get_UUID (NUGGET_TYPE_COLLECTION, UUID_TYPE_NUGGET_TYPE, uuidCollection);

    if ((context = calloc (1, sizeof (struct RazorbackContext))) == NULL) {
        rzb_log (LOG_ERR,LOG_C_CORE, "%s: Failed to malloc new context", __func__);
        return NULL;
    }

    uuid_copy (context->uuidNuggetId, nuggetId);
    uuid_copy (context->uuidNuggetType, uuidCollection);
    uuid_copy (context->uuidApplicationType, applicationType);
    context->iFlags = CONTEXT_FLAG_STAND_ALONE;
    context->inspector.dataTypeCount = 0;
    context->inspector.dataTypeList = NULL;
    context->pCommandHooks = NULL;
    context->inspector.hooks = NULL;


    if (!Razorback_Init_Context (context)) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to initialize context", __func__);
        Razorback_Destroy_Context(context);
        return NULL;
    }
    return context;
}

int
Kill_Output_Thread(void *ut, void *ud) {
    Thread_t *thread = ut;
    Thread_InterruptAndJoin(thread);
    Thread_Destroy(thread);
    return LIST_EACH_OK;
}

SO_PUBLIC void
Razorback_Shutdown_Context (struct RazorbackContext *context) {
    CommandAndControl_Pause();
    CommandAndControl_SendBye(context);

    Inspection_Shutdown(context);

    List_Remove(sg_ContextList, context);

    CommandAndControl_Unpause();
    if ((context->iFlags & CONTEXT_FLAG_STAND_ALONE) ==
        CONTEXT_FLAG_STAND_ALONE) {
       CommandAndControl_Shutdown();
    }

    if (context->inspector.judgmentQueue != NULL) {
        Queue_Terminate(context->inspector.judgmentQueue);
        context->inspector.judgmentQueue = NULL;
    }

    if (context->output.threads != NULL) {
        List_ForEach(context->output.threads, Kill_Output_Thread, NULL);
        List_Destroy(context->output.threads);
    }

    Razorback_Destroy_Context(context);

}

struct ContextHook
{
    int (*function) (struct RazorbackContext *, void *);
    void *userData;
};

static int
ForEach_Context_Wrapper(struct RazorbackContext *context, void *data) {
    struct ContextHook *hook = data;
    Thread_t * thread = Thread_GetCurrent();
    struct RazorbackContext *prev;
    int ret;

    if (thread != NULL) {
        prev = Thread_GetContext(thread);
        Thread_ChangeContext(thread,context);
    }

    ret= hook->function(context, hook->userData);

    if (thread != NULL) {
        Thread_ChangeContext(thread,prev);
        Thread_Destroy(thread);
    }

    return ret;
}

bool
Razorback_ForEach_Context (int (*function) (struct RazorbackContext *, void *), void *userData) {
    struct ContextHook hook;
    hook.function = function;
    hook.userData = userData;
    return List_ForEach(sg_ContextList, (int (*)(void *, void *))ForEach_Context_Wrapper, &hook);
}


SO_PUBLIC bool
Razorback_Render_Verdict (struct Judgment *judgment) {
    struct Message *message;
    struct RazorbackContext *context;

    ASSERT(judgment != NULL);
    if (judgment == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Judgment is NULL", __func__);
        return false;
    }

    context = Thread_GetCurrentContext();

    if ((message = MessageJudgmentSubmission_Initialize (JUDGMENT_REASON_ALERT, judgment)) == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to create message", __func__);
        return false;
    }

    Mutex_Lock (sg_mPauseLock);
    Queue_Put (context->inspector.judgmentQueue, message);
    Mutex_Unlock (sg_mPauseLock);
    ((struct MessageJudgmentSubmission *)message->message)->pJudgment = NULL;
    message->destroy(message);

    return true;
}


SO_PUBLIC bool
Razorback_Output_Launch (struct RazorbackContext *context, struct RazorbackOutputHooks *hooks) {
    char *nugName, *threadName;
    Thread_t *thread;

    ASSERT(context != NULL);
    if (context == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Context is NULL", __func__);
        return false;
    }
    ASSERT(hooks != NULL);
    if (hooks == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Hooks is NULL", __func__);
        return false;
    }

    nugName =
        UUID_Get_NameByUUID (context->uuidApplicationType,
                             UUID_TYPE_NUGGET_TYPE);
    threadName = NULL;
    if (asprintf (&threadName, "Output Thread: %s", nugName) == -1) {
        rzb_log (LOG_ERR, LOG_C_CORE, "%s: Failed to allocate thread name", __func__);
        free(nugName);
        return false;
    }
    free(nugName);
    if ((thread = Thread_Launch(Razorback_Output_Thread, hooks, threadName, context)) == NULL) {
        rzb_log (LOG_ERR, LOG_C_CORE, "%s: Failed to launch thread.", __func__);
        return false;
    }
    List_Push(context->output.threads, thread);
    return true;
}


static void
Razorback_Output_Thread (Thread_t *thread)
{
    struct Message *message;
    struct RazorbackOutputHooks *hooks;
    struct TelemetrySpan *processSpan;
    char *name;
    bool processSuccess;
    const char *processError;

    ASSERT(thread != NULL);
    if (thread == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Thread is NULL", __func__);
        return;
    }

    hooks = (struct RazorbackOutputHooks*) Thread_GetUserData(thread);
    if (hooks == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Hooks is NULL", __func__);
        return;
    }


    switch (hooks->messageType) {
        case MESSAGE_TYPE_ALERT_PRIMARY:
            if (asprintf(&name, "Alert.%s", hooks->pattern) == -1) {
                rzb_log (LOG_ERR, LOG_C_CORE, "%s: Failed to allocate queue name", __func__);
                return;
            }
            break;
        case MESSAGE_TYPE_ALERT_CHILD:
            if (asprintf(&name, "ChildAlert.%s", hooks->pattern) == -1) {
                rzb_log (LOG_ERR, LOG_C_CORE, "%s: Failed to allocate queue name", __func__);
                return;
            }
            break;
        case MESSAGE_TYPE_OUTPUT_EVENT:
            if (asprintf(&name, "Event.%s", hooks->pattern) == -1) {
                rzb_log (LOG_ERR, LOG_C_CORE, "%s: Failed to allocate queue name", __func__);
                return;
            }
            break;
        default:
            rzb_log(LOG_ERR, LOG_C_CORE, "%s: Unsupported output message type %u", __func__, hooks->messageType);
            return;
    }

    if ((hooks->queue = Queue_Create (name, true, QUEUE_FLAG_RECV)) == NULL){
        rzb_log (LOG_ERR, LOG_C_CORE, "%s: Failed to connect to MQ - Inspector Queue",
                 __func__);
        free(name);
        return;
    }

    rzb_log(LOG_DEBUG, LOG_C_CORE, "%s: Inspection Thread Launched", __func__);

    while (!Thread_IsStopped(thread)) {
        if ((message = Queue_Get (hooks->queue)) == NULL) {
            // timeout
            if (errno == EAGAIN || errno == EINTR)
                continue;
            // error
            rzb_log (LOG_ERR, LOG_C_CORE,
                     "%s: Dropped block due to failure of InspectorQueue_Get()",
                     __func__);
            // drop message
            continue;
        }

        if (message->type != hooks->messageType) {
            processSpan = Telemetry_StartMessageProcessSpan(hooks->queue, message);
            Telemetry_EndSpan(processSpan, false, "unexpected output message type");
            message->destroy(message);
            continue;
        }

        processSpan = Telemetry_StartMessageProcessSpan(hooks->queue, message);
        processSuccess = true;
        processError = NULL;
        switch (message->type) {
            case MESSAGE_TYPE_ALERT_PRIMARY:
                hooks->handleAlertPrimary((struct MessageAlertPrimary*)message->message);
                break;
            case MESSAGE_TYPE_ALERT_CHILD:
                hooks->handleAlertChild((struct MessageAlertChild *)message->message);
                break;
            case MESSAGE_TYPE_OUTPUT_EVENT:
                hooks->handleEvent((struct MessageOutputEvent *)message->message);
                break;
            default:
                rzb_log(LOG_ERR, LOG_C_CORE, "%s: Unsupported output message type %u",
                        __func__, message->type);
                processSuccess = false;
                processError = "unsupported output message type";
                break;
        }
        Telemetry_EndSpan(processSpan, processSuccess, processError);
        message->destroy(message);
    }
    Queue_Terminate(hooks->queue);
    free(name);
}

static int
Context_KeyCmp(void *a, const void *id) {
    struct RazorbackContext *cA = (struct RazorbackContext*) a;
    // XXX: This is a hack
    return uuid_compare(cA->uuidNuggetId, (const unsigned char *)id);
}

static int
Context_Cmp(void *a, void *b) {
    struct RazorbackContext *cA = (struct RazorbackContext*) a;
    struct RazorbackContext *cB = (struct RazorbackContext*) b;
    if (a == b)
        return 0;
    return uuid_compare(cA->uuidNuggetId, cB->uuidNuggetId);
}
