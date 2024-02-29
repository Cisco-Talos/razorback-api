
/** @file connected_entity.h
 * ConnectedEntity structures and functions
 */

#ifndef	RAZORBACK_CONNECTED_ENTITY_H
#define	RAZORBACK_CONNECTED_ENTITY_H

#include <razorback/types.h>
#include <razorback/messages.h>
#include <pthread.h>
#include <time.h>

/** Dispatcher Entity Extension
 */
struct DispatcherEntity
{
    uint8_t priority;           ///< Dispatcher priority
    uint32_t flags;             ///< Dispatcher flags
    struct List * addressList;  ///< Dispatcher address list.
    uint8_t protocol;
    uint16_t port;
    bool usable;
};

/** Entity participating in a razorback cloud.
 */
struct ConnectedEntity
{
    uuid_t uuidNuggetId;                    ///< identifying uuid of the entity
    uuid_t uuidNuggetType;                  ///< identifying uuid of the entity
    uuid_t uuidApplicationType;             ///< identifying uuid of the entity
    time_t tTimeOfLastHello;                ///< time-stamp of last hello received
    uint8_t locality;                       ///< Configured locality of the entity
    struct DispatcherEntity *dispatcher;    ///< Dispatcher entity information
};

/** Updates the timestamp an entry in the list
 * @param message A hello message from the nugget.
 * @return true on success, false otherwise
 */
extern bool ConnectedEntityList_Update (struct Message *message);

/** Counts the number of entries in the list
 * @return the number of items in the list.
 */
extern uint32_t ConnectedEntityList_Count (void);
extern void ConnectedEntity_Destroy(struct ConnectedEntity *entity);

extern bool
ConnectedEntityList_AddPruneListener(void (*entityRemoved) (struct ConnectedEntity *entity, uint32_t remainingCount));

extern bool
ConnectedEntityList_ForEach_Entity (int (*function) (struct ConnectedEntity *, void *), void *userData);

extern struct ConnectedEntity *ConnectedEntityList_GetDedicatedDispatcher(void);
extern struct ConnectedEntity * ConnectedEntityList_GetDispatcher_HighestPriority();

#endif // RAZORBACK_CONNECTED_ENTITY_H
