#include "config.h"
#include <razorback/api.h>
#include <razorback/binary_buffer.h>
#include <razorback/storage.h>
#include <razorback/block.h>
#include <razorback/debug.h>
#include <razorback/log.h>
#include <razorback/event.h>
#include <razorback/thread.h>

#ifdef HAVE_SYS_ENDIAN_H
#include <sys/endian.h>
#endif

#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>

#include "runtime_config.h"

#include <stdio.h>

/* DEPRECATED
SO_PUBLIC bool
BinaryBuffer_CreateMessage (struct BinaryBuffer *p_pBB,
                            struct MessageHeader *p_pMesgHeader)
{
    ASSERT (p_pBB != NULL);

    if (p_pMesgHeader->iLength > g_iMaxMessageSize)
        return false;

    p_pBB->pBuffer = calloc (p_pMesgHeader->iLength, sizeof (uint8_t));
    if (p_pBB->pBuffer == NULL)
    {
        return false;
    }
	
    // set the length
    p_pBB->iLength = p_pMesgHeader->iLength;

    BinaryBuffer_Put_uint32_t (p_pBB, p_pMesgHeader->iType);
    BinaryBuffer_Put_uint32_t (p_pBB, p_pMesgHeader->iLength);
    BinaryBuffer_Put_uint32_t (p_pBB, p_pMesgHeader->iVersion);

    // done
    return true;
}
*/

SO_PUBLIC struct BinaryBuffer *
BinaryBuffer_Create (uint32_t p_iSize)
{
    struct BinaryBuffer *l_pBuffer;
    if ((l_pBuffer = calloc (1, sizeof (struct BinaryBuffer))) == NULL)
    {
        rzb_perror ("BinaryBuffer_Create: calloc failure: %s");
        return NULL;
    }


    if (p_iSize > (uint32_t) Config_getMaxBlockSize ())
        return NULL;


    // set the flags
    l_pBuffer->iFlags = 0x00000000;

    l_pBuffer->iLength = p_iSize;

    // alocate the buffer
    if ((l_pBuffer->pBuffer = calloc (p_iSize, sizeof (uint8_t))) == NULL)
    {
        rzb_log (LOG_ERR,
                 "%s: failed due to lack of memory", __func__);
        free (l_pBuffer);
        return NULL;
    }

    // set the offset
    l_pBuffer->iOffset = 0;

    // done
    return l_pBuffer;
}

SO_PUBLIC void
BinaryBuffer_Destroy (struct BinaryBuffer *p_pBuffer)
{
    ASSERT (p_pBuffer != NULL);
    ASSERT (p_pBuffer->pBuffer != NULL);

    // free the heap memory
    if (p_pBuffer->iFlags == 0x00000000)
        free (p_pBuffer->pBuffer);
    free (p_pBuffer);
}

SO_PUBLIC bool
BinaryBuffer_Put_uint8_t (struct BinaryBuffer *p_pBuffer, uint8_t p_iValue)
{
    ASSERT (p_pBuffer != NULL);
    ASSERT (p_pBuffer->pBuffer != NULL);

    // check that space exists
    if (p_pBuffer->iOffset + (uint32_t) sizeof (uint8_t) > p_pBuffer->iLength)
    {
        rzb_log (LOG_ERR, "%s: failed due to overrun", __func__);
        return false;
    }

    // place in the buffer in network order
    p_pBuffer->pBuffer[p_pBuffer->iOffset] = p_iValue;

    // update length
    p_pBuffer->iOffset += (uint32_t) sizeof (uint8_t);

    // done
    return true;
}

SO_PUBLIC bool
BinaryBuffer_Put_uint16_t (struct BinaryBuffer * p_pBuffer, uint16_t p_iValue)
{
    ASSERT (p_pBuffer != NULL);
    ASSERT (p_pBuffer->pBuffer != NULL);

    // check that space exists
    if (p_pBuffer->iOffset + (uint32_t) sizeof (uint16_t) >
        p_pBuffer->iLength)
    {
        rzb_log (LOG_ERR, "%s: failed due to overrun", __func__);
        return false;
    }

    // place in the buffer in network order
    *((uint16_t *) & p_pBuffer->pBuffer[p_pBuffer->iOffset]) =
        htobe16 (p_iValue);

    // update length
    p_pBuffer->iOffset += (uint32_t) sizeof (uint16_t);

    // done
    return true;
}

SO_PUBLIC bool
BinaryBuffer_Put_uint32_t (struct BinaryBuffer * p_pBuffer, uint32_t p_iValue)
{
    ASSERT (p_pBuffer != NULL);
    ASSERT (p_pBuffer->pBuffer != NULL);

    // check that space exists
    if (p_pBuffer->iOffset + (uint32_t) sizeof (uint32_t) >
        p_pBuffer->iLength)
    {
        rzb_log (LOG_ERR, "%s: failed due to overrun", __func__);
        return false;
    }
    // place in the buffer in network order
    *((uint32_t *) & p_pBuffer->pBuffer[p_pBuffer->iOffset]) =
        htobe32 (p_iValue);

    // update length
    p_pBuffer->iOffset += (uint32_t) sizeof (uint32_t);

    // done
    return true;
}

