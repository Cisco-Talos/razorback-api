#include "config.h"
#include <razorback/debug.h>
#include <razorback/command_queue.h>
#include <razorback/queue.h>
#include <razorback/ntlv.h>
#include <razorback/binary_buffer.h>
#include <razorback/log.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>

/** Globals
*/
static struct Queue *sg_pCommandQueue;

static void
CommandQueue_GetQueueName (uint8_t * p_sQueueName)
{
    sprintf ((char *) p_sQueueName, "/topic/COMMAND");
}

SO_PUBLIC bool
CommandQueue_Initialize ()
{
    // the name
    uint8_t l_sQueueName[128];

    // transform to correct name
    CommandQueue_GetQueueName (l_sQueueName);

    // initialize the queue
    if ((sg_pCommandQueue = Queue_Create (l_sQueueName, QUEUE_FLAG_SEND|QUEUE_FLAG_RECV)) == NULL)
    {
        rzb_log (LOG_ERR,
                 "CommandQueue_Initialize failed due to failure of Queue_Initialize");
        return false;
    }

    // done
    return true;
}

SO_PUBLIC void
CommandQueue_Terminate (void)
{

    // terminate the queue
    Queue_Terminate (sg_pCommandQueue);
}

SO_PUBLIC bool
CommandQueue_Get (union CcMessageUnion *p_pMessage)
{
    ASSERT (p_pMessage != NULL);

    struct BinaryBuffer *l_pBuffer;
    uint32_t l_iIndex;
    uint8_t *l_sTempStringStorage;


    // check if ready
    if (!Socket_ReadyForRead (sg_pCommandQueue->pReadSocket))
    {
        errno = EAGAIN;
        return false;
    }

    // read from the queue
    if ((l_pBuffer = Queue_Get (sg_pCommandQueue)) == NULL)
    {
        //BinaryBuffer_Destroy (l_pBuffer);
        rzb_log (LOG_ERR,
                 "CommandQueue_Get failed due to failure of Queue_Get");
        return false;
    }

    // parse the buffer
    if (!BinaryBuffer_Get_MessageHeader
        (l_pBuffer, &p_pMessage->mchHeader.mhHeader))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        rzb_log (LOG_ERR,
                 "CommandQueue_Get failed due to failure of BinaryBuffer_Get_MessageHeader");
        return false;
    }

    if (!BinaryBuffer_Get_UUID
        (l_pBuffer, p_pMessage->mchHeader.uuidSourceNugget))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        rzb_log (LOG_ERR,
                 "CommandQueue_Get failed due to failure of BinaryBuffer_Get_UUID");
        return false;
    }

    if (!BinaryBuffer_Get_UUID
        (l_pBuffer, p_pMessage->mchHeader.uuidDestNugget))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        rzb_log (LOG_ERR,
                 "CommandQueue_Get failed due to failure of BinaryBuffer_Get_UUID");
        return false;
    }

    // now parse message type specific information
    switch (p_pMessage->mchHeader.mhHeader.iType)
    {
    case MESSAGE_TYPE_HELLO:
        if (!BinaryBuffer_Get_UUID
            (l_pBuffer, p_pMessage->mhHello.uuidNuggetType))
        {
            BinaryBuffer_Destroy (l_pBuffer);
            rzb_log (LOG_ERR,
                     "CommandQueue_Get ( HELLO ) failed due to failure of BinaryBuffer_Get_UUID");
            return false;
        }
        if (!BinaryBuffer_Get_UUID
            (l_pBuffer, p_pMessage->mhHello.uuidApplicationType))
        {
            BinaryBuffer_Destroy (l_pBuffer);
            rzb_log (LOG_ERR,
                     "CommandQueue_Get ( HELLO ) failed due to failure of BinaryBuffer_Get_UUID");
            return false;
        }
        break;
    case MESSAGE_TYPE_REG_REQ:
        if (!BinaryBuffer_Get_UUID
            (l_pBuffer, p_pMessage->mrrRegReq.uuidNuggetType))
        {
            BinaryBuffer_Destroy (l_pBuffer);
            rzb_log (LOG_ERR,
                     "CommandQueue_Get ( REG_REQ ) failed due to failure of BinaryBuffer_Get_UUID");
            return false;
        }
        if (!BinaryBuffer_Get_UUID
            (l_pBuffer, p_pMessage->mrrRegReq.uuidApplicationType))
        {
            BinaryBuffer_Destroy (l_pBuffer);
            rzb_log (LOG_ERR,
                     "CommandQueue_Get ( REG_REQ ) failed due to failure of BinaryBuffer_Get_UUID");
            return false;
        }
        if (!BinaryBuffer_Get_uint32_t
            (l_pBuffer, &p_pMessage->mrrRegReq.iDataTypeCount))
        {
            BinaryBuffer_Destroy (l_pBuffer);
            rzb_log (LOG_ERR,
                     "CommandQueue_Get ( REG_REQ ) failed due to failure of BinaryBuffer_Get_uint32_t");
            return false;
        }
        if (p_pMessage->mrrRegReq.iDataTypeCount != 0){
			if((p_pMessage->mrrRegReq.pDataTypeList = (uuid_t *) malloc (sizeof (uuid_t) * p_pMessage->mrrRegReq.iDataTypeCount)) == NULL)
        	{
            	BinaryBuffer_Destroy (l_pBuffer);
            	rzb_log (LOG_ERR, "CommandQueue_Get ( REG_REQ ) failed due to lack of memory");
            	return false;
			}
		
        	for (l_iIndex = 0; l_iIndex < p_pMessage->mrrRegReq.iDataTypeCount; l_iIndex++)
	            if (!BinaryBuffer_Get_UUID (l_pBuffer, p_pMessage->mrrRegReq.pDataTypeList[l_iIndex]))
            	{
                	free (p_pMessage->mrrRegReq.pDataTypeList);
                	BinaryBuffer_Destroy (l_pBuffer);
                	rzb_log (LOG_ERR, "CommandQueue_Get ( REG_REQ ) failed due to failure of BinaryBuffer_Get_UUID");
	                return false;
				}
        } else 
			p_pMessage->mrrRegReq.pDataTypeList = NULL;
        break;
    case MESSAGE_TYPE_CONFIG_UPDATE:
        if ((p_pMessage->mcuConfigUpdate.ntlvConfigurationList = NTLVList_Create () ) == NULL ) 
        {
            rzb_log (LOG_ERR,
                     "CommandQueue_Get ( CONFIG_UPDATE ) failed due to failure of NTLVList_Create");
            return false;
        }
        if (!BinaryBuffer_Get_NTLVList
            (l_pBuffer, p_pMessage->mcuConfigUpdate.ntlvConfigurationList))
        {
            BinaryBuffer_Destroy (l_pBuffer);
            NTLVList_Destroy(p_pMessage->mcuConfigUpdate.ntlvConfigurationList);
            rzb_log (LOG_ERR,
                     "CommandQueue_Get ( CONFIG_UPDATE ) failed due to failure of BinaryBuffer_Get_NTLVList");
            return false;
        }
        break;
    case MESSAGE_TYPE_CONFIG_ACK:
        if (!BinaryBuffer_Get_UUID
            (l_pBuffer, p_pMessage->mcaConfigAck.uuidNuggetType))
        {
            BinaryBuffer_Destroy (l_pBuffer);
            rzb_log (LOG_ERR,
                     "CommandQueue_Get ( CONFIG_ACK ) failed due to failure of BinaryBuffer_Get_UUID");
            return false;
        };
        if (!BinaryBuffer_Get_UUID
            (l_pBuffer, p_pMessage->mcaConfigAck.uuidApplicationType))
        {
            BinaryBuffer_Destroy (l_pBuffer);
            rzb_log (LOG_ERR,
                     "CommandQueue_Get ( CONFIG_ACK ) failed due to failure of BinaryBuffer_Get_UUID");
            return false;
        };
        break;
    case MESSAGE_TYPE_STATS_RESP:
        // treat NTLV list as uninitialized
        if ((p_pMessage->msrStatsResp.ntlvStatsList= NTLVList_Create())== NULL)
        {
            rzb_log (LOG_ERR,
                     "CommandQueue_Get ( STATS_RESP ) failed due to failure of NTLVList_Create");
            return false;
        }
        if (!BinaryBuffer_Get_NTLVList
            (l_pBuffer, p_pMessage->msrStatsResp.ntlvStatsList))
        {
            BinaryBuffer_Destroy (l_pBuffer);
            rzb_log (LOG_ERR,
                     "CommandQueue_Get ( STATS_RESP ) failed due to failure of BinaryBuffer_Get_NTLVList");
            return false;
        }
        break;
    case MESSAGE_TYPE_TERM:
        if ((l_sTempStringStorage = BinaryBuffer_Get_String (l_pBuffer)) == NULL)
        {
            BinaryBuffer_Destroy (l_pBuffer);
            rzb_log (LOG_ERR,
                     "CommandQueue_Get ( TERM ) failed due to failure of BinaryBuffer_Get_String");
            return false;
        }
        if ((p_pMessage->mtTerminate.sTerminateReason =
             (uint8_t *) malloc (strlen ((const char *) l_sTempStringStorage)
                                 + 1)) == NULL)
        {
			free (l_sTempStringStorage);
            BinaryBuffer_Destroy (l_pBuffer);
            rzb_log (LOG_ERR,
                     "CommandQueue_Get ( TERM ) failed due to lack of memory");
            return false;
        }
        strcpy ((char *) p_pMessage->mtTerminate.sTerminateReason,
                (const char *) l_sTempStringStorage);
		free (l_sTempStringStorage);
        break;
    default:
        break;
    }

    BinaryBuffer_Destroy (l_pBuffer);

    return true;
}

