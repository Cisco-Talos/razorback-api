/** @file list.h
 * Linked list functions.
 */
#ifndef RAZORBACK_LIST_H
#define RAZORBACK_LIST_H
#include <stdint.h>
#include <stdlib.h>
#ifdef _MSC_VER
#else //_MSC_VER
#include <stdbool.h>
#include <stdatomic.h>
#endif //_MSC_VER
#include <razorback/visibility.h>

#ifdef __cplusplus
extern "C" {
#endif

/** List modes
 * @{
 */
#define LIST_MODE_GENERIC	0	///< Generic list
#define LIST_MODE_STACK 	1	///< Stack list
#define LIST_MODE_QUEUE 	2	///< Queue list
/// @}

/** List iteration returns.
 * @{
 */
#define LIST_EACH_OK 		0	///< Node successfully processed
#define LIST_EACH_ERROR 	1	///< Error processing node
#define LIST_EACH_REMOVE 	2	///< Node successfully processed, remove from list.
#define LIST_EACH_END       3   ///< Node successfully processed, end to loop
/// @}

typedef struct _List List_t;


/** Create a new List.
 * @param mode The list operation mode
 * @param cmp Function pointer to the node comparator.
 * @param keyCmp Function pointer to the node key comparator.
 * @param destroy Function pointer to the node data destructor.
 * @param clone Function pointer to the node clone routine. (Optional - Use NULL if cloning is not supported)
 * @param nodeLock Function pointer to the node lock routine. (Optional - Use NULL if node locking is not required)
 * @param nodeUnlock Function pointer to the node unlock routine. (Optional - Use NULL if node locking is not required)
 * @return A new List on success, NULL on error.
 */
SO_PUBLIC extern List_t * List_Create(int mode,
        int (*cmp)(void *, void *), 
        int (*keyCmp)(void *, void *), 
        void (*destroy)(void *), 
        void *(*clone)(void *),
        void (*nodeLock)(void *),
        void (*nodeUnlock)(void *));

/** Add an item to a List.
 * @param list The List to add the item to.
 * @param item The item to add.
 * @return true on success, false on error.
 */
SO_PUBLIC extern bool List_Push(List_t *list, void *item);

/** Remove the next item from a List.
 * @note For a list in queue or stack mode, this function will block until an item is available. In gerneral mode NULL will be returned if there is item to remove.
 * @param list The List to remove the item from.
 * @return A pointer to the removed item on success, NULL on failure.
 */
SO_PUBLIC extern void * List_Pop(List_t *list);

/** Remove the item from the list.
 * @param list The List to remove the item from.
 * @param item The item to remove.
 * @return true if the item was removed, false if the item was not found.
 */
SO_PUBLIC extern bool List_Remove(List_t *list, void *item);

/** Search a list for the item by key.
 * @param list The List to search.
 * @param id The node key.
 * @return A pointer to the item if found, NULL if not.
 */
SO_PUBLIC extern void * List_Find(List_t *list, void *id);

/** Iterate all items in a list executing op for each node.
 * @param list The List to iterate.
 * @param op Function pointer to the routine to run on every node. Op should return on of the list iteration status.
 * @param userData User data to pass as the ud argument to op.
 * @return true on success, false on error.
 */
SO_PUBLIC extern bool List_ForEach(List_t *list, int (*op)(void *i, void *ud), void *userData);

/** Get the number of items in a List in a thread safe fashion.
 * @param list The list to get the size of
 * @return The number of items in the list.
 */
SO_PUBLIC extern size_t List_Length(List_t *list);

/** Remove all items from a List.
 * @param list The List to clear.
 */
SO_PUBLIC extern void List_Clear(List_t *list);
/** Lock a list.
 * @param list The List to lock.
 * @return true on success, false on error
 */
SO_PUBLIC extern void List_Lock(List_t *list);

/** Unlock a list.
 * @param list The List to unlock.
 * @return true on success, false on error.
 */
SO_PUBLIC extern void List_Unlock(List_t *list);

/** Destroy a List
 * @param list The List to destroy.
 */
SO_PUBLIC extern void List_Destroy(List_t *list);

/** Clone a list and its contents.
 * @note The list must have been created with a none NULL clone function pointer.
 * @param list The List to clone.
 * @return A new list on success, NULL on error.
 */
SO_PUBLIC extern List_t* List_Clone (List_t *source);

/** Transfer list contents
 * @param dest The destination list
 * @param source The source list
 * @return true on success, false on error.
 */
SO_PUBLIC extern bool List_Transfer (List_t *dest, List_t *source);

#ifdef __cplusplus
}
#endif
#endif //RAZORBACK_LIST_H
