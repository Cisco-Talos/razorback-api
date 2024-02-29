#include "config.h"

#include <razorback/debug.h>
#include <razorback/api.h>
#include <razorback/uuids.h>
#include <razorback/list.h>
#include <razorback/log.h>
#include <razorback/thread.h>
#include <razorback/event.h>
#include <razorback/ntlv.h>
#include <razorback/block.h>
#include <razorback/storage.h>
#include <razorback/queue.h>
#include <razorback/inspector_queue.h>
#include <razorback/judgment.h>

#include <errno.h>
#include <pthread.h>

#include "api_internal.h"
#include "command_and_control.h"
#include "submission_private.h"
#include "judgment_private.h"

static bool Razorback_Inspection_Launch (struct RazorbackContext *p_pContext);
static void Razorback_Inspection_Thread (struct Thread *p_pThread);
static int Context_KeyCmp(void *a, void *b);
static int Context_Cmp(void *a, void *b);

static struct List *sg_ContextList;
void initApi(void)
{
    sg_ContextList = List_Create(LIST_MODE_GENERIC, 
            Context_Cmp, 
            Context_KeyCmp, NULL, NULL);
}

static void 
Razorback_Remove_Context(struct RazorbackContext *context)
{
    List_Remove(sg_ContextList, context);
}

static void Razorback_Destroy_Context(struct RazorbackContext *context)
{
    sem_destroy(&context->regSem);
    free(context);
}

SO_PUBLIC bool
Razorback_Init_Context (struct RazorbackContext *p_pContext)
{
    ASSERT(p_pContext != NULL);
    uuid_t l_pUuid;

    // Init the registration semaphore.
    sem_init (&p_pContext->regSem, 0, 0);

    List_Push(sg_ContextList, p_pContext);
    // Launch C&C for this context
    if (!CommandAndControl_Start (p_pContext))
    {
        Razorback_Remove_Context(p_pContext);
        return false;
    }

    // Launch Inspection Thread
    UUID_Get_UUID (NUGGET_TYPE_INSPECTION, UUID_TYPE_NUGGET_TYPE, l_pUuid);
    if (uuid_compare (p_pContext->uuidNuggetType, l_pUuid) == 0)
    {
        if ((p_pContext->judgmentQueue = Queue_Create(JUDGMENT_QUEUE, QUEUE_FLAG_SEND)) == NULL)
        {
            Razorback_Remove_Context(p_pContext);
            return false;
        }

        if (!Razorback_Inspection_Launch (p_pContext))
        {
            Razorback_Remove_Context(p_pContext);
            return false;
        }
        // XXX: This has interesting side effects -> this is not the context we expect later in execution.
        if (!Submission_Init (p_pContext))
        {
            rzb_log(LOG_ERR, "%s: Failed to initialize submission api", __func__);
            Razorback_Remove_Context(p_pContext);
            return false;
        }
    }

    UUID_Get_UUID (NUGGET_TYPE_COLLECTION, UUID_TYPE_NUGGET_TYPE, l_pUuid);
    if (uuid_compare (p_pContext->uuidNuggetType, l_pUuid) == 0)
    {
        // XXX: This has interesting side effects -> this is not the context we expect later in execution.
        if (!Submission_Init (p_pContext))
        {
            rzb_log(LOG_ERR, "%s: Failed to initialize submission api", __func__);
            Razorback_Remove_Context(p_pContext);
            return false;
        }
    }
    return true;
}

SO_PUBLIC struct RazorbackContext *
Razorback_LookupContext (uuid_t p_uuidNugget)
{
    return (struct RazorbackContext *)List_Find(sg_ContextList, p_uuidNugget);
}

SO_PUBLIC struct RazorbackContext *
Razorback_Init_Inspection_Context (uuid_t p_uuidNuggetId,
                                   uuid_t p_uuidApplicationType,
                                   uint32_t p_iDataTypeCount,
                                   uuid_t * p_pDataTypeList,
                                   struct RazorbackInspectionHooks
                                   *p_pInspectionHooks)
{
    struct RazorbackContext *l_pContext;
    uuid_t l_uuidInspector;
    UUID_Get_UUID (NUGGET_TYPE_INSPECTION, UUID_TYPE_NUGGET_TYPE, l_uuidInspector);

    if (p_pInspectionHooks == NULL)
    {
        rzb_log (LOG_ERR, "%s: Inspection Hooks NULL", __func__);
        return NULL;
    }

    if ((l_pContext = calloc (1, sizeof (struct RazorbackContext))) == NULL)
    {
        rzb_log (LOG_ERR, "%s: Failed to malloc new context", __func__);
        return NULL;
    }

    uuid_copy (l_pContext->uuidNuggetId, p_uuidNuggetId);
    uuid_copy (l_pContext->uuidNuggetType, l_uuidInspector);
    uuid_copy (l_pContext->uuidApplicationType, p_uuidApplicationType);
    l_pContext->iFlags = 0;
    l_pContext->iDataTypeCount = p_iDataTypeCount;
    l_pContext->pDataTypeList = p_pDataTypeList;
    l_pContext->pCommandHooks = NULL;
    l_pContext->pInspectionHooks = p_pInspectionHooks;

    if (!Razorback_Init_Context (l_pContext))
    {
        Razorback_Destroy_Context(l_pContext);
        return NULL;
    }

    return l_pContext;
}

