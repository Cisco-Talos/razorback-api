#include "config.h"
#include <razorback/binary_buffer.h>
#include <razorback/block.h>
#include <razorback/debug.h>
#include <razorback/log.h>

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
                 "BinaryBuffer_Initialize failed due to lack of memory");
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
        rzb_log (LOG_ERR, "BinaryBuffer_Put_uint8_t failed due to overrun");
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
        rzb_log (LOG_ERR, "BinaryBuffer_Put_uint16_t failed due to overrun");
        return false;
    }

    // place in the buffer in network order
    *((uint16_t *) & p_pBuffer->pBuffer[p_pBuffer->iOffset]) =
        htons (p_iValue);

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
        rzb_log (LOG_ERR, "BinaryBuffer_Put_uint32_t failed due to overrun");
        return false;
    }
    // place in the buffer in network order
    *((uint32_t *) & p_pBuffer->pBuffer[p_pBuffer->iOffset]) =
        htonl (p_iValue);

    // update length
    p_pBuffer->iOffset += (uint32_t) sizeof (uint32_t);

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
        rzb_log (LOG_ERR, "BinaryBuffer_Put_ByteArray failed due to overrun");
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
        rzb_log (LOG_ERR, "BinaryBuffer_Get_uint8_t failed due to overrun");
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
        rzb_log (LOG_ERR, "BinaryBuffer_Get_uint16_t failed due to overrun");
        return false;
    }
    // read the value
    *p_pValue =
        ntohs (*((uint16_t *) & p_pBuffer->pBuffer[p_pBuffer->iOffset]));

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
        rzb_log (LOG_ERR, "BinaryBuffer_Get_uint32_t failed due to overrun");
        return false;
    }
    // read the value
    *p_pValue =
        ntohl (*((uint32_t *) & p_pBuffer->pBuffer[p_pBuffer->iOffset]));

    // adjust the offset
    p_pBuffer->iOffset += (uint32_t) sizeof (uint32_t);

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
			     "BinaryBuffer_Get_String failed due to empty string");
		return NULL;
	}

	if (l_iSize == l_iBytesLeft)
	{
        rzb_log (LOG_ERR,
                 "BinaryBuffer_Get_String failed due to buffer overrun");
		return NULL;
	}

	if ((pString = calloc(l_iSize+1, sizeof(uint8_t))) == NULL) 
	{
		rzb_log (LOG_ERR,
				"BinaryBuffer_Get_String could not allocate memory");
        return NULL;
	}

	if (!BinaryBuffer_Get_ByteArray(p_pBuffer, l_iSize, pString))
	{
        rzb_log (LOG_ERR,
                "BinaryBuffer_Get_String failed due to failed of BinaryBuffer_Get_ByteArray");
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
        rzb_log (LOG_ERR, "BinaryBuffer_Get_ByteArray failed due to overrun");
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
                 "BinaryBuffer_Get_MessageHeader failed due to failure of BinaryBuffer_Get_uint32_t ( Type )");
        return false;
    }
    if (!BinaryBuffer_Get_uint32_t (p_pBuffer, &p_pMesgHeader->iLength))
    {
        rzb_log (LOG_ERR,
                 "BinaryBuffer_Get_MessageHeader failed due to failure of BinaryBuffer_Get_uint32_t ( Length )");
        return false;
    }
    if (!BinaryBuffer_Get_uint32_t (p_pBuffer, &p_pMesgHeader->iVersion))
    {
        rzb_log (LOG_ERR,
                 "BinaryBuffer_Get_MessageHeader failed due to failure of BinaryBuffer_Get_uint32_t ( Version )");
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
                 "BinaryBuffer_Put_MessageHeader failed due to failure of BinaryBuffer_Put_uint32_t ( Type )");
        return false;
    }
    if (!BinaryBuffer_Put_uint32_t (p_pBuffer, p_pMesgHeader->iLength))
    {
        rzb_log (LOG_ERR,
                 "BinaryBuffer_Put_MessageHeader failed due to failure of BinaryBuffer_Put_uint32_t ( Length )");
        return false;
    }
    if (!BinaryBuffer_Put_uint32_t (p_pBuffer, p_pMesgHeader->iVersion))
    {
        rzb_log (LOG_ERR,
                 "BinaryBuffer_Put_MessageHeader failed due to failure of BinaryBuffer_Put_uint32_t ( Version )");
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
                     "BinaryBuffer_Get_UUID failed due to failure of BinaryBuffer_Get_uint8_t");
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
                     "BinaryBuffer_Put_UUID failed due to failure of BinaryBuffer_Put_uint8_t");
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
                 "BinaryBuffer_Put_NTLVItem failed due to failure of BinaryBuffer_Put_UUID");
        return false;
    }
    if (!BinaryBuffer_Put_UUID (p_pBuffer, p_pItem->uuidType))
    {
        rzb_log (LOG_ERR,
                 "BinaryBuffer_Put_NTLVItem failed due to failure of BinaryBuffer_Put_UUID");
        return false;
    }
    if (!BinaryBuffer_Put_uint32_t (p_pBuffer, p_pItem->iLength))
    {
        rzb_log (LOG_ERR,
                 "BinaryBuffer_Put_NTLVItem failed due to failure of BinaryBuffer_Put_uint32_t");
        return false;
    }
    if (!BinaryBuffer_Put_ByteArray
        (p_pBuffer, p_pItem->iLength, p_pItem->pData))
    {
        rzb_log (LOG_ERR,
                 "BinaryBuffer_Put_NTLVItem failed due to failure of BinaryBuffer_Put_ByteArray");
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
                 "BinaryBuffer_Put_NTLVList failed due to lack overrun");
        return false;
    }

    if (!BinaryBuffer_Put_uint32_t (p_pBuffer, NTLVList_Count (p_pList)))
    {
        rzb_log (LOG_ERR,
                 "BinaryBuffer_Put_NTLVList failed due failure of BinaryBuffer_Put_uint32_t");
        return false;
    }

    // put each entry
    for (l_pEntry = p_pList->pHead; l_pEntry != NULL;
         l_pEntry = l_pEntry->pNext)
    {
        if (!BinaryBuffer_Put_NTLVItem (p_pBuffer, l_pEntry->pItem))
        {
            rzb_log (LOG_ERR,
                     "BinaryBuffer_Put_NTLVList failed due failure of BinaryBuffer_Put_NTLVItem");
            return false;
        }
    }
    // done
    return true;
}

