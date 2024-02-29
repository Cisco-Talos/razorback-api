#include "config.h"
#include <razorback/debug.h>
#include <razorback/input_queue.h>
#include <razorback/binary_buffer.h>
#include <razorback/queue.h>
#include <razorback/block.h>
#include <stdio.h>
#include <errno.h>
#include "command_and_control.h"
/** Globals
*/
static struct Queue *sg_pInputQueue;

static void
InputQueue_GetQueueName (uint8_t * p_sQueueName)
{
    sprintf ((char *) p_sQueueName, "/queue/INPUT");
}

SO_PUBLIC bool
InputQueue_Initialize (int p_iFlags)
{
    // the name
    uint8_t l_sQueueName[128];

    // transform to correct name
    InputQueue_GetQueueName (l_sQueueName);

    // initialize the queue
    if ((sg_pInputQueue = Queue_Create
         (l_sQueueName, p_iFlags)) == NULL)
        return false;

    // done
    return true;
}

SO_PUBLIC void
InputQueue_Terminate (void)
{
    // terminate the queue
    Queue_Terminate (sg_pInputQueue);

    // done
}

SO_PUBLIC bool
InputQueue_Get (struct MessageBlockSubmission *p_pMessage)
{
    ASSERT (p_pMessage != NULL);
    struct BinaryBuffer *l_pBuffer;

    // check if ready
    if (!Socket_ReadyForRead (sg_pInputQueue->pReadSocket))
    {
        errno = EAGAIN;
        return false;
    }

    // read fromn the queue
    if ((l_pBuffer = Queue_Get (sg_pInputQueue)) == NULL)
        return false;

    // parse the buffer
    if (!BinaryBuffer_Get_MessageHeader (l_pBuffer, &p_pMessage->mhHeader))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        return false;
    }
    if (!BinaryBuffer_Get_UUID (l_pBuffer, p_pMessage->uuidNuggetId))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        return false;
    }
    if (!BinaryBuffer_Get_UUID (l_pBuffer, p_pMessage->uuidApplicationType))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        return false;
    }
    if (!BinaryBuffer_Get_uint64_t (l_pBuffer, &p_pMessage->iSeconds))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        return false;
    }
    if (!BinaryBuffer_Get_uint64_t (l_pBuffer, &p_pMessage->iNanoSecs))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        return false;
    }
    if (!BinaryBuffer_Get_uint32_t (l_pBuffer, &p_pMessage->iReason))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        return false;
    }
    if((p_pMessage->pBlock = Block_Create ()) == NULL )
    {
        BinaryBuffer_Destroy (l_pBuffer);
        return false;
    }
    if (!BinaryBuffer_Get_Block (l_pBuffer, p_pMessage->pBlock))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        Block_Destroy (p_pMessage->pBlock);
        return false;
    }

    BinaryBuffer_Destroy (l_pBuffer);
    // done
    return true;
}

SO_PUBLIC bool
InputQueue_Put (struct MessageBlockSubmission * p_pMessage)
{
    ASSERT (p_pMessage != NULL);
    pthread_mutex_lock(&sg_mPauseLock);
    struct BinaryBuffer *l_pBuffer;

    if ((l_pBuffer =
         BinaryBuffer_Create (p_pMessage->mhHeader.iLength)) == NULL)
    {
        pthread_mutex_unlock(&sg_mPauseLock);
        return false;
    }
    // parse the buffer
    if (!BinaryBuffer_Put_MessageHeader (l_pBuffer, &p_pMessage->mhHeader))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        pthread_mutex_unlock(&sg_mPauseLock);
        return false;
    }

    if (!BinaryBuffer_Put_UUID (l_pBuffer, p_pMessage->uuidNuggetId))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        pthread_mutex_unlock(&sg_mPauseLock);
        return false;
    }

    if (!BinaryBuffer_Put_UUID (l_pBuffer, p_pMessage->uuidApplicationType))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        pthread_mutex_unlock(&sg_mPauseLock);
        return false;
    }

    if (!BinaryBuffer_Put_uint64_t (l_pBuffer, p_pMessage->iSeconds))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        pthread_mutex_unlock(&sg_mPauseLock);
        return false;
    }
    if (!BinaryBuffer_Put_uint64_t (l_pBuffer, p_pMessage->iNanoSecs))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        pthread_mutex_unlock(&sg_mPauseLock);
        return false;
    }
    if (!BinaryBuffer_Put_uint32_t (l_pBuffer, p_pMessage->iReason))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        pthread_mutex_unlock(&sg_mPauseLock);
        return false;
    }

    if (!BinaryBuffer_Put_Block (l_pBuffer, p_pMessage->pBlock))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        pthread_mutex_unlock(&sg_mPauseLock);
        return false;
    }

    // put in the queue
    if (!Queue_Put (sg_pInputQueue, l_pBuffer))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        pthread_mutex_unlock(&sg_mPauseLock);
        return false;
    }

    BinaryBuffer_Destroy (l_pBuffer);
    pthread_mutex_unlock(&sg_mPauseLock);

    // done
    return true;
}
