/*
 * Copyright (c) 2011-2026 Cisco Systems, Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *  MA 02110-1301, USA.
 */

#include "config.h"

#include <razorback/debug.h>
#include <razorback/connected_entity.h>
#include <razorback/log.h>
#include <razorback/thread.h>
#include <razorback/timer.h>
#include <razorback/uuids.h>
#include <razorback/list.h>

#include <signal.h>

#include "connected_entity_private.h"
#include "runtime_config.h"
#include "transfer/core.h"
#define SEARCH_KEY_NUGGET_ID    (1 << 0)
#define SEARCH_KEY_APP_TYPE     (1 << 1)
#define SEARCH_KEY_NUGGET_TYPE  (1 << 2)
#define SEARCH_KEY_DD           (1 << 3)
#define SEARCH_KEY_LOCALITY     (1 << 4)
#define SEARCH_KEY_USABLE       (1 << 5)

static int ConnectedEntity_KeyCmp(void *a, const void *id);
static int ConnectedEntity_Cmp(void *a, void *b);
static void ConnectedEntity_Delete(void *a);
static int ConnectedEntityHook_KeyCmp(void *a, const void *id);
static int ConnectedEntityHook_Cmp(void *a, void *b);
static void ConnectedEntityHook_Delete(void *a);
static bool ConnectedEntity_IsDispatcherType(uuid_t nuggetType);

struct ConnectedEntityKey {
    int searchKeys;
    unsigned char *nuggetId;
    unsigned char *appType;
    unsigned char *nuggetType;
    uint32_t locality;
};


static struct Timer *timer;  ///< The prune timer

struct ConnectedEntityHook {
    void (*entityRemoved) (struct ConnectedEntity *entity, uint32_t remainingCount);
};

static List_t *sg_pEntityList = NULL;
static List_t *sg_pHookList = NULL;

static void ConnectedEntityList_Prune (void *userData);

static bool
ConnectedEntity_IsDispatcherType(uuid_t nuggetType)
{
    bool isDispatcher = false;

    (void)UUID_Is_Named_UUID(NUGGET_TYPE_DISPATCHER, UUID_TYPE_NUGGET_TYPE,
                             nuggetType, &isDispatcher);
    return isDispatcher;
}

bool
ConnectedEntityList_Start (void) {
    ASSERT (sg_pEntityList == NULL);
    if (sg_pEntityList != NULL) {
        rzb_log(LOG_ERR, LOG_C_CNC, "%s: already started", __func__);
        return false;
    }

    sg_pEntityList = List_Create(LIST_MODE_GENERIC,
            ConnectedEntity_Cmp, // Cmp
            ConnectedEntity_KeyCmp, // KeyCmp
            ConnectedEntity_Delete, // destroy
            NULL, // clone
            NULL, // Lock
            NULL); //Unlock

    sg_pHookList = List_Create(LIST_MODE_GENERIC,
            ConnectedEntityHook_Cmp, // Cmp
            ConnectedEntityHook_KeyCmp, // KeyCmp
            ConnectedEntityHook_Delete, // destroy
            NULL, // clone
            NULL, // Lock
            NULL); //Unlock

    if ((timer = Timer_Create(Config_getHelloTime () / 2, ConnectedEntityList_Prune, NULL)) == NULL) {
        rzb_log(LOG_ERR, LOG_C_CNC, "%s: failed to create prune timer", __func__);
        return false;
    }

    return true;
}

void
ConnectedEntityList_Stop (void) {
    ASSERT(sg_pEntityList != NULL);
    if (sg_pEntityList == NULL)
       return;

    Timer_Destroy(timer);
    List_Destroy(sg_pEntityList);
}

/** Return a entry or NULL
 */