SO_PUBLIC bool
BinaryBuffer_Get_NTLVList (struct BinaryBuffer * p_pBuffer,
                           struct NTLVList * p_pList)
{
    ASSERT (p_pBuffer != NULL);
    ASSERT (p_pList != NULL);

    uint32_t l_iCount;
    uint32_t l_iIndex;
    uuid_t l_uuiNameTemp;
    uuid_t l_uuiTypeTemp;
    uint32_t l_iSizeTemp;
    uint8_t *l_pDataTemp;

    // clear the list
    NTLVList_Clear (p_pList);

    // get the count
    if (!BinaryBuffer_Get_uint32_t (p_pBuffer, &l_iCount))
    {
        NTLVList_Clear (p_pList);
        rzb_log (LOG_ERR,
                 "BinaryBuffer_Get_NTLVList failed due failure of BinaryBuffer_Get_uint32_t");
        return false;
    }

    // get each entry
    for (l_iIndex = 0; l_iIndex < l_iCount; l_iIndex++)
    {
        // get each field
        if (!BinaryBuffer_Get_UUID (p_pBuffer, l_uuiNameTemp))
        {
            NTLVList_Clear (p_pList);
            rzb_log (LOG_ERR,
                     "BinaryBuffer_Get_NTLVList failed due failure of BinaryBuffer_Get_UUID");
            return false;
        }
        if (!BinaryBuffer_Get_UUID (p_pBuffer, l_uuiTypeTemp))
        {
            NTLVList_Clear (p_pList);
            rzb_log (LOG_ERR,
                     "BinaryBuffer_Get_NTLVList failed due failure of BinaryBuffer_Get_UUID");
            return false;
        }
        if (!BinaryBuffer_Get_uint32_t (p_pBuffer, &l_iSizeTemp))
        {
            NTLVList_Clear (p_pList);
            rzb_log (LOG_ERR,
                     "BinaryBuffer_Get_NTLVList failed due failure of BinaryBuffer_Get_uint32_t");
            return false;
        }
        l_pDataTemp = calloc (l_iSizeTemp, sizeof (uint8_t));
        if (l_pDataTemp == NULL)
        {
            NTLVList_Clear (p_pList);
            rzb_log (LOG_ERR,
                     "BinaryBuffer_Get_NTLVList failed due to lack of memory");
            return false;
        }
        if (!BinaryBuffer_Get_ByteArray (p_pBuffer, l_iSizeTemp, l_pDataTemp))
        {
            free (l_pDataTemp);
            NTLVList_Clear (p_pList);
            rzb_log (LOG_ERR,
                     "BinaryBuffer_Get_NTLVList failed due failure of BinaryBuffer_Get_ByteArray");
            return false;
        }
        if (!NTLVList_Add
            (p_pList, l_uuiNameTemp, l_uuiTypeTemp, l_iSizeTemp, l_pDataTemp))
        {
            free (l_pDataTemp);
            NTLVList_Clear (p_pList);
            rzb_log (LOG_ERR,
                     "BinaryBuffer_Get_NTLVList failed due failure of NTLVList_Add");
            return false;
        }
        free (l_pDataTemp);
    }

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
                 "BinaryBuffer_Put_Hash failed due failure of BinaryBuffer_Put_uint32_t");
        return false;
    }
    if (!BinaryBuffer_Put_uint32_t (p_pBuffer, p_pHash->iSize))
    {
        rzb_log (LOG_ERR,
                 "BinaryBuffer_Put_Hash failed due failure of BinaryBuffer_Put_uint32_t");
        return false;
    }
    if (!BinaryBuffer_Put_ByteArray (p_pBuffer, p_pHash->iSize,
                                     p_pHash->pData))
    {
        rzb_log (LOG_ERR,
                 "BinaryBuffer_Put_Hash failed due failure of BinaryBuffer_Put_ByteArray");
        return false;
    }

    return true;
}

