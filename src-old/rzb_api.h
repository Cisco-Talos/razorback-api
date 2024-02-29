#ifndef __RZB_API_H__
#define __RZB_API_H__

#include <uuid/uuid.h>
#include "rzb_api_types.h"
#include "rzb_thread.h"

typedef struct _handlerNode
{
    void (*fp)(BLOCK_META_DATA *metaData);
    uuid_t *acceptedTypes;
    size_t numTypes;
    uuid_t functionid;
    struct _handlerNode *next;
} handlerNode;

extern handlerNode *head;

HRESULT rzbServer(const char *port, rzb_thread_func_t func, unsigned to_secs, const char *description);

#endif  /* __RZB_API_H__ */

