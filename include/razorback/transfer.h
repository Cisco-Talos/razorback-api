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

//
// Created by amishhammer on 3/2/24.
//

#ifndef RAZORBACK_TRANSFER_H
#define RAZORBACK_TRANSFER_H
#include <razorback/visibility.h>
#include <razorback/types.h>
#include <razorback/connected_entity.h>

#define TRANSFER_MODE_FILE 0
#define TRANSFER_MODE_SSH 1
#define TRANSFER_MODE_HTTP 2
#define TRANSFER_MODE_HTTPS 3
#define TRANSFER_MODE_CUSTOM 255

enum TransferStatus {
    TRANSFER_OK,                ///< Block was transfered successfully
    TRANSFER_FAIL_DISPATCHER,   ///< An error was encountered talking to the dispatcher (dispatcher will be marked as bad)
    TRANSFER_FAIL_LOCAL,        ///< A local error occured (Dispatcher status will be unchanged)
};

/**
 * Transfer implementation descriptor.
 */
struct TransportDescriptor {
    uint8_t id;
    const char *name;
    const char *description;
    enum TransferStatus (*store)(struct BlockPoolItem *item, struct ConnectedEntity *dispatcher);
    enum TransferStatus (*fetch)(struct Block *block, struct ConnectedEntity *dispatcher);
};

#define DECL_TRANSFER_INIT bool transferInit(void)

#define DECL_TRANSFER_STORE enum TransferStatus Plugin_Store(struct BlockPoolItem *item, struct ConnectedEntity *dispatcher)
#define DECL_TRANSFER_FETCH enum TransferStatus Plugin_Fetch(struct Block *block, struct ConnectedEntity *dispatcher)

/**
 * Register a transport descriptor.
 * @param desc Transport descriptor to register.
 * @return true on success, false on failure.
 */
SO_PUBLIC extern bool Transport_Register(struct TransportDescriptor *desc);

/**
 * Store a block using the file transport.
 * @param item Block pool item to store.
 * @param dispatcher Dispatcher to use for the transfer.
 * @return Transfer status result.
 */
SO_PUBLIC extern enum TransferStatus Transfer_File_Store(
    struct BlockPoolItem *item,
    struct ConnectedEntity *dispatcher
);

/**
 * Fetch a block using the file transport.
 * @param block Block to fetch.
 * @param dispatcher Dispatcher to use for the transfer.
 * @return Transfer status result.
 */
SO_PUBLIC extern enum TransferStatus Transfer_File_Fetch(
    struct Block *block,
    struct ConnectedEntity *dispatcher
);

/**
 * Store a block using the SSH transport.
 * @param item Block pool item to store.
 * @param dispatcher Dispatcher to use for the transfer.
 * @return Transfer status result.
 */
SO_PUBLIC extern enum TransferStatus Transfer_SSH_Store(
    struct BlockPoolItem *item,
    struct ConnectedEntity *dispatcher
);

/**
 * Fetch a block using the SSH transport.
 * @param block Block to fetch.
 * @param dispatcher Dispatcher to use for the transfer.
 * @return Transfer status result.
 */
SO_PUBLIC extern enum TransferStatus Transfer_SSH_Fetch(
    struct Block *block,
    struct ConnectedEntity *dispatcher
);

/**
 * Store a block using the HTTP transport.
 * @param item Block pool item to store.
 * @param dispatcher Dispatcher to use for the transfer.
 * @return Transfer status result.
 */
SO_PUBLIC extern enum TransferStatus Transfer_HTTP_Store(
    struct BlockPoolItem *item,
    struct ConnectedEntity *dispatcher
);

/**
 * Fetch a block using the HTTP transport.
 * @param block Block to fetch.
 * @param dispatcher Dispatcher to use for the transfer.
 * @return Transfer status result.
 */
SO_PUBLIC extern enum TransferStatus Transfer_HTTP_Fetch(
    struct Block *block,
    struct ConnectedEntity *dispatcher
);

/**
 * Prepare a file for block transfer.
 * @param block Block to operate on.
 * @param file Destination file path.
 * @param temp Set to true when the prepared file should be treated as temporary.
 * @return true on success, false on failure.
 */
SO_PUBLIC extern bool Transfer_Prepare_File(struct Block *block, char *file, bool temp);
#endif //RAZORBACK_TRANSFER_H
