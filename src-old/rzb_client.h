#ifndef RZB_CLIENT_H
#define RZB_CLIENT_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include <uuid/uuid.h>

#include "rzb_api_types.h"
#include "rzb_global.h"

HRESULT nuggetServer(const char *);
HRESULT requestMD5CacheCheck(const char *addr, const char *port, const unsigned char *hash,
                             unsigned length, uuid_t datatype);
HRESULT remoteMD5CacheCheck(unsigned *eventid, unsigned char *hash, unsigned length, uuid_t datatype,
                            NetworkCacheResponse *response);

#endif /* RZB_CLIENT_H */
