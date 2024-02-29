#include "config.h"

#include <razorback/debug.h>
#include <razorback/api.h>
#include <razorback/uuids.h>
#include <razorback/log.h>
#include <razorback/thread.h>
#include <razorback/event.h>
#include <razorback/ntlv.h>
#include <razorback/block.h>
#include <razorback/storage.h>
#include <razorback/queue.h>
#include <razorback/inspector_queue.h>
#include <razorback/judgment.h>
#include <razorback/judgment_queue.h>

#include <errno.h>
#include <pthread.h>

#include "api_internal.h"
#include "command_and_control.h"
#include "submission_private.h"
#include "judgment_private.h"

static bool Razorback_Inspection_Launch (struct RazorbackContext *p_pContext);
static void Razorback_Inspection_Thread (struct Thread *p_pThread);


struct ContextListNode
{
    struct RazorbackContext *pContext;
    struct ContextListNode *pNext;

};

struct ContextList
{
    pthread_mutex_t mutex;
    struct ContextListNode *pHead;
    struct ContextListNode *pTail;
};

static struct ContextList sg_ContextList;
void initApi(void)
{
    pthread_mutexattr_t mutexAttr;
    memset(&mutexAttr, 0, sizeof(pthread_mutexattr_t));
    pthread_mutexattr_init(&mutexAttr);
    pthread_mutexattr_settype(&mutexAttr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&sg_ContextList.mutex, &mutexAttr); 
}

static void 
Razorback_Remove_Context(struct RazorbackContext *context)
{
    struct ContextListNode *l_pNode = sg_ContextList.pHead;
    struct ContextListNode *l_pPrev = NULL;
    pthread_mutex_lock (&sg_ContextList.mutex);
    while (l_pNode != NULL)
    {
        if (context == l_pNode->pContext)
        {
            if (l_pNode == sg_ContextList.pHead)
            {
                sg_ContextList.pHead = NULL;
                sg_ContextList.pTail = NULL;
            } 
            else if (l_pNode == sg_ContextList.pTail)
            {
                sg_ContextList.pTail = l_pPrev;
                l_pPrev->pNext = NULL;
            }
            else
                l_pPrev->pNext = l_pNode->pNext;

            free(l_pNode);
            pthread_mutex_unlock (&sg_ContextList.mutex);
            return;
        }
        l_pPrev = l_pNode;
        l_pNode = l_pNode->pNext;
    }
    pthread_mutex_unlock (&sg_ContextList.mutex);
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
    struct ContextListNode *l_pNode;
    uuid_t l_pUuid;

    // Init the registration semaphore.
    sem_init (&p_pContext->regSem, 0, 0);

    if ((l_pNode = calloc (1, sizeof (struct ContextListNode))) == NULL)
        return false;

    l_pNode->pContext = p_pContext;


    pthread_mutex_lock (&sg_ContextList.mutex);
    if (sg_ContextList.pHead == NULL)
    {
        sg_ContextList.pHead = l_pNode;
        sg_ContextList.pTail = l_pNode;
    }
    else
    {
        sg_ContextList.pTail->pNext = l_pNode;
        sg_ContextList.pTail = l_pNode;
    }

    pthread_mutex_unlock (&sg_ContextList.mutex);

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
        if (!Razorback_Inspection_Launch (p_pContext))
        {
            Razorback_Remove_Context(p_pContext);
            return false;
        }
        if (!Submission_Init (p_pContext))
        {
            rzb_log(LOG_ERR, "%s: Failed to initialize submission api", __func__);
            Razorback_Remove_Context(p_pContext);
            return false;
        }
        JudgmentQueue_Initialize(QUEUE_FLAG_SEND);
    }

    UUID_Get_UUID (NUGGET_TYPE_COLLECTION, UUID_TYPE_NUGGET_TYPE, l_pUuid);
    if (uuid_compare (p_pContext->uuidNuggetType, l_pUuid) == 0)
    {
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
    struct RazorbackContext *l_pContext = NULL;
    struct ContextListNode *l_pNode = sg_ContextList.pHead;
    pthread_mutex_lock (&sg_ContextList.mutex);
    while (l_pNode != NULL)
    {
        if (uuid_compare (p_uuidNugget, l_pNode->pContext->uuidNuggetId) == 0)
        {
            l_pContext = l_pNode->pContext;
            break;
        }
        l_pNode = l_pNode->pNext;
    }
    pthread_mutex_unlock (&sg_ContextList.mutex);

    return l_pContext;
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

    if (!Data_Storage_Curl_Initialize()) 
    {
        Razorback_Destroy_Context(l_pContext);
        return NULL;
    }

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
    pthread_mutex_lock (&sg_ContextList.mutex);
    if (context->pInspectionThread != NULL)
        Thread_Stop(context->pInspectionThread);
    Razorback_Remove_Context(context);
    Razorback_Destroy_Context(context);
    pthread_mutex_unlock (&sg_ContextList.mutex);
    CommandAndControl_Unpause();
}

