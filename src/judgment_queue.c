#include "config.h"
#include <razorback/debug.h>
#include <razorback/judgment_queue.h>
#include <razorback/queue.h>
#include <razorback/queue_list.h>
#include <razorback/binary_buffer.h>
#include <razorback/block.h>
#include <errno.h>


static void
JudgmentQueue_GetQueueName (uint8_t * p_sQueueName)
{
    sprintf ((char *) p_sQueueName, "/queue/JUDGMENT");
}

SO_PUBLIC struct Queue *
JudgmentQueue_Initialize (int p_iFlags)
{
    // the name
    uint8_t l_sQueueName[128];
    // transform to correct name
    JudgmentQueue_GetQueueName (l_sQueueName);
    // initialize the queue
    return Queue_Create(l_sQueueName, p_iFlags);
}

SO_PUBLIC void
JudgmentQueue_Terminate (struct Queue *queue)
{
    Queue_Terminate (queue);
}

SO_PUBLIC struct MessageJudgmentSubmission *
JudgmentQueue_Get (struct Queue *queue)
{
    struct MessageJudgmentSubmission *message;
    // temporary variables
    struct BinaryBuffer *l_pBuffer;


    // read from the queue
    if ((l_pBuffer = Queue_Get (queue)) == NULL)
        return NULL;

    if ((message = calloc(1,sizeof(struct MessageJudgmentSubmission))) == NULL)
        return NULL;

    // parse the buffer
    if (!BinaryBuffer_Get_MessageHeader (l_pBuffer, &message->mhHeader))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        MessageJudgmentSubmission_Destroy(message);
        return NULL;
    }

    if (!BinaryBuffer_Get_uint8_t (l_pBuffer, &message->iReason))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        MessageJudgmentSubmission_Destroy(message);
        return NULL;
    }
    if (!BinaryBuffer_Get_Judgment (l_pBuffer, &message->pJudgment))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        MessageJudgmentSubmission_Destroy(message);
        return NULL;
    }

    BinaryBuffer_Destroy (l_pBuffer);

    return message;
}

SO_PUBLIC bool
JudgmentQueue_Put (struct Queue *queue,  struct MessageJudgmentSubmission * p_pMessage)
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

    if (!BinaryBuffer_Put_uint8_t (l_pBuffer, p_pMessage->iReason))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        return false;
    }
    if (!BinaryBuffer_Put_Judgment (l_pBuffer, p_pMessage->pJudgment))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        return false;
    }


    // put in the queue
    if (!Queue_Put (queue, l_pBuffer))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        return false;
    }

    BinaryBuffer_Destroy (l_pBuffer);

    // done
    return true;
}