SO_PUBLIC struct RazorbackContext *
Razorback_Init_Collection_Context (uuid_t p_uuidNuggetId,
                                   uuid_t p_uuidApplicationType)
{
    struct RazorbackContext *l_pContext;
    uuid_t l_uuidInspector;
        UUID_Get_UUID (NUGGET_TYPE_COLLECTION, UUID_TYPE_NUGGET_TYPE, l_uuidInspector);

    if ((l_pContext = calloc (1, sizeof (struct RazorbackContext))) == NULL)
    {
        rzb_log (LOG_ERR, "%s: Failed to malloc new context", __func__);
        return NULL;
    }

    uuid_copy (l_pContext->uuidNuggetId, p_uuidNuggetId);
    uuid_copy (l_pContext->uuidNuggetType, l_uuidInspector);
    uuid_copy (l_pContext->uuidApplicationType, p_uuidApplicationType);
    l_pContext->iFlags = CONTEXT_FLAG_STAND_ALONE;
    l_pContext->iDataTypeCount = 0;
    l_pContext->pDataTypeList = NULL;
    l_pContext->pCommandHooks = NULL;
    l_pContext->pInspectionHooks = NULL;


    if (!Razorback_Init_Context (l_pContext))
    {
        Razorback_Destroy_Context(l_pContext);
        return NULL;
    }
    return l_pContext;
}

SO_PUBLIC void
Razorback_Shutdown_Context (struct RazorbackContext *context)
{
    CommandAndControl_Pause();
    CommandAndControl_SendBye(context);

    if (context->pInspectionThread != NULL)
        Thread_InterruptAndJoin(context->pInspectionThread);

    List_Remove(sg_ContextList, context);

    CommandAndControl_Unpause();
    if ((context->iFlags & CONTEXT_FLAG_STAND_ALONE) ==
        CONTEXT_FLAG_STAND_ALONE)
    {
       CommandAndControl_Shutdown();
    }
    if (context->judgmentQueue != NULL)
        Queue_Terminate(context->judgmentQueue);
    Razorback_Destroy_Context(context);

}

struct ContextHook
{
    int (*function) (struct RazorbackContext *, void *);
    void *userData;
};

static int
ForEach_Context_Wrapper(struct RazorbackContext *context, void *data)
{
    struct ContextHook *hook = data;
    struct Thread * thread = Thread_GetCurrent();
    struct RazorbackContext *prev;
    int ret;
    if (thread != NULL)
    {
        prev = Thread_GetContext(thread);
        Thread_ChangeContext(thread,context); 
    }
    ret= hook->function(context, hook->userData);
    if (thread != NULL)
        Thread_ChangeContext(thread,prev);
    return ret;
}

bool
Razorback_ForEach_Context (int (*function) (struct RazorbackContext *, void *), void *userData)
{
    struct ContextHook hook;
    hook.function = function;
    hook.userData = userData;
    return List_ForEach(sg_ContextList, (int (*)(void *, void *))ForEach_Context_Wrapper, &hook);
}

static bool
Razorback_Inspection_Launch (struct RazorbackContext *p_pContext)
{
    char *nugName, *threadName;

    nugName =
        UUID_Get_NameByUUID (p_pContext->uuidApplicationType,
                             UUID_TYPE_NUGGET_TYPE);
    threadName = NULL;
    if (asprintf (&threadName, "Inspection Thread: %s", nugName) == -1)
    {
        rzb_log (LOG_ERR, "%s: Failed to allocate thread name", __func__);
        free(nugName);
        return false;
    }
    free(nugName);
    if ((p_pContext->pInspectionThread = Thread_Launch
        (Razorback_Inspection_Thread, NULL, threadName, p_pContext)) == NULL)
    {
        rzb_log (LOG_ERR, "%s: Failed to launch thread.", __func__);
        return false;
    }
    return true;
}

