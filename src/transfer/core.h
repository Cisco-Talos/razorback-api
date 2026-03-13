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

#ifndef TRANSFER_CORE_H
#define TRANSFER_CORE_H
#include <razorback/types.h>
#include <razorback/connected_entity.h>
#include <razorback/transfer.h>


#ifdef __cplusplus
extern "C" {
#endif



char * Transfer_generateFilename (struct Block *block);

bool  Transport_IsSupported(uint8_t protocol);
enum TransferStatus Transfer_Store(struct BlockPoolItem *item, struct ConnectedEntity *dispatcher);
enum TransferStatus Transfer_Fetch(struct Block *block, struct ConnectedEntity *dispatcher);
void Transfer_Free(struct Block *block, struct ConnectedEntity *dispatcher);


// Init functions
bool File_Init(void);
bool SSH_Init(void);
bool HTTP_Init(void);
#ifdef __cplusplus
}
#endif
#endif