static struct ConnectedEntity *
ConnectedEntityList_GetEntity (struct Message *message)
{
    // TODO - Needs to return the entity in the locked state
    struct MessageHello *hello;
    struct ConnectedEntity *ret = NULL;
    struct ConnectedEntityKey key;
    uuid_t source,dest;

    ASSERT (sg_pEntityList != NULL);
    if (sg_pEntityList == NULL) {
        rzb_log (LOG_ERR, LOG_C_CNC,
                 "%s: Failed due to entity list not initialized", __func__);
        return NULL;
    }
    ASSERT (message != NULL);
    if (message == NULL) {
        rzb_log (LOG_ERR, LOG_C_CNC, "%s: Failed due to NULL message", __func__);
        return NULL;
    }
    ASSERT (message->type == MESSAGE_TYPE_HELLO);
    if (message->type != MESSAGE_TYPE_HELLO) {
        rzb_log (LOG_ERR, LOG_C_CNC,
                 "%s: Failed due to message not being a HELLO", __func__);
        return NULL;
    }

    hello = message->message;
    Message_Get_Nuggets(message, source,dest);
    key.searchKeys = SEARCH_KEY_NUGGET_ID;
    key.nuggetId = source;
    key.appType = NULL;
    key.nuggetType = NULL;
    ret = List_Find(sg_pEntityList, &key);
    // This will return a the node locked
    if (ret == NULL) {
        if ((ret =
                     calloc(1, sizeof(struct ConnectedEntity))) == NULL) {
            return NULL;
        }

        uuid_copy(ret->uuidNuggetId, source);
        uuid_copy(ret->uuidNuggetType, hello->uuidNuggetType);
        uuid_copy(ret->uuidApplicationType, hello->uuidApplicationType);
        ret->locality = hello->locality;
        if (ConnectedEntity_IsDispatcherType(ret->uuidNuggetType)) {
            if ((ret->dispatcher = calloc(1, sizeof(struct DispatcherEntity))) == NULL) {
                free(ret);
                return NULL;
            }
            ret->dispatcher->priority = hello->priority;
            ret->dispatcher->flags = hello->flags;
            ret->dispatcher->port = hello->port;
            ret->dispatcher->protocol = hello->protocol;
            ret->dispatcher->usable = Transport_IsSupported(hello->protocol);
            if ((ret->dispatcher->addressList = List_Clone(hello->addressList)) == NULL) {
                free(ret->dispatcher);
                free(ret);
                return NULL;
            }
        }
        // Lock node before putting it in the list
        List_Push(sg_pEntityList, ret);
    }
    return ret;
}

SO_PUBLIC bool
ConnectedEntityList_Update (struct Message *message) {
    struct ConnectedEntity *entity = NULL;
    struct MessageHello *hello;

    ASSERT (sg_pEntityList != NULL);
    if (sg_pEntityList == NULL) {
        rzb_log (LOG_ERR, LOG_C_CNC,
                 "%s: Failed due to entity list not initialized", __func__);
        return false;
    }
    ASSERT (message != NULL);
    if (message == NULL) {
        rzb_log (LOG_ERR, LOG_C_CNC, "%s: Failed due to NULL message", __func__);
        return false;
    }
    ASSERT (message->type == MESSAGE_TYPE_HELLO);
    if (message->type != MESSAGE_TYPE_HELLO) {
        rzb_log (LOG_ERR, LOG_C_CNC,
                 "%s: Failed due to message not being a HELLO", __func__);
        return false;
    }

    hello = message->message;

    if ((entity = ConnectedEntityList_GetEntity(message)) == NULL) {
        rzb_log(LOG_ERR, LOG_C_CNC,
                "%s: Failed due to failure of _GetEntry()", __func__);
        return false;
    }

    entity->tTimeOfLastHello = time(NULL);
    if (ConnectedEntity_IsDispatcherType(entity->uuidNuggetType)) {
        entity->dispatcher->flags = hello->flags;
        entity->dispatcher->priority = hello->priority;
    }
    // Drop the entity lock
    return true;
}

SO_PUBLIC uint32_t
ConnectedEntityList_GetCount (void) {
    return List_Length(sg_pEntityList);
}

struct CountEntity {
    uint32_t count;
    struct ConnectedEntity *entity;
};

