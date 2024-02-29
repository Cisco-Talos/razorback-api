#ifndef RAZORBACK_BLOCK_POOL_PRIVATE_H
#define RAZORBACK_BLOCK_POOL_PRIVATE_H
#include <razorback/block_pool.h>

#define BLOCK_POOL_KEEP 0
#define BLOCK_POOL_DESTROY 1

bool BlockPool_Init(struct RazorbackContext *p_pContext);
void BlockPool_ForEachItem(int (*function) (struct BlockPoolItem *));

void BlockPool_SetStatus(struct BlockPoolItem *p_pItem, uint32_t p_iStatus);
uint32_t BlockPool_GetStatus(struct BlockPoolItem *p_pItem);
void BlockPool_SetFlags(struct BlockPoolItem *p_pItem, uint32_t p_iFlags);
void BlockPool_DestroyItemDataList(struct BlockPoolItem *p_pItem);
#endif
