/** @file rzb_utils_types.h
 * Razorback Utils Library types.
 */
#ifndef RZB_UTILS_TYPES_H
#define RZB_UTILS_TYPES_H

#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#if defined(__FreeBSD__)
#include <sys/socket.h>
#endif
#include <sys/types.h>
#include <unistd.h>
#include <uuid/uuid.h>

#define RZB_HASH_SIZE    16

/** Standard Return Codes
 */
typedef enum
{
    R_NOT_FOUND = 333,
    R_FOUND = 444,  
    R_SUCCESS = 555,
    R_FAIL = -1,
    R_MALLOC_FAIL = -2,
    R_ADD_FAIL = -3,
    R_DB_FAIL = -4,
    R_DUP_UID = -5,
    R_NO_ROUTE_TABLE = 4141,
    R_BUSY = -53,
} HRESULT;

/*
 *
 * NETWORK INFO
 *
 */
/** IP Address structure capiable of holding an IPv4 or IPv6 address.
 */
typedef struct _RZB_IP
{
    sa_family_t family;
    union
    {
        struct in_addr ipv4;
        struct in6_addr ipv6;
    } ip;
} RZB_IP;

/** Metadata for a data block
 */
typedef struct _BLOCK_META_DATA
{
    unsigned eventid;
    unsigned timestamp;           // Seconds since epoch
    uint8_t *data;
    unsigned size;
    uuid_t datatype;
    unsigned char parent_hash[RZB_HASH_SIZE];      // If this is 00
    void *parent_data;        // "NULL" means this is top-level datablock
    unsigned parent_size;         // Ignored if parent_data is "NULL"
    RZB_IP src_ip;
    RZB_IP dst_ip;
    unsigned short ip_proto;            // Optional
    unsigned short src_port;            // Optional
    unsigned short dst_port;            // Optional
} BLOCK_META_DATA;

#endif /* RZB_UTILS_TYPES_H */