SO_PUBLIC bool
BinaryBuffer_Put_uint64_t (struct BinaryBuffer * p_pBuffer, uint64_t p_iValue)
{
    ASSERT (p_pBuffer != NULL);
    ASSERT (p_pBuffer->pBuffer != NULL);

    // check that space exists
    if (p_pBuffer->iOffset + (uint64_t) sizeof (uint64_t) >
        p_pBuffer->iLength)
    {
        rzb_log (LOG_ERR, "%s: failed due to overrun", __func__);
        return false;
    }
    // place in the buffer in network order
    *((uint64_t *) & p_pBuffer->pBuffer[p_pBuffer->iOffset]) =
        htobe64 (p_iValue);

    // update length
    p_pBuffer->iOffset += (uint64_t) sizeof (uint64_t);

    // done
    return true;
}

SO_PUBLIC bool
BinaryBuffer_Put_ByteArray (struct BinaryBuffer * p_pBuffer,
                            uint32_t p_iSize, const uint8_t * p_pByteArray)
{
    ASSERT (p_pBuffer != NULL);
    ASSERT (p_pBuffer->pBuffer != NULL);
    ASSERT (p_pByteArray != NULL);

    // check that space exists
    if (p_pBuffer->iOffset + p_iSize > p_pBuffer->iLength)
    {
        rzb_log (LOG_ERR, "%s: failed due to overrun", __func__);
        return false;
    }
    // place in the buffer in network order
    memcpy (((uint8_t *) & p_pBuffer->pBuffer[p_pBuffer->iOffset]),
            p_pByteArray, p_iSize);

    // update length
    p_pBuffer->iOffset += p_iSize;

    // done
    return true;
}

SO_PUBLIC bool
BinaryBuffer_Put_String (struct BinaryBuffer * p_pBB,
                         const uint8_t * p_sString)
{
    ASSERT (p_pBB != NULL);
    ASSERT (p_pBB->pBuffer != NULL);
    ASSERT (p_sString != NULL);

    uint32_t l_iSize;
    // determine the size
    l_iSize = (uint32_t) (strlen ((char *) p_sString) + 1) * sizeof (uint8_t);

    return BinaryBuffer_Put_ByteArray (p_pBB, l_iSize, p_sString);
}


SO_PUBLIC bool
BinaryBuffer_Get_uint8_t (struct BinaryBuffer * p_pBuffer, uint8_t * p_pValue)
{
    ASSERT (p_pBuffer != NULL);
    ASSERT (p_pBuffer->pBuffer != NULL);
    ASSERT (p_pValue != NULL);

    // check that space exists
    if (p_pBuffer->iOffset + (uint32_t) sizeof (uint8_t) > p_pBuffer->iLength)
    {
        rzb_log (LOG_ERR, "%s: failed due to overrun", __func__);
        return false;
    }

    // read the value
    *p_pValue = p_pBuffer->pBuffer[p_pBuffer->iOffset];

    // adjust the offset
    p_pBuffer->iOffset += (uint32_t) sizeof (uint8_t);

    // done
    return true;
}

SO_PUBLIC bool
BinaryBuffer_Get_uint16_t (struct BinaryBuffer * p_pBuffer,
                           uint16_t * p_pValue)
{
    ASSERT (p_pBuffer != NULL);
    ASSERT (p_pBuffer->pBuffer != NULL);
    ASSERT (p_pValue != NULL);

    // check that space exists
    if (p_pBuffer->iOffset + (uint32_t) sizeof (uint16_t) >
        p_pBuffer->iLength)
    {
        rzb_log (LOG_ERR, "%s: failed due to overrun", __func__);
        return false;
    }
    // read the value
    *p_pValue =
        be16toh (*((uint16_t *) & p_pBuffer->pBuffer[p_pBuffer->iOffset]));

    // adjust the offset
    p_pBuffer->iOffset += (uint32_t) sizeof (uint16_t);

    // done
    return true;
}

SO_PUBLIC bool
BinaryBuffer_Get_uint32_t (struct BinaryBuffer * p_pBuffer,
                           uint32_t * p_pValue)
{
    ASSERT (p_pBuffer != NULL);
    ASSERT (p_pBuffer->pBuffer != NULL);
    ASSERT (p_pValue != NULL);

    // check that space exists
    if (p_pBuffer->iOffset + (uint32_t) sizeof (uint32_t) >
        p_pBuffer->iLength)
    {
        rzb_log (LOG_ERR, "%s: failed due to overrun", __func__);
        return false;
    }
    // read the value
    *p_pValue =
        be32toh (*((uint32_t *) & p_pBuffer->pBuffer[p_pBuffer->iOffset]));

    // adjust the offset
    p_pBuffer->iOffset += (uint32_t) sizeof (uint32_t);

    // done
    return true;
}

