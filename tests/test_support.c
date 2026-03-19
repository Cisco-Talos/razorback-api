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

#include <razorback/block.h>
#include <razorback/event.h>
#include <razorback/hash.h>
#include <razorback/judgment.h>
#include <razorback/log.h>
#include <razorback/ntlv.h>
#include <razorback/nugget.h>
#include <razorback/uuids.h>

#include <stdarg.h>
#include <stdlib.h>

uint32_t
Config_getHashType(void)
{
    return HASH_TYPE_SHA256;
}

void
rzb_log(unsigned level, uint64_t component, const char *fmt, ...)
{
    va_list args;

    (void)level;
    (void)component;
    (void)fmt;

    va_start(args, fmt);
    va_end(args);
}

void
Block_Destroy(struct Block *block)
{
    free(block);
}

void
Event_Destroy(struct Event *event)
{
    free(event);
}

void
EventId_Destroy(struct EventId *eventId)
{
    free(eventId);
}

void
Judgment_Destroy(struct Judgment *judgment)
{
    free(judgment);
}

void
Nugget_Destroy(struct Nugget *nugget)
{
    free(nugget);
}

List_t *
NTLVList_Create(void)
{
    return NULL;
}

bool
NTLVList_Add(List_t *list, uuid_t name, uuid_t type, uint32_t size,
             const uint8_t *data)
{
    if (list == NULL || data == NULL || size == 0)
        return false;
    (void)name;
    (void)type;
    return false;
}

bool
UUID_Get_UUID(const char *name, int type, uuid_t uuid)
{
    (void)name;
    (void)type;
    uuid_clear(uuid);
    return false;
}

List_t *
UUID_Create_List(void)
{
    return NULL;
}

bool
UUID_Add_List_Entry(List_t *list, uuid_t uuid, const char *name,
                    const char *desc)
{
    (void)list;
    (void)uuid;
    (void)name;
    (void)desc;
    return false;
}
