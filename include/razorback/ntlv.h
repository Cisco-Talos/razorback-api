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
#ifndef RAZORBACK_NTLV_H
#define RAZORBACK_NTLV_H
#include <razorback/visibility.h>
#include <razorback/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Name Type Length Block
 */
struct NTLVItem
{
    uuid_t uuidName;   ///< The UUID of the data type name.
    uuid_t uuidType;   ///< The UUID of the data type in this block
    uint32_t iLength;  ///< The length of the data in this block
    uint8_t *pData;    ///< The data
};

SO_PUBLIC extern List_t * NTLVList_Create(void);
/** Add a new entry to a user data list
 * @param *p_pList The destination
 * @param uuidName The name
 * @param uuidType The type
 * @param p_iSize The size
 * @param *p_pData The data
 * @return true on success, false on failure.
 */
SO_PUBLIC extern bool NTLVList_Add (List_t *p_pList, uuid_t uuidName,
                          uuid_t uuidType, uint32_t p_iSize,
                          const uint8_t * p_pData);

SO_PUBLIC extern bool NTLVList_Get (List_t *p_pList, uuid_t uuidName,
                          uuid_t uuidType, uint32_t *p_iSize,
                          const uint8_t ** p_pData);


#ifdef __cplusplus
}
#endif
#endif
