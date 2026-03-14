/*
 * Copyright (c) 2011-2026 Cisco Systems, Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *  MA 02110-1301, USA.
 */

#include "config.h"

#include <razorback/debug.h>
#include <razorback/list.h>
#include <razorback/ntlv.h>
#include <razorback/log.h>
#include <stdlib.h>
#include <string.h>

struct NTLV_Key
{
    uuid_t name;
    uuid_t type;
};
SO_PUBLIC bool
NTLVList_Get (List_t *list, uuid_t name,
              uuid_t type, uint32_t *size, const uint8_t ** data)
{
    struct NTLVItem *item;
    struct NTLV_Key key;
    uuid_copy(key.name, name);
    uuid_copy(key.type, type);

    item = (struct NTLVItem *)List_Find(list, &key);
    if (item == NULL)
        return false;

    *data = item->pData;
    *size = item->iLength;
    return true;
}

SO_PUBLIC bool
NTLVList_Add (List_t *list, uuid_t name,
              uuid_t type, uint32_t size, const uint8_t * data)
{
    struct NTLVItem *item;

    ASSERT (list != NULL);
    ASSERT (data != NULL);
    ASSERT (size > 0);
    if (list == NULL) {
        rzb_log (LOG_ERR, LOG_C_CORE, "%s: failed due NULL list", __func__);
        return false;
    }
    if (data == NULL) {
        rzb_log (LOG_ERR, LOG_C_CORE, "%s: failed due NULL data", __func__);
        return false;
    }
    if (size == 0) {
        rzb_log (LOG_ERR, LOG_C_CORE, "%s: failed due to zero size", __func__);
        return false;
    }

    // create new entry
    if ((item = (struct NTLVItem *) calloc (1, sizeof (struct NTLVItem)))== NULL) {
        rzb_log (LOG_ERR, LOG_C_CORE, "%s: failed due to out of memory on item", __func__);
        return false;
    }

    item->pData = calloc (size, sizeof (uint8_t));
    if (item->pData == NULL)
    {
        free (item);
        rzb_log (LOG_ERR, LOG_C_CORE,
                 "%s: failed due to out of memory on item data", __func__);
        return false;
    }
    List_Push(list, item);

    uuid_copy (item->uuidName, name);
    uuid_copy (item->uuidType, type);
    item->iLength = size;
    memcpy (item->pData, data, size);
    return true;
}

static int
NTLV_KeyCmp(void *a, const void *id)
{
    struct NTLVItem *item = (struct NTLVItem *)a;
    const struct NTLV_Key *key = (const struct NTLV_Key *)id;
    if ( (uuid_compare(key->name, item->uuidName) == 0) ||
            ( uuid_compare(key->type, item->uuidName) == 0 ))
    {
        return 0;
    }
    return -1;
}

static int
NTLV_Cmp(void *a, void *b)
{
    struct NTLVItem *iA = (struct NTLVItem *)a;
    struct NTLVItem *iB = (struct NTLVItem *)b;
    if (a == b)
        return 0;
    if ( (uuid_compare(iA->uuidName, iB->uuidName) == 0) ||
            ( uuid_compare(iA->uuidType, iB->uuidType) == 0 ))
    {
        return 0;
    }
    return -1;
}

static void
NTLV_Delete(void *a)
{
    struct NTLVItem *item = (struct NTLVItem *)a;
    free(item->pData);
    free(item);
}

void *
NTLV_Clone(void *o)
{
    struct NTLVItem *item;
    struct NTLVItem *orig = (struct NTLVItem *)o;
    if ((item = calloc(1,sizeof(struct NTLVItem))) == NULL)
        return NULL;
    if((item->pData = calloc(orig->iLength, sizeof(uint8_t))) == NULL)
    {
        free(item);
        return NULL;
    }
    uuid_copy (item->uuidName, orig->uuidName);
    uuid_copy (item->uuidType, orig->uuidType);
    item->iLength = orig->iLength;
    memcpy (item->pData, orig->pData, orig->iLength);
    return item;
}

SO_PUBLIC List_t *
NTLVList_Create (void)
{
    return List_Create(LIST_MODE_GENERIC,
            NTLV_Cmp,
            NTLV_KeyCmp,
            NTLV_Delete,
            NTLV_Clone, NULL, NULL);
}

