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
static struct Queue *sg_pJudgmentQueue = NULL;

static void
JudgmentQueue_GetQueueName (uint8_t * p_sQueueName)
{
    sprintf ((char *) p_sQueueName, "/queue/JUDGMENT");
}

SO_PUBLIC bool
JudgmentQueue_Initialize (int p_iFlags)
{
    if (sg_pJudgmentQueue != NULL)
        return true;

    // the name
    uint8_t l_sQueueName[128];
    // transform to correct name
    JudgmentQueue_GetQueueName (l_sQueueName);
    // initialize the queue
    if ((sg_pJudgmentQueue = Queue_Create
            (l_sQueueName, p_iFlags)) == NULL)
        return false;

    return true;
}

SO_PUBLIC void
JudgmentQueue_Terminate (void)
{
    Queue_Terminate (sg_pJudgmentQueue);
}

SO_PUBLIC bool
JudgmentQueue_Get (struct MessageJudgmentSubmission *p_pMessage)
{
    ASSERT (p_pMessage != NULL);

    // temporary variables
    struct BinaryBuffer *l_pBuffer;


    // check if ready
    if (!Socket_ReadyForRead (sg_pJudgmentQueue->pReadSocket))
    {
        errno = EAGAIN;
        return false;
    }

    // read from the queue
    if ((l_pBuffer = Queue_Get (sg_pJudgmentQueue)) == NULL)
    {
        return false;
    }

    // parse the buffer
    if (!BinaryBuffer_Get_MessageHeader (l_pBuffer, &p_pMessage->mhHeader))
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

    if (!BinaryBuffer_Get_uint8_t (l_pBuffer, &p_pMessage->iReason))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        return false;
    }

    if (!BinaryBuffer_Get_uint8_t (l_pBuffer, &p_pMessage->iPriority))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        return false;
    }

    if (!BinaryBuffer_Get_uint32_t (l_pBuffer, &p_pMessage->iSfFlags))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        return false;
    }

    if (!BinaryBuffer_Get_uint32_t (l_pBuffer, &p_pMessage->iEntFlags))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        return false;
    }

    if (!BinaryBuffer_Get_Block (l_pBuffer, &p_pMessage->pBlock))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        return false;
    }
    if (!BinaryBuffer_Get_EventId (l_pBuffer, &p_pMessage->pEventId))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        return false;
    }
    if (!BinaryBuffer_Get_NTLVList (l_pBuffer, &p_pMessage->alertMetadata))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        return false;
    }

    BinaryBuffer_Destroy (l_pBuffer);

    return true;
}

SO_PUBLIC bool
JudgmentQueue_Put (struct MessageJudgmentSubmission * p_pMessage)
{
    ASSERT (p_pMessage != NULL);

    struct BinaryBuffer *l_pBuffer;

    if ((l_pBuffer =
         BinaryBuffer_Create (p_pMessage->mhHeader.iLength)) == NULL)
        return false;


    if (!BinaryBuffer_Put_MessageHeader (l_pBuffer, &p_pMessage->mhHeader))
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

    if (!BinaryBuffer_Put_uint8_t (l_pBuffer, p_pMessage->iReason))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        return false;
    }

    if (!BinaryBuffer_Put_uint8_t (l_pBuffer, p_pMessage->iPriority))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        return false;
    }

    if (!BinaryBuffer_Put_uint32_t (l_pBuffer, p_pMessage->iSfFlags))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        return false;
    }

    if (!BinaryBuffer_Put_uint32_t (l_pBuffer, p_pMessage->iEntFlags))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        return false;
    }

    if (!BinaryBuffer_Put_Block (l_pBuffer, p_pMessage->pBlock))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        return false;
    }
    if (!BinaryBuffer_Put_EventId (l_pBuffer, p_pMessage->pEventId))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        return false;
    }
    if (!BinaryBuffer_Put_NTLVList (l_pBuffer, p_pMessage->alertMetadata))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        return false;
    }


    // put in the queue
    if (!Queue_Put (sg_pJudgmentQueue, l_pBuffer))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        return false;
    };

    BinaryBuffer_Destroy (l_pBuffer);

    // done
    return true;
}
