//
// Created by amishhammer on 3/2/24.
//

#ifndef RAZORBACK_TRANSFER_H
#define RAZORBACK_TRANSFER_H
#include <razorback/visibility.h>
#include <razorback/types.h>
#include <razorback/connected_entity.h>

#define DECL_TRANSFER_INIT bool transferInit(void)

#define DECL_TRANSFER_STORE bool Plugin_Store(struct BlockPoolItem *item, struct ConnectedEntity *dispatcher)
#define DECL_TRANSFER_FETCH bool Plugin_Fetch(struct Block *block, struct ConnectedEntity *dispatcher)


#endif //RAZORBACK_TRANSFER_H
