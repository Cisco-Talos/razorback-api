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

/** @file block.h
 * Block functions
 */

#ifndef RAZORBACK_BLOCK_H
#define RAZORBACK_BLOCK_H
#include <razorback/visibility.h>
#include <razorback/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Create a block.
 * @return a new Block or NULL on error.
 */
SO_PUBLIC extern struct Block * Block_Create(void);

/**
 * Destroy a block.
 * @param p_pBlock Block to operate on.
 * @return No return value.
 */
SO_PUBLIC extern void Block_Destroy(struct Block *p_pBlock);

/**
 * Clone a block.
 * @param p_pSource the source.
 * @return Requested object on success, or NULL on failure.
 */
SO_PUBLIC extern struct Block * Block_Clone(const struct Block *p_pSource);

/**
 * Add metadata to a block.
 * @param block The block to add metadata to.
 * @param uuidName The UUID of the metadata name.
 * @param uuidType The UUID of the metadata data type.
 * @param data The data.
 * @param size The size of the data.
 * @return true on success false on error.
 */
SO_PUBLIC extern bool Block_MetaData_Add(
    struct Block *block,
    uuid_t uuidName,
    uuid_t uuidType,
    uint8_t *data,
    uint32_t size
);

/**
 * Add file name metadata to a block.
 * @param block The block.
 * @param fileName The file name.
 * @return true on success, false on error.
 */
SO_PUBLIC extern bool Block_MetaData_Add_FileName(struct Block *block, const char * fileName);
#ifdef __cplusplus
}
#endif
#endif // RAZORBACK_BLOCKID_H
