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
                 "%s: failed due to failure of Queue_Initialize", __func__);
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
                 "%s: failed due to failure of Queue_Get", __func__);
        return false;
    }

    // parse the buffer
    if (!BinaryBuffer_Get_MessageHeader
        (l_pBuffer, &p_pMessage->mchHeader.mhHeader))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        rzb_log (LOG_ERR,
                 "%s: failed due to failure of BinaryBuffer_Get_MessageHeader", __func__);
        return false;
    }

    if (!BinaryBuffer_Get_UUID
        (l_pBuffer, p_pMessage->mchHeader.uuidSourceNugget))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        rzb_log (LOG_ERR,
                 "%s: failed due to failure of BinaryBuffer_Get_UUID", __func__);
        return false;
    }

    if (!BinaryBuffer_Get_UUID
        (l_pBuffer, p_pMessage->mchHeader.uuidDestNugget))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        rzb_log (LOG_ERR,
                 "%s: failed due to failure of BinaryBuffer_Get_UUID", __func__);
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
                     "%s: ( HELLO ) failed due to failure of BinaryBuffer_Get_UUID", __func__);
            return false;
        }
        if (!BinaryBuffer_Get_UUID
            (l_pBuffer, p_pMessage->mhHello.uuidApplicationType))
        {
            BinaryBuffer_Destroy (l_pBuffer);
            rzb_log (LOG_ERR,
                     "%s: ( HELLO ) failed due to failure of BinaryBuffer_Get_UUID", __func__);
            return false;
        }
        break;
    case MESSAGE_TYPE_REG_REQ:
        if (!BinaryBuffer_Get_UUID
            (l_pBuffer, p_pMessage->mrrRegReq.uuidNuggetType))
        {
            BinaryBuffer_Destroy (l_pBuffer);
            rzb_log (LOG_ERR,
                     "%s: ( REG_REQ ) failed due to failure of BinaryBuffer_Get_UUID", __func__);
            return false;
        }
        if (!BinaryBuffer_Get_UUID
            (l_pBuffer, p_pMessage->mrrRegReq.uuidApplicationType))
        {
            BinaryBuffer_Destroy (l_pBuffer);
            rzb_log (LOG_ERR,
                     "%s: ( REG_REQ ) failed due to failure of BinaryBuffer_Get_UUID", __func__);
            return false;
        }
        if (!BinaryBuffer_Get_uint32_t
            (l_pBuffer, &p_pMessage->mrrRegReq.iDataTypeCount))
        {
            BinaryBuffer_Destroy (l_pBuffer);
            rzb_log (LOG_ERR,
                     "%s: ( REG_REQ ) failed due to failure of BinaryBuffer_Get_uint32_t", __func__);
            return false;
        }
        if (p_pMessage->mrrRegReq.iDataTypeCount != 0){
			if((p_pMessage->mrrRegReq.pDataTypeList = (uuid_t *) malloc (sizeof (uuid_t) * p_pMessage->mrrRegReq.iDataTypeCount)) == NULL)
        	{
            	BinaryBuffer_Destroy (l_pBuffer);
            	rzb_log (LOG_ERR, "%s: ( REG_REQ ) failed due to lack of memory", __func__);
            	return false;
			}
		
        	for (l_iIndex = 0; l_iIndex < p_pMessage->mrrRegReq.iDataTypeCount; l_iIndex++)
	            if (!BinaryBuffer_Get_UUID (l_pBuffer, p_pMessage->mrrRegReq.pDataTypeList[l_iIndex]))
            	{
                	free (p_pMessage->mrrRegReq.pDataTypeList);
                	BinaryBuffer_Destroy (l_pBuffer);
                	rzb_log (LOG_ERR, "%s: ( REG_REQ ) failed due to failure of BinaryBuffer_Get_UUID", __func__);
	                return false;
				}
        } else 
			p_pMessage->mrrRegReq.pDataTypeList = NULL;
        break;
    case MESSAGE_TYPE_CONFIG_UPDATE:
        if (!BinaryBuffer_Get_uint32_t(l_pBuffer, &p_pMessage->mcuConfigUpdate.ntlvTypesCount))
        {
            rzb_log(LOG_ERR, "%s: (CONFIG_UPDATE) Failed to get NTLV Type Count", __func__);
            return false;
        }
        if (p_pMessage->mcuConfigUpdate.ntlvTypesCount > 0)
        {
            if (!BinaryBuffer_Get_uint32_t(l_pBuffer, &p_pMessage->mcuConfigUpdate.ntlvTypesNamesSize))
            {
                rzb_log(LOG_ERR, "%s: (CONFIG_UPDATE) Failed to get NTLV Type name size", __func__);
                return false;
            }
            p_pMessage->mcuConfigUpdate.ntlvTypesUuids = calloc(p_pMessage->mcuConfigUpdate.ntlvTypesCount, sizeof(uuid_t));
            p_pMessage->mcuConfigUpdate.ntlvTypesNames = calloc(p_pMessage->mcuConfigUpdate.ntlvTypesNamesSize, sizeof(char));
            if ((p_pMessage->mcuConfigUpdate.ntlvTypesUuids == NULL ) ||
                    (p_pMessage->mcuConfigUpdate.ntlvTypesNames == NULL ) )
            {
                rzb_log(LOG_ERR, "%s: (CONFIG_UPDATE) failed to allocate income message structures", __func__);
                return false;
            }
            if (!BinaryBuffer_Get_ByteArray(l_pBuffer, p_pMessage->mcuConfigUpdate.ntlvTypesCount * sizeof(uuid_t), (uint8_t*)p_pMessage->mcuConfigUpdate.ntlvTypesUuids))
            {
                rzb_log(LOG_ERR, "%s: (CONFIG_UPDATE) failed to read Types uuids", __func__);
                return false;
            }

            if (!BinaryBuffer_Get_ByteArray(l_pBuffer, p_pMessage->mcuConfigUpdate.ntlvTypesNamesSize * sizeof(char), (uint8_t*)p_pMessage->mcuConfigUpdate.ntlvTypesNames))
            {
                rzb_log(LOG_ERR, "%s: (CONFIG_UPDATE) failed to read Types names", __func__);
                return false;
            }
        }
        if (!BinaryBuffer_Get_uint32_t(l_pBuffer, &p_pMessage->mcuConfigUpdate.ntlvNamesCount))
        {
            rzb_log(LOG_ERR, "%s: (CONFIG_UPDATE) Failed to put NTLV Name Count", __func__);
            return false;
        }
        if (p_pMessage->mcuConfigUpdate.ntlvNamesCount > 0)
        {
            if (!BinaryBuffer_Get_uint32_t(l_pBuffer, &p_pMessage->mcuConfigUpdate.ntlvNamesNamesSize))
            {
                rzb_log(LOG_ERR, "%s: (CONFIG_UPDATE) Failed to put NTLV Name Names size", __func__);
                return false;
            }
            p_pMessage->mcuConfigUpdate.ntlvNamesUuids = calloc(p_pMessage->mcuConfigUpdate.ntlvNamesCount, sizeof(uuid_t));
            p_pMessage->mcuConfigUpdate.ntlvNamesNames = calloc(p_pMessage->mcuConfigUpdate.ntlvNamesNamesSize, sizeof(char));
            if ((p_pMessage->mcuConfigUpdate.ntlvNamesUuids == NULL ) ||
                    (p_pMessage->mcuConfigUpdate.ntlvNamesNames == NULL ) )
            {
                rzb_log(LOG_ERR, "%s: (CONFIG_UPDATE) failed to allocate income message structures", __func__);
                return false;
            }
            if (!BinaryBuffer_Get_ByteArray(l_pBuffer, p_pMessage->mcuConfigUpdate.ntlvNamesCount * sizeof(uuid_t), (uint8_t*)p_pMessage->mcuConfigUpdate.ntlvNamesUuids))
            {
                rzb_log(LOG_ERR, "%s: (CONFIG_UPDATE) failed to read Name uuids", __func__);
                return false;
            }

            if (!BinaryBuffer_Get_ByteArray(l_pBuffer, p_pMessage->mcuConfigUpdate.ntlvNamesNamesSize * sizeof(char), (uint8_t*)p_pMessage->mcuConfigUpdate.ntlvNamesNames))
            {
                rzb_log(LOG_ERR, "%s: (CONFIG_UPDATE) failed to read Name names", __func__);
                return false;
            }
        }

        // FIX ME
        break;
    case MESSAGE_TYPE_CONFIG_ACK:
        if (!BinaryBuffer_Get_UUID
            (l_pBuffer, p_pMessage->mcaConfigAck.uuidNuggetType))
        {
            BinaryBuffer_Destroy (l_pBuffer);
            rzb_log (LOG_ERR,
                     "%s: ( CONFIG_ACK ) failed due to failure of BinaryBuffer_Get_UUID", __func__);
            return false;
        }
        if (!BinaryBuffer_Get_UUID
            (l_pBuffer, p_pMessage->mcaConfigAck.uuidApplicationType))
        {
            BinaryBuffer_Destroy (l_pBuffer);
            rzb_log (LOG_ERR,
                     "%s: ( CONFIG_ACK ) failed due to failure of BinaryBuffer_Get_UUID", __func__);
            return false;
        }
        break;
    case MESSAGE_TYPE_STATS_RESP:
        // treat NTLV list as uninitialized
        if ((p_pMessage->msrStatsResp.ntlvStatsList= NTLVList_Create())== NULL)
        {
            rzb_log (LOG_ERR,
                     "%s: ( STATS_RESP ) failed due to failure of NTLVList_Create", __func__);
            return false;
        }
        if (!BinaryBuffer_Get_NTLVList
            (l_pBuffer, &p_pMessage->msrStatsResp.ntlvStatsList))
        {
            BinaryBuffer_Destroy (l_pBuffer);
            rzb_log (LOG_ERR,
                     "%s: ( STATS_RESP ) failed due to failure of BinaryBuffer_Get_NTLVList", __func__);
            return false;
        }
        break;
    case MESSAGE_TYPE_TERM:
        if ((l_sTempStringStorage = BinaryBuffer_Get_String (l_pBuffer)) == NULL)
        {
            BinaryBuffer_Destroy (l_pBuffer);
            rzb_log (LOG_ERR,
                     "%s: ( TERM ) failed due to failure of BinaryBuffer_Get_String", __func__);
            return false;
        }
        if ((p_pMessage->mtTerminate.sTerminateReason =
             (uint8_t *) malloc (strlen ((const char *) l_sTempStringStorage)
                                 + 1)) == NULL)
        {
			free (l_sTempStringStorage);
            BinaryBuffer_Destroy (l_pBuffer);
            rzb_log (LOG_ERR,
                     "%s: ( TERM ) failed due to lack of memory", __func__);
            return false;
        }
        strcpy ((char *) p_pMessage->mtTerminate.sTerminateReason,
                (const char *) l_sTempStringStorage);
		free (l_sTempStringStorage);
        break;
    case MESSAGE_TYPE_REG_ERR:
        if ((l_sTempStringStorage = BinaryBuffer_Get_String (l_pBuffer)) == NULL)
        {
            BinaryBuffer_Destroy (l_pBuffer);
            rzb_log (LOG_ERR,
                     "%s: ( TERM ) failed due to failure of BinaryBuffer_Get_String", __func__);
            return false;
        }
        if ((p_pMessage->meError.sMessage = 
             (uint8_t *) malloc (strlen ((const char *) l_sTempStringStorage)
                                 + 1)) == NULL)
        {
			free (l_sTempStringStorage);
            BinaryBuffer_Destroy (l_pBuffer);
            rzb_log (LOG_ERR,
                     "%s: ( TERM ) failed due to lack of memory", __func__);
            return false;
        }
        strcpy ((char *) p_pMessage->meError.sMessage,
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
                 "%s: failed due to failure of BinaryBuffer_Create", __func__);
        return false;
    }


    // parse the buffer
    if (!BinaryBuffer_Put_MessageHeader
        (l_pBuffer, &p_pMessage->mchHeader.mhHeader))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        rzb_log (LOG_ERR,
                 "%s: failed due to failure of BinaryBuffer_Put_MessageHeader", __func__);
        return false;
    }

    if (!BinaryBuffer_Put_UUID
        (l_pBuffer, p_pMessage->mchHeader.uuidSourceNugget))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        rzb_log (LOG_ERR,
                 "%s: failed due to failure of BinaryBuffer_Put_UUID", __func__);
        return false;
    }

    if (!BinaryBuffer_Put_UUID
        (l_pBuffer, p_pMessage->mchHeader.uuidDestNugget))
    {
        BinaryBuffer_Destroy (l_pBuffer);
        rzb_log (LOG_ERR,
                 "%s: failed due to failure of BinaryBuffer_Put_UUID", __func__);
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
                     "%s: ( HELLO ) failed due to failure of BinaryBuffer_Put_UUID", __func__);
            return false;
        }
        if (!BinaryBuffer_Put_UUID
            (l_pBuffer, p_pMessage->mhHello.uuidApplicationType))
        {
            BinaryBuffer_Destroy (l_pBuffer);
            rzb_log (LOG_ERR,
                     "%s: ( HELLO ) failed due to failure of BinaryBuffer_Put_UUID", __func__);
            return false;
        }
        break;
    case MESSAGE_TYPE_REG_REQ:
        if (!BinaryBuffer_Put_UUID
            (l_pBuffer, p_pMessage->mrrRegReq.uuidNuggetType))
        {
            BinaryBuffer_Destroy (l_pBuffer);
            rzb_log (LOG_ERR,
                     "%s: ( REG_REQ ) failed due to failure of BinaryBuffer_Put_UUID", __func__);
            return false;
        }
        if (!BinaryBuffer_Put_UUID
            (l_pBuffer, p_pMessage->mrrRegReq.uuidApplicationType))
        {
            BinaryBuffer_Destroy (l_pBuffer);
            rzb_log (LOG_ERR,
                     "%s: ( REG_REQ ) failed due to failure of BinaryBuffer_Put_UUID", __func__);
            return false;
        }
        if (!BinaryBuffer_Put_uint32_t
            (l_pBuffer, p_pMessage->mrrRegReq.iDataTypeCount))
        {
            BinaryBuffer_Destroy (l_pBuffer);
            rzb_log (LOG_ERR,
                     "%s: ( REG_REQ ) failed due to failure of BinaryBuffer_Put_uint32_t", __func__);
            return false;
        }
        for (l_iIndex = 0; l_iIndex < p_pMessage->mrrRegReq.iDataTypeCount;
             l_iIndex++)
            if (!BinaryBuffer_Put_UUID
                (l_pBuffer, p_pMessage->mrrRegReq.pDataTypeList[l_iIndex]))
            {
                BinaryBuffer_Destroy (l_pBuffer);
                rzb_log (LOG_ERR,
                         "%s: ( REG_REQ ) failed due to failure of BinaryBuffer_Put_UUID", __func__);
                return false;
            }
        break;
    case MESSAGE_TYPE_CONFIG_UPDATE:
        if (!BinaryBuffer_Put_uint32_t(l_pBuffer, p_pMessage->mcuConfigUpdate.ntlvTypesCount))
        {
            rzb_log(LOG_ERR, "%s: (CONFIG_UPDATE) Failed to put NTLV Type Count", __func__);
            return false;
        }
        if (!BinaryBuffer_Put_uint32_t(l_pBuffer, p_pMessage->mcuConfigUpdate.ntlvTypesNamesSize))
        {
            rzb_log(LOG_ERR, "%s: (CONFIG_UPDATE) Failed to put NTLV Type name size", __func__);
            return false;
        }
        if (p_pMessage->mcuConfigUpdate.ntlvTypesCount > 0)
        {
            if (!BinaryBuffer_Put_ByteArray(l_pBuffer,p_pMessage->mcuConfigUpdate.ntlvTypesCount*16,  (uint8_t *)p_pMessage->mcuConfigUpdate.ntlvTypesUuids))
            {
                rzb_log(LOG_ERR, "%s: (CONFIG_UPDATE) Failed to put NTLV Type uuids", __func__);
                return false;
            }

            if (!BinaryBuffer_Put_ByteArray(l_pBuffer,p_pMessage->mcuConfigUpdate.ntlvTypesNamesSize,  (uint8_t *)p_pMessage->mcuConfigUpdate.ntlvTypesNames))
            {
                rzb_log(LOG_ERR, "%s: (CONFIG_UPDATE) Failed to put NTLV Type names", __func__);
                return false;
            }
        }
        if (!BinaryBuffer_Put_uint32_t(l_pBuffer, p_pMessage->mcuConfigUpdate.ntlvNamesCount))
        {
            rzb_log(LOG_ERR, "%s: (CONFIG_UPDATE) Failed to put NTLV Name Count", __func__);
            return false;
        }
        if (!BinaryBuffer_Put_uint32_t(l_pBuffer, p_pMessage->mcuConfigUpdate.ntlvNamesNamesSize))
        {
            rzb_log(LOG_ERR, "%s: (CONFIG_UPDATE) Failed to put NTLV Name name size", __func__);
            return false;
        }
        if (p_pMessage->mcuConfigUpdate.ntlvNamesCount > 0)
        {
            if (!BinaryBuffer_Put_ByteArray(l_pBuffer,p_pMessage->mcuConfigUpdate.ntlvNamesCount*16,  (uint8_t *)p_pMessage->mcuConfigUpdate.ntlvNamesUuids))
            {
                rzb_log(LOG_ERR, "%s: (CONFIG_UPDATE) Failed to put NTLV Name uuids", __func__);
                return false;
            }
            if (!BinaryBuffer_Put_ByteArray(l_pBuffer,p_pMessage->mcuConfigUpdate.ntlvNamesNamesSize,  (uint8_t *)p_pMessage->mcuConfigUpdate.ntlvNamesNames))
            {
                rzb_log(LOG_ERR, "%s: (CONFIG_UPDATE) Failed to put NTLV Name names", __func__);
                return false;
            }
        }

        break;
    case MESSAGE_TYPE_CONFIG_ACK:
        if (!BinaryBuffer_Put_UUID
            (l_pBuffer, p_pMessage->mcaConfigAck.uuidNuggetType))
        {
            BinaryBuffer_Destroy (l_pBuffer);
            rzb_log (LOG_ERR,
                     "%s: ( CONFIG_ACK ) failed due to failure of BinaryBuffer_Put_UUID ( Nug Type )", __func__);
            return false;
        };
        if (!BinaryBuffer_Put_UUID
            (l_pBuffer, p_pMessage->mcaConfigAck.uuidApplicationType))
        {
            BinaryBuffer_Destroy (l_pBuffer);
            rzb_log (LOG_ERR,
                     "%s: ( CONFIG_ACK ) failed due to failure of BinaryBuffer_Put_UUID ( App Type) ", __func__);
            return false;
        };
        break;
    case MESSAGE_TYPE_STATS_RESP:
        if (!BinaryBuffer_Put_NTLVList
            (l_pBuffer, p_pMessage->msrStatsResp.ntlvStatsList))
        {
            BinaryBuffer_Destroy (l_pBuffer);
            rzb_log (LOG_ERR,
                     "%s: ( STATS_RESP ) failed due to failure of BinaryBuffer_Put_NTLVList", __func__);
            return false;
        }
        break;
    case MESSAGE_TYPE_TERM:
        if (!BinaryBuffer_Put_String
            (l_pBuffer, p_pMessage->mtTerminate.sTerminateReason))
        {
            BinaryBuffer_Destroy (l_pBuffer);
            rzb_log (LOG_ERR,
                     "%s: ( TERM ) failed due to failure of BinaryBuffer_Put_String", __func__);
            return false;
        }
        break;
    case MESSAGE_TYPE_REG_ERR:
        if (!BinaryBuffer_Put_String
            (l_pBuffer, p_pMessage->meError.sMessage))
        {
            BinaryBuffer_Destroy (l_pBuffer);
            rzb_log (LOG_ERR,
                     "%s: ( TERM ) failed due to failure of BinaryBuffer_Put_String", __func__);
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
                 "%s: failed due to failure of Queue_Put", __func__);
        return false;
    }

    BinaryBuffer_Destroy (l_pBuffer);

    return true;
}
