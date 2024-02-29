/** @file connectedentity.h
 * ConnectedEntity structures and functions
 */

#ifndef	RAZORBACK_CONNECTED_ENTITY_H
#define	RAZORBACK_CONNECTED_ENTITY_H

#include <razorback/types.h>
#include <pthread.h>
#include <time.h>

/** Defines for ConnectedEntityEntry.iState
 */
#define CONNECTEDENTITY_STATE_REQUESTEDREGISTER  0x00000001
#define CONNECTEDENTITY_STATE_REGISTERING        0x00000002
#define CONNECTEDENTITY_STATE_CONFIGURING        0x00000004
#define CONNECTEDENTITY_STATE_PAUSED             0x00000008
#define CONNECTEDENTITY_STATE_RUNNING            0x00000010
#define CONNECTEDENTITY_STATE_TERMINATED         0x00000011
#define CONNECTEDENTITY_STATE_ERROR              0x00000012


/** Updates the timestamp an entry in the list
 * @param p_uuidNuggetId  The nugget ID of the entity
 * @param p_uuidNuggetType The nugget type of the entity
 * @param p_uuidApplicationType The application type of the entity
 * @return true on success, false otherwise
 */
extern bool ConnectedEntityList_Update (uuid_t p_uuidNuggetId,
                                        uuid_t p_uuidNuggetType,
                                        uuid_t p_uuidApplicationType);

/** Updates the state an entry in the list
 * @param p_uuidNuggetId  The nugget ID of the entity
 * @param p_uuidNuggetType The nugget type of the entity
 * @param p_uuidApplicationType The application type of the entity
 * @return true on success, false otherwise
 */
extern bool ConnectedEntityList_ChangeState (uuid_t p_uuidNuggetId,
                                             uuid_t p_uuidNuggetType,
                                             uuid_t p_uuidApplicationType,
                                             uint32_t p_iState);

/** Changes the state of the specified entity
 * @param p_uuidNuggetId  The nugget ID of the entity
 * @param p_uuidNuggetType The nugget type of the entity
 * @param p_uuidApplicationType The application type of the entity
 * @param p_pState the state
 * @return true if success and the value at p_pState set to the current state, false if not found
 */
extern bool ConnectedEntityList_GetState (uuid_t p_uuidNuggetId,
                                          uuid_t p_uuidNuggetType,
                                          uuid_t p_uuidApplicationType,
                                          uint32_t * p_pState);

/** Counts the number of entries in the list
 * @return the number of items in the list.
 */
extern uint32_t ConnectedEntityList_Count (void);

#endif // RAZORBACK_CONNECTED_ENTITY_H
