/** @file block.h
 * Block functions
 */

#ifndef	RAZORBACK_BLOCK_H
#define	RAZORBACK_BLOCK_H

#include <razorback/types.h>


/** Constructs a block
 * @return a new Block or NULL on error.
 */
extern struct Block * Block_Create (void);

/** Destroys a block id
 * @param p_pBlockId the block to destroy
 */
extern void Block_Destroy (struct Block *p_pBlock);

/** Copies a block id
 * @param p_pDestination the destination
 * @param p_pSource the source
 */
extern bool Block_Copy (struct Block *p_pDestination,
                        const struct Block *p_pSource);

/** Calculate the Wire Size of A Block
 * @param the block
 * @return the size
 */
extern uint32_t Block_BinaryLength (struct Block *p_pBlock);
#endif // RAZORBACK_BLOCKID_H
