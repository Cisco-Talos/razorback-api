#include "config.h"

#include <razorback/debug.h>
#include <razorback/api.h>
#include <razorback/uuids.h>
#include <razorback/log.h>
#include <razorback/thread.h>
#include <razorback/ntlv.h>
#include <razorback/queue.h>
#include <razorback/inspector_queue.h>

#include <errno.h>
#include <pthread.h>


#include "api_internal.h"
#include "command_and_control.h"
#include "submission_private.h"
#include "console.h"

static bool Razorback_Inspection_Launch(struct RazorbackContext *p_pContext);
static void Razorback_Inspection_Thread(struct Thread *p_pThread);

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

static struct ContextList sg_ContextList =
    { PTHREAD_MUTEX_INITIALIZER, NULL, NULL };


SO_PUBLIC bool
Razorback_Init_Context (struct RazorbackContext *p_pContext)
{
    struct ContextListNode *l_pNode;
    uuid_t *l_pUuid;

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
        // TODO: Free Node
        //free(l_pNode);
        return false;
    }

    if ((p_pContext->iFlags & CONTEXT_FLAG_STAND_ALONE) ==
        CONTEXT_FLAG_STAND_ALONE)
    {
//        Console_Start(p_pContext);
    }

    // Launch Inspection Thread
    l_pUuid = UUID_Get_UUID(NUGGET_TYPE_INSPECTION, UUID_TYPE_NUGGET_TYPE);
    if (uuid_compare(p_pContext->uuidNuggetType, *l_pUuid) == 0) 
    {
        if(!Razorback_Inspection_Launch(p_pContext)) 
        {
            return false;
        }
        Submission_Init(p_pContext);
    }
    
    l_pUuid = UUID_Get_UUID(NUGGET_TYPE_COLLECTION, UUID_TYPE_NUGGET_TYPE);
    if (uuid_compare(p_pContext->uuidNuggetType, *l_pUuid) == 0) 
        Submission_Init(p_pContext);

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
Razorback_Init_Inspection_Context (
        uuid_t p_uuidNuggetId, uuid_t p_uuidApplicationType,
        uint32_t p_iDataTypeCount, uuid_t *p_pDataTypeList,
        struct RazorbackInspectionHooks *p_pInspectionHooks)
{
    struct RazorbackContext *l_pContext;
    uuid_t *l_uuidInspector = UUID_Get_UUID(NUGGET_TYPE_INSPECTION, UUID_TYPE_NUGGET_TYPE);

    rzb_log(LOG_DEBUG, "Razorback_Init_Inspection_Context: Called");

    if (p_pInspectionHooks == NULL) 
    {
        rzb_log(LOG_ERR, "Razorback_Init_Inspection_Context: Inspection Hooks NULL");
        return NULL;
    }

    if ((l_pContext = calloc(1,sizeof(struct RazorbackContext))) == NULL )
    {
        rzb_log(LOG_ERR, "Razorback_Init_Inspection_Context: Failed to malloc new context");
        return NULL;
    }
    
    uuid_copy(l_pContext->uuidNuggetId, p_uuidNuggetId);
    uuid_copy(l_pContext->uuidNuggetType, *l_uuidInspector);
    uuid_copy(l_pContext->uuidApplicationType, p_uuidApplicationType);
    l_pContext->iFlags=0;
    l_pContext->iDataTypeCount=p_iDataTypeCount;
    l_pContext->pDataTypeList=p_pDataTypeList;
    l_pContext->pCommandHooks=NULL;
    l_pContext->pInspectionHooks=p_pInspectionHooks;


    if (!Razorback_Init_Context(l_pContext))
    {
        free(l_pContext);
        return NULL;
    }

    return l_pContext;
}

SO_PUBLIC struct RazorbackContext *
Razorback_Init_Collection_Context (
        uuid_t p_uuidNuggetId, uuid_t p_uuidApplicationType)
{
    struct RazorbackContext *l_pContext;
    uuid_t *l_uuidInspector = UUID_Get_UUID(NUGGET_TYPE_COLLECTION, UUID_TYPE_NUGGET_TYPE);

    rzb_log(LOG_DEBUG, "Razorback_Init_Collection_Context: Called");


    if ((l_pContext = calloc(1,sizeof(struct RazorbackContext))) == NULL )
    {
        rzb_log(LOG_ERR, "Razorback_Init_Collection_Context: Failed to malloc new context");
        return NULL;
    }
    
    uuid_copy(l_pContext->uuidNuggetId, p_uuidNuggetId);
    uuid_copy(l_pContext->uuidNuggetType, *l_uuidInspector);
    uuid_clear(l_pContext->uuidApplicationType);
    l_pContext->iFlags=CONTEXT_FLAG_STAND_ALONE;
    l_pContext->iDataTypeCount=0;
    l_pContext->pDataTypeList=NULL;
    l_pContext->pCommandHooks=NULL;
    l_pContext->pInspectionHooks=NULL;


    if (!Razorback_Init_Context(l_pContext))
    {
        free(l_pContext);
        return NULL;
    }
    return l_pContext;
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

static bool Razorback_Inspection_Launch(struct RazorbackContext *p_pContext) 
{

    // TODO: Flesh out name.
    if (Thread_Launch (Razorback_Inspection_Thread, NULL, (char *) "Inspection Thread", p_pContext) == NULL)
    {
        rzb_log (LOG_ERR, "C&C Error: Failed to launch C&C thread.");
        return false;
    }
    return true;
}

static void Razorback_Inspection_Thread(struct Thread *p_pThread) {
    struct RazorbackContext *l_pContext;
    l_pContext = Thread_GetContext(p_pThread);
    struct MessageInspectionSubmission l_misMessage;
    struct Block *l_pBlock;
    struct Queue *l_pQueue;
    if ((l_pQueue = InspectorQueue_Initialize(l_pContext->uuidApplicationType, QUEUE_FLAG_RECV)) == NULL)
    {
        rzb_log (LOG_ERR, "Inspection Thread: Failed to connect to MQ.");
        return;
    }
    while (true) // TODO: This should not be for ever and ever ever.
    {
        if (!InspectorQueue_Get (l_pQueue, &l_misMessage))
        {
            // timeout
            if (errno == EAGAIN)
                continue;
            // error
            rzb_log (LOG_ERR,
                     "Dropped block due to failure of InspectorQueue_Get()");
            // drop message
            continue;
        }
        if (l_misMessage.pBlock == NULL )
        {
            rzb_log(LOG_ERR, "Inspection Thread: Failed dispatch message due to NULL block");
            continue;
        }
        l_pBlock = l_misMessage.pBlock;
        l_misMessage.pBlock=NULL;
//        MessageInspectionSubmission_Destroy(&l_misMessage);
        // TODO:  Submit the result
        l_pContext->pInspectionHooks->processBlock(l_pBlock);
        pthread_mutex_lock(&sg_mPauseLock);
        rzb_log(LOG_DEBUG, "This message is originating from judgement submission with pause lock held");
        pthread_mutex_unlock(&sg_mPauseLock);
    }
    return;
}

