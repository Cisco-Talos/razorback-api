/** @file ntlv.h
 * Name Type Length Value data field wrapper.
 */
#ifndef	RAZORBACK_NTLV_H
#define	RAZORBACK_NTLV_H
#include <razorback/types.h>


/** Add a new entry to a user data list
 * @param *p_pList The destination
 * @param uuidName The name
 * @param uuidType The type
 * @param p_iSize The size
 * @param *p_pData The data
 * @return true on success, false on failure.
 */
extern bool NTLVList_Add (struct NTLVList *p_pList, uuid_t uuidName,
                          uuid_t uuidType, uint32_t p_iSize,
                          const uint8_t * p_pData);

/** Clear a user data list
 * @param *p_pList The list to clear
 */
extern void NTLVList_Clear (struct NTLVList *p_pList);

/** Get the number of items in a list
 * @param *p_pList The list
 * @return The number of items in the list.
 */
extern uint32_t NTLVList_Count (const struct NTLVList *p_pList);

/** Get the size of the items in a list
 * @param *p_pList The list
 * @return The size of the items in the list.
 */
extern uint32_t NTLVList_Size (const struct NTLVList *p_pList);

/** Create an empty list
 * @return A new list or NULL on error
 */
extern struct NTLVList * NTLVList_Create (void);

/** Destroy a list.
 */
extern void NTLVList_Destroy (struct NTLVList *p_pList);

/** Copies a list
 * @param *p_pDest The destination 
 * @param *p_pSource The source
 * @return True on success false on error.
 */
extern bool NTLVList_Copy (struct NTLVList *p_pDest,
                           const struct NTLVList *p_pSource);

/** Copy a list, consuming the source's contents
 * @param *p_pDest The destination 
 * @param *p_pSource The source
 */
extern void NTLVList_Consume (struct NTLVList *p_pDest,
                              struct NTLVList *p_pSource);

#endif