SO_PUBLIC bool
BinaryBuffer_Get_uint64_t (struct BinaryBuffer * p_pBuffer,
                           uint64_t * p_pValue)
{
    ASSERT (p_pBuffer != NULL);
    ASSERT (p_pBuffer->pBuffer != NULL);
    ASSERT (p_pValue != NULL);

    // check that space exists
    if (p_pBuffer->iOffset + (uint64_t) sizeof (uint64_t) >
        p_pBuffer->iLength)
    {
        rzb_log (LOG_ERR, "%s: failed due to overrun", __func__);
        return false;
    }
    // read the value
    *p_pValue =
        be64toh (*((uint64_t *) & p_pBuffer->pBuffer[p_pBuffer->iOffset]));

    // adjust the offset
    p_pBuffer->iOffset += (uint64_t) sizeof (uint64_t);

    // done
    return true;
}

SO_PUBLIC uint8_t *
BinaryBuffer_Get_String (struct BinaryBuffer * p_pBuffer)
{
    ASSERT (p_pBuffer != NULL);
    ASSERT (p_pBuffer->pBuffer != NULL);

	uint32_t l_iBytesLeft = p_pBuffer->iLength - p_pBuffer->iOffset;
    uint32_t l_iSize;
	uint8_t *pString;
    
	l_iSize = strnlen ((char *)&p_pBuffer->pBuffer[p_pBuffer->iOffset], 
			           l_iBytesLeft);

	if (l_iSize == 0)
	{
		rzb_log (LOG_ERR,
			     "%s: failed due to empty string", __func__);
		return NULL;
	}

	if (l_iSize == l_iBytesLeft)
	{
        rzb_log (LOG_ERR,
                 "%s: failed due to buffer overrun", __func__);
		return NULL;
	}

	if ((pString = calloc(l_iSize+1, sizeof(uint8_t))) == NULL) 
	{
		rzb_log (LOG_ERR,
				"%s: could not allocate memory", __func__);
        return NULL;
	}

	if (!BinaryBuffer_Get_ByteArray(p_pBuffer, l_iSize+1, pString))
	{
        rzb_log (LOG_ERR,
                "%s: failed due to failed of BinaryBuffer_Get_ByteArray", __func__);
		free(pString);
		return NULL;
	}

    return pString;
}

SO_PUBLIC bool
BinaryBuffer_Get_ByteArray (struct BinaryBuffer * p_pBuffer,
                            uint32_t p_iSize, uint8_t * p_pByteArray)
{
    ASSERT (p_pBuffer != NULL);
    ASSERT (p_pBuffer->pBuffer != NULL);
    ASSERT (p_pByteArray != NULL);

    // check that space exists
    if (p_pBuffer->iOffset + p_iSize > p_pBuffer->iLength)
    {
        rzb_log (LOG_ERR, "%s: failed due to overrun", __func__);
        return false;
    }
    // read the value
    memcpy (p_pByteArray, &p_pBuffer->pBuffer[p_pBuffer->iOffset], p_iSize);

    // adjust the offset
    p_pBuffer->iOffset += p_iSize;

    // done
    return true;
}

SO_PUBLIC bool
BinaryBuffer_Get_MessageHeader (struct BinaryBuffer * p_pBuffer,
                                struct MessageHeader * p_pMesgHeader)
{
    ASSERT (p_pBuffer != NULL);
    ASSERT (p_pBuffer->pBuffer != NULL);
    ASSERT (p_pMesgHeader != NULL);

    if (!BinaryBuffer_Get_uint32_t (p_pBuffer, &p_pMesgHeader->iType))
    {
        rzb_log (LOG_ERR,
                 "%s: failed due to failure of BinaryBuffer_Get_uint32_t ( Type )", __func__);
        return false;
    }
    if (!BinaryBuffer_Get_uint32_t (p_pBuffer, &p_pMesgHeader->iLength))
    {
        rzb_log (LOG_ERR,
                 "%s: failed due to failure of BinaryBuffer_Get_uint32_t ( Length )", __func__);
        return false;
    }
    if (!BinaryBuffer_Get_uint32_t (p_pBuffer, &p_pMesgHeader->iVersion))
    {
        rzb_log (LOG_ERR,
                 "%s: failed due to failure of BinaryBuffer_Get_uint32_t ( Version )", __func__);
        return false;
    }
    return true;
}

SO_PUBLIC bool
BinaryBuffer_Put_MessageHeader (struct BinaryBuffer * p_pBuffer,
                                struct MessageHeader * p_pMesgHeader)
{
    ASSERT (p_pBuffer != NULL);
    ASSERT (p_pBuffer->pBuffer != NULL);
    ASSERT (p_pMesgHeader != NULL);

