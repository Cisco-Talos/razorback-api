#ifndef TRANSFER_CORE_H
#define TRANSFER_CORE_H
#include <razorback/types.h>
#include <razorback/connected_entity.h>

#ifdef __cplusplus
extern "C" {
#endif

struct TransportDescriptor {
    uint8_t id;
    const char *name;
    const char *description;
    bool (*store)(struct BlockPoolItem *item, struct ConnectedEntity *dispatcher);
    bool (*fetch)(struct Block *block, struct ConnectedEntity *dispatcher);
    void (*free)(struct Block *block);
};

char * Transfer_generateFilename (struct Block *block);

bool Transport_Register(struct TransportDescriptor *desc);

bool  Transport_IsSupported(uint8_t protocol);
bool Transfer_Store(struct BlockPoolItem *item, struct ConnectedEntity *dispatcher);
bool Transfer_Fetch(struct Block *block, struct ConnectedEntity *dispatcher);
void Transfer_Free(struct Block *block, struct ConnectedEntity *dispatcher);


// Init functions
bool File_Init(void);
bool SSH_Init(void);
#ifdef __cplusplus
}
#endif
#endif
