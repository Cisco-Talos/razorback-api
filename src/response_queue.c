#include "config.h"
#include <razorback/debug.h>
#include <razorback/response_queue.h>
#include <razorback/queue.h>
#include <razorback/queue_list.h>
#include <razorback/block_id.h>
#include <razorback/log.h>
#include <errno.h>

/** Globals
*/
static struct QueueList sg_qlResponseQueue;
static bool sg_bResponseInitialized = false;

static void
ResponseQueue_GetQueueName (uuid_t p_pCollectorId, uint8_t * p_sQueueName)
{
    Queue_GetQueueName ((const uint8_t *) "/topic/RESPONSE", p_pCollectorId,
                        p_sQueueName);
}

SO_PUBLIC struct Queue *
ResponseQueue_Initialize (uuid_t p_pCollectorId, int p_iFlags)
{
    // the name
    uint8_t l_sQueueName[128];
    // the queue from the list
    struct Queue *l_pQueue;

    // setup the global variables
    if (!sg_bResponseInitialized)
    {
        QueueList_Initialize (&sg_qlResponseQueue);
        sg_bResponseInitialized = true;
    };

    // transform to correct name
    ResponseQueue_GetQueueName (p_pCollectorId, l_sQueueName);

    // does this queue already exist?
    // if so, done
    l_pQueue = QueueList_Find (&sg_qlResponseQueue, p_pCollectorId);
    if (l_pQueue != NULL)
        return l_pQueue;

    // initialize the queue
    if ((l_pQueue = Queue_Create (l_sQueueName, p_iFlags)) == NULL)
    {
        rzb_log (LOG_ERR,
                 "ResponseQueue_Initialize failed due to failure of Queue_Initialize");
        return NULL;
    }

    // find the queue
    if (!QueueList_Add (&sg_qlResponseQueue, l_pQueue, p_pCollectorId))
    {
        rzb_log (LOG_ERR,
                 "ResponseQueue_Initialize failed due to failure of QueueList_Add");
        return NULL;
    }


    // done
    return l_pQueue;
}

SO_PUBLIC void
ResponseQueue_Terminate (uuid_t p_pCollectorId)
{
    // the queue from the list
    struct Queue *l_pQueue;

    // if never initialized, do nothing
    if (!sg_bResponseInitialized)
        return;

    // find the queue and terminate it
    l_pQueue = QueueList_Find (&sg_qlResponseQueue, p_pCollectorId);
    if (l_pQueue != NULL)
        Queue_Terminate (l_pQueue);
}

SO_PUBLIC bool
ResponseQueue_Get (struct Queue * p_pQueue, struct MessageCacheResp *p_pMessage)
{
    ASSERT (p_pMessage != NULL);
    ASSERT (p_pQueue != NULL);

    // temporary variables
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
                 "ResponseQueue_Get failed due to failure of Queue_Get");
        return false;
    }

    // parse the buffer
    if (!BinaryBuffer_Get_MessageHeader (l_pBuffer, &p_pMessage->mhHeader))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        rzb_log (LOG_ERR,
                 "ResponseQueue_Get failed due to failure of BinaryBuffer_Get_MessageHeader");
        return false;
    }

    if (!BinaryBuffer_Get_BlockId (l_pBuffer, &p_pMessage->bidBlock))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        rzb_log (LOG_ERR,
                 "ResponseQueue_Get failed due to failure of BinaryBuffer_Get_BlockId");
        return false;
    }

    if (!BinaryBuffer_Get_uint32_t (l_pBuffer, &p_pMessage->iSfFlags))
    {
        BlockId_Destroy (&p_pMessage->bidBlock);
        BinaryBuffer_Destroy (l_pBuffer);
        rzb_log (LOG_ERR,
                 "ResponseQueue_Get failed due to failure of BinaryBuffer_Get_uint32_t");
        return false;
    }
    if (!BinaryBuffer_Get_uint32_t (l_pBuffer, &p_pMessage->iEntFlags))
    {
        BlockId_Destroy (&p_pMessage->bidBlock);
        BinaryBuffer_Destroy (l_pBuffer);
        rzb_log (LOG_ERR,
                 "ResponseQueue_Get failed due to failure of BinaryBuffer_Get_uint32_t");
        return false;
    }

    BinaryBuffer_Destroy (l_pBuffer);

    return true;
}

SO_PUBLIC bool
ResponseQueue_Put (struct MessageCacheResp * p_pMessage, uuid_t p_pCollectorId)
{
    ASSERT (p_pMessage != NULL);

    // temporary variables
    struct BinaryBuffer *l_pBuffer;
    struct Queue *l_pQueue;
    l_pQueue = QueueList_Find (&sg_qlResponseQueue, p_pCollectorId);
    if (l_pQueue == NULL)
        l_pQueue = ResponseQueue_Initialize(p_pCollectorId, QUEUE_FLAG_SEND);

    // create the buffer
    if ((l_pBuffer =
         BinaryBuffer_Create (p_pMessage->mhHeader.iLength)) == NULL)
    {
        rzb_log (LOG_ERR,
                 "ResponseQueue_Put failed due to failure of BinaryBuffer_Create");
        return false;
    }

    if (!BinaryBuffer_Put_MessageHeader (l_pBuffer, &p_pMessage->mhHeader))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        rzb_log (LOG_ERR,
                 "ResponseQueue_Put failed due to failure of BinaryBuffer_Put_MessageHeader");
        return false;
    }
    if (!BinaryBuffer_Put_BlockId (l_pBuffer, &p_pMessage->bidBlock))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        rzb_log (LOG_ERR,
                 "ResponseQueue_Put failed due to failure of BinaryBuffer_Put_BlockId");
        return false;
    }
    if (!BinaryBuffer_Put_uint32_t (l_pBuffer, p_pMessage->iSfFlags))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        rzb_log (LOG_ERR,
                 "ResponseQueue_Put failed due to failure of BinaryBuffer_Put_uint32_t");
        return false;
    }
    if (!BinaryBuffer_Put_uint32_t (l_pBuffer, p_pMessage->iEntFlags))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        rzb_log (LOG_ERR,
                 "ResponseQueue_Put failed due to failure of BinaryBuffer_Put_uint32_t");
        return false;
    }
    // put in the queue
    if (!Queue_Put (l_pQueue, l_pBuffer))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        rzb_log (LOG_ERR,
                 "ResponseQueue_Get failed due to failure of Queue_Put");
        return false;
    }
    BinaryBuffer_Destroy (l_pBuffer);

    // done
    return true;
}