    if (!BinaryBuffer_Put_uint32_t (p_pBuffer, p_pMesgHeader->iType))
    {
        rzb_log (LOG_ERR,
                 "%s: failed due to failure of BinaryBuffer_Put_uint32_t ( Type )", __func__);
        return false;
    }
    if (!BinaryBuffer_Put_uint32_t (p_pBuffer, p_pMesgHeader->iLength))
    {
        rzb_log (LOG_ERR,
                 "%s: failed due to failure of BinaryBuffer_Put_uint32_t ( Length )", __func__);
        return false;
    }
    if (!BinaryBuffer_Put_uint32_t (p_pBuffer, p_pMesgHeader->iVersion))
    {
        rzb_log (LOG_ERR,
                 "%s: failed due to failure of BinaryBuffer_Put_uint32_t ( Version )", __func__);
        return false;
    }

    return true;
}


SO_PUBLIC bool
BinaryBuffer_Get_UUID (struct BinaryBuffer * p_pBuffer, uuid_t p_uuid)
{
    ASSERT (p_pBuffer != NULL);
    ASSERT (p_pBuffer->pBuffer != NULL);
    ASSERT (p_uuid != NULL);

    uint32_t l_iIndex;

    for (l_iIndex = 0; l_iIndex < 16; l_iIndex++)
    {
        if (!BinaryBuffer_Get_uint8_t (p_pBuffer, &p_uuid[l_iIndex]))
        {
            rzb_log (LOG_ERR,
                     "%s: failed due to failure of BinaryBuffer_Get_uint8_t", __func__);
            return false;
        }
    }
    return true;
}

SO_PUBLIC bool
BinaryBuffer_Put_UUID (struct BinaryBuffer * p_pBuffer, uuid_t p_uuid)
{
    ASSERT (p_pBuffer != NULL);
    ASSERT (p_pBuffer->pBuffer != NULL);
    ASSERT (p_uuid != NULL);

    uint32_t l_iIndex;

    for (l_iIndex = 0; l_iIndex < 16; l_iIndex++)
    {
        if (!BinaryBuffer_Put_uint8_t (p_pBuffer, p_uuid[l_iIndex]))
        {
            rzb_log (LOG_ERR,
                     "%s: failed due to failure of BinaryBuffer_Put_uint8_t", __func__);
            return false;
        }
    }
    return true;
}

static bool
BinaryBuffer_Put_NTLVItem (struct BinaryBuffer *p_pBuffer,
                           struct NTLVItem *p_pItem)
{
    ASSERT (p_pBuffer != NULL);
    ASSERT (p_pItem != NULL);

    if (!BinaryBuffer_Put_UUID (p_pBuffer, p_pItem->uuidName))
    {
        rzb_log (LOG_ERR,
                 "%s: failed due to failure of BinaryBuffer_Put_UUID", __func__);
        return false;
    }
    if (!BinaryBuffer_Put_UUID (p_pBuffer, p_pItem->uuidType))
    {
        rzb_log (LOG_ERR,
                 "%s: failed due to failure of BinaryBuffer_Put_UUID", __func__);
        return false;
    }
    if (!BinaryBuffer_Put_uint32_t (p_pBuffer, p_pItem->iLength))
    {
        rzb_log (LOG_ERR,
                 "%s: failed due to failure of BinaryBuffer_Put_uint32_t", __func__);
        return false;
    }
    if (!BinaryBuffer_Put_ByteArray
        (p_pBuffer, p_pItem->iLength, p_pItem->pData))
    {
        rzb_log (LOG_ERR,
                 "%s: failed due to failure of BinaryBuffer_Put_ByteArray", __func__);
        return false;
    }

    return true;
}

SO_PUBLIC bool
BinaryBuffer_Put_NTLVList (struct BinaryBuffer * p_pBuffer,
                           struct NTLVList * p_pList)
{
    ASSERT (p_pBuffer != NULL);
    ASSERT (p_pList != NULL);

    struct NTLVListItem *l_pEntry;

    if (p_pBuffer->iOffset + NTLVList_Size (p_pList) > p_pBuffer->iLength)
    {
        rzb_log (LOG_ERR,
                 "%s: failed due to lack overrun", __func__);
        return false;
    }

    if (!BinaryBuffer_Put_uint32_t (p_pBuffer, NTLVList_Count (p_pList)))
    {
        rzb_log (LOG_ERR,
                 "%s: failed due failure of BinaryBuffer_Put_uint32_t", __func__);
        return false;
    }

    // put each entry
    for (l_pEntry = p_pList->pHead; l_pEntry != NULL;
         l_pEntry = l_pEntry->pNext)
    {
        if (!BinaryBuffer_Put_NTLVItem (p_pBuffer, l_pEntry->pItem))
        {
            rzb_log (LOG_ERR,
                     "%s: failed due failure of BinaryBuffer_Put_NTLVItem", __func__);
            return false;
        }
    }
    // done
    return true;
}

SO_PUBLIC bool
BinaryBuffer_Get_NTLVList (struct BinaryBuffer * p_pBuffer,
                           struct NTLVList ** p_pList)
{
    ASSERT (p_pBuffer != NULL);
    struct NTLVList * l_pList;
    uint32_t l_iCount;
    uint32_t l_iIndex;
    uuid_t l_uuiNameTemp;
    uuid_t l_uuiTypeTemp;
    uint32_t l_iSizeTemp;
    uint8_t *l_pDataTemp;