SO_PUBLIC bool
BinaryBuffer_Get_Hash (struct BinaryBuffer * p_pBuffer, struct Hash * p_pHash)
{
    ASSERT (p_pBuffer != NULL);
    ASSERT (p_pBuffer != NULL);

    if (!BinaryBuffer_Get_uint32_t (p_pBuffer, &p_pHash->iType))
    {
        rzb_log (LOG_ERR,
                 "BinaryBuffer_Get_Hash failed due failure of BinaryBuffer_Get_uint32_t");
        return false;
    }

    if (!BinaryBuffer_Get_uint32_t (p_pBuffer, &p_pHash->iSize))
    {
        rzb_log (LOG_ERR,
                 "BinaryBuffer_Get_Hash failed due failure of BinaryBuffer_Get_uint32_t");
        return false;
    }
    if ((p_pHash->pData = calloc (p_pHash->iSize, sizeof (uint8_t))) == NULL)
    {
        rzb_log (LOG_ERR, "BinaryBuffer_Get_Hash failed due lack of memory");
        return false;
    }

    if (!BinaryBuffer_Get_ByteArray
        (p_pBuffer, p_pHash->iSize, p_pHash->pData))
    {
        free (p_pHash->pData);
        rzb_log (LOG_ERR,
                 "BinaryBuffer_Get_Hash failed due failure of BinaryBuffer_Get_ByteArray");
        return false;
    }
    p_pHash->iFlags = HASH_FLAG_FINAL;

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
                 "BinaryBuffer_Put_BlockId failed due failure of BinaryBuffer_Put_UUID");
        return false;
    }
    if (!BinaryBuffer_Put_uint32_t (p_pBuffer, p_pId->iLength))
    {
        rzb_log (LOG_ERR,
                 "BinaryBuffer_Put_BlockId failed due failure of BinaryBuffer_Put_uint32_t");
        return false;
    }
    if (!BinaryBuffer_Put_Hash (p_pBuffer, p_pId->pHash))
    {
        rzb_log (LOG_ERR,
                 "BinaryBuffer_Put_BlockId failed due failure of BinaryBuffer_Put_Hash");
        return false;
    }
    return true;
}