bool
Razorback_ForEach_Context (bool (*function) (struct RazorbackContext *))
{
    struct ContextListNode *l_pNode = sg_ContextList.pHead;
    pthread_mutex_lock (&sg_ContextList.mutex);
    while (l_pNode != NULL)
    {
        if (!function (l_pNode->pContext))
        {
            pthread_mutex_unlock (&sg_ContextList.mutex);
            return false;
        }
        l_pNode = l_pNode->pNext;
    }
    pthread_mutex_unlock (&sg_ContextList.mutex);
    return true;
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
    struct MessageInspectionSubmission l_misMessage;
    struct MessageJudgmentSubmission l_mjsMessage;
    struct Block *l_pBlock, *l_pClonedBlock;
    struct EventId *l_pEventId;
    struct Queue *l_pQueue;
    uint8_t l_iResult, *l_pData;
    struct Judgment *judgment;

    if (!JudgmentQueue_Initialize (QUEUE_FLAG_SEND))
    {
        rzb_log (LOG_ERR, "%s: Failed to connect to MQ - Judgment Queue.",
                 __func__);
        return;
    }

    if ((l_pQueue =
         InspectorQueue_Initialize (l_pContext->uuidApplicationType,
                                    QUEUE_FLAG_RECV)) == NULL)
    {
        rzb_log (LOG_ERR, "%s: Failed to connect to MQ - Inspector Queue",
                 __func__);
        return;
    }

    while (!Thread_IsStopped(p_pThread))                // TODO: This should not be for ever and ever ever.
    {
        if (!InspectorQueue_Get (l_pQueue, &l_misMessage))
        {
            // timeout
            if (errno == EAGAIN)
                continue;
            // error
            rzb_log (LOG_ERR,
                     "%s: Dropped block due to failure of InspectorQueue_Get()",
                     __func__);
            // drop message
            continue;
        }
        if (l_misMessage.pBlock == NULL)
        {
            rzb_log (LOG_ERR, "%s: Failed dispatch message due to NULL block",
                     __func__);
            continue;
        }
        if (l_misMessage.pBlock->pId->pHash == NULL)
        {
            rzb_log (LOG_ERR, "%s: Failed dispatch message due to NULL Hash",
                     __func__);
            continue;
        }

		if (l_misMessage.pBlock->ticketSize != 0) {
			if (RetrieveDataBlock(l_misMessage.pBlock) == 0) {
    	        rzb_log (LOG_ERR, "%s: Failed to retrieve data ticket",
        	             __func__);
 	           continue;
			}
		}

        l_pBlock = l_misMessage.pBlock;
        l_misMessage.pBlock = NULL;
        l_pData = l_pBlock->pData;
        l_pBlock->pData = NULL;
        if ((l_pEventId = EventId_Clone(l_misMessage.eventId)) == NULL)
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
                                                        l_misMessage.eventId,
                                                        l_misMessage.pEventMetadata);

        MessageInspectionSubmission_Destroy (&l_misMessage);
        if ((l_iResult != JUDGMENT_REASON_DONE)
            && (l_iResult != JUDGMENT_REASON_ERROR)
            && (l_iResult != JUDGMENT_REASON_DEFERRED))
        {
            rzb_log (LOG_ERR, "%s: Bad return from inspection", __func__);
            continue;
        }

        // Destroy the copy, we dont need it any more
        Block_Destroy (l_pClonedBlock);

        // Lock the pause lock before submitting judgment
        pthread_mutex_lock (&sg_mPauseLock);
        judgment = Judgment_Create(l_pEventId);
        MessageJudgmentSubmission_Initialize (&l_mjsMessage, l_iResult, judgment);

        JudgmentQueue_Put (&l_mjsMessage);
        MessageJudgmentSubmission_Destroy (&l_mjsMessage);
        pthread_mutex_unlock (&sg_mPauseLock);
        Block_Destroy(l_pBlock);
        EventId_Destroy(l_pEventId);
    }
    return;
}

SO_PUBLIC bool
Razorback_Render_Verdict (struct Judgment *p_pJudgment)
{
    struct MessageJudgmentSubmission l_mjsMessage;
    struct RazorbackContext *l_pContext;

    l_pContext = Thread_GetCurrentContext ();

    MessageJudgmentSubmission_Initialize (&l_mjsMessage, JUDGMENT_REASON_ALERT, p_pJudgment);
    pthread_mutex_lock (&sg_mPauseLock);
    JudgmentQueue_Put (&l_mjsMessage);
    pthread_mutex_unlock (&sg_mPauseLock);
    l_mjsMessage.pJudgment = NULL;
    MessageJudgmentSubmission_Destroy (&l_mjsMessage);

    return true;
}

