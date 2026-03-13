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

/** @file block_id.h
 * BlockId functions
 */

#ifndef RAZORBACK_BLOCK_ID_H
#define RAZORBACK_BLOCK_ID_H
#include <razorback/visibility.h>
#include <razorback/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Compare two block identifiers for equality.
 * @param p_pA the first BlockId to compare.
 * @param p_pB the second BlockId to compare.
 * @return true if they are the same, false if they are not.
 */
SO_PUBLIC extern bool BlockId_IsEqual(const struct BlockId *p_pA, const struct BlockId *p_pB);

/**
 * Convert a block identifier to text.
 * @param p_pA the block id to convert.
 * @param p_sText the destination text.
 * @return No return value.
 */
SO_PUBLIC extern void BlockId_ToText(const struct BlockId *p_pA, uint8_t * p_sText);

/**
 * Create a block identifier without allocating nested objects.
 * @return a new block ID or null on error.
 */
SO_PUBLIC extern struct BlockId * BlockId_Create_Shallow(void);

/**
 * Create a block identifier.
 * @return a new block ID or null on error.
 */
SO_PUBLIC extern struct BlockId * BlockId_Create(void);

/**
 * Get the string length needed to represent a block identifier.
 * @param p_pB B object.
 * @return the size of the string.
 */
SO_PUBLIC extern uint32_t BlockId_StringLength(struct BlockId *p_pB);

/**
 * Destroy a block identifier.
 * @param p_pBlockId the block to destroy.
 * @return No return value.
 */
SO_PUBLIC extern void BlockId_Destroy(struct BlockId *p_pBlockId);

/**
 * Clone a block identifier.
 * @param p_pSource the source.
 * @return A copy of the block id on success, NULL on an error.
 */
SO_PUBLIC extern struct BlockId * BlockId_Clone(const struct BlockId *p_pSource);

#ifdef __cplusplus
}
#endif
#endif // RAZORBACK_BLOCKID_H