    // clear the list
    if ((l_pList = NTLVList_Create()) == NULL)
    {
        *p_pList = NULL;
        return false;
    }
    // get the count
    if (!BinaryBuffer_Get_uint32_t (p_pBuffer, &l_iCount))
    {
        NTLVList_Destroy (l_pList);
        *p_pList = NULL;
        rzb_log (LOG_ERR,
                 "%s: failed due failure of BinaryBuffer_Get_uint32_t", __func__);
        return false;
    }

    // get each entry
    for (l_iIndex = 0; l_iIndex < l_iCount; l_iIndex++)
    {
        // get each field
        if (!BinaryBuffer_Get_UUID (p_pBuffer, l_uuiNameTemp))
        {
            NTLVList_Destroy (l_pList);
            *p_pList = NULL;
            rzb_log (LOG_ERR,
                     "%s: failed due failure of BinaryBuffer_Get_UUID", __func__);
            return false;
        }
        if (!BinaryBuffer_Get_UUID (p_pBuffer, l_uuiTypeTemp))
        {
            NTLVList_Destroy (l_pList);
            *p_pList = NULL;
            rzb_log (LOG_ERR,
                     "%s: failed due failure of BinaryBuffer_Get_UUID", __func__);
            return false;
        }
        if (!BinaryBuffer_Get_uint32_t (p_pBuffer, &l_iSizeTemp))
        {
            NTLVList_Destroy (l_pList);
            *p_pList = NULL;
            rzb_log (LOG_ERR,
                     "%s: failed due failure of BinaryBuffer_Get_uint32_t", __func__);
            return false;
        }
        l_pDataTemp = calloc (l_iSizeTemp, sizeof (uint8_t));
        if (l_pDataTemp == NULL)
        {
            NTLVList_Destroy (l_pList);
            *p_pList = NULL;
            rzb_log (LOG_ERR,
                     "%s: failed due to lack of memory", __func__);
            return false;
        }
        if (!BinaryBuffer_Get_ByteArray (p_pBuffer, l_iSizeTemp, l_pDataTemp))
        {
            free (l_pDataTemp);
            NTLVList_Destroy (l_pList);
            *p_pList = NULL;
            rzb_log (LOG_ERR,
                     "%s: failed due failure of BinaryBuffer_Get_ByteArray", __func__);
            return false;
        }
        if (!NTLVList_Add
            (l_pList, l_uuiNameTemp, l_uuiTypeTemp, l_iSizeTemp, l_pDataTemp))
        {
            free (l_pDataTemp);
            NTLVList_Destroy (l_pList);
            *p_pList = NULL;
            rzb_log (LOG_ERR,
                     "%s: failed due failure of NTLVList_Add", __func__);
            return false;
        }
        free (l_pDataTemp);
    }
    *p_pList = l_pList;
    return true;
}

SO_PUBLIC bool
BinaryBuffer_Put_Hash (struct BinaryBuffer * p_pBuffer,
                       const struct Hash * p_pHash)
{
    ASSERT (p_pBuffer != NULL);
    ASSERT (p_pHash != NULL);
    ASSERT (p_pHash->iFlags & HASH_FLAG_FINAL);
    if (!BinaryBuffer_Put_uint32_t (p_pBuffer, p_pHash->iType))
    {
        rzb_log (LOG_ERR,
                 "%s: failed due failure of BinaryBuffer_Put_uint32_t", __func__);
        return false;
    }
    if (!BinaryBuffer_Put_uint32_t (p_pBuffer, p_pHash->iSize))
    {
        rzb_log (LOG_ERR,
                 "%s: failed due failure of BinaryBuffer_Put_uint32_t", __func__);
        return false;
    }
    if (!BinaryBuffer_Put_ByteArray (p_pBuffer, p_pHash->iSize,
                                     p_pHash->pData))
    {
        rzb_log (LOG_ERR,
                 "%s: failed due failure of BinaryBuffer_Put_ByteArray", __func__);
        return false;
    }

    return true;
}

