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

#ifndef RAZORBACK_BLOCK_POOL_PRIVATE_H
#define RAZORBACK_BLOCK_POOL_PRIVATE_H
#include <razorback/block_pool.h>
#ifdef __cplusplus
extern "C" {
#endif
#define BLOCK_POOL_KEEP 0
#define BLOCK_POOL_DESTROY 2

bool BlockPool_Init(struct RazorbackContext *p_pContext);
void BlockPool_ForEachItem(int (*function) (struct BlockPoolItem *, void *), void *userData);
void BlockPool_Item_Lock(void *a);
void BlockPool_Item_Unlock(void *a);

void BlockPool_SetStatus(struct BlockPoolItem *p_pItem, uint32_t p_iStatus);
uint32_t BlockPool_GetStatus(struct BlockPoolItem *p_pItem);
void BlockPool_SetFlags(struct BlockPoolItem *p_pItem, uint32_t p_iFlags);
void BlockPool_DestroyItemDataList(struct BlockPoolData *p_pItem);
#ifdef __cplusplus
}
#endif
#endif
