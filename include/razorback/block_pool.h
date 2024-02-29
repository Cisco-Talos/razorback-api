/** @file block_pool.h
 
 * Data block storage pool.
 */
#ifndef RAZORBACK_BLOCK_POOL_H
#define RAZORBACK_BLOCK_POOL_H

#include <razorback/types.h>
#include <razorback/api.h>
#include <pthread.h>

/** Block Pool Item States
 * @{
 */
#define BLOCK_POOL_STATUS_COLLECTING         0x01  ///< Collector is adding data
#define BLOCK_POOL_STATUS_FINALIZED          0x02  ///< Collector finished adding data
#define BLOCK_POOL_STATUS_CHECK_GLOBAL_CACHE 0x04  ///< Waiting for GC Check
#define BLOCK_POOL_STATUS_CHECKING_GLOBAL_CACHE 0x08  ///< Waiting for GC Check
#define BLOCK_POOL_STATUS_SUBMIT_DATA        0x10 ///< Submit block
#define BLOCK_POOL_STATUS_PAGED              0x11  ///< Block is currently paging out
#define BLOCK_POOL_STATUS_DESTROY            0x12  ///< Destroy this item
/// @}

#define BLOCK_POOL_DATA_FLAG_MMAPED     0x01    ///< Data block is mmaped to a file
#define BLOCK_POOL_DATA_FLAG_MALLOCD    0x02    ///< Data block is malloc'd



/** Create a new item in the bool.
 * @return NULL on error or a pointer to a BlockPoolItem
 */
extern struct BlockPoolItem *BlockPool_CreateItem (struct RazorbackContext *p_pContext);

/** Add data to a item in the block pool
 * @param *p_pItem the item to add data to.
 * @param *p_sName the data type name
 * @return true on success false on error.
 */
extern bool BlockPool_SetItemDataType(struct BlockPoolItem *p_pItem, char * p_sName);

/** Add data to a item in the block pool
 * @param *p_pItem the item to add data to.
 * @param *p_pData the data to add.
 * @param p_iLength the length of the data to add.
 * @param p_iFlags the data block flags
 * @return true on success false on error.
 */
extern bool BlockPool_AddData (struct BlockPoolItem *p_pItem, uint8_t * p_pData,
                               uint32_t p_iLength, int p_iFlags);

/** Finialize the block pool item for submission
 * @param *p_pItem the item to finalize
 * @ return true on success false on failure.
 */
extern bool BlockPool_FinalizeItem (struct BlockPoolItem *p_pItem);

/** Remove an item from the block pool.
 * @param *p_pItem the item to remove.
 * @return true on success false on error.
 */
extern bool BlockPool_DestroyItem (struct BlockPoolItem *p_pItem);

#endif