SO_PUBLIC bool
BinaryBuffer_Get_Hash (struct BinaryBuffer * p_pBuffer, struct Hash ** p_pHash)
{
    ASSERT (p_pBuffer != NULL);
    struct Hash *l_pHash;

    if ((l_pHash = calloc(1, sizeof (struct Hash))) == NULL)
    {
        *p_pHash = NULL;
        return false;
    }

    if (!BinaryBuffer_Get_uint32_t (p_pBuffer, &l_pHash->iType))
    {
        Hash_Destroy(l_pHash);
        *p_pHash = NULL;
        rzb_log (LOG_ERR,
                 "%s: failed due failure of BinaryBuffer_Get_uint32_t", __func__);
        return false;
    }

    if (!BinaryBuffer_Get_uint32_t (p_pBuffer, &l_pHash->iSize))
    {
        Hash_Destroy(l_pHash);
        *p_pHash = NULL;
        rzb_log (LOG_ERR,
                 "%s: failed due failure of BinaryBuffer_Get_uint32_t", __func__);
        return false;
    }
    if ((l_pHash->pData = calloc (l_pHash->iSize, sizeof (uint8_t))) == NULL)
    {
        Hash_Destroy(l_pHash);
        *p_pHash = NULL;
        rzb_log (LOG_ERR, "%s: failed due lack of memory", __func__);
        return false;
    }

    if (!BinaryBuffer_Get_ByteArray
        (p_pBuffer, l_pHash->iSize, l_pHash->pData))
    {
        Hash_Destroy(l_pHash);
        *p_pHash = NULL;
        rzb_log (LOG_ERR,
                 "%s: failed due failure of BinaryBuffer_Get_ByteArray", __func__);
        return false;
    }
    l_pHash->iFlags = HASH_FLAG_FINAL;
    *p_pHash = l_pHash;

    return true;
}



SO_PUBLIC bool
BinaryBuffer_Put_BlockId (struct BinaryBuffer * p_pBuffer,
                          struct BlockId * p_pId)
{
    ASSERT (p_pBuffer != NULL);
    ASSERT (p_pId != NULL);

    if (!BinaryBuffer_Put_UUID (p_pBuffer, p_pId->uuidDataType))
    {
        rzb_log (LOG_ERR,
                 "%s: failed due failure of BinaryBuffer_Put_UUID", __func__);
        return false;
    }
    if (!BinaryBuffer_Put_uint64_t (p_pBuffer, p_pId->iLength))
    {
        rzb_log (LOG_ERR,
                 "%s: failed due failure of BinaryBuffer_Put_uint64_t", __func__);
        return false;
    }
    if (!BinaryBuffer_Put_Hash (p_pBuffer, p_pId->pHash))
    {
        rzb_log (LOG_ERR,
                 "%s: failed due failure of BinaryBuffer_Put_Hash", __func__);
        return false;
    }
    return true;
}

SO_PUBLIC bool
BinaryBuffer_Get_BlockId (struct BinaryBuffer * p_pBuffer,
                          struct BlockId ** p_pId)
{
    ASSERT (p_pBuffer != NULL);
    struct BlockId * l_pId;

    if ((l_pId = calloc(1, sizeof(struct BlockId))) == NULL)
    {
        return false;
    }
    if (!BinaryBuffer_Get_UUID (p_pBuffer, l_pId->uuidDataType))
    {
        rzb_log (LOG_ERR,
                 "%s: failed due failure of BinaryBuffer_Get_UUID", __func__);
        return false;
    }
    if (!BinaryBuffer_Get_uint64_t (p_pBuffer, &l_pId->iLength))
    {
        rzb_log (LOG_ERR,
                 "%s: failed due failure of BinaryBuffer_Get_uint64_t", __func__);
        return false;
    }
    if (!BinaryBuffer_Get_Hash (p_pBuffer, &l_pId->pHash))
    {
        free (l_pId->pHash);
        rzb_log (LOG_ERR,
                 "%s: failed due failure of BinaryBuffer_Get_Hash", __func__);
        return false;
    }
    *p_pId = l_pId;
    return true;
}

SO_PUBLIC bool
BinaryBuffer_Put_Block (struct BinaryBuffer * p_pBuffer,
                        struct Block * p_pBlock)
{
    ASSERT (p_pBuffer != NULL);
    ASSERT (p_pBlock != NULL);
    struct BlockPoolData *l_pData;
    if (!BinaryBuffer_Put_BlockId (p_pBuffer, p_pBlock->pId))
    {
        rzb_log (LOG_ERR,
                 "%s: failed due failure of BinaryBuffer_Put_BlockId", __func__);
        return false;
    }

    if (p_pBlock->pParent == NULL)
    {
        if (!BinaryBuffer_Put_uint8_t (p_pBuffer, 0))
        {
            rzb_log (LOG_ERR,
                     "%s: failed due failure of BinaryBuffer_Put_uint8_t", __func__);
            return false;
        }
    }
    else
    {
        if (!BinaryBuffer_Put_uint8_t (p_pBuffer, 1))
        {
            rzb_log (LOG_ERR,
                     "%s: failed due failure of BinaryBuffer_Put_uint8_t", __func__);
            return false;
        }
        if (!BinaryBuffer_Put_BlockId (p_pBuffer, p_pBlock->pParent))
        {
            rzb_log (LOG_ERR,
                     "%s: failed due failure of BinaryBuffer_Put_BlockId", __func__);
            return false;
        }
    }

    if (!BinaryBuffer_Put_NTLVList (p_pBuffer, p_pBlock->pMetaDataList))
    {
        rzb_log (LOG_ERR,
                 "%s: failed due failure of BinaryBuffer_Put_NTLVList", __func__);
        return false;
    }

