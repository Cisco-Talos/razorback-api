#ifndef RAZORBACK_BLOCK_POOL_PRIVATE_H
#define RAZORBACK_BLOCK_POOL_PRIVATE_H
#include <razorback/block_pool.h>

#define BLOCK_POOL_KEEP 0
#define BLOCK_POOL_DESTROY 1

void BlockPool_ForEachItem(int (*function) (struct BlockPoolItem *));

#endif