SO_PUBLIC bool
BinaryBuffer_Get_BlockId (struct BinaryBuffer * p_pBuffer,
                          struct BlockId * p_pId)
{
    ASSERT (p_pBuffer != NULL);
    ASSERT (p_pId != NULL);

    if (!BinaryBuffer_Get_UUID (p_pBuffer, p_pId->uuidDataType))
    {
        rzb_log (LOG_ERR,
                 "BinaryBuffer_Get_BlockId failed due failure of BinaryBuffer_Get_UUID");
        return false;
    }
    if (!BinaryBuffer_Get_uint32_t (p_pBuffer, &p_pId->iLength))
    {
        rzb_log (LOG_ERR,
                 "BinaryBuffer_Get_BlockId failed due failure of BinaryBuffer_Get_uint32_t");
        return false;
    }
    if ((p_pId->pHash =
         (struct Hash *) malloc (sizeof (struct Hash))) == NULL)
    {
        rzb_log (LOG_ERR,
                 "BinaryBuffer_Get_BlockId failed due lack of memory");
        return false;
    }
    if (!BinaryBuffer_Get_Hash (p_pBuffer, p_pId->pHash))
    {
        free (p_pId->pHash);
        rzb_log (LOG_ERR,
                 "BinaryBuffer_Get_BlockId failed due failure of BinaryBuffer_Get_Hash");
        return false;
    }
    return true;
}

SO_PUBLIC bool
BinaryBuffer_Put_Block (struct BinaryBuffer * p_pBuffer,
                        struct Block * p_pBlock)
{
    ASSERT (p_pBuffer != NULL);
    ASSERT (p_pBlock != NULL);
    struct BlockPoolData *l_pData;
    if (!BinaryBuffer_Put_BlockId (p_pBuffer, &p_pBlock->bidId))
    {
        rzb_log (LOG_ERR,
                 "BinaryBuffer_Put_Block failed due failure of BinaryBuffer_Put_BlockId");
        return false;
    }

    if (p_pBlock->bidParent == NULL)
    {
        if (!BinaryBuffer_Put_uint8_t (p_pBuffer, 0))
        {
            rzb_log (LOG_ERR,
                     "BinaryBuffer_Put_Block failed due failure of BinaryBuffer_Put_uint8_t");
            return false;
        }
    }
    else
    {
        if (!BinaryBuffer_Put_uint8_t (p_pBuffer, 1))
        {
            rzb_log (LOG_ERR,
                     "BinaryBuffer_Put_Block failed due failure of BinaryBuffer_Put_uint8_t");
            return false;
        }
        if (!BinaryBuffer_Put_BlockId (p_pBuffer, p_pBlock->bidParent))
        {
            rzb_log (LOG_ERR,
                     "BinaryBuffer_Put_Block failed due failure of BinaryBuffer_Put_BlockId");
            return false;
        }
    }

    if (!BinaryBuffer_Put_NTLVList (p_pBuffer, p_pBlock->pMetaDataList))
    {
        rzb_log (LOG_ERR,
                 "BinaryBuffer_Put_Block failed due failure of BinaryBuffer_Put_NTLVList");
        return false;
    }

