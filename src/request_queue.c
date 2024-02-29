#include "config.h"
#include <razorback/debug.h>
#include <razorback/queue.h>
#include <razorback/queue_list.h>
#include <razorback/request_queue.h>
#include <razorback/binary_buffer.h>
#include <razorback/log.h>
#include <errno.h>

static void
RequestQueue_GetQueueName (uint8_t * p_sQueueName)
{
    sprintf ((char *) p_sQueueName, "/queue/REQUEST");
}

SO_PUBLIC struct Queue *
RequestQueue_Initialize (int p_iFlags)
{
    // the name
    uint8_t l_sQueueName[128];
    // transform to correct name
    RequestQueue_GetQueueName (l_sQueueName);
    // initialize the queue
    return Queue_Create(l_sQueueName, p_iFlags);
}

SO_PUBLIC void
RequestQueue_Terminate (struct Queue * queue)
{
    Queue_Terminate (queue);
}

SO_PUBLIC struct MessageCacheReq *
RequestQueue_Get (struct Queue *queue )
{
    struct MessageCacheReq *message;
    struct BinaryBuffer *l_pBuffer;


    // read from the queue
    if ((l_pBuffer = Queue_Get (queue)) == NULL)
    {
        if (errno != EINTR)
            rzb_log (LOG_ERR, "%s: failed due to failure of Queue_Get", __func__);

        return NULL;
    }
    if ((message = calloc(1,sizeof(struct MessageCacheReq))) == NULL)
        return NULL;

    // parse the data
    if (!BinaryBuffer_Get_MessageHeader (l_pBuffer, &message->mhHeader))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        rzb_log (LOG_ERR,
                 "%s: failed due to failure of BinaryBuffer_Get_MessageHeader", __func__);
        MessageCacheReq_Destroy(message);
        return NULL;
    }
    if (!BinaryBuffer_Get_UUID (l_pBuffer, message->uuidRequestor))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        rzb_log (LOG_ERR,
                 "%s: failed due to failure of BinaryBuffer_Get_UUID", __func__);
        MessageCacheReq_Destroy(message);
        return NULL;
    }
    if (!BinaryBuffer_Get_BlockId (l_pBuffer, &message->pId))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        rzb_log (LOG_ERR,
                 "%s: failed due to failure of BinaryBuffer_GetBlockId", __func__);
        MessageCacheReq_Destroy(message);
        return NULL;
    }

    BinaryBuffer_Destroy (l_pBuffer);

    // done
    return message;
}

SO_PUBLIC bool
RequestQueue_Put (struct Queue *queue, struct MessageCacheReq * p_pMessage)
{
    ASSERT (p_pMessage != NULL);

    // temporary variables
    struct BinaryBuffer *l_pBuffer;

    // create the buffer
    if ((l_pBuffer =
         BinaryBuffer_Create (p_pMessage->mhHeader.iLength)) == NULL)
    {
        rzb_log (LOG_ERR,
                 "%s: failed due to failure of RequestQueue_Put", __func__);
        return false;
    }

    if (!BinaryBuffer_Put_MessageHeader (l_pBuffer, &p_pMessage->mhHeader))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        rzb_log (LOG_ERR,
                 "%s: failed due to failure of BinaryBuffer_Put_MessageHeader", __func__);
        return false;
    }

    if (!BinaryBuffer_Put_UUID (l_pBuffer, p_pMessage->uuidRequestor))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        rzb_log (LOG_ERR,
                 "%s: failed due to failure of BinaryBuffer_Put_UUID", __func__);
        return false;
    }

    if (!BinaryBuffer_Put_BlockId (l_pBuffer, p_pMessage->pId))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        rzb_log (LOG_ERR,
                 "%s: failed due to failure of BinaryBuffer_Put_BlockId", __func__);
        return false;
    }

    // put in the queue
    if (!Queue_Put (queue, l_pBuffer))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        rzb_log (LOG_ERR,
                 "%s: failed due to failure of Queue_Put", __func__);
        return false;
    }

    BinaryBuffer_Destroy (l_pBuffer);

    // done
    return true;
}
