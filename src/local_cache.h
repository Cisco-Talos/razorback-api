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

#ifndef RAZORBACK_LOCAL_CACHE_H
#define RAZORBACK_LOCAL_CACHE_H

#include <razorback/types.h>
#include <razorback/lock.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef enum
{
    LT_T1,
    LT_T2,
    LT_B1,
    LT_B2,
    LT_NONE
} LISTTYPE;

typedef enum
{
    GOODHASH,
    BADHASH,
    URL,
    ALL
} CacheType;

typedef enum
{
    FULL
} ClearMethod;

typedef struct _ENTRY
{
    struct _ENTRY *next;
    struct _ENTRY *prev;
    LISTTYPE listtype;
    unsigned size;
    uint8_t *key;
    uint32_t sfflags;
    uint32_t entflags;
} ENTRY;

typedef struct _CACHE
{
    unsigned size;
    unsigned entries;
    unsigned listSize[LT_NONE];
    double target;
    Mutex_t * cachemutex;
    ENTRY *LRU[LT_NONE];
    ENTRY *MRU[LT_NONE];
    ENTRY *entrylist;
} CACHE;

Lookup_Result checkLocalEntry(uint8_t *key, uint32_t size, uint32_t *sfflags, uint32_t *entflags, CacheType type);
Lookup_Result addLocalEntry(uint8_t *key, uint32_t size, uint32_t sfflags, uint32_t entflags, CacheType type);
Lookup_Result updateLocalEntry(uint8_t *key, uint32_t size, uint32_t sfflags, uint32_t entflags, CacheType type);
Lookup_Result removeLocalEntry(uint8_t *key, uint32_t size, CacheType type);
Lookup_Result clearLocalEntry(CacheType type, ClearMethod method);
#ifdef __cplusplus
}
#endif
#endif