    if ( (p_pBlock->pData == NULL) &&
         (p_pBlock->pPoolItem == NULL)) 
    {
        if (!BinaryBuffer_Put_uint8_t (p_pBuffer, 0))
        {
            rzb_log (LOG_ERR,
                     "%s: failed due failure of BinaryBuffer_Put_uint8_t", __func__);
            return false;
        }
    }
    else
    {
        if (!BinaryBuffer_Put_uint8_t (p_pBuffer, 1))
        {
            rzb_log (LOG_ERR,
                     "%s: failed due failure of BinaryBuffer_Put_uint8_t", __func__);
            return false;
        }
        if (p_pBlock->pData != NULL) 
        {
            if (!BinaryBuffer_Put_ByteArray
                (p_pBuffer, p_pBlock->pId->iLength, p_pBlock->pData))
            {
                rzb_log (LOG_ERR,
                         "%s: failed due failure of BinaryBuffer_Put_ByteArray", __func__);
                return false;
            }
        } 
        else 
        {
            l_pData = p_pBlock->pPoolItem->pDataHead;
            while (l_pData != NULL)
            {
                if (!BinaryBuffer_Put_ByteArray
                    (p_pBuffer, l_pData->iLength, l_pData->pData))
                {
                    rzb_log(LOG_ERR, "%s: buf: %d, off: %d, data: %d", __func__, p_pBuffer->iLength, p_pBuffer->iOffset, p_pBlock->pId->iLength);
                    rzb_log (LOG_ERR,
                             "%s: failed due failure of BinaryBuffer_Put_ByteArray", __func__);
                    return false;
                }
                l_pData = l_pData->pNext;

            }
            
        }
    }

    return true;
}


SO_PUBLIC bool
BinaryBuffer_Get_Block (struct BinaryBuffer * p_pBuffer,
                        struct Block ** p_pBlock)
{
    ASSERT (p_pBuffer != NULL);
    uint8_t l_iHas = 0;
    struct Block * l_pBlock;
    if ((l_pBlock = calloc(1, sizeof(struct Block))) == NULL)
    {
        *p_pBlock = NULL;
        return false;
    }

    if (!BinaryBuffer_Get_BlockId (p_pBuffer, &l_pBlock->pId))
    {
        rzb_log (LOG_ERR,
                 "%s: failed due failure of BinaryBuffer_Get_BlockId", __func__);
        Block_Destroy(l_pBlock);
        *p_pBlock = NULL;
        return false;
    }

    if (!BinaryBuffer_Get_uint8_t (p_pBuffer, &l_iHas))
    {
        rzb_log (LOG_ERR,
                 "%s: failed due failure of BinaryBuffer_Get_uint8_t", __func__);
        Block_Destroy(l_pBlock);
        *p_pBlock = NULL;
        return false;
    }

    if (l_iHas == 1)
    {
        if (!BinaryBuffer_Get_BlockId (p_pBuffer, &l_pBlock->pParent))
        {
            rzb_log (LOG_ERR,
                     "%s: failed due failure of BinaryBuffer_Get_BlockId", __func__);
            Block_Destroy(l_pBlock);
            *p_pBlock = NULL;
            return false;
        }
    }
    else
        l_pBlock->pParent = NULL;


    if (!BinaryBuffer_Get_NTLVList (p_pBuffer, &l_pBlock->pMetaDataList))
    {
        rzb_log (LOG_ERR,
                 "%s: failed due failure of BinaryBuffer_Get_NTLVList", __func__);
        Block_Destroy(l_pBlock);
        *p_pBlock = NULL;
        return false;
    }

    if (!BinaryBuffer_Get_uint8_t (p_pBuffer, &l_iHas))
    {
        rzb_log (LOG_ERR,
                 "%s: failed due failure of BinaryBuffer_Get_uint8_t", __func__);
        Block_Destroy(l_pBlock);
        *p_pBlock = NULL;
        return false;
    }
    if (l_iHas == 1)
    {
		//This needs to actively receive....
        if ((l_pBlock->pData = malloc (l_pBlock->pId->iLength)) == NULL)
        {

            rzb_log (LOG_ERR,
                     "BinaryBuffer_Get_Block failed due failure of BinaryBuffer_Get_ByteArray");
            Block_Destroy(l_pBlock);
            *p_pBlock = NULL;
            return false;

        }

        if (!BinaryBuffer_Get_ByteArray
            (p_pBuffer, l_pBlock->pId->iLength, l_pBlock->pData))
        {
            rzb_log (LOG_ERR,
                     "BinaryBuffer_Get_Block failed due failure of BinaryBuffer_Get_ByteArray");
            Block_Destroy(l_pBlock);
            *p_pBlock = NULL;
            return false;
        }

        if ((Thread_GetCurrentContext()->iFlags & CONTEXT_FLAG_DISPATCHER) && l_pBlock->pId->iLength > g_StorageThreshold) {
            if (StoreDataBlock(l_pBlock) == 0) {
                rzb_log (LOG_ERR,
                         "BinaryBuffer_Get_Block failed due to failure of StoreDataAsFile");
                Block_Destroy(l_pBlock);
                *p_pBlock = NULL;
                return false;
            }
            l_pBlock->isStored = 1;
        }	
    }
    else
        l_pBlock->pData = NULL;

