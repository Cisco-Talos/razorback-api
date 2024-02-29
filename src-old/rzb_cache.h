#ifndef RZB_CACHE_H
#define RZB_CACHE_H

#include "rzb_api_types.h"

typedef enum
{
    LT_T1,
    LT_T2,
    LT_B1,
    LT_B2,
    LT_NONE
} LISTTYPE;

typedef enum
{
    GOODMD5,
    BADMD5,
    URL
} CACHETYPE;

typedef struct _ENTRY
{
    struct _ENTRY *next;
    struct _ENTRY *prev;
    LISTTYPE listtype;
    unsigned size;
    union
    {
        uint8_t chksum[RZB_HASH_SIZE];
        char *url;
    } d;
} ENTRY;

HRESULT addLocalEntry(ENTRY *, CACHETYPE);
HRESULT checkLocalEntry(ENTRY *, CACHETYPE);
void finicache(void);

#endif
