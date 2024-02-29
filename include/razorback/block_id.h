/** @file block_id.h
 * BlockId functions
 */

#ifndef	RAZORBACK_BLOCK_ID_H
#define	RAZORBACK_BLOCK_ID_H

#include <razorback/types.h>


/** Tests whether two blockids are equal
 * @param p_pA the first BlockId to compare
 * @param p_pB the second BlockId to compare
 * @return true if they are the same, false if they are not
 */
extern bool BlockId_IsEqual (const struct BlockId *p_pA,
                             const struct BlockId *p_pB);

/** Returns the maximum block size
 * @return the maximum block size
 */
extern uint32_t BlockId_MaxCount (void);

/** Tests whether a block size is valid
 * @param p_iCount the size to test
 * @return true if a valid size
 */
extern bool BlockId_IsValidCount (uint32_t p_iCount);

/** Tests whether a block type is valid
 * @param p_pType uuid_t of the type
 * @return true if a valid type
 */
extern bool BlockId_IsValidType (const uuid_t p_pType);

/** Converts a block id to text
 * @param p_pA the block id to convert
 * @param p_sText the destination text 
 */
extern void BlockId_ToText (const struct BlockId *p_pA, uint8_t * p_sText);

/** Converts a block id to a uint32_t
 * @param p_pA the block id to convert
 * @return the uint32_t
 */
extern uint32_t BlockId_ToHash (const struct BlockId *p_pA);

/** Converts a block id to a uint32_t and returns that value's modulus with the supplied parameter
 * @param p_pA the block id to convert
 * @param p_iModulus the modulus
 * @return the uint32_t % p_iModulus
 */
extern uint32_t BlockId_ToHashMod (const struct BlockId *p_pA,
                                   uint32_t p_iModulus);

/** Constructs a block id
 * @param p_pA the block id to construct
 * @param p_pType the type of the block
 * @param p_iCount the size of the block
 * @param p_sData the data
 * @return true if ok, false otherwise
 */
extern bool BlockId_Initialize (struct BlockId *p_pA, const uuid_t p_pType,
                                uint32_t p_iCount, const uint8_t * p_sData);

/** Returns the size of string required in the ToText function
 * @return the size of the string
 */
extern uint32_t BlockId_StringLength (struct BlockId *p_pB);

/** Destroys a block id
 * @param p_pBlockId the block to destroy
 */
extern void BlockId_Destroy (struct BlockId *p_pBlockId);

/** Copies a block id
 * @param p_pDestination the destination
 * @param p_pSource the source
 */
extern bool BlockId_Copy (struct BlockId *p_pDestination,
                          const struct BlockId *p_pSource);

/** Gets binary length of block id
 * @param p_pBlockId the block id
 * @return the size when placed in a binary buffer
 */
extern uint32_t BlockId_BinaryLength (const struct BlockId *p_pBlockId);


#endif // RAZORBACK_BLOCKID_H
