/** @file ntlv.h
 * Name Type Length Value data field wrapper.
 */
#ifndef	RAZORBACK_STRING_LIST_H
#define	RAZORBACK_STRING_LIST_H
#include <razorback/types.h>
extern struct List * StringList_Create(void);
/** Add a new entry to a user data list
 * @param *p_pList The destination
 * @param string The string to add.
 * @return true on success, false on failure.
 */
extern bool StringList_Add (struct List *p_pList, const char *string);


#if 0
extern bool NTLVList_Get (struct List *p_pList, uuid_t uuidName,
                          uuid_t uuidType, uint32_t *p_iSize,
                          const uint8_t ** p_pData);
#endif
/** Get the size of the items in a list
 * @param *p_pList The list
 * @return The size of the items in the list.
 */
extern uint32_t StringList_Size (struct List *list);


#endif
