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
static struct Queue *sg_pLogQueue;

static void
LogQueue_GetQueueName (uint8_t * p_sQueueName)
{
    sprintf ((char *) p_sQueueName, "/queue/LOG");
}

SO_PUBLIC bool
LogQueue_Initialize (int p_iFlags)
{
    // the name
    uint8_t l_sQueueName[128];

    // transform to correct name
    LogQueue_GetQueueName (l_sQueueName);
    // initialize the queue
    if ((sg_pLogQueue = Queue_Create
            (l_sQueueName, p_iFlags)) == NULL)
        return false;

    return true;
}

SO_PUBLIC void
LogQueue_Terminate (void)
{
    Queue_Terminate (sg_pLogQueue);
}

SO_PUBLIC struct MessageLogSubmission *
LogQueue_Get (void)
{
    struct MessageLogSubmission *message;
    struct BinaryBuffer *l_pBuffer;
    uint8_t l_iHas = 0;

    // read from the queue
    if ((l_pBuffer = Queue_Get (sg_pLogQueue)) == NULL)
    {
        if (errno != EINTR)
            rzb_log (LOG_ERR, "%s: failed due to failure of Queue_Get", __func__);
        return NULL;
    }
    if ((message = calloc(1,sizeof(struct MessageLogSubmission))) == NULL)
        return NULL;

    // parse the data
    if (!BinaryBuffer_Get_MessageHeader (l_pBuffer, &message->mhHeader))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        rzb_log (LOG_ERR,
                 "%s: failed due to failure of BinaryBuffer_Get_MessageHeader", __func__);
        MessageLog_Destroy(message);
        return NULL;
    }
    if (!BinaryBuffer_Get_UUID (l_pBuffer, message->uuidNuggetId))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        rzb_log (LOG_ERR,
                 "%s: failed due to failure of BinaryBuffer_Get_UUID", __func__);
        MessageLog_Destroy(message);
        return NULL;
    }
    if (!BinaryBuffer_Get_uint8_t (l_pBuffer, &message->iPriority))
    {
        rzb_log (LOG_ERR,
                 "%s: failed due failure of BinaryBuffer_Get_uint8_t", __func__);
        BinaryBuffer_Destroy (l_pBuffer);
        MessageLog_Destroy(message);
        return NULL;
    }
    if (!BinaryBuffer_Get_uint8_t (l_pBuffer, &l_iHas))
    {
        rzb_log (LOG_ERR,
                 "%s: failed due failure of BinaryBuffer_Get_uint8_t", __func__);
        BinaryBuffer_Destroy (l_pBuffer);
        MessageLog_Destroy(message);
        return NULL;
    }

    if (l_iHas == 1)
    {
        if (!BinaryBuffer_Get_EventId (l_pBuffer, &message->pEventId))
        {
            BinaryBuffer_Destroy (l_pBuffer);
            rzb_log (LOG_ERR,
                     "%s: failed due to failure of BinaryBuffer_Get_EventId", __func__);
            MessageLog_Destroy(message);
            return NULL;
        }
    }
    else
        message->pEventId = NULL;

    if ((message->sMessage = BinaryBuffer_Get_String(l_pBuffer)) == NULL)
    {
        BinaryBuffer_Destroy (l_pBuffer);
        rzb_log (LOG_ERR,
                 "%s: failed due to failure of BinaryBuffer_Get_String", __func__);
        MessageLog_Destroy(message);
        return NULL;
    }

    BinaryBuffer_Destroy (l_pBuffer);

    // done
    return message;
}

SO_PUBLIC bool
LogQueue_Put (struct MessageLogSubmission * p_pMessage)
{
    ASSERT (p_pMessage != NULL);

    // temporary variables
    struct BinaryBuffer *l_pBuffer;

    // create the buffer
    if ((l_pBuffer =
         BinaryBuffer_Create (p_pMessage->mhHeader.iLength)) == NULL)
    {
        rzb_log (LOG_ERR,
                 "%s: failed due to failure of LogQueue_Put", __func__);
        return false;
    }

    if (!BinaryBuffer_Put_MessageHeader (l_pBuffer, &p_pMessage->mhHeader))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        rzb_log (LOG_ERR,
                 "%s: failed due to failure of BinaryBuffer_Put_MessageHeader", __func__);
        return false;
    }

    if (!BinaryBuffer_Put_UUID (l_pBuffer, p_pMessage->uuidNuggetId))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        rzb_log (LOG_ERR,
                 "%s: failed due to failure of BinaryBuffer_Put_UUID", __func__);
        return false;
    }
    if (!BinaryBuffer_Put_uint8_t (l_pBuffer, p_pMessage->iPriority))
    {
        rzb_log (LOG_ERR,
                 "%s: failed due failure of BinaryBuffer_Put_uint8_t", __func__);
        return false;
    }

    if (p_pMessage->pEventId == NULL)
    {
        if (!BinaryBuffer_Put_uint8_t (l_pBuffer, 0))
        {
            rzb_log (LOG_ERR,
                     "%s: failed due failure of BinaryBuffer_Put_uint8_t", __func__);
            return false;
        }
    }
    else
    {
        if (!BinaryBuffer_Put_uint8_t (l_pBuffer, 1))
        {
            rzb_log (LOG_ERR,
                     "%s: failed due failure of BinaryBuffer_Put_uint8_t", __func__);
            return false;
        }
        if (!BinaryBuffer_Put_EventId (l_pBuffer, p_pMessage->pEventId))
        {
            BinaryBuffer_Destroy (l_pBuffer);
            rzb_log (LOG_ERR,
                     "%s: failed due to failure of BinaryBuffer_Put_BlockId", __func__);
            return false;
        }
    }
    if (!BinaryBuffer_Put_String (l_pBuffer, p_pMessage->sMessage))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        rzb_log (LOG_ERR,
                 "%s: failed due to failure of BinaryBuffer_Put_String", __func__);
        return false;
    }

    // put in the queue
    if (!Queue_Put (sg_pLogQueue, l_pBuffer))
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