    if ( (p_pBlock->pData == NULL) &&
         (p_pBlock->pPoolItem == NULL)) 
    {
        if (!BinaryBuffer_Put_uint8_t (p_pBuffer, 0))
        {
            rzb_log (LOG_ERR,
                     "BinaryBuffer_Put_Block failed due failure of BinaryBuffer_Put_uint8_t");
            return false;
        }
    }
    else
    {
        if (!BinaryBuffer_Put_uint8_t (p_pBuffer, 1))
        {
            rzb_log (LOG_ERR,
                     "BinaryBuffer_Put_Block failed due failure of BinaryBuffer_Put_uint8_t");
            return false;
        }
        if (p_pBlock->pData != NULL) 
        {
            if (!BinaryBuffer_Put_ByteArray
                (p_pBuffer, p_pBlock->bidId.iLength, p_pBlock->pData))
            {
                rzb_log (LOG_ERR,
                         "BinaryBuffer_Put_Block failed due failure of BinaryBuffer_Put_ByteArray");
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
                    rzb_log(LOG_ERR, "buf: %d, data: %d", p_pBuffer->iLength, p_pBlock->bidId.iLength);
                    rzb_log (LOG_ERR,
                             "BinaryBuffer_Put_Block failed due failure of BinaryBuffer_Put_ByteArray");
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
                        struct Block * p_pBlock)
{
    ASSERT (p_pBuffer != NULL);
    ASSERT (p_pBlock != NULL);
    uint8_t l_iHas = 0;

    if (!BinaryBuffer_Get_BlockId (p_pBuffer, &p_pBlock->bidId))
    {
        rzb_log (LOG_ERR,
                 "BinaryBuffer_Get_Block failed due failure of BinaryBuffer_Get_BlockId");
        return false;
    }
    if (!BinaryBuffer_Get_uint8_t (p_pBuffer, &l_iHas))
    {
        rzb_log (LOG_ERR,
                 "BinaryBuffer_Get_Block failed due failure of BinaryBuffer_Get_uint8_t");
        return false;
    }
    if (l_iHas == 1)
    {
        if ((p_pBlock->bidParent = malloc (sizeof (struct BlockId))) == NULL)
        {
            rzb_log (LOG_ERR,
                     "BinaryBuffer_Get_Block failed due lack of memory");

            return false;
        }

        if (!BinaryBuffer_Get_BlockId (p_pBuffer, p_pBlock->bidParent))
        {
            rzb_log (LOG_ERR,
                     "BinaryBuffer_Get_Block failed due failure of BinaryBuffer_Get_BlockId");
            return false;
        }
    }
    else
        p_pBlock->bidParent = NULL;


    if (!BinaryBuffer_Get_NTLVList (p_pBuffer, p_pBlock->pMetaDataList))
    {
        rzb_log (LOG_ERR,
                 "BinaryBuffer_Get_Block failed due failure of BinaryBuffer_Get_NTLVList");
        return false;
    }

    if (!BinaryBuffer_Get_uint8_t (p_pBuffer, &l_iHas))
    {
        rzb_log (LOG_ERR,
                 "BinaryBuffer_Get_Block failed due failure of BinaryBuffer_Get_uint8_t");
        return false;
    }
    if (l_iHas == 1)
    {
        p_pBlock->pData = malloc (p_pBlock->bidId.iLength);
        if (p_pBlock->pData == NULL)
        {
            rzb_log (LOG_ERR,
                     "BinaryBuffer_Get_Block failed due to lack of memory");
            Block_Destroy(p_pBlock);
            return false;
        }

        if (!BinaryBuffer_Get_ByteArray
            (p_pBuffer, p_pBlock->bidId.iLength, p_pBlock->pData))
        {
            rzb_log (LOG_ERR,
                     "BinaryBuffer_Get_Block failed due failure of BinaryBuffer_Get_ByteArray");
            Block_Destroy(p_pBlock);
            return false;
        }
    }
    else
        p_pBlock->pData = NULL;

    return true;
}
