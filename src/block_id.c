#include "config.h"
#include <razorback/debug.h>
#include <razorback/block_id.h>
#include <razorback/hash.h>
#include <razorback/log.h>

#include <string.h>
#include <stdio.h>

#include "runtime_config.h"

SO_PUBLIC bool
BlockId_IsEqual (const struct BlockId * p_pA, const struct BlockId * p_pB)
{
    ASSERT (p_pA != NULL);
    ASSERT (p_pB != NULL);

    // check if pointers equal
    if (p_pA == p_pB)
        return true;

    bool uuid = (uuid_compare (p_pA->uuidDataType, p_pB->uuidDataType) == 0);
    bool hash = Hash_IsEqual (p_pA->pHash, p_pB->pHash);
    bool length = (p_pA->iLength == p_pB->iLength);
    return (uuid && hash && length);
}

SO_PUBLIC uint32_t
BlockId_MaxCount (void)
{
    // return value
    return Config_getMaxBlockSize ();
}

SO_PUBLIC bool
BlockId_IsValidCount (uint32_t p_iCount)
{
    // return test
    return (p_iCount <= BlockId_MaxCount ());
}

SO_PUBLIC bool
BlockId_IsValidType (const uuid_t p_pType)
{
    ASSERT (p_pType != NULL);


    // return test
    return true;
}

SO_PUBLIC void
BlockId_ToText (const struct BlockId *p_pA, uint8_t * p_sText)
{
    ASSERT (p_pA != NULL);
    ASSERT (p_sText != NULL);

    char l_sUUID[UUID_STRING_LENGTH];
    char *l_sHash;

    // create the text string
    uuid_unparse (p_pA->uuidDataType, l_sUUID);
    l_sHash = Hash_ToText (p_pA->pHash);
    sprintf ((char *) p_sText, "%s-%8.8x-%s", l_sUUID, p_pA->iLength,
             (char *) l_sHash);
    free(l_sHash);
}

SO_PUBLIC uint32_t
BlockId_ToHash (const struct BlockId *p_pA)
{
    ASSERT (p_pA != NULL);

    const uint8_t *l_sT;
    uint32_t l_iI;
    uint32_t l_iHash;

    // create the hash by adding all bytes together
    l_sT = (const uint8_t *) p_pA;
    l_iHash = 0;
    for (l_iI = 0; l_iI < sizeof (struct BlockId); l_iI++)
        l_iHash += l_sT[l_iI];

    // done
    return l_iHash;
}

SO_PUBLIC uint32_t
BlockId_ToHashMod (const struct BlockId * p_pA, uint32_t p_iModulus)
{
    ASSERT (p_pA != NULL);

    const uint8_t *l_sT;
    uint32_t l_iI;
    uint32_t l_iHash;

    // create the hash by adding all bytes together
    l_sT = (const uint8_t *) p_pA;
    l_iHash = 0;
    for (l_iI = 0; l_iI < sizeof (struct BlockId); l_iI++)
        l_iHash += l_sT[l_iI];

    // done
    return (l_iHash % p_iModulus);
}

SO_PUBLIC bool
BlockId_Initialize (struct BlockId * p_pA, const uuid_t p_pType,
                    uint32_t p_iCount, const uint8_t * p_sData)
{
    ASSERT (p_pA != NULL);
    ASSERT (p_sData != NULL);
    ASSERT (p_pType != NULL);
    ASSERT (BlockId_IsValidType (p_pType));
    ASSERT (BlockId_IsValidCount (p_iCount));

    // copy the type
    uuid_copy (p_pA->uuidDataType, p_pType);

    // copy the count
    p_pA->iLength = p_iCount;

    // intialize the sha
    if ((p_pA->pHash = Hash_Create()) == NULL)
    {
        rzb_log (LOG_ERR, "BlockId_Initialize: failed due to lack of memory: Hash_Create");
        return false;
    }
    return true;
}

SO_PUBLIC uint32_t
BlockId_StringLength (struct BlockId *p_pB)
{
    // return the value
    return UUID_STRING_LENGTH + Hash_StringLength (p_pB->pHash) + 9;  // "%s-%8.8x-%s"
}

SO_PUBLIC void
BlockId_Destroy (struct BlockId *p_pBlockId)
{
    Hash_Destroy (p_pBlockId->pHash);
}

SO_PUBLIC bool
BlockId_Copy (struct BlockId *p_pDestination, const struct BlockId *p_pSource)
{
    ASSERT (p_pDestination != NULL);
    ASSERT (p_pSource != NULL);

    if ((p_pDestination->pHash =
         (struct Hash *) malloc (sizeof (struct Hash))) == NULL)
    {
        rzb_log (LOG_ERR, "BlockId_Copy failed due to lack of memory");
        return false;
    };
    if (!Hash_Copy (p_pDestination->pHash, p_pSource->pHash))
    {
        rzb_log (LOG_ERR, "BlockId_Copy failed due to failure of Hash_Copy");
        return false;
    };
    uuid_copy (p_pDestination->uuidDataType, p_pSource->uuidDataType);
    p_pDestination->iLength = p_pSource->iLength;

    // done
    return true;
}

SO_PUBLIC uint32_t
BlockId_BinaryLength (const struct BlockId * p_pBlockId)
{
    ASSERT (p_pBlockId != NULL);

    return Hash_DigestLength (p_pBlockId->pHash) +
        (uint32_t) sizeof (p_pBlockId->uuidDataType) +
        (uint32_t) sizeof (p_pBlockId->iLength);
}