static int
ConnectedEntityList_CountNuggets(void *item, void *userData)
{
    struct ConnectedEntity *entity = item;
    struct CountEntity *counter = userData;
    ASSERT(entity != NULL);
    ASSERT(counter != NULL);
    if (entity == NULL || counter == NULL) {
        rzb_log(LOG_ERR, LOG_C_CNC, "%s: NULL entity or counter", __func__);
        return LIST_EACH_OK;
    }
    if ((uuid_compare(counter->entity->uuidNuggetType, entity->uuidNuggetType) == 0) &&
            (uuid_compare(counter->entity->uuidApplicationType, entity->uuidApplicationType) == 0))
    {
        counter->count++;
    }
    return LIST_EACH_OK;
}

static int
ConnectedEntityList_HookWrapper(void *item, void *userData) {
    struct ConnectedEntityHook *hook = item;
    struct CountEntity *counter = userData;
    ASSERT(hook != NULL);
    ASSERT(counter != NULL);
    if (hook == NULL || counter == NULL) {
        rzb_log(LOG_ERR, LOG_C_CNC, "%s: NULL hook or counter", __func__);
        return LIST_EACH_OK;
    }
    hook->entityRemoved(counter->entity, counter->count-1);
    return LIST_EACH_OK;
}

static int
ConnectedEntityList_PruneEntity(void *item, void *userData)
{
    struct ConnectedEntity *entity = item;
    struct CountEntity counter;
    time_t l_tTimeNow = time (NULL);
    time_t l_iDeadTime = Config_getDeadTime ();
    ASSERT(entity != NULL);
    if (entity == NULL) {
        rzb_log(LOG_ERR, LOG_C_CNC, "%s: NULL entity", __func__);
        return LIST_EACH_OK;
    }
    if ((entity->tTimeOfLastHello > 0) &&
        (l_tTimeNow - entity->tTimeOfLastHello > l_iDeadTime)) {
        counter.count = 0;
        counter.entity = entity;
        List_ForEach(sg_pEntityList, ConnectedEntityList_CountNuggets, &counter);
        List_ForEach(sg_pHookList, ConnectedEntityList_HookWrapper, &counter);
        return LIST_EACH_REMOVE;
    }
    return LIST_EACH_OK;
}

static void
ConnectedEntityList_Prune (void *userData) {
    ASSERT (sg_pEntityList != NULL);
    if (sg_pEntityList == NULL) {
        rzb_log(LOG_ERR, LOG_C_CNC, "%s: entity list not initialized", __func__);
        return;
    }

    List_ForEach(sg_pEntityList, ConnectedEntityList_PruneEntity, NULL);
}

SO_PUBLIC bool
ConnectedEntityList_AddPruneListener(void (*entityRemoved) (struct ConnectedEntity *entity, uint32_t remainingCount))
{
    struct ConnectedEntityHook *l_pHook;
    if (sg_pHookList == NULL) {
        rzb_log(LOG_ERR, LOG_C_CNC, "%s: hook list not initialized", __func__);
        return false;
    }

    if ((l_pHook = calloc(1, sizeof(struct ConnectedEntityHook))) == NULL) {
        rzb_log(LOG_ERR, LOG_C_CNC, "%s: fail to allocate new node", __func__);
        return false;
    }
    l_pHook->entityRemoved = entityRemoved;
    List_Push(sg_pHookList, l_pHook);
    return true;
}

SO_PUBLIC bool
ConnectedEntityList_ForEach_Entity (int (*function) (struct ConnectedEntity *, void *), void *userData) {
    List_ForEach(sg_pEntityList, (int (*)(void *, void *))function, userData);
    return true;
}

