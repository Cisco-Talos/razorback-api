#include "config.h"

#include <stdio.h>
#include <netinet/in.h>
#include <uuid/uuid.h>
#include <arpa/inet.h>

#include "rzb_workstation_global.h"
#include "rzb_workstation_api.h"
#include "rzb_conf.h"
#include "rzb_network.h"

static HRESULT getNuggetEntry(RZBNetworkConnection *net, WKST_DATA_RESPONSE *resp, ROUTE_ENTRY_BLOCK *re);
static HRESULT getAppEntry(RZBNetworkConnection *net, WKST_DATA_RESPONSE *resp, APP_ENTRY_BLOCK *ae);
static HRESULT getDataEntry(RZBNetworkConnection *net, WKST_DATA_RESPONSE *resp, DATA_ENTRY_BLOCK *de);

HRESULT getLogsByNum(LOG_BLOCK **logs, unsigned num)
{
    SNAKECHARMER scpacket;
    WKST_REQ_TYPE req;
    LOG_REQUEST_PACKET log_req;
    LOG_RESPONSE_PACKET resp;
    LOG_BLOCK *block;
    LOG_BLOCK **next_block;
    RZBNetworkConnection *net;
    HRESULT rval;

    *logs = NULL;

    protocolFill(&scpacket);
    scpacket.msgtype = WKST;
    uuid_copy(scpacket.datatype, NO_DATA_TYPE);

    req.msg_type = SHOW_LOGS;
    log_req.type = LOGS_BY_NUMBER;
    log_req.data = num;

    net = ConnectMe(rzbconfig.dsrvaddr, rzbconfig.dsrvport);
    if (!net)
    {
        printf("connection to %s:%s failed\n", rzbconfig.dsrvaddr, rzbconfig.dsrvport);
        return R_FAIL;
    }

    if (sendWrap(net, &scpacket, sizeof(scpacket)) != R_SUCCESS)
    {
        NetworkConnectionDestroy(net);
        return R_FAIL;
    }
    if (sendWrap(net, &req, sizeof(req)) != R_SUCCESS)
    {
        NetworkConnectionDestroy(net);
        return R_FAIL;
    }
    if (sendWrap(net, &log_req, sizeof(log_req)) != R_SUCCESS)
    {
        NetworkConnectionDestroy(net);
        return R_FAIL;
    }

    if (recvWrap(net, &resp, sizeof(resp)) != R_SUCCESS)
    {
        NetworkConnectionDestroy(net);
        return R_FAIL;
    }

    next_block = logs;
    while (resp.id != 0)
    {
        rval = R_FAIL;
        block = calloc(1, sizeof(*block));
        if ((block = calloc(1, sizeof(*block))) == NULL)
            break;
        *next_block = block;
        next_block = &block->next;
        block->id = resp.id;
        block->timestamp = resp.timestamp;
        block->level = resp.level;
        if ((block->src = malloc(resp.src_size+1)) == NULL)
            break;
        if ((block->msg = malloc(resp.msg_size+1)) == NULL)
            break;
        if (recvWrap(net, block->src, resp.src_size) != R_SUCCESS)
            break;
        block->src[resp.src_size] = '\x00';
        if (recvWrap(net, block->msg, resp.msg_size) != R_SUCCESS)
            break;
        block->msg[resp.msg_size] = '\x00';
        if (recvWrap(net, &resp, sizeof(resp)) != R_SUCCESS)
            break;

        rval = R_SUCCESS;
    }

    if (rval != R_SUCCESS)
    {
        while ((block = *logs))
        {
            *logs = block->next;
            if (block->src)
                free(block->src);
            if (block->msg)
                free(block->msg);
            free(block);
        }
    }
    NetworkConnectionDestroy(net);
    return rval;
}

static void freeRouteEntryBlock(ROUTE_ENTRY_BLOCK *re)
{
    if (re->name)
        free(re->name);
    free(re);
}

static void freeAppEntryBlock(APP_ENTRY_BLOCK *ae)
{
    ROUTE_ENTRY_BLOCK *re;

    if (ae->name)
        free(ae->name);
    while ((re = ae->nug_list))
    {
        ae->nug_list = re->next;
        freeRouteEntryBlock(re);
    }
    free(ae);
}

static void freeDataBlockEntry(DATA_ENTRY_BLOCK *de)
{
    APP_ENTRY_BLOCK *ae;

    if (de->name)
        free(de->name);
    while ((ae = de->app_list))
    {
        de->app_list = ae->next;
        freeAppEntryBlock(ae);
    }
    free(de);
}

