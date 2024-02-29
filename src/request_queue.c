#include "config.h"
#include <razorback/debug.h>
#include <razorback/queue.h>
#include <razorback/queue_list.h>
#include <razorback/request_queue.h>
#include <razorback/binary_buffer.h>
#include <razorback/log.h>
#include <errno.h>

/** Globals
*/
static struct QueueList sg_qlRequestQueue;
static bool sg_bRequestInitialized = false;

static void
RequestQueue_GetQueueName (uuid_t p_pDispatcherId, uint8_t * p_sQueueName)
{
    Queue_GetQueueName ((const uint8_t *) "/topic/REQUEST", p_pDispatcherId,
                        p_sQueueName);
}

SO_PUBLIC struct Queue *
RequestQueue_Initialize (uuid_t p_pDispatcherId, int p_iFlags)
{
    // the name
    uint8_t l_sQueueName[128];
    // the queue from the list
    struct Queue *l_pQueue;

    // setup the global variables
    if (!sg_bRequestInitialized)
    {
        QueueList_Initialize (&sg_qlRequestQueue);
        sg_bRequestInitialized = true;
    };

    // transform to correct name
    RequestQueue_GetQueueName (p_pDispatcherId, l_sQueueName);

    // does this queue already exist?
    // if so, done
    l_pQueue = QueueList_Find (&sg_qlRequestQueue, p_pDispatcherId);
    if (l_pQueue != NULL)
        return l_pQueue;

    // initialize the queue
    if ((l_pQueue = Queue_Create (l_sQueueName, p_iFlags)) == NULL)
    {
        rzb_log (LOG_ERR,
                 "RequestQueue_Initialize failed due to failure of Queue_Intialize");
        return NULL;
    }
    // find the queue
    if (!QueueList_Add (&sg_qlRequestQueue, l_pQueue, p_pDispatcherId))
    {
        // TODO: Terminate Queue.
        rzb_log (LOG_ERR,
                 "RequestQueue_Initialize failed due to failure of QueueList_Add");
        return NULL;
    }
    return l_pQueue;
}

SO_PUBLIC void
RequestQueue_Terminate (uuid_t p_pDispatcherId)
{
    struct Queue * l_pQueue;
    // if we never initialized, do nothing
    if (!sg_bRequestInitialized)
        return;


    // find the queue and terminate it
    l_pQueue = QueueList_Find (&sg_qlRequestQueue, p_pDispatcherId);
    if (l_pQueue != NULL)
        Queue_Terminate (l_pQueue);
}

SO_PUBLIC bool
RequestQueue_Get (struct Queue * p_pQueue, struct MessageCacheReq *p_pMessage)
{
    ASSERT (p_pMessage != NULL);
    ASSERT (p_pQueue != NULL);

    struct BinaryBuffer *l_pBuffer;

    // check if ready
    if (!Socket_ReadyForRead (p_pQueue->pReadSocket))
    {
        errno = EAGAIN;
        return false;
    }

    // read from the queue
    if ((l_pBuffer = Queue_Get (p_pQueue)) == NULL)
    {
        rzb_log (LOG_ERR,
                 "RequestQueue_Get failed due to failure of Queue_Get");
        return false;
    }
    // parse the data
    if (!BinaryBuffer_Get_MessageHeader (l_pBuffer, &p_pMessage->mhHeader))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        rzb_log (LOG_ERR,
                 "RequestQueue_Get failed due to failure of BinaryBuffer_Get_MessageHeader");
        return false;
    }
    if (!BinaryBuffer_Get_UUID (l_pBuffer, p_pMessage->uuidRequestor))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        rzb_log (LOG_ERR,
                 "RequestQueue_Get failed due to failure of BinaryBuffer_Get_UUID");
        return false;
    }
    if (!BinaryBuffer_Get_BlockId (l_pBuffer, &p_pMessage->bidBlock))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        rzb_log (LOG_ERR,
                 "RequestQueue_Get failed due to failure of BinaryBuffer_GetBlockId");
        return false;
    }

    BinaryBuffer_Destroy (l_pBuffer);

    // done
    return true;
}

SO_PUBLIC bool
RequestQueue_Put (struct MessageCacheReq * p_pMessage, uuid_t p_pDispatcherId)
{
    ASSERT (p_pMessage != NULL);

    // temporary variables
    struct BinaryBuffer *l_pBuffer;
    struct Queue *l_pQueue;

    // create the buffer
    if ((l_pBuffer =
         BinaryBuffer_Create (p_pMessage->mhHeader.iLength)) == NULL)
    {
        rzb_log (LOG_ERR,
                 "RequestQueue_Put failed due to failure of RequestQueue_Put");
        return false;
    }

    if (!BinaryBuffer_Put_MessageHeader (l_pBuffer, &p_pMessage->mhHeader))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        rzb_log (LOG_ERR,
                 "RequestQueue_Put failed due to failure of BinaryBuffer_Put_MessageHeader");
        return false;
    }

    if (!BinaryBuffer_Put_UUID (l_pBuffer, p_pMessage->uuidRequestor))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        rzb_log (LOG_ERR,
                 "RequestQueue_Put failed due to failure of BinaryBuffer_Put_UUID");
        return false;
    }

    if (!BinaryBuffer_Put_BlockId (l_pBuffer, &p_pMessage->bidBlock))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        rzb_log (LOG_ERR,
                 "RequestQueue_Put failed due to failure of BinaryBuffer_Put_BlockId");
        return false;
    }

    // find the queue
    l_pQueue = QueueList_Find (&sg_qlRequestQueue, p_pDispatcherId);
    if (l_pQueue == NULL)
    {
        BinaryBuffer_Destroy (l_pBuffer);
        rzb_log (LOG_ERR,
                 "RequestQueue_Put failed due to failure of QueueList_Find");
        return false;
    }

    // put in the queue
    if (!Queue_Put (l_pQueue, l_pBuffer))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        rzb_log (LOG_ERR,
                 "RequestQueue_Put failed due to failure of Queue_Put");
        return false;
    }

    BinaryBuffer_Destroy (l_pBuffer);

    // done
    return true;
}
