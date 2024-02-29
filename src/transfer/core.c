#include "config.h"
#include <razorback/debug.h>
#include <razorback/types.h>
#include <razorback/log.h>
#include <razorback/hash.h>
#include "transfer/core.h"
#include "runtime_config.h"

struct List *sg_transportList = NULL;
static int Transport_Cmp(void *a, void *b);
static int Transport_KeyCmp(void *a, void *key);

char *
Transfer_generateFilename (struct Block *block)
{
    char *hash;
    char *filename;
    if ((hash = Hash_ToText (block->pId->pHash)) == NULL)
    {
        rzb_log (LOG_ERR, "%s: Could not convert hash to text", __func__);
        return NULL;
    }
    if (asprintf(&filename, "%s.%ju", hash, block->pId->iLength) == -1)
    {
        free(hash);
        return NULL;
    }
    free(hash);
    return filename;
}

bool
Transfer_Init(void)
{
    sg_transportList = List_Create(LIST_MODE_GENERIC,
            Transport_Cmp,
            Transport_KeyCmp,
            NULL,
            NULL, // Clone
            NULL, // Lock
            NULL); // Unlock
    if (sg_transportList == NULL)
        return false;
    if (!File_Init())
        return false;
    if (!SSH_Init())
        return false;
    return true;
}

bool 
Transport_Register(struct TransportDescriptor *desc)
{
    return List_Push(sg_transportList, desc);
}

bool 
Transport_IsSupported(uint8_t protocol)
{
    struct TransportDescriptor *trans= NULL;
    trans = List_Find(sg_transportList, &protocol);
    return !(trans == NULL);
}

bool 
Transfer_Store(struct BlockPoolItem *item, struct ConnectedEntity *dispatcher)
{
    struct TransportDescriptor *trans = NULL;
    uint8_t file = 0;
    if (dispatcher->locality == Config_getLocalityId()) // Same locality always use file
    {
        trans = List_Find(sg_transportList, &file); 
    }
    else 
        trans = List_Find(sg_transportList, &dispatcher->dispatcher->protocol);
    if (trans == NULL)
    {
        return false;
    }
    //rzb_log(LOG_ERR, "%s: locality: %u, protocol: %u", __func__, dispatcher->locality, dispatcher->dispatcher->protocol);
    //rzb_log(LOG_ERR, "%s: Transport: %s", __func__, trans->name);
    return trans->store(item, dispatcher);
}

bool 
Transfer_Fetch(struct Block *block, struct ConnectedEntity *dispatcher)
{
    struct TransportDescriptor *trans = NULL;
    uint8_t file = 0;
    if (dispatcher->locality == Config_getLocalityId()) // Same locality always use file
        trans = List_Find(sg_transportList, &file); 
    else 
        trans = List_Find(sg_transportList, &dispatcher->dispatcher->protocol);
    if (trans == NULL)
        return false;
    //rzb_log(LOG_ERR, "%s: locality: %u, protocol: %u", __func__, dispatcher->locality, dispatcher->dispatcher->protocol);
    //rzb_log(LOG_ERR, "%s: Transport: %s", __func__, trans->name);
    return trans->fetch(block, dispatcher);
}

void 
Transfer_Free(struct Block *block, struct ConnectedEntity *dispatcher)
{
    struct TransportDescriptor *trans = NULL;
    uint8_t file = 0;
    if (dispatcher->locality == Config_getLocalityId()) // Same locality always use file
        trans = List_Find(sg_transportList, &file); 
    else 
        trans = List_Find(sg_transportList, &dispatcher->dispatcher->protocol);
    if (trans == NULL)
        return;
    trans->free(block);
}

static int 
Transport_Cmp(void *a, void *b)
{
    struct TransportDescriptor *dA = a;
    struct TransportDescriptor *dB = b;
    if (dA == dB)
        return 0;
    return (dA->id - dB->id);
}
static int 
Transport_KeyCmp(void *a, void *key)
{
    struct TransportDescriptor *dA = a;
    uint8_t *id = key;
    return (dA->id - *id);
}