static struct ConnectedEntity *
ConnectedEntity_Clone(struct ConnectedEntity *orig) {
    struct ConnectedEntity *ret;
    ASSERT(orig != NULL);
    if (orig == NULL) {
        rzb_log(LOG_ERR, LOG_C_CNC, "%s: NULL original", __func__);
        return NULL;
    }

    if ((ret = calloc(1,sizeof(struct ConnectedEntity))) == NULL) {
        rzb_log(LOG_ERR, LOG_C_CNC, "%s: fail to allocate new node", __func__);
        return NULL;
    }

    memcpy(ret, orig, sizeof(struct ConnectedEntity));

    if ((ret->dispatcher = calloc(1,sizeof(struct DispatcherEntity))) == NULL) {
        rzb_log(LOG_ERR, LOG_C_CNC, "%s: fail to allocate new dispatcher", __func__);
        free(ret);
        return NULL;
    }

    memcpy(ret->dispatcher, orig->dispatcher, sizeof(struct DispatcherEntity));

    if ((ret->dispatcher->addressList = List_Clone(orig->dispatcher->addressList)) == NULL) {
        rzb_log(LOG_ERR, LOG_C_CNC, "%s: fail to clone address list", __func__);
        free(ret->dispatcher);
        free(ret);
        return NULL;
    }
    return ret;
}

SO_PUBLIC struct ConnectedEntity *
ConnectedEntityList_GetDedicatedDispatcher(void) {
    struct ConnectedEntity *ret = NULL;
    struct ConnectedEntity *node;
    struct ConnectedEntityKey key;
    ASSERT (sg_pEntityList != NULL);
    if (sg_pEntityList == NULL) {
        rzb_log (LOG_ERR, LOG_C_CNC,
                 "%s: Failed due to entity list not initialized", __func__);
        return NULL;
    }

    memset(&key, 0, sizeof(struct ConnectedEntityKey));
    key.searchKeys= SEARCH_KEY_DD;

    node = List_Find(sg_pEntityList, &key);
    if (node == NULL) {
        return NULL;
    }
    ret = ConnectedEntity_Clone(node);

    // Note ret will be locked, need to unlock it
    return ret;
}
static int
ConnectedEntityList_CollectDispatchers(void *item, void *userData) {
    struct ConnectedEntity *entity = item;
    List_t *list = userData;
    ASSERT(entity != NULL);
    if (entity == NULL) {
        rzb_log(LOG_ERR, LOG_C_CNC, "%s: NULL entity", __func__);
        return LIST_EACH_OK;
    }

    ASSERT(list != NULL);
    if (list == NULL) {
        rzb_log(LOG_ERR, LOG_C_CNC, "%s: NULL list", __func__);
        return LIST_EACH_OK;
    }

    if (ConnectedEntity_IsDispatcherType(entity->uuidNuggetType)) {
        // Todo - need to clone the dispatcher here
        List_Push(list, entity);
    }

    return LIST_EACH_OK;
}


struct ConnectedEntity *
ConnectedEntityList_GetDispatcher(void) {
    List_t *dispatchers = NULL;
    uint32_t dispatcherCount = 0;
    uint8_t locality = Config_getLocalityId();
    conf_int_t *localities = Config_getLocalityBackupOrder();
    conf_int_t localityCount = Config_getLocalityBackupCount();
    struct ConnectedEntity *entity = NULL;
    struct ConnectedEntity *ret = NULL;
    conf_int_t i;
    struct ConnectedEntityKey searchKey;

    memset(&searchKey, 0, sizeof(struct ConnectedEntityKey));

    dispatchers = List_Create(LIST_MODE_GENERIC,
            ConnectedEntity_Cmp, // Cmp
            ConnectedEntity_KeyCmp, // KeyCmp
            NULL, // destroy
            NULL, // clone
            NULL, // Lock
            NULL); //Unlock
    if (dispatchers == NULL) {
        rzb_log(LOG_ERR, LOG_C_CNC, "%s: Failed to create dispatcher list", __func__);
        return NULL;
    }

    List_ForEach(sg_pEntityList, ConnectedEntityList_CollectDispatchers, dispatchers);
    // There should be a better way to do this without reaching into the list.
    dispatcherCount = List_Length(dispatchers);
    if (dispatcherCount == 0) {
        List_Destroy(dispatchers);
        rzb_log(LOG_ERR, LOG_C_CNC, "%s: No dispatchers", __func__);
        return NULL;
    }
    // Our locality
    searchKey.locality = locality;
    searchKey.searchKeys |= SEARCH_KEY_LOCALITY;
    searchKey.searchKeys |= SEARCH_KEY_USABLE;
    entity = List_Find(dispatchers, &searchKey);
    if (entity != NULL) {
        goto getdispdone;
    }

    // Backup localities in order
    for (i = 0; i < localityCount; i++) {
        searchKey.locality = localities[i];
        entity = List_Find(dispatchers, &searchKey);
        if (entity != NULL) {
            goto getdispdone;
        }
    }
    // Random locality
    searchKey.searchKeys = SEARCH_KEY_USABLE;
    entity = List_Find(dispatchers, &searchKey);


getdispdone:
    if (entity == NULL) {
        rzb_log(LOG_ERR, LOG_C_CNC, "%s: Failed to find any usable dispatchers", __func__);
    } else {
        ret = ConnectedEntity_Clone(entity);
    }
    List_Destroy(dispatchers);
    if (ret == NULL) {
        rzb_log(LOG_ERR, LOG_C_CNC, "%s: Failed to clone dispatcher", __func__);
    }

    return ret;
}

