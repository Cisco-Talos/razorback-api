/** @file rzb_thread.h
 * Razorback Threading Utilities
 */
#ifndef _RZB_THREAD_H
#define _RZB_THREAD_H

#include "rzb_utils_types.h"
#include "rzb_network.h"

/*
 *
 * THREADING INFO
 *
 */
struct _THREADARGS;
typedef void *(*rzb_thread_func_t)(struct _THREADARGS *);
typedef struct _THREADARGS
{
    struct _THREADARGS *next;
    RZBNetworkConnection *net;
    unsigned threadindex;
    unsigned nuggetid;
    unsigned eventid;
    unsigned char *data;
    unsigned size;
    uuid_t type;
    BLOCK_META_DATA *metaData;
    const char *ta_description;
} THREADARGS;

void unthreadme(THREADARGS *threadargs);
HRESULT threadme(rzb_thread_func_t fp, THREADARGS *threadargs, const char *description);
unsigned getActiveTheadCount(void);
HRESULT waitForIdle(unsigned to_secs);
void dumpActiveTheads(void);

#endif

