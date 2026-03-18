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
#include <razorback/log.h>
#include <razorback/list.h>
#include <razorback/string_list.h>
#include <stdlib.h>
#include <string.h>



static int
StringList_ItemSize(const char *item, uint32_t *counter)
{
    *counter += strlen(item)+1;
    return LIST_EACH_OK;
}

SO_PUBLIC uint32_t
StringList_Size (List_t * list)
{
    uint32_t size = 0;

    ASSERT (list != NULL);
    if (list == NULL)
        return 0;

    List_ForEach(list, (int (*)(void *, void *))StringList_ItemSize, &size);

    return size + sizeof (uint32_t);
}

static int
String_KeyCmp(void *a, const void *id)
{
    char *item = a;
    const char *key = id;
    if (key == item)
        return 0;

    return strcmp(item,key);
}

static int
String_Cmp(void *a, void *b)
{
    char *iA = a;
    char *iB = b;
    if (a == b)
        return 0;
    return strcmp(iA, iB);
}

static void
String_Delete(void *a)
{
    char *item = a;
    free(item);
}

void *
String_Clone(void *o)
{
    const char *orig = o;

    ASSERT(orig != NULL);
    if (orig == NULL)
        return NULL;

    return strdup(orig);
}


SO_PUBLIC List_t *
StringList_Create (void)
{
    return List_Create(LIST_MODE_GENERIC,
            String_Cmp,
            String_KeyCmp,
            String_Delete,
            String_Clone, NULL, NULL);
}

SO_PUBLIC bool
StringList_Add (List_t *p_pList, const char *string)
{
    char *new;
    ASSERT(p_pList != NULL);
    ASSERT(string != NULL);
    if (p_pList == NULL || string == NULL)
        return false;

    if ((new = String_Clone((void *)string)) == NULL)
        return false;
    List_Push(p_pList, new);
    return true;
}
