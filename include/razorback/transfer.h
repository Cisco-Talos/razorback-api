//
// Created by amishhammer on 3/2/24.
//

#ifndef RAZORBACK_TRANSFER_H
#define RAZORBACK_TRANSFER_H
#include <razorback/visibility.h>
#include <razorback/types.h>
#include <razorback/connected_entity.h>

struct TransportDescriptor {
    uint8_t id;
    const char *name;
    const char *description;
    bool (*store)(struct BlockPoolItem *item, struct ConnectedEntity *dispatcher);
    bool (*fetch)(struct Block *block, struct ConnectedEntity *dispatcher);
};

#define DECL_TRANSFER_INIT bool transferInit(void)

#define DECL_TRANSFER_STORE bool Plugin_Store(struct BlockPoolItem *item, struct ConnectedEntity *dispatcher)
#define DECL_TRANSFER_FETCH bool Plugin_Fetch(struct Block *block, struct ConnectedEntity *dispatcher)

SO_PUBLIC extern bool Transport_Register(struct TransportDescriptor *desc);

SO_PUBLIC extern bool Transfer_File_Store(struct BlockPoolItem *item, struct ConnectedEntity *dispatcher);
SO_PUBLIC extern bool Transfer_File_Fetch(struct Block *block, struct ConnectedEntity *dispatcher);

SO_PUBLIC extern bool Transfer_SSH_Store(struct BlockPoolItem *item, struct ConnectedEntity *dispatcher);
SO_PUBLIC extern bool Transfer_SSH_Fetch(struct Block *block, struct ConnectedEntity *dispatcher);

SO_PUBLIC extern bool Transfer_Prepare_File(struct Block *block, char *file, bool temp);
#endif //RAZORBACK_TRANSFER_H
