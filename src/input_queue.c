#include "config.h"
#include <razorback/debug.h>
#include <razorback/input_queue.h>
#include <razorback/binary_buffer.h>
#include <razorback/queue.h>
#include <razorback/block.h>
#include <razorback/event.h>
#include <stdio.h>
#include <errno.h>
#include "command_and_control.h"

static void
InputQueue_GetQueueName (uint8_t * p_sQueueName)
{
    sprintf ((char *) p_sQueueName, "/queue/INPUT");
}

SO_PUBLIC struct Queue *
InputQueue_Initialize (int p_iFlags)
{
    // the name
    uint8_t l_sQueueName[128];

    // transform to correct name
    InputQueue_GetQueueName (l_sQueueName);

    // initialize the queue
    return Queue_Create(l_sQueueName, p_iFlags);
}

SO_PUBLIC void
InputQueue_Terminate (struct Queue *queue)
{
    // terminate the queue
    Queue_Terminate (queue);

    // done
}

SO_PUBLIC struct MessageBlockSubmission *
InputQueue_Get (struct Queue *queue)
{
    struct MessageBlockSubmission *message;
    struct BinaryBuffer *l_pBuffer;

    // read fromn the queue
    if ((l_pBuffer = Queue_Get (queue)) == NULL)
        return NULL;

    if ((message = calloc(1,sizeof(struct MessageBlockSubmission))) == NULL)
        return NULL;

    // parse the buffer
    if (!BinaryBuffer_Get_MessageHeader (l_pBuffer, &message->mhHeader))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        MessageBlockSubmission_Destroy(message);
        return NULL;
    }
    if (!BinaryBuffer_Get_uint32_t (l_pBuffer, &message->iReason))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        MessageBlockSubmission_Destroy(message);
        return NULL;
    }
    if (!BinaryBuffer_Get_Event (l_pBuffer, &message->pEvent))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        MessageBlockSubmission_Destroy(message);
        return NULL;
    }
    BinaryBuffer_Destroy (l_pBuffer);
    // done
    return message;
}

SO_PUBLIC bool
InputQueue_Put (struct Queue *queue, struct MessageBlockSubmission * p_pMessage)
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

    if (!BinaryBuffer_Put_uint32_t (l_pBuffer, p_pMessage->iReason))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        pthread_mutex_unlock(&sg_mPauseLock);
        return false;
    }

    if (!BinaryBuffer_Put_Event (l_pBuffer, p_pMessage->pEvent))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        pthread_mutex_unlock(&sg_mPauseLock);
        return false;
    }

    // put in the queue
    if (!Queue_Put (queue, l_pBuffer))
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
