#ifndef RZB_WORKSTATION_GLOBAL_H
#define RZB_WORKSTATION_GLOBAL_H

#include <arpa/inet.h>
#include <sys/types.h>

#include "rzb_api_types.h"
#include "rzb_global.h"

/* Default Sizes */
#define INIT_ROUTE_TABLE_SIZE = 1000;

/* Structures to support initial requests */
typedef enum
{
    ADD_DATA_TYPE,
    ADD_APP_TYPE,
    AUTH_NUGGET,
    SHOW_ROUTING_TABLE,
    SHOW_AUTH_DATA_TYPES,
    SHOW_AUTH_NUGGET_TYPES,
    SHOW_NUG_INFO,
    SHOW_LOGS
} WKST_MESSAGE;

typedef enum
{
    NO_ROUTE_TABLE,
    ROUTE_TABLE_START,
    ROUTE_TABLE_END,
    ROUTE_DATA_ENTRY,
    ROUTE_APP_ENTRY,
    ROUTE_NUGGET_ENTRY
} WKST_RESPONSE;

typedef enum
{
    LOGS_BY_NUMBER,
    LOGS_BY_FUNCTION,
    LOGS_BY_SEVERITY,
    ALL_LOGS
} LOG_TYPES;

typedef struct _LOG_REQUEST_PACKET
{
    unsigned type;
    unsigned data;
} LOG_REQUEST_PACKET;

typedef struct _LOG_RESPONSE_PACKET
{
    unsigned id;
    unsigned timestamp;
    unsigned level;
    unsigned src_size;
    unsigned msg_size;
} LOG_RESPONSE_PACKET;

typedef struct _WKST_REQ_TYPE
{
    unsigned msg_type;
} WKST_REQ_TYPE;

typedef struct _WKST_DATA_RESPONSE
{
    unsigned resp_type;
    unsigned resp_size;
} WKST_DATA_RESPONSE;

typedef struct _ROUTE_HEADER_PACKET
{
    uuid_t datatype;
    char dt_name[MAX_DESCRIPTION_SIZE];
} ROUTE_HEADER_PACKET;

typedef struct _ROUTE_NUGGET_PACKET
{
    unsigned nugid;
    struct in_addr nugaddr;
    unsigned short nugport;
    char name[MAX_DESCRIPTION_SIZE];
} ROUTE_NUGGET_PACKET;

HRESULT getRouteTable(DATA_ENTRY_BLOCK **route_table);
HRESULT getLogsByNum(LOG_BLOCK **logs, unsigned num);

#endif /* RZB_WORKSTATION_GLOBAL_H */

