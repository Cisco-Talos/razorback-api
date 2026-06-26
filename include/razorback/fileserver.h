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

/** @file fileserver.h
 * Dispatcher-next HTTP(S) fileserver block transport.
 */
#ifndef RAZORBACK_FILESERVER_H
#define RAZORBACK_FILESERVER_H

#include <razorback/types.h>
#include <razorback/visibility.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct RzbNextFileserverClient RzbNextFileserverClient_t;

enum RzbNextFileserverStatus
{
    RZB_NEXT_FILESERVER_OK = 0,
    RZB_NEXT_FILESERVER_LOCAL_ERROR = 1,
    RZB_NEXT_FILESERVER_NOT_FOUND = 2,
    RZB_NEXT_FILESERVER_RETRYABLE = 3,
    RZB_NEXT_FILESERVER_HTTP_ERROR = 4,
    RZB_NEXT_FILESERVER_PAYLOAD_MISMATCH = 5
};

SO_PUBLIC extern RzbNextFileserverClient_t * RzbNextFileserverClient_Create(
    const char *baseUrl,
    uint64_t fetchTimeoutSeconds,
    uint64_t uploadTimeoutSeconds
);
SO_PUBLIC extern void RzbNextFileserverClient_Destroy(
    RzbNextFileserverClient_t *client
);
SO_PUBLIC extern enum RzbNextFileserverStatus RzbNextFileserver_BuildUrl(
    const RzbNextFileserverClient_t *client,
    const struct BlockId *blockId,
    char **url
);
SO_PUBLIC extern enum RzbNextFileserverStatus RzbNextFileserver_StoreFile(
    RzbNextFileserverClient_t *client,
    const struct BlockId *blockId,
    const char *fileName
);
SO_PUBLIC extern enum RzbNextFileserverStatus RzbNextFileserver_StoreBytes(
    RzbNextFileserverClient_t *client,
    const struct BlockId *blockId,
    const uint8_t *data,
    size_t length
);
SO_PUBLIC extern enum RzbNextFileserverStatus RzbNextFileserver_StoreBlockPoolItem(
    RzbNextFileserverClient_t *client,
    struct BlockPoolItem *item
);
SO_PUBLIC extern enum RzbNextFileserverStatus RzbNextFileserver_FetchToFile(
    RzbNextFileserverClient_t *client,
    const struct BlockId *blockId,
    char **fileName
);
SO_PUBLIC extern enum RzbNextFileserverStatus RzbNextFileserver_FetchBlock(
    RzbNextFileserverClient_t *client,
    struct Block *block
);
SO_PUBLIC extern bool RzbNextFileserver_AttachFileToBlock(
    struct Block *block,
    char *fileName,
    bool tempFile
);
SO_PUBLIC extern void RzbNextFileserver_FreeBlockData(struct Block *block);

#ifdef __cplusplus
}
#endif
#endif /* RAZORBACK_FILESERVER_H */
