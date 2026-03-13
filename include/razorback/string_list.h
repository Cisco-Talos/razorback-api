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

/** @file ntlv.h
 * Name Type Length Value data field wrapper.
 */
#ifndef RAZORBACK_STRING_LIST_H
#define RAZORBACK_STRING_LIST_H
#include <razorback/visibility.h>
#include <razorback/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Create a string list.
 * @return Requested object on success, or NULL on failure.
 */
SO_PUBLIC extern List_t * StringList_Create(void);

/**
 * Add a new entry to a user data list.
 * @param p_pList The destination.
 * @param string The string to add.
 * @return true on success, false on failure.
 */
SO_PUBLIC extern bool StringList_Add(List_t *p_pList, const char *string);


#if 0

/**
 * Get an entry from an NTLV list.
 * @param p_pList List to operate on.
 * @param uuidName Name UUID value.
 * @param uuidType Type UUID value.
 * @param p_iSize Size value.
 * @param p_pData Data value.
 * @return true on success, false on failure.
 */
SO_PUBLIC extern bool NTLVList_Get(
    List_t *p_pList,
    uuid_t uuidName,
    uuid_t uuidType,
    uint32_t *p_iSize,
    const uint8_t ** p_pData
);
#endif

/**
 * Get the size of the items in a list.
 * @param list The list.
 * @return The size of the items in the list.
 */
SO_PUBLIC extern uint32_t StringList_Size(List_t *list);

#ifdef __cplusplus
}
#endif
#endif
