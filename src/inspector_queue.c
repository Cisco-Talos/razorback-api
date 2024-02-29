#include "config.h"
#include <razorback/debug.h>
#include <razorback/inspector_queue.h>
#include <razorback/queue.h>
#include <razorback/queue_list.h>
#include <razorback/block.h>
#include <razorback/log.h>
#include <razorback/event.h>
#include <errno.h>

/** Globals
*/
static struct QueueList sg_qlInspectorQueue;
static bool sg_bInspectorIntialized = false;

static void
InspectorQueue_GetQueueName (uuid_t p_pApplicationType,
                             uint8_t * p_sQueueName)
{
    Queue_GetQueueName ((const uint8_t *) "/queue/INSPECTOR",
                        p_pApplicationType, p_sQueueName);
}

SO_PUBLIC struct Queue *
InspectorQueue_Initialize (uuid_t p_pApplicationType, int p_iFlags)
{

    // the name
    uint8_t l_sQueueName[128];
    // the queue from the list
    struct Queue *l_pQueue;

    // setup the global variables
    if (!sg_bInspectorIntialized)
    {
        QueueList_Initialize (&sg_qlInspectorQueue);
        sg_bInspectorIntialized = true;
    }

    // transform to correct name
    InspectorQueue_GetQueueName (p_pApplicationType, l_sQueueName);

    // does this queue already exist?
    // if so, done
    l_pQueue = QueueList_Find (&sg_qlInspectorQueue, p_pApplicationType);
    if (l_pQueue != NULL)
        return l_pQueue;

    // initialize the queue
    if ((l_pQueue = Queue_Create (l_sQueueName, p_iFlags)) == NULL)
        return NULL;

    // find the queue
    if (!QueueList_Add (&sg_qlInspectorQueue, l_pQueue, p_pApplicationType))
    {   
        // TODO: Terminate the queue.
        return NULL;
    }


    return l_pQueue;
}

SO_PUBLIC void
InspectorQueue_Terminate (uuid_t p_pApplicationType)
{
    // the queue from the list
    struct Queue *l_pQueue;

    // if we never initialized, do nothing
    if (!sg_bInspectorIntialized)
        return;

    // find the queue and terminate it
    l_pQueue = QueueList_Find (&sg_qlInspectorQueue, p_pApplicationType);
    if (l_pQueue != NULL)
        Queue_Terminate (l_pQueue);
}

SO_PUBLIC bool
InspectorQueue_Get (struct Queue *p_pQueue, struct MessageInspectionSubmission *p_pMessage)
{
    ASSERT (p_pMessage != NULL);
    ASSERT (p_pQueue != NULL);

    struct BinaryBuffer *l_pBuffer;
    // the queue from the list


    // check if ready
    if (!Socket_ReadyForRead (p_pQueue->pReadSocket))
    {
        errno = EAGAIN;
        return false;
    }

    // read from the queue
    if ((l_pBuffer = Queue_Get (p_pQueue)) == NULL)
        return false;
    
    // parse the buffer
    if (!BinaryBuffer_Get_MessageHeader (l_pBuffer, &p_pMessage->mhHeader))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        return false;
    }

    if (!BinaryBuffer_Get_uint32_t (l_pBuffer, &p_pMessage->iReason))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        return false;
    }
    if (!BinaryBuffer_Get_EventId (l_pBuffer, &p_pMessage->eventId))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        return false;
    }
    if (!BinaryBuffer_Get_NTLVList (l_pBuffer, &p_pMessage->pEventMetadata))
    {
        EventId_Destroy(p_pMessage->eventId);
        BinaryBuffer_Destroy (l_pBuffer);
        return false;
    }
    
    if (!BinaryBuffer_Get_Block (l_pBuffer, &p_pMessage->pBlock))
    {
        NTLVList_Destroy(p_pMessage->pEventMetadata);
        EventId_Destroy(p_pMessage->eventId);
        BinaryBuffer_Destroy (l_pBuffer);
        return false;
    }

    BinaryBuffer_Destroy (l_pBuffer);
    // done
    return true;
}

SO_PUBLIC bool
InspectorQueue_Put (struct Queue *p_pQueue, struct MessageInspectionSubmission * p_pMessage)
{
    ASSERT (p_pMessage != NULL);
    ASSERT (p_pQueue != NULL);
    struct BinaryBuffer *l_pBuffer;

    if ((l_pBuffer =
         BinaryBuffer_Create (p_pMessage->mhHeader.iLength)) == NULL)
        return false;

    if (!BinaryBuffer_Put_MessageHeader (l_pBuffer, &p_pMessage->mhHeader))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        return false;
    }

    if (!BinaryBuffer_Put_uint32_t (l_pBuffer, p_pMessage->iReason))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        return false;
    }
    if (!BinaryBuffer_Put_EventId (l_pBuffer, p_pMessage->eventId))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        return false;
    }
    if (!BinaryBuffer_Put_NTLVList (l_pBuffer, p_pMessage->pEventMetadata))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        return false;
    }

    if (!BinaryBuffer_Put_Block (l_pBuffer, p_pMessage->pBlock))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        return false;
    }


    // put in the queue
    if (!Queue_Put (p_pQueue, l_pBuffer))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        return false;
    }

    BinaryBuffer_Destroy (l_pBuffer);
    return true;
}
