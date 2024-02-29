#include "config.h"

#include <errno.h>
#include <stdio.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <openssl/evp.h>
#include <uuid/uuid.h>
#include <arpa/inet.h>

#include "rzb_alert_api.h"
#include "rzb_api_types.h"
#include "rzb_conf.h"
#include "rzb_log.h"
#include "rzb_network.h"
#include "rzb_alert_util.h"

/* Private API Functions */
static HRESULT handleSendBlob(RZBNetworkConnection *net, ALERT *alert);
static HRESULT buildAlertHeader(ALERT_HEADER *header, ALERT *alert);
static HRESULT waitForServerResponse(RZBNetworkConnection *net, ALERT *data);
static HRESULT sendAlertPacket(ALERT *alert, ALERT_HEADER *header, RZBNetworkConnection **net);

// PUBLIC API ENTRIES //

/* sendAlert
 * Public API to detection and correlation nuggets to send alerts to dispatcher
 *
 * IN:   *server_ip, *server_port, *ALERT
 * OUT:  HRESULT
 * NOTE: Return is either failure from buildAlertHeader/sendAlertPacket or
 *       the result of the waitForServerResponse function, that handles
 *       additional requests from the server.
 * NOTE: Client is responsible for freeing all passed pointers.
*/
HRESULT getAlertData(unsigned alertID, unsigned blob_type,
                     unsigned char **data, unsigned *size)
{
    SNAKECHARMER   scpacket;
    BLOB_REQUEST   request;
    BLOB_HEADER    header;
    unsigned char *tmp_data = NULL;

    protocolFill(&scpacket);
    scpacket.msgtype = REQBLOB;
    request.alertID = alertID;
    request.blob_type = blob_type;

    *data = NULL;

    RZBNetworkConnection *net = ConnectMe(rzbconfig.dsrvaddr, rzbconfig.dsrvport);
    if (!net)
    {
        rzb_log(LOG_ERR, "connection to %s:%s failed", rzbconfig.dsrvaddr, rzbconfig.dsrvport);
        return R_FAIL;
    }

    if (sendWrap(net, &scpacket, sizeof(scpacket)) != R_SUCCESS)
    {
        rzb_log(LOG_EMERG, "Failed scpacket");
        NetworkConnectionDestroy(net);
        return R_FAIL;
    }
    if (sendWrap(net, &request, sizeof(request)) != R_SUCCESS)
    {
        rzb_log(LOG_EMERG, "Failed request");
        NetworkConnectionDestroy(net);
        return R_FAIL;
    }
    if (recvWrap(net, &header, sizeof(header)) != R_SUCCESS)
    {
        rzb_log(LOG_EMERG, "Failed header");
        NetworkConnectionDestroy(net);
        return R_FAIL;
    }


    if ((tmp_data = malloc(header.blobSize + 1)) == NULL)
    {
        NetworkConnectionDestroy(net);
        return R_MALLOC_FAIL;
    }

    *size = header.blobSize;

    if (recvWrap(net, tmp_data, *size) != R_SUCCESS)
    {
        rzb_log(LOG_ERR, "Failed data");
        acknowledge(net, &scpacket);
        free(tmp_data);
        NetworkConnectionDestroy(net);
        *data = NULL;
        return R_FAIL;
    }

    tmp_data[header.blobSize] = 0;
    acknowledge(net, &scpacket);
    *data = tmp_data;
    NetworkConnectionDestroy(net);
    return R_SUCCESS;
}

HRESULT sendAlert(ALERT *alert)
{
    ALERT_HEADER header;
    HRESULT status;
    RZBNetworkConnection *net;

    if ((buildAlertHeader(&header, alert)) != R_SUCCESS)
        return R_FAIL;
    if ((sendAlertPacket(alert, &header, &net)) != R_SUCCESS)
        return R_FAIL;

    status = waitForServerResponse(net, alert);
    NetworkConnectionDestroy(net);

    return status;
}