SO_PUBLIC bool
CommandQueue_Put (union CcMessageUnion * p_pMessage)
{
    ASSERT (p_pMessage != NULL);

    // temporary variables
    struct BinaryBuffer *l_pBuffer;
    uint32_t l_iIndex;

    // create the buffer
    if ((l_pBuffer =
         BinaryBuffer_Create (p_pMessage->mchHeader.mhHeader.iLength)) ==
        NULL)
    {
        rzb_log (LOG_ERR,
                 "CommandQueue_Put failed due to failure of BinaryBuffer_Create");
        return false;
    }


    // parse the buffer
    if (!BinaryBuffer_Put_MessageHeader
        (l_pBuffer, &p_pMessage->mchHeader.mhHeader))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        rzb_log (LOG_ERR,
                 "CommandQueue_Put failed due to failure of BinaryBuffer_Put_MessageHeader");
        return false;
    }

    if (!BinaryBuffer_Put_UUID
        (l_pBuffer, p_pMessage->mchHeader.uuidSourceNugget))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        rzb_log (LOG_ERR,
                 "CommandQueue_Put failed due to failure of BinaryBuffer_Put_UUID");
        return false;
    }

    if (!BinaryBuffer_Put_UUID
        (l_pBuffer, p_pMessage->mchHeader.uuidDestNugget))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        rzb_log (LOG_ERR,
                 "CommandQueue_Put failed due to failure of BinaryBuffer_Put_UUID");
        return false;
    }

    // now parse message type specific information
    switch (p_pMessage->mchHeader.mhHeader.iType)
    {
    case MESSAGE_TYPE_HELLO:
        if (!BinaryBuffer_Put_UUID
            (l_pBuffer, p_pMessage->mhHello.uuidNuggetType))
        {
            BinaryBuffer_Destroy (l_pBuffer);
            rzb_log (LOG_ERR,
                     "CommandQueue_Put ( HELLO ) failed due to failure of BinaryBuffer_Put_UUID");
            return false;
        }
        if (!BinaryBuffer_Put_UUID
            (l_pBuffer, p_pMessage->mhHello.uuidApplicationType))
        {
            BinaryBuffer_Destroy (l_pBuffer);
            rzb_log (LOG_ERR,
                     "CommandQueue_Put ( HELLO ) failed due to failure of BinaryBuffer_Put_UUID");
            return false;
        }
        break;
    case MESSAGE_TYPE_REG_REQ:
        if (!BinaryBuffer_Put_UUID
            (l_pBuffer, p_pMessage->mrrRegReq.uuidNuggetType))
        {
            BinaryBuffer_Destroy (l_pBuffer);
            rzb_log (LOG_ERR,
                     "CommandQueue_Put ( REG_REQ ) failed due to failure of BinaryBuffer_Put_UUID");
            return false;
        }
        if (!BinaryBuffer_Put_UUID
            (l_pBuffer, p_pMessage->mrrRegReq.uuidApplicationType))
        {
            BinaryBuffer_Destroy (l_pBuffer);
            rzb_log (LOG_ERR,
                     "CommandQueue_Put ( REG_REQ ) failed due to failure of BinaryBuffer_Put_UUID");
            return false;
        }
        if (!BinaryBuffer_Put_uint32_t
            (l_pBuffer, p_pMessage->mrrRegReq.iDataTypeCount))
        {
            BinaryBuffer_Destroy (l_pBuffer);
            rzb_log (LOG_ERR,
                     "CommandQueue_Put ( REG_REQ ) failed due to failure of BinaryBuffer_Put_uint32_t");
            return false;
        }
        for (l_iIndex = 0; l_iIndex < p_pMessage->mrrRegReq.iDataTypeCount;
             l_iIndex++)
            if (!BinaryBuffer_Put_UUID
                (l_pBuffer, p_pMessage->mrrRegReq.pDataTypeList[l_iIndex]))
            {
                BinaryBuffer_Destroy (l_pBuffer);
                rzb_log (LOG_ERR,
                         "CommandQueue_Put ( REG_REQ ) failed due to failure of BinaryBuffer_Put_UUID");
                return false;
            }
        break;
    case MESSAGE_TYPE_CONFIG_UPDATE:
        if (!BinaryBuffer_Put_NTLVList
            (l_pBuffer, p_pMessage->mcuConfigUpdate.ntlvConfigurationList))
        {
            BinaryBuffer_Destroy (l_pBuffer);
            rzb_log (LOG_ERR,
                     "CommandQueue_Put ( CONFIG_UPDATE ) failed due to failure of BinaryBuffer_Put_NTLVList");
            return false;
        }
        break;
    case MESSAGE_TYPE_CONFIG_ACK:
        if (!BinaryBuffer_Put_UUID
            (l_pBuffer, p_pMessage->mcaConfigAck.uuidNuggetType))
        {
            BinaryBuffer_Destroy (l_pBuffer);
            rzb_log (LOG_ERR,
                     "CommandQueue_Put ( CONFIG_ACK ) failed due to failure of BinaryBuffer_Put_UUID ( Nug Type )");
            return false;
        };
        if (!BinaryBuffer_Put_UUID
            (l_pBuffer, p_pMessage->mcaConfigAck.uuidApplicationType))
        {
            BinaryBuffer_Destroy (l_pBuffer);
            rzb_log (LOG_ERR,
                     "CommandQueue_Put ( CONFIG_ACK ) failed due to failure of BinaryBuffer_Put_UUID ( App Type) ");
            return false;
        };
        break;
    case MESSAGE_TYPE_STATS_RESP:
        if (!BinaryBuffer_Put_NTLVList
            (l_pBuffer, p_pMessage->msrStatsResp.ntlvStatsList))
        {
            BinaryBuffer_Destroy (l_pBuffer);
            rzb_log (LOG_ERR,
                     "CommandQueue_Put ( STATS_RESP ) failed due to failure of BinaryBuffer_Put_NTLVList");
            return false;
        }
        break;
    case MESSAGE_TYPE_TERM:
        if (!BinaryBuffer_Put_String
            (l_pBuffer, p_pMessage->mtTerminate.sTerminateReason))
        {
            BinaryBuffer_Destroy (l_pBuffer);
            rzb_log (LOG_ERR,
                     "CommandQueue_Put ( TERM ) failed due to failure of BinaryBuffer_Put_String");
            return false;
        }
        break;
    default:
        break;
    }

    // put in the queue
    if (!Queue_Put (sg_pCommandQueue, l_pBuffer))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        rzb_log (LOG_ERR,
                 "CommandQueue_Put failed due to failure of Queue_Put");
        return false;
    }

    BinaryBuffer_Destroy (l_pBuffer);

    return true;
}
