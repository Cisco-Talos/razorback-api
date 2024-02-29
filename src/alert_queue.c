#include "config.h"
#include <razorback/debug.h>
#include <razorback/alert_queue.h>
#include <razorback/queue.h>
#include <razorback/binary_buffer.h>
#include <razorback/block.h>
#include <razorback/log.h>
#include <stdio.h>
#include <errno.h>

static struct Queue *sg_pAlertQueue;


static void
AlertQueue_GetQueueName (uint8_t * p_sQueueName)
{
    sprintf ((char *) p_sQueueName, "/topic/ALERT");
}

SO_PUBLIC bool
AlertQueue_Initialize (int p_iFlags)
{
    // the name
    uint8_t l_sQueueName[128];

    // transform to correct name
    AlertQueue_GetQueueName (l_sQueueName);

    // initialize the queue
    if ((sg_pAlertQueue =
         Queue_Create (l_sQueueName, p_iFlags)) == NULL)
    {
        rzb_log (LOG_ERR,
                 "AlertQueue_Initialize failed due to failure of Queue_Initialize");
        return false;
    }

    // done
    return true;
}

SO_PUBLIC void
AlertQueue_Terminate (void)
{
    // terminate the queue
    Queue_Terminate (sg_pAlertQueue);

    // done
}

SO_PUBLIC bool
AlertQueue_Get (struct MessageAlert *p_pMessage)
{
    ASSERT (p_pMessage != NULL);

    struct BinaryBuffer *l_pBuffer;

    // check if ready
    if (!Socket_ReadyForRead (sg_pAlertQueue->pReadSocket))
    {
        errno = EAGAIN;
        return false;
    }

    // read from the queue

    if ((l_pBuffer = Queue_Get (sg_pAlertQueue)) == NULL)
    {
        rzb_log (LOG_ERR,
                 "AlertQueue_Get failed due to failure of Queue_Get");
        return false;
    }

    // parse the buffer
    if (!BinaryBuffer_Get_MessageHeader (l_pBuffer, &p_pMessage->mhHeader))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        rzb_log (LOG_ERR,
                 "AlertQueue_Get failed due to failure of BinaryBuffer_Get_MessageHeader");
        return false;
    }

    if (!BinaryBuffer_Get_uint32_t (l_pBuffer, &p_pMessage->iDisposition))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        rzb_log (LOG_ERR,
                 "AlertQueue_Get failed due to failure of BinaryBuffer_Get_uint32_t");
        return false;
    }

    if (!BinaryBuffer_Get_UUID (l_pBuffer, p_pMessage->uuidInspectorId))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        rzb_log (LOG_ERR,
                 "AlertQueue_Get failed due to failure of BinaryBuffer_Get_UUID");
        return false;
    }

    if ((p_pMessage->pBlock = Block_Create ()) == NULL )
    {
        BinaryBuffer_Destroy (l_pBuffer);
        rzb_log (LOG_ERR,
                 "AlertQueue_Get failed due to failure of Block_Create");
        return false;
    }

    if (!BinaryBuffer_Get_Block (l_pBuffer, p_pMessage->pBlock))
    {
        Block_Destroy (p_pMessage->pBlock);
        BinaryBuffer_Destroy (l_pBuffer);
        rzb_log (LOG_ERR,
                 "AlertQueue_Get failed due to failure of BinaryBuffer_Get_Block");
        return false;
    }

    // destroy the buffer
    BinaryBuffer_Destroy (l_pBuffer);

    // done
    return true;
}

SO_PUBLIC bool
AlertQueue_Put (struct MessageAlert * p_pMessage)
{
    ASSERT (p_pMessage != NULL);

    struct BinaryBuffer *l_pBuffer;

    // create the buffer
    if ((l_pBuffer =
         BinaryBuffer_Create (p_pMessage->mhHeader.iLength)) == NULL)
    {
        rzb_log (LOG_ERR,
                 "AlertQueue_Put failed due to failure of BinaryBuffer_Create");
        return false;
    }

    // parse the data
    if (!BinaryBuffer_Put_MessageHeader (l_pBuffer, &p_pMessage->mhHeader))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        rzb_log (LOG_ERR,
                 "AlertQueue_Put failed due to failure of BinaryBuffer_Put_MessageHeader");
        return false;
    }

    if (!BinaryBuffer_Put_uint32_t (l_pBuffer, p_pMessage->iDisposition))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        rzb_log (LOG_ERR,
                 "AlertQueue_Put failed due to failure of BinaryBuffer_Put_uint32_t");
        return false;
    }

    if (!BinaryBuffer_Put_UUID (l_pBuffer, p_pMessage->uuidInspectorId))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        rzb_log (LOG_ERR,
                 "AlertQueue_Put failed due to failure of BinaryBuffer_Put_UUID");
        return false;
    }

    if (!BinaryBuffer_Put_Block (l_pBuffer, p_pMessage->pBlock))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        rzb_log (LOG_ERR,
                 "AlertQueue_Put failed due to failure of BinaryBuffer_Put_Block");
        return false;
    }

    // put in the queue
    if (!Queue_Put (sg_pAlertQueue, l_pBuffer))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        rzb_log (LOG_ERR,
                 "AlertQueue_Put failed due to failure of Queue_Put");
        return false;
    }

    BinaryBuffer_Destroy (l_pBuffer);

    // done
    return true;
}