static int
ConnectedEntityList_CollectHighDispatcher(void *item, void *userData) {
    struct ConnectedEntity *entity = item;
    struct ConnectedEntity **cur = userData;
    ASSERT(entity != NULL);
    if (entity == NULL) {
        rzb_log(LOG_ERR, LOG_C_CNC, "%s: NULL entity", __func__);
        return LIST_EACH_OK;
    }
    ASSERT(cur != NULL);
    if (cur == NULL) {
        rzb_log(LOG_ERR, LOG_C_CNC, "%s: NULL current", __func__);
        return LIST_EACH_OK;
    }

    if (ConnectedEntity_IsDispatcherType(entity->uuidNuggetType))
    {
        if (*cur == NULL)
            *cur = entity;
        else if ((*cur)->dispatcher->priority < entity->dispatcher->priority)
            *cur = entity;
    }

    return LIST_EACH_OK;
}


SO_PUBLIC struct ConnectedEntity *
ConnectedEntityList_GetDispatcher_HighestPriority() {
    struct ConnectedEntity *ret = NULL;

    ASSERT (sg_pEntityList != NULL);
    if (sg_pEntityList == NULL) {
        rzb_log (LOG_ERR, LOG_C_CNC,
                 "%s: Failed due to entity list not initialized", __func__);
        return NULL;
    }

    List_ForEach(sg_pEntityList, ConnectedEntityList_CollectHighDispatcher, &ret);
    if (ret != NULL)
        ret = ConnectedEntity_Clone(ret);

    return ret;
}

struct CE_SlaveSearch {
    uint8_t locality;
    bool found;
};
static int
ConnectedEntityList_CollectSlaveInLocality(void *item, void *userData) {
    struct ConnectedEntity *entity = item;
    struct CE_SlaveSearch *search = userData;
    // TODO - Replace with LIST_EACH_LAST if found
    if (search->found) {
        return LIST_EACH_OK;
    }

    if (ConnectedEntity_IsDispatcherType(entity->uuidNuggetType))
    {
        if(entity->locality != search->locality)
            return LIST_EACH_OK;

        if ((entity->dispatcher->flags & DISP_HELLO_FLAG_LS) != 0)
            search->found = true;
    }

    return LIST_EACH_OK;
}

SO_PUBLIC bool
ConnectedEntityList_SlaveInLocality(uint8_t locality) {
    struct CE_SlaveSearch search;
    search.locality = locality;
    search.found = false;

    List_ForEach(sg_pEntityList, ConnectedEntityList_CollectSlaveInLocality, &search);
    return search.found;
}

bool
ConnectedEntityList_MarkDispatcherUnusable(uuid_t nuggetId) {
    struct ConnectedEntity *dispatcher;
    struct ConnectedEntityKey key;
    key.searchKeys = SEARCH_KEY_NUGGET_ID;
    key.nuggetId = nuggetId;
    key.appType = NULL;
    key.nuggetType = NULL;
    dispatcher = List_Find(sg_pEntityList, &key);
    if (dispatcher == NULL) {
        return false;
    }
    dispatcher->dispatcher->usable = false;
    return true;
}