HRESULT deliverJudgement(JUDGEMENT *verdict)
{
    SNAKECHARMER scpacket;
    RZBNetworkConnection *net = ConnectMe(rzbconfig.dsrvaddr, rzbconfig.dsrvport);
    if (!net)
    {
        rzb_log(LOG_ERR, "connection to %s:%s failed", rzbconfig.dsrvaddr, rzbconfig.dsrvport);
        return R_FAIL;
    }

    protocolFill(&scpacket);
    scpacket.msgtype = VERDICT;

    if (sendWrap(net, &scpacket, sizeof(scpacket)) != R_SUCCESS)
    {
        NetworkConnectionDestroy(net);
        return R_FAIL;
    }

    if (sendWrap(net, verdict, sizeof(*verdict)) != R_SUCCESS)
    {
        NetworkConnectionDestroy(net);
        return R_FAIL;
    }

    if (recvWrap(net, &scpacket, sizeof(scpacket)) == R_SUCCESS)
    {
        NetworkConnectionDestroy(net);
        if (scpacket.msgtype == RST)
            return R_FAIL;
        else
            return R_SUCCESS;
    }
    NetworkConnectionDestroy(net);
    return R_FAIL;
}

// PRIVATE API ENTRIES //

/* sendAlertPacket
 * Private API to send the initial alert packet.
 *
 * IN:   *ALERT, *ALERT_HEADER, *server_IP, *server_port, *unsigned
 * OUT:  HRESULT
 * NOTE: int passed by reference is the network socket
 *
*/
static HRESULT sendAlertPacket(ALERT *alert, ALERT_HEADER *header, RZBNetworkConnection **net)
{
    SNAKECHARMER scpacket;
    RZBNetworkConnection *tmp;

    // Build snake charmer packet
    tmp = ConnectMe(rzbconfig.dsrvaddr, rzbconfig.dsrvport);
    if (!tmp)
        return R_FAIL;
    //memcpy(scpacket->nugtype, "\xd9\x5a\xee\x72\x91\x86\x42\x36\xbf\x23\x8f\xf7\x7d\xac\x63", 16);
    uuid_copy(scpacket.datatype, alert->dataType);
    memcpy(scpacket.magic, "NRT", 3);
    scpacket.version = 1;
    scpacket.msgtype = ALRT;

    // Unless all phases of the alert transmission are successful, return failed.
    if (sendWrap(tmp, &scpacket, sizeof(scpacket)) == R_SUCCESS)
    {
        if (sendWrap(tmp, header, sizeof(*header)) == R_SUCCESS)
        {
            if (sendWrap(tmp, &alert->msg_size, sizeof(alert->msg_size)) == R_SUCCESS)
            {
                if (sendWrap(tmp, alert->msg, alert->msg_size) == R_SUCCESS)
                {
                    if (recvWrap(tmp, &scpacket, sizeof(scpacket)) == R_SUCCESS)
                    {
                        if (scpacket.msgtype != RST)
                        {
                            *net = tmp;
                            return R_SUCCESS;
                        }
                    }
                }
            }
        }
    }

    NetworkConnectionDestroy(tmp);
    return R_FAIL;
}

/* buildAlertHeader
 * Private API to build the initail portion of the alert notification.
 *
 * IN:   **ALERT_HEADER, *ALERT
 * OUT:  HRESULT
 * NOTE:  The * ALERT_HEADER structure only contains fixed-lenghth data.
 * Variable length data is handled separately.
 *
*/
static HRESULT buildAlertHeader(ALERT_HEADER *header, ALERT *alert)
{
    // Copy over standard data
    header->event_id = alert->event_id;   //XXX Need to get the real number
    header->nugget_id = 1;  //XXX Need to get the real number
    header->timestamp = (unsigned)time(NULL);
    header->priority = alert->priority;

    // Translate IP info and populate structure
    if (inet_pton(AF_INET, alert->src_ip, &header->src_ip.ip) == 1)
        header->src_ip.family = AF_INET;
    else if (inet_pton(AF_INET6, alert->src_ip, &header->src_ip.ip) == 1)
        header->src_ip.family = AF_INET6;
    else
        return R_FAIL;
    if (inet_pton(AF_INET, alert->dst_ip, &header->dst_ip.ip) == 1)
        header->dst_ip.family = AF_INET;
    else if (inet_pton(AF_INET6, alert->dst_ip, &header->dst_ip.ip) == 1)
        header->dst_ip.family = AF_INET6;
    else
        return R_FAIL;
    header->timestamp = (unsigned)time(NULL);
    header->priority = alert->priority;
    header->ip_proto = alert->ip_proto;
    header->src_port = alert->src_port;
    header->dst_port = alert->dst_port;

    // Zero out flags and md5sums
    header->flags1 = 0;
    header->flags2 = 0;

    // Set flags to indicate to the dispatch server if
    // additional data beyond the required is available
    if (alert->sd_size != 0 && alert->short_data != NULL)
        header->flags1 |= SHORT_DATA;
    if (alert->ld_size != 0 && alert->long_data != NULL)
        header->flags1 |= LONG_DATA;

    // Copy over the data type and main component hash
    uuid_copy(header->dataType, alert->dataType);
    memcpy(header->main_hash, alert->main_hash, sizeof(header->main_hash));

    // Data blocks that will be stored as components require flags and hash/size duple
    if ((alert->db_size != 0) && (alert->data_block != NULL))
    {
        md5sum(alert->data_block, alert->db_size, header->db_hash);
        header->db_size = alert->db_size;
        header->flags1 |= FULL_BLOCK;
    }
    if ((alert->norm_size != 0) && (alert->norm_block != NULL))
    {
        md5sum(alert->norm_block, alert->norm_size, header->norm_hash);
        header->norm_size = alert->norm_size;
        header->flags1 |= NORM_BLOCK;
    }

    // Enforce size limits on short data and message
    if (alert->sd_size > MAX_SHORT_SIZE)
        alert->sd_size = MAX_SHORT_SIZE;
    if (alert->msg_size > MAX_MSG_SIZE)
        alert->msg_size = MAX_MSG_SIZE;

    return R_SUCCESS;
}

