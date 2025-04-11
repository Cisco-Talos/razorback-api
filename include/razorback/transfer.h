//
// Created by amishhammer on 3/2/24.
//

#ifndef RAZORBACK_TRANSFER_H
#define RAZORBACK_TRANSFER_H
#include <razorback/visibility.h>
#include <razorback/types.h>
#include <razorback/connected_entity.h>

enum TransferStatus {
    TRANSFER_OK,                ///< Block was transfered successfully
    TRANSFER_FAIL_DISPATCHER,   ///< An error was encountered talking to the dispatcher (dispatcher will be marked as bad)
    TRANSFER_FAIL_LOCAL,        ///< A local error occured (Dispatcher status will be unchanged)
};
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

SO_PUBLIC extern bool Transport_Register(struct TransportDescriptor *desc);

SO_PUBLIC extern enum TransferStatus Transfer_File_Store(struct BlockPoolItem *item, struct ConnectedEntity *dispatcher);
SO_PUBLIC extern enum TransferStatus Transfer_File_Fetch(struct Block *block, struct ConnectedEntity *dispatcher);

SO_PUBLIC extern enum TransferStatus Transfer_SSH_Store(struct BlockPoolItem *item, struct ConnectedEntity *dispatcher);
SO_PUBLIC extern enum TransferStatus Transfer_SSH_Fetch(struct Block *block, struct ConnectedEntity *dispatcher);

SO_PUBLIC extern enum TransferStatus Transfer_HTTP_Store(struct BlockPoolItem *item, struct ConnectedEntity *dispatcher);
SO_PUBLIC extern enum TransferStatus Transfer_HTTP_Fetch(struct Block *block, struct ConnectedEntity *dispatcher);

SO_PUBLIC extern bool Transfer_Prepare_File(struct Block *block, char *file, bool temp);
#endif //RAZORBACK_TRANSFER_H
