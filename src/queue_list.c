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
#include <razorback/queue_list.h>
#include <razorback/log.h>
#include <razorback/uuids.h>
#include <string.h>

static int QueueList_KeyCmp(void *a, void *id);
static int QueueList_Cmp(void *a, void *b);

SO_PUBLIC List_t *
QueueList_Create (void)
{
    return List_Create(LIST_MODE_GENERIC,
            QueueList_Cmp, // Cmp
            QueueList_KeyCmp, // KeyCmp
            NULL, // Delete
            NULL, // Clone
            NULL, // Lock
            NULL); // Unlock
}

SO_PUBLIC struct Queue *
QueueList_Find (List_t *p_pList, const uuid_t p_pId)
{
    struct QueueListEntry *entry;

    ASSERT (p_pList != NULL);
    ASSERT (p_pId != NULL);
    if (p_pList == NULL)
        return NULL;
    if (p_pId == NULL)
        return NULL;
    if ((entry = List_Find(p_pList, (void *)p_pId)) == NULL)
        return NULL;

    return entry->pQueue;
}

SO_PUBLIC bool
QueueList_Add (List_t * p_pList, struct Queue * p_pQ,
               const uuid_t p_pId)
{
    struct QueueListEntry *l_pEntry;

    ASSERT (p_pList != NULL);
    ASSERT (p_pId != NULL);
    if (p_pList == NULL)
        return false;
    if (p_pId == NULL)
        return false;


    if ((l_pEntry = calloc (1,sizeof (struct QueueListEntry))) == NULL)
    {
        rzb_log (LOG_ERR, LOG_C_QUEUE, "%s: failed due to lack of memory", __func__);
        return false;
    }

    uuid_copy (l_pEntry->uuiKey, p_pId);
    l_pEntry->pQueue = p_pQ;

    return List_Push(p_pList, l_pEntry);
}

SO_PUBLIC bool
QueueList_Remove (List_t *p_pList, const uuid_t p_pId)
{
    struct QueueListEntry *entry;

    ASSERT (p_pList != NULL);
    ASSERT (p_pId != NULL);
    if (p_pList == NULL)
        return false;
    if (p_pId == NULL)
        return false;
    if ((entry = List_Find(p_pList, (void *)p_pId)) == NULL)
        return false;

    List_Remove(p_pList, entry);
    return true;
}

static int QueueList_KeyCmp(void *a, void *id) 
{
    unsigned char *uuid=(unsigned char *)id;
    struct QueueListEntry *entry = (struct QueueListEntry *)a;
    return uuid_compare(uuid,entry->uuiKey);
}
static int QueueList_Cmp(void *a, void *b)
{
    if (a == b)
        return 0;

    return -1;
}