    *p_pBlock = l_pBlock;
    return true;
}

SO_PUBLIC bool
BinaryBuffer_Put_EventId (struct BinaryBuffer *p_pBuffer,
                                    struct EventId *p_pEventId)
{
    if (!BinaryBuffer_Put_UUID (p_pBuffer, p_pEventId->uuidNuggetId))
    {
        rzb_log(LOG_ERR, "%s: Failed to put nugget id", __func__);
        return false;
    }

    if (!BinaryBuffer_Put_uint64_t (p_pBuffer, p_pEventId->iSeconds))
    {
        rzb_log(LOG_ERR, "%s: Failed to put seconds", __func__);
        return false;
    }
    if (!BinaryBuffer_Put_uint64_t (p_pBuffer, p_pEventId->iNanoSecs))
    {
        rzb_log(LOG_ERR, "%s: Failed to put nano seconds", __func__);
        return false;
    }

    return true;
}

SO_PUBLIC bool
BinaryBuffer_Get_EventId (struct BinaryBuffer *p_pBuffer,
                                    struct EventId **p_pEventId)
{
    struct EventId *eventId = NULL;
    if ((eventId = EventId_Create()) == NULL)
    {
        rzb_log(LOG_ERR, "%s: Failed to create event", __func__);
        return false;
    }

    if (!BinaryBuffer_Get_UUID (p_pBuffer, eventId->uuidNuggetId))
    {
        rzb_log(LOG_ERR, "%s: Failed to get nugget id", __func__);
        return false;
    }

    if (!BinaryBuffer_Get_uint64_t (p_pBuffer, &eventId->iSeconds))
    {
        rzb_log(LOG_ERR, "%s: Failed to get seconds", __func__);
        return false;
    }
    if (!BinaryBuffer_Get_uint64_t (p_pBuffer, &eventId->iNanoSecs))
    {
        rzb_log(LOG_ERR, "%s: Failed to get nano seconds", __func__);
        return false;
    }
    *p_pEventId = eventId;
    return true;
}

SO_PUBLIC bool 
BinaryBuffer_Put_Event (struct BinaryBuffer *p_pBuffer,
                                    struct Event *p_pEvent)
{
    if (!BinaryBuffer_Put_EventId (p_pBuffer, p_pEvent->pId))
    {
        rzb_log(LOG_ERR, "%s: Failed to put nugget id", __func__);
        return false;
    }

    if (!BinaryBuffer_Put_UUID (p_pBuffer, p_pEvent->uuidApplicationType))
    {
        rzb_log(LOG_ERR, "%s: Failed to put app type", __func__);
        return false;
    }

    if (!BinaryBuffer_Put_NTLVList (p_pBuffer, p_pEvent->pMetaDataList))
    {
        rzb_log(LOG_ERR, "%s: Failed to put metadata list", __func__);
        return false;
    }
    if (!BinaryBuffer_Put_Block (p_pBuffer, p_pEvent->pBlock))
    {
        rzb_log(LOG_ERR, "%s: Failed to put block", __func__);
        return false;
    }
    return true;
}

SO_PUBLIC bool 
BinaryBuffer_Get_Event (struct BinaryBuffer *p_pBuffer,
                                    struct Event **p_pEvent)
{
    struct Event *l_pEvent;
    if ((l_pEvent = calloc(1, sizeof (struct Event))) == NULL)
    {
        *p_pEvent = NULL;
        return false;
    }

    if (!BinaryBuffer_Get_EventId (p_pBuffer, &l_pEvent->pId))
    {
        rzb_log(LOG_ERR, "%s: Failed to get event id", __func__);
        Event_Destroy(l_pEvent);
        *p_pEvent = NULL;
        return false;
    }
    if (!BinaryBuffer_Get_UUID (p_pBuffer, l_pEvent->uuidApplicationType))
    {
        rzb_log(LOG_ERR, "%s: Failed to get app type", __func__);
        Event_Destroy(l_pEvent);
        *p_pEvent = NULL;
        return false;
    }
    if (!BinaryBuffer_Get_NTLVList (p_pBuffer, &l_pEvent->pMetaDataList))
    {
        rzb_log(LOG_ERR, "%s: Failed to get metadata list", __func__);
        Event_Destroy(l_pEvent);
        *p_pEvent = NULL;
        return false;
    }
    if (!BinaryBuffer_Get_Block (p_pBuffer, &l_pEvent->pBlock))
    {
        rzb_log(LOG_ERR, "%s: Failed to get block", __func__);
        Event_Destroy(l_pEvent);
        *p_pEvent = NULL;
        return false;
    }
    *p_pEvent = l_pEvent;
    return true;
}

