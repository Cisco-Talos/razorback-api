#ifndef NRT_GLOBAL_H
#define NRT_GLOBAL_H

#if 0
#include <stdio.h>

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/mman.h>
#endif
#include <limits.h>
#include <netinet/in.h>
#include <uuid/uuid.h>

#include "rzb_api_types.h"

#define MAX_DESCRIPTION_SIZE 1048

#ifndef ULONG_MAX
    #define ULONG_MAX   0xFFFFFFFF //4294967295UL // ULONG_MAX from limits.h
#endif

/* Network Cache Responsees */
typedef enum
{
    KNOWN_GOOD_RESPONSE = 2,
    KNOWN_BAD_RESPONSE = 1,
    UNKNOWN_MD5_RESPONSE = 0,
} NetworkCacheResponse;

/* Network Message Size Defines */
#define MAX_IP_SIZE           16   /* vvv.www.xxx.yyy + \x00 */

#define SAFE_MD5_SIZE   33

typedef enum
{
    CAP_DETAILS
} METATYPE;

/*
 *
 * PROTOCOL
 *
 */

typedef struct _REGPACKET
{
    unsigned nuggetid;
    struct in_addr nugaddr;
    unsigned short nugport;
    uuid_t nugtype;
    uuid_t nugapp;
    uuid_t datatype;
    unsigned short dummy;
    unsigned freeinstances;
    char name[256];
} REGPACKET;

typedef struct _NEWCAPPACKET
{
    unsigned nuggetid;
    struct in_addr nugaddr;
    unsigned short nugport;
    uuid_t nugtype;
    uuid_t nugapp;
    uuid_t datatype;
} NEWCAPPACKET;

// The parent_hash is used to track sub-components in the database.
// For example, if we receive a PDF file hash:a, and it has a JavaScript
// block hash:b and that block has a shellcode block hash:c, the transit would be
// as follows:
// reqpacket: hash:a, p_hash:a <-- Matching hash/p_hash means this is a top-level block
// reqpacket: hash:b, p_hash:a <-- The javascript is a child to the PDF
// regpacket: hash:c, p_hash:b <-- The shellcode is a child to the JavaScript.
// In this way, from the database we can describe relationships between components:
//
// a <- b <- c
//
// If we had another PDF file hash:d, with the same javascript block, we could then
// describe the relationships as:
//
// a <-|
//     |--b <- c
// d <-|
//
// This is useful from a forensics and Intelligence Driven Response perspective
typedef struct _REQPACKET
{
    unsigned      eventid;             // Temporary id, resoved by dispatcher on alert
    unsigned      timestamp;           // Seconds since epoch
    unsigned char     hash[RZB_HASH_SIZE];             // Hash of current block
    unsigned      filesize;
    uuid_t            datatype;
    unsigned char     parent_hash[RZB_HASH_SIZE];      // MD5 of parent block
    unsigned      parent_size;
    RZB_IP      src_ip;
    RZB_IP      dst_ip;
    unsigned short    ip_proto;            // Optional
    unsigned short    src_port;            // Optional
    unsigned short    dst_port;            // Optional
} REQPACKET;

typedef struct _MD5CACHEREQUEST
{
    unsigned filesize;
    unsigned char hash[RZB_HASH_SIZE];
    uuid_t datatype;
} MD5CACHEREQUEST;

typedef struct _MD5CACHERESPONSE
{
    uint32_t result;
    uint32_t detectionID;
    uint32_t referralcount;
} MD5CACHERESPONSE;

#endif /* NRT_GLOBAL_H */