static int
ConnectedEntity_KeyCmp(void *a, const void *id) {
    struct ConnectedEntity *entity = a;
    const struct ConnectedEntityKey *key = id;
    int ret = -1;
    if ((key->searchKeys & SEARCH_KEY_NUGGET_ID) != 0) {
        ASSERT(key->nuggetId != NULL);
        if (key->nuggetId == NULL) {
            rzb_log(LOG_ERR, LOG_C_CNC, "%s: NULL nugget ID in key", __func__);
            return -1;
        }

        if (uuid_compare(entity->uuidNuggetId, key->nuggetId) == 0) {
            ret = 0;
        }
    }
    if ((key->searchKeys & SEARCH_KEY_APP_TYPE) != 0) {
        ASSERT(key->appType != NULL);
        if (key->appType == NULL) {
            rzb_log(LOG_ERR, LOG_C_CNC, "%s: NULL app type in key", __func__);
            return -1;
        }
        if (uuid_compare(entity->uuidApplicationType, key->appType) == 0) {
            ret = 0;
        } else if (ret == 0) {
            ret = -1;
        }
    }
    if ((key->searchKeys & SEARCH_KEY_NUGGET_TYPE) != 0) {
        ASSERT(key->nuggetType != NULL);
        if (key->nuggetType == NULL) {
            rzb_log(LOG_ERR, LOG_C_CNC, "%s: NULL nugget type in key", __func__);
            return -1;
        }
        if (uuid_compare(entity->uuidNuggetType, key->nuggetType) == 0) {
            ret = 0;
        } else if (ret == 0) {
            ret = -1;
        }
    }
    if ((key->searchKeys & SEARCH_KEY_DD) != 0) {
        if (entity->dispatcher != NULL) {
            if ((entity->dispatcher->flags & DISP_HELLO_FLAG_DD) != 0) {
                ret = 0;
            } else if (ret == 0) {
                ret = -1;
            }
        }
    }
    if ((key->searchKeys & SEARCH_KEY_LOCALITY) != 0) {
        if (entity->locality == key->locality) {
            ret = 0;
        } else if (ret == 0) {
            ret = -1;
        }
    }
    if ((key->searchKeys & SEARCH_KEY_USABLE) != 0) {
        if (entity->dispatcher != NULL) {
            if (entity->dispatcher->usable) {
                ret = 0;
            } else if (ret == 0) {
                ret = -1;
            }
        }
    }
    return ret;
}
static int
ConnectedEntity_Cmp(void *a, void *b) {
    if (a == b) {
        return 0;
    }

    return -1;
}

static void
ConnectedEntity_Delete(void *a) {
    ASSERT(a != NULL);
    if (a == NULL) {
        rzb_log(LOG_ERR, LOG_C_CNC, "%s: NULL entity", __func__);
        return;
    }
    ConnectedEntity_Destroy(a);
}

static int
ConnectedEntityHook_KeyCmp(void *a, const void *id) {
    (void)id;
    return -1;
}
static int
ConnectedEntityHook_Cmp(void *a, void *b) {
    if (a == b) {
        return 0;
    }

    return -1;
}

static void
ConnectedEntityHook_Delete(void *a) {
    ASSERT(a != NULL);
    if (a == NULL) {
        rzb_log(LOG_ERR, LOG_C_CNC, "%s: NULL hook", __func__);
        return;
    }
    free(a);
}

SO_PUBLIC void
ConnectedEntity_Destroy(struct ConnectedEntity *entity) {
    ASSERT(entity != NULL);
    if (entity == NULL) {
        rzb_log(LOG_ERR, LOG_C_CNC, "%s: NULL entity", __func__);
        return;
    }
    if (entity->dispatcher != NULL) {
        if (entity->dispatcher->addressList != NULL)
            List_Destroy(entity->dispatcher->addressList);

        free(entity->dispatcher);
    }
    free(entity);
}
