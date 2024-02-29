#include "config.h"

#include <razorback/debug.h>
#include <razorback/ntlv.h>
#include <razorback/log.h>
#include <stdlib.h>
#include <string.h>

SO_PUBLIC bool
NTLVList_Add (struct NTLVList *p_pList, uuid_t p_uuidName,
              uuid_t p_uuidType, uint32_t p_iSize, const uint8_t * p_pData)
{
    ASSERT (p_pList != NULL);
    ASSERT (p_pData != NULL);
    ASSERT (p_iSize > 0);

    struct NTLVItem *l_pTempItem;
    struct NTLVListItem *l_pTempListItem;

    // create new entry
    l_pTempItem = (struct NTLVItem *) calloc (1, sizeof (struct NTLVItem));
    l_pTempListItem =
        (struct NTLVListItem *) calloc (1, sizeof (struct NTLVListItem));

    if (l_pTempItem == NULL)
    {
        rzb_log (LOG_ERR, "%s: failed due to out of memory on item", __func__);
        return false;
    }

    if (l_pTempListItem == NULL)
    {
        free (l_pTempItem);
        rzb_log (LOG_ERR,
                 "%s: failed due to out of memory on list item", __func__);
        return false;
    }
    l_pTempListItem->pItem = l_pTempItem;

    uuid_copy (l_pTempItem->uuidName, p_uuidName);
    uuid_copy (l_pTempItem->uuidType, p_uuidType);
    l_pTempItem->iLength = p_iSize;
    l_pTempItem->pData = calloc (p_iSize, sizeof (uint8_t));
    if (l_pTempItem->pData == NULL)
    {
        free (l_pTempItem);
        free (l_pTempListItem);
        rzb_log (LOG_ERR,
                 "%s: failed due to out of memory on item data", __func__);
        return false;
    };
    memcpy (l_pTempItem->pData, p_pData, p_iSize);

    // add to end of list
    if (p_pList->pHead == NULL)
    {
        p_pList->pHead = l_pTempListItem;
        p_pList->pTail = l_pTempListItem;
    }
    else
    {
        p_pList->pTail->pNext = l_pTempListItem;
        p_pList->pTail = l_pTempListItem;
    }
    p_pList->iCount++;
    return true;
}

SO_PUBLIC void
NTLVList_Clear (struct NTLVList *p_pList)
{
    ASSERT (p_pList != NULL);

    struct NTLVListItem *l_pTemp;

    // traverse list, removing each element
    while (p_pList->pHead != NULL)
    {
        free (p_pList->pHead->pItem->pData);
        free (p_pList->pHead->pItem);
        l_pTemp = p_pList->pHead->pNext;
        free (p_pList->pHead);
        p_pList->pHead = l_pTemp;
    }
    p_pList->iCount = 0;
}

SO_PUBLIC void
NTLVList_Destroy (struct NTLVList *p_pList)
{
    ASSERT (p_pList != NULL);
    NTLVList_Clear(p_pList);
    free(p_pList);
}

SO_PUBLIC uint32_t
NTLVList_Count (const struct NTLVList *p_pList)
{
    ASSERT (p_pList != NULL);
    uint32_t count = p_pList->iCount;
    return count;
}

SO_PUBLIC uint32_t
NTLVList_Size (const struct NTLVList * p_pList)
{
    ASSERT (p_pList != NULL);

    struct NTLVListItem *l_pItem;
    uint32_t l_iSize;

    l_iSize = 0;
    l_pItem = p_pList->pHead;
    while (l_pItem != NULL)
    {
        l_iSize += l_pItem->pItem->iLength;
        l_iSize += 16 * 2;      // 2 UUID's
        l_iSize += sizeof (uint32_t);   // Size field;
        l_pItem = l_pItem->pNext;
    }

    return l_iSize + sizeof (uint32_t);
}

SO_PUBLIC struct NTLVList *
NTLVList_Create (void)
{
    struct NTLVList *l_pList;
    if ((l_pList = calloc (1, sizeof (struct NTLVList))) == NULL)
    {
        rzb_log(LOG_ERR, "%s: Failed to allocate memory for new ntlv list.", __func__);
        return NULL;
    }
    l_pList->iCount=0;
    // set head to NULL
    l_pList->pHead = NULL;
    l_pList->pTail = NULL;
    return l_pList;
}

SO_PUBLIC bool
NTLVList_Copy (struct NTLVList *p_pDest, const struct NTLVList *p_pSource)
{
    ASSERT (p_pDest != NULL);
    ASSERT (p_pSource != NULL);

    struct NTLVListItem *l_pListItem;

    l_pListItem = p_pSource->pHead;
    while (l_pListItem != NULL)
    {
        if (!NTLVList_Add (p_pDest,
                           l_pListItem->pItem->uuidName,
                           l_pListItem->pItem->uuidType,
                           l_pListItem->pItem->iLength,
                           l_pListItem->pItem->pData))
        {
            rzb_log (LOG_ERR,
                     "%s: failed due to failure of NTLVList_Add", __func__);
            return false;
        };

        l_pListItem = l_pListItem->pNext;
    }
    return true;
}

SO_PUBLIC void
NTLVList_Consume (struct NTLVList *p_pDest, struct NTLVList *p_pSource)
{
    ASSERT (p_pDest != NULL);
    ASSERT (p_pSource != NULL);

    // if source is null, we're done
    if (p_pSource == NULL)
        return;

    // move the list from source to dest
    if (p_pDest->pHead == NULL)
    {
        p_pDest->pHead = p_pSource->pHead;
        p_pDest->pTail = p_pSource->pTail;
    }
    else
    {
        p_pDest->pTail->pNext = p_pSource->pHead;
        p_pDest->pTail = p_pSource->pTail;
    }
    p_pSource->pHead = NULL;
}

SO_PUBLIC struct NTLVItem *
NTLVList_Find (const struct NTLVList *p_pSource, uuid_t p_pName)
{
    ASSERT (p_pSource != NULL);
    ASSERT (p_pName != NULL);

    // local variables
    struct NTLVListItem *l_pListItem;
    l_pListItem = p_pSource->pHead;

    // search for the item
    while (l_pListItem != NULL)
    {
        if (uuid_compare (l_pListItem->pItem->uuidName, p_pName) == 0)
            break;
        l_pListItem = l_pListItem->pNext;
    }
    if (l_pListItem == NULL)
        return NULL;
    else
        return l_pListItem->pItem;
}
