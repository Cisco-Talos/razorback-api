#include "config.h"
#include <razorback/debug.h>
#include <razorback/judgment_queue.h>
#include <razorback/queue.h>
#include <razorback/queue_list.h>
#include <razorback/binary_buffer.h>
#include <razorback/block.h>
#include <errno.h>

/** Globals
*/
static struct QueueList sg_qlJudgmentQueue;
static bool sg_bJudgmentInitialized = false;

static void
JudgmentQueue_GetQueueName (uuid_t p_pDispatcherId, uint8_t * p_sQueueName)
{
    Queue_GetQueueName ((const uint8_t *) "/topic/JUDGMENT", p_pDispatcherId,
                        p_sQueueName);
}

SO_PUBLIC struct Queue *
JudgmentQueue_Initialize (uuid_t p_pDispatcherId, int p_iFlags)
{
    // the name
    uint8_t l_sQueueName[128];
    // the queue from the list
    struct Queue *l_pQueue;

    // setup global variables
    if (!sg_bJudgmentInitialized)
    {
        QueueList_Initialize (&sg_qlJudgmentQueue);
        sg_bJudgmentInitialized = true;
    };

    // transform to correct name
    JudgmentQueue_GetQueueName (p_pDispatcherId, l_sQueueName);

    // does this queue already exist?
    // if so, done
    l_pQueue = QueueList_Find (&sg_qlJudgmentQueue, p_pDispatcherId);
    if (l_pQueue != NULL)
        return l_pQueue;

    // initialize the queue
    if ((l_pQueue = Queue_Create (l_sQueueName, p_iFlags)) == NULL)
        return NULL;

    // find the queue
    if (!QueueList_Add (&sg_qlJudgmentQueue, l_pQueue, p_pDispatcherId))
    {
        // TODO: Terminate the queue.
        return NULL;
    }

    return l_pQueue;
}

SO_PUBLIC void
JudgmentQueue_Terminate (uuid_t p_pDispatcherId)
{
    // the queue from the list
    struct Queue *l_pQueue;

    // if never intitizlized, do nothing
    if (!sg_bJudgmentInitialized)
        return;

    // find the queue and terminate it
    l_pQueue = QueueList_Find (&sg_qlJudgmentQueue, p_pDispatcherId);
    if (l_pQueue != NULL)
        Queue_Terminate (l_pQueue);
}

SO_PUBLIC bool
JudgmentQueue_Get (struct Queue *p_pQueue, struct MessageJudgmentSubmission *p_pMessage)
{
    ASSERT (p_pMessage != NULL);
    ASSERT (p_pQueue != NULL);

    // temporary variables
    struct BinaryBuffer *l_pBuffer;


    // check if ready
    if (Socket_ReadyForRead (p_pQueue->pReadSocket))
    {
        errno = EAGAIN;
        return false;
    }

    // read from the queue
    if ((l_pBuffer = Queue_Get (p_pQueue)) == NULL)
    {
        return false;
    }

    // parse the buffer
    if (!BinaryBuffer_Get_MessageHeader (l_pBuffer, &p_pMessage->mhHeader))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        return false;
    }

    if (!BinaryBuffer_Get_uint32_t (l_pBuffer, &p_pMessage->iDisposition))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        return false;
    }

    if (!BinaryBuffer_Get_UUID (l_pBuffer, p_pMessage->uuidInspectorId))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        return false;
    }

    if (!BinaryBuffer_Get_UUID (l_pBuffer, p_pMessage->uuidApplicationId))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        return false;
    }

    if ((p_pMessage->pBlock = Block_Create()) == NULL)
    {
        BinaryBuffer_Destroy (l_pBuffer);
        return false;
    }

    if (!BinaryBuffer_Get_Block (l_pBuffer, p_pMessage->pBlock))
    {
        Block_Destroy (p_pMessage->pBlock);
        BinaryBuffer_Destroy (l_pBuffer);
        return false;
    }

    BinaryBuffer_Destroy (l_pBuffer);

    return true;
}

SO_PUBLIC bool
JudgmentQueue_Put (struct MessageJudgmentSubmission * p_pMessage,
                   uuid_t p_pDispatcherId)
{
    ASSERT (p_pMessage != NULL);

    struct BinaryBuffer *l_pBuffer;
    struct Queue *l_pQueue;

    if ((l_pBuffer =
         BinaryBuffer_Create (p_pMessage->mhHeader.iLength)) == NULL)
        return false;


    if (!BinaryBuffer_Put_MessageHeader (l_pBuffer, &p_pMessage->mhHeader))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        return false;
    }
    if (!BinaryBuffer_Put_uint32_t (l_pBuffer, p_pMessage->iDisposition))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        return false;
    }
    if (!BinaryBuffer_Put_UUID (l_pBuffer, p_pMessage->uuidInspectorId))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        return false;
    }
    if (!BinaryBuffer_Put_UUID (l_pBuffer, p_pMessage->uuidApplicationId))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        return false;
    }
    if (!BinaryBuffer_Put_Block (l_pBuffer, p_pMessage->pBlock))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        return false;
    }

    l_pQueue = QueueList_Find (&sg_qlJudgmentQueue, p_pDispatcherId);
    if (l_pQueue == NULL)
    {
        BinaryBuffer_Destroy (l_pBuffer);
        return false;
    }

    // put in the queue
    if (!Queue_Put (l_pQueue, l_pBuffer))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        return false;
    };

    BinaryBuffer_Destroy (l_pBuffer);

    // done
    return true;
}