/* waitForServerResponse
 * Loop to handle additional requests from the dispatcher
 * for information.
 *
 * IN:   unsigned, *ALERT
 * OUT:  HRESULT
 * NOTE: RST packet means data complete success
 *       ERR means failure at some point on the server side
 *       REQBLOB requests additional data
*/
static HRESULT waitForServerResponse(RZBNetworkConnection *net, ALERT *data)
{
    SNAKECHARMER scpacket;

    for (;;)
    {
        // Wait for next instruction
        if (recvWrap(net, &scpacket, sizeof(scpacket)) == R_FAIL)
            return R_FAIL;

        // Process instructions;
        switch (scpacket.msgtype)
        {
            case ACK:
                break;
            case ERR:
                rzb_log(LOG_ERR, "Error occured");
                return R_FAIL;
            case REQBLOB:
                if ((handleSendBlob(net, data)) == R_FAIL)
                {
                    rzb_log(LOG_ERR, "Failed to handle blob request: %s", strerror(errno));
                    return R_FAIL;
                }
                break;
            case RST:
                return R_SUCCESS;
            default:
                rzb_log(LOG_ERR, "Invalid packet type: %s", strerror(errno));
                return R_FAIL;
        }
    }
}

/* handleSendBlob
 * Handle reqeuts from the dispatch server for optional data.
 *
 * IN:   unsigned, *ALERT
 * OUT:  HRESULT
 * NOTE  This function sends Short_data, Long_data, data blocks and
 *       normalized data blocks.
*/
static HRESULT handleSendBlob(RZBNetworkConnection *net, ALERT *alert)
{
    SNAKECHARMER scpacket;
    BLOB_REQUEST req;
    BLOB_HEADER resp;
    const char *data;

    protocolFill(&scpacket);
    scpacket.msgtype = SENDBLOB;
    if (recvWrap(net, &req, sizeof(req)) == R_SUCCESS)
    {
        if (sendWrap(net, &scpacket, sizeof(scpacket)) != R_SUCCESS)
        {
            rzb_log(LOG_ERR, "Failed to send snakecharm");
            return R_FAIL;
        }
    }
    else
    {
        rzb_log(LOG_ERR, "Failed to receive blob request packet");
        return R_FAIL;
    }

    switch (req.blob_type)
    {
        case SHORT_DATA:
            resp.blobSize = alert->sd_size;
            data = alert->short_data;
            break;
        case LONG_DATA:
            resp.blobSize = alert->ld_size;
            data = alert->long_data;
            break;
        case FULL_BLOCK:
            resp.blobSize = alert->db_size;
            data = alert->data_block;
            break;
        case NORM_BLOCK:
            resp.blobSize = alert->norm_size;
            data = alert->norm_block;
            break;
        default:
            rzb_log(LOG_ERR, "In handleSendBlob, incorrect data type: %u", req.blob_type);
            return R_FAIL;
    }

    if (sendWrap(net, &resp, sizeof(resp)) == R_SUCCESS)
    {
        if (sendWrap(net, data, resp.blobSize) == R_SUCCESS)
        {
            if (recvWrap(net, &scpacket, sizeof(scpacket)) == R_SUCCESS)
            {
                if (scpacket.msgtype != ACK)
                {
                    rzb_log(LOG_ERR, "Non-Ack message received: %u", scpacket.msgtype);
                    return R_FAIL;
                }
                return R_SUCCESS;
            }
        }
    }

    return R_FAIL;
}