static void
Razorback_Inspection_Thread (struct Thread *p_pThread)
{
    struct RazorbackContext *l_pContext;
    l_pContext = Thread_GetContext (p_pThread);
    struct Message *message;
    struct MessageInspectionSubmission *l_misMessage;
    struct Message *l_mjsMessage;
    struct Block *l_pBlock, *l_pClonedBlock;
    struct EventId *l_pEventId;
    struct Queue *l_pQueue;
    uint8_t l_iResult, *l_pData;
    struct Judgment *judgment;

    if ((l_pQueue =
         InspectorQueue_Initialize (l_pContext->uuidApplicationType,
                                    QUEUE_FLAG_RECV)) == NULL)
    {
        rzb_log (LOG_ERR, "%s: Failed to connect to MQ - Inspector Queue",
                 __func__);
        return;
    }
    rzb_log(LOG_DEBUG, "%s: Inspection Thread Launched", __func__);
    p_pThread->pUserData = l_pQueue;

    while (!Thread_IsStopped(p_pThread))                // TODO: This should not be for ever and ever ever.
    {
        if ((message = Queue_Get (l_pQueue)) == NULL)
        {
            // timeout
            if (errno == EAGAIN || errno == EINTR)
                continue;
            // error
            rzb_log (LOG_ERR,
                     "%s: Dropped block due to failure of InspectorQueue_Get()",
                     __func__);
            // drop message
            continue;
        }
        l_misMessage = message->message;
        if (l_misMessage->pBlock == NULL)
        {
            rzb_log (LOG_ERR, "%s: Failed dispatch message due to NULL block",
                     __func__);
            continue;
        }
        if (l_misMessage->pBlock->pId->pHash == NULL)
        {
            rzb_log (LOG_ERR, "%s: Failed dispatch message due to NULL Hash",
                     __func__);
            continue;
        }

		if (l_misMessage->ticket != NULL) {
			if (RetrieveDataBlock(l_misMessage->pBlock, l_misMessage->ticket) == 0) {
    	        rzb_log (LOG_ERR, "%s: Failed to retrieve data ticket",
        	             __func__);
 	           continue;
			}
		}
        if (l_misMessage->pBlock->pData == NULL)
        {
            rzb_log (LOG_ERR, "%s: No data block",__func__);
           continue;
        }
        l_pBlock = l_misMessage->pBlock;
        l_misMessage->pBlock = NULL;
        l_pData = l_pBlock->pData;
        l_pBlock->pData = NULL;
        if ((l_pEventId = EventId_Clone(l_misMessage->eventId)) == NULL)
        {
            rzb_log (LOG_ERR, "%s: Failed create new event id", __func__);
            continue;
        }
        

        // Clone the block for the inspector to use.
        if ((l_pClonedBlock = Block_Clone (l_pBlock)) == NULL)
        {
            rzb_log (LOG_ERR, "%s: Failed create new block", __func__);
            continue;
        }

        l_pClonedBlock->pData = l_pData;


        l_iResult =
            l_pContext->pInspectionHooks->processBlock (l_pClonedBlock,
                                                        l_misMessage->eventId,
                                                        l_misMessage->pEventMetadata);

        message->destroy(message);
        if ((l_iResult != JUDGMENT_REASON_DONE)
            && (l_iResult != JUDGMENT_REASON_ERROR)
            && (l_iResult != JUDGMENT_REASON_DEFERRED))
        {
            rzb_log (LOG_ERR, "%s: Bad return from inspection", __func__);
            continue;
        }


        // Lock the pause lock before submitting judgment
        pthread_mutex_lock (&sg_mPauseLock);
        judgment = Judgment_Create(l_pEventId, l_pClonedBlock->pId);
        // Destroy the copy, we dont need it any more
        Block_Destroy (l_pClonedBlock);
        if ((l_mjsMessage = MessageJudgmentSubmission_Initialize (l_iResult, judgment)) == NULL)
        {
            rzb_log(LOG_ERR, "%s: Failed to create message", __func__);
        }
        else
        {
            Queue_Put (l_pContext->judgmentQueue, l_mjsMessage);
            l_mjsMessage->destroy(l_mjsMessage);
        }
        pthread_mutex_unlock (&sg_mPauseLock);
        Block_Destroy(l_pBlock);
        EventId_Destroy(l_pEventId);
    }
    rzb_log(LOG_DEBUG, "%s: Inspection Thread Exiting", __func__);
    return;
}

SO_PUBLIC bool
Razorback_Render_Verdict (struct Judgment *p_pJudgment)
{
    struct Message *l_mjsMessage;
    struct RazorbackContext *l_pContext;
    l_pContext = Thread_GetContext (Thread_GetCurrent());

    if ((l_mjsMessage = MessageJudgmentSubmission_Initialize (JUDGMENT_REASON_ALERT, p_pJudgment)) == NULL)
    {
        rzb_log(LOG_ERR, "%s: Failed to create message", __func__);
        return false;
    }
    pthread_mutex_lock (&sg_mPauseLock);
    Queue_Put (l_pContext->judgmentQueue, l_mjsMessage);
    pthread_mutex_unlock (&sg_mPauseLock);
    ((struct MessageJudgmentSubmission *)l_mjsMessage->message)->pJudgment = NULL;
    l_mjsMessage->destroy(l_mjsMessage);

    return true;
}

static int 
Context_KeyCmp(void *a, void *id)
{
    struct RazorbackContext *cA = (struct RazorbackContext*) a;
    // XXX: This is a hack
    return uuid_compare(cA->uuidNuggetId, (unsigned char *) id);
}
static int 
Context_Cmp(void *a, void *b)
{
    struct RazorbackContext *cA = (struct RazorbackContext*) a;
    struct RazorbackContext *cB = (struct RazorbackContext*) b;
    if (a == b)
        return 0;
    return uuid_compare(cA->uuidNuggetId, cB->uuidNuggetId);
}