HRESULT getRouteTable(DATA_ENTRY_BLOCK **route_table)
{
    SNAKECHARMER scpacket;
    WKST_REQ_TYPE req;
    WKST_DATA_RESPONSE resp;
    DATA_ENTRY_BLOCK *de;
    RZBNetworkConnection *net;

    *route_table = NULL;

    protocolFill(&scpacket);
    scpacket.msgtype = WKST;
    uuid_copy(scpacket.datatype, NO_DATA_TYPE);

    /* WKST TYPE */
    req.msg_type = SHOW_ROUTING_TABLE;

    printf("Getting route table for %s:%s\n", rzbconfig.dsrvaddr, rzbconfig.dsrvport);
    net = ConnectMe(rzbconfig.dsrvaddr, rzbconfig.dsrvport);
    if (!net)
    {
        printf("connection to %s:%s failed\n", rzbconfig.dsrvaddr, rzbconfig.dsrvport);
        return R_FAIL;
    }

    /* ASK FOR THE ROUTE TABLE */

    if (sendWrap(net, &scpacket, sizeof(scpacket)) != R_SUCCESS)
    {
        printf("Failed snedwrap of sc");
        NetworkConnectionDestroy(net);
        return R_FAIL;
    }

    if (sendWrap(net, &req, sizeof(req)) != R_SUCCESS)
    {
        printf("Failed sendWrap of req\n");
        NetworkConnectionDestroy(net);
        return R_FAIL;
    }

    if (recvWrap(net, &resp, sizeof(resp)) != R_SUCCESS)
    {
        printf("Failed recvwrap of resp\n");
        NetworkConnectionDestroy(net);
        return R_FAIL;
    }

    if (resp.resp_type != ROUTE_TABLE_START)
    {
        if (resp.resp_type == NO_ROUTE_TABLE)
        {
            NetworkConnectionDestroy(net);
            return R_NO_ROUTE_TABLE;
        }
        else
        {
            printf("Response type not route table start\n");
            NetworkConnectionDestroy(net);
            return R_FAIL;
        }
    }

    printf("Receiving route table\n");
    if (recvWrap(net, &resp, sizeof(resp)) != R_SUCCESS)
    {
        NetworkConnectionDestroy(net);
        return R_FAIL;
    }

    de = calloc(1, sizeof(*de));
    if (de == NULL)
        return R_MALLOC_FAIL;

    *route_table = de;

    while (resp.resp_type == ROUTE_DATA_ENTRY)
    {
        printf("Route data entry...\n");
        if (getDataEntry(net, &resp, de) != R_SUCCESS)
        {
            while ((de = *route_table))
            {
                *route_table = de->next;
                freeDataBlockEntry(de);
            }
            NetworkConnectionDestroy(net);
            return R_FAIL;
        }
        de = de->next;
    }
    NetworkConnectionDestroy(net);
    return R_SUCCESS;
}

static HRESULT getDataEntry(RZBNetworkConnection *net, WKST_DATA_RESPONSE *resp, DATA_ENTRY_BLOCK *de)
{
    APP_ENTRY_BLOCK *ae;
    ROUTE_HEADER_PACKET data;

    if (recvWrap(net, &data, sizeof(data)) != R_SUCCESS)
    {
        printf("Failed to receive data entry\n");
        return R_FAIL;
    }

    de->name = strdup(data.dt_name);

    uuid_unparse(data.datatype, de->datatype);

    if (recvWrap(net, resp, sizeof(*resp)) != R_SUCCESS)
    {
        printf("Failed to receive next data response after data entry\n");
        return R_FAIL;
    }

    if ((de->app_list = ae = calloc(1, sizeof(*ae))) == NULL)
        return R_FAIL;

    while (resp->resp_type == ROUTE_APP_ENTRY)
    {
        if (getAppEntry(net, resp, ae) != R_SUCCESS)
        {
            printf("Failed in getAppEntry\n");
            while ((ae = de->app_list))
            {
                de->app_list = ae->next;
                freeAppEntryBlock(ae);
            }
            return R_FAIL;
        }
        ae = ae->next;
    }
    if (resp->resp_type == ROUTE_DATA_ENTRY && (de->next = calloc(1, sizeof(*de->next))) == NULL)
        return R_FAIL;

    return R_SUCCESS;
}

static HRESULT getAppEntry(RZBNetworkConnection *net, WKST_DATA_RESPONSE *resp, APP_ENTRY_BLOCK *ae)
{
    ROUTE_ENTRY_BLOCK *re;
    ROUTE_HEADER_PACKET data;

    if (recvWrap(net, &data, sizeof(data)) != R_SUCCESS)
    {
        printf("failed to receive route-header-packet\n");
        return R_FAIL;
    }

    ae->name = strdup(data.dt_name);
    uuid_unparse(data.datatype, ae->apptype);

    if (recvWrap(net, resp, sizeof(*resp)) != R_SUCCESS)
    {
        printf("Failed to get wkst_data_response in getAppEntry\n");
        return R_FAIL;
    }

    if ((ae->nug_list = re = calloc(1, sizeof(ROUTE_ENTRY_BLOCK))) == NULL)
        return R_FAIL;

    while (resp->resp_type == ROUTE_NUGGET_ENTRY)
    {
        if (getNuggetEntry(net, resp, re) != R_SUCCESS)
        {
            while ((re = ae->nug_list))
            {
                ae->nug_list = re->next;
                freeRouteEntryBlock(re);
            }
            return R_FAIL;
        }
        re = re->next;
    }

    if (resp->resp_type == ROUTE_APP_ENTRY && (ae->next = calloc(1, sizeof(*ae->next))) == NULL)
        return R_FAIL;

    return R_SUCCESS;
}

static HRESULT getNuggetEntry(RZBNetworkConnection *net, WKST_DATA_RESPONSE *resp, ROUTE_ENTRY_BLOCK *re)
{
    ROUTE_NUGGET_PACKET data;

    if (recvWrap(net, &data, sizeof(data)) != R_SUCCESS)
    {
        printf("Failed to get route nugget packet\n");
        return R_FAIL;
    }

    if (recvWrap(net, resp, sizeof(*resp)) != R_SUCCESS)
    {
        printf("Failed to get response packet in getNuggetEntry\n");
        return R_FAIL;
    }

    re->nugid = data.nugid;
    re->nugport = data.nugport;
    inet_ntop(AF_INET, &data.nugaddr, re->nugaddr, sizeof(re->nugaddr));
    re->name = strdup(data.name);
    if (resp->resp_type == ROUTE_NUGGET_ENTRY && (re->next = calloc(1, sizeof(*re->next))) == NULL)
        return R_FAIL;

    return R_SUCCESS;
}

