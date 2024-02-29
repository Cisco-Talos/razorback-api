#include "config.h"

#include <netinet/in.h>
#include <arpa/inet.h>
#include <uuid/uuid.h>
#include <arpa/inet.h>

#include "rzb_intel_global.h"
#include "rzb_network.h"
#include "rzb_conf.h"
#include "rzb_alert_util.h"

static INLINE HRESULT sendOptionalData(RZBNetworkConnection *net, const char *data, unsigned size)
{
    if (size == 0)
        return R_SUCCESS;
    if (sendWrap(net, data, size) != R_SUCCESS)
        return R_FAIL;

    return R_SUCCESS;
}

static INLINE HRESULT sendBlob(RZBNetworkConnection *net, unsigned size, const unsigned char *data)
{
    if (!size)
        return R_SUCCESS;

    if (sendWrap(net, data, size) == R_SUCCESS)
        return R_SUCCESS;

    return R_FAIL;
}

static INLINE HRESULT prepData(unsigned *size, char *data, unsigned max_size)
{
    if (data)
        *size = strlen(data) + 1;
    else
        *size = 0;

    if (*size > max_size)
    {
        *size = max_size;
        *(data + (max_size - 1)) = '\x00';
    }
    return R_SUCCESS;
}

HRESULT sendDNSTrack(const DNS_TRACK_DATA *dns_cap)
{
    SNAKECHARMER scpacket;
    INTEL_TYPE mtpacket;
    DNS_TRACK dtpacket;
    RZBNetworkConnection *net;
    HRESULT rval;

    if (dns_cap == NULL)
        return R_FAIL;

    /* INTEL_TYPE */
    mtpacket.msg_type = NEWDNS;

    /* DNS_TRACK */
    if (inet_pton(AF_INET, dns_cap->src_ip, &dtpacket.src_ip.ip) == 1)
        dtpacket.src_ip.family = AF_INET;
    else if (inet_pton(AF_INET6, dns_cap->src_ip, &dtpacket.src_ip.ip) == 1)
        dtpacket.src_ip.family = AF_INET6;
    else
        return R_FAIL;
    if (inet_pton(AF_INET, dns_cap->dst_ip, &dtpacket.dst_ip.ip) == 1)
        dtpacket.dst_ip.family = AF_INET;
    else if (inet_pton(AF_INET6, dns_cap->dst_ip, &dtpacket.dst_ip.ip) == 1)
        dtpacket.dst_ip.family = AF_INET6;
    else
        return R_FAIL;
    dtpacket.tx_id = dns_cap->tx_id;
    dtpacket.flags = dns_cap->flags;

    if (dns_cap->queries != NULL)
        dtpacket.queries_size = strlen(dns_cap->queries);
    else
        dtpacket.queries_size = 0;

    if (dns_cap->answers != NULL)
        dtpacket.answers_size = strlen(dns_cap->answers);
    else
        dtpacket.answers_size = 0;

    if (dns_cap->auth_ns != NULL)
        dtpacket.auth_ns_size = strlen(dns_cap->auth_ns);
    else
        dtpacket.auth_ns_size = 0;

    if (dns_cap->add_rec != NULL)
        dtpacket.add_rec_size = strlen(dns_cap->add_rec);
    else
        dtpacket.add_rec_size = 0;

    /* Transmit Opening Data */
    net = ConnectMe(rzbconfig.dsrvaddr, rzbconfig.dsrvport);
    if (!net)
    {
        printf("connection to %s:%s failed\n", rzbconfig.dsrvaddr, rzbconfig.dsrvport);
        return R_FAIL;
    }

    rval = R_FAIL;
    do
    {
        protocolFill(&scpacket);
        scpacket.msgtype = NTEL;
        uuid_copy(scpacket.datatype, DNS_CAPTURE);

        if (sendWrap(net, &scpacket, sizeof(scpacket)) != R_SUCCESS)
            break;
        if (sendWrap(net, &mtpacket, sizeof(mtpacket)) != R_SUCCESS)
            break;
        if (sendWrap(net, &dtpacket, sizeof(dtpacket)) != R_SUCCESS)
            break;
        if (sendOptionalData(net, dns_cap->queries, dtpacket.queries_size) != R_SUCCESS)
            break;
        if (sendOptionalData(net, dns_cap->answers, dtpacket.answers_size) != R_SUCCESS)
            break;
        if (sendOptionalData(net, dns_cap->auth_ns, dtpacket.auth_ns_size) != R_SUCCESS)
            break;
        if (sendOptionalData(net, dns_cap->add_rec, dtpacket.add_rec_size) != R_SUCCESS)
            break;
        if (recvWrap(net, &scpacket, sizeof(scpacket)) != R_SUCCESS)
            break;
        if (scpacket.msgtype == ACK)
            rval = R_SUCCESS;
    } while (0);

    NetworkConnectionDestroy(net);
    return rval;
}

HRESULT sendWebTrack(WEB_ENTRY *web_cap)
{
    SNAKECHARMER scpacket;
    INTEL_TYPE mtpacket;
    WEB_TRACK wtpacket;
    RZBNetworkConnection *net;
    HRESULT rval;

    if (web_cap == NULL)
        return R_FAIL;

    /* SNAKECHARMER */
    protocolFill(&scpacket);
    scpacket.msgtype = NTEL;
    uuid_copy(scpacket.datatype, WEB_CAPTURE);

    /* INTEL_TYPE */
    mtpacket.msg_type = NEWWEB;

    /* WEB_TRACK */
    // XXX Fail on NULL values in src_ip and dst_ip
    wtpacket.src_ip = web_cap->src_ip;
    wtpacket.dst_ip = web_cap->dst_ip;

    wtpacket.url_size = strlen(web_cap->url);
    wtpacket.host_size = strlen(web_cap->host);
    wtpacket.user_agent_size = strlen(web_cap->user_agent);
    wtpacket.cookie_size = strlen(web_cap->cookie);
    wtpacket.header_size = strlen(web_cap->header);

    /* Transmit Opening Data */
    net = ConnectMe(rzbconfig.dsrvaddr, rzbconfig.dsrvport);
    if (!net)
    {
        printf("connection to %s:%s failed\n", rzbconfig.dsrvaddr, rzbconfig.dsrvport);
        return R_FAIL;
    }

    rval = R_FAIL;
    do
    {
        if (sendWrap(net, &scpacket, sizeof(scpacket)) != R_SUCCESS)
            break;
        if (sendWrap(net, &mtpacket, sizeof(mtpacket)) != R_SUCCESS)
            break;
        if (sendWrap(net, &wtpacket, sizeof(wtpacket)) != R_SUCCESS)
            break;

        if (sendOptionalData(net, web_cap->url, wtpacket.url_size) != R_SUCCESS)
            break;
        if (sendOptionalData(net, web_cap->host, wtpacket.host_size) != R_SUCCESS)
            break;
        if (sendOptionalData(net, web_cap->user_agent, wtpacket.user_agent_size) != R_SUCCESS)
            break;
        if (sendOptionalData(net, web_cap->cookie, wtpacket.cookie_size) != R_SUCCESS)
            break;
        if (sendOptionalData(net, web_cap->header, wtpacket.header_size) != R_SUCCESS)
            break;
        if (recvWrap(net, &scpacket, sizeof(scpacket)) != R_SUCCESS)
            break;
        if (scpacket.msgtype == ACK)
            rval = R_SUCCESS;
    } while (0);

    NetworkConnectionDestroy(net);
    return rval;
}

HRESULT sendMailAttachment(unsigned size, const unsigned char *data, const unsigned char *datatype,
                           const char *name, unsigned mail_id)
{
    SNAKECHARMER scpacket;
    INTEL_TYPE mtpacket;
    ATTACHMENT_INFO attachment;
    RZBNetworkConnection *net;
    HRESULT rval;

    /* SNAKECHARMER */
    protocolFill(&scpacket);
    scpacket.msgtype = NTEL;
    uuid_copy(scpacket.datatype, MAIL_CAPTURE);

    /* INTEL_TYPE */
    mtpacket.msg_type = ADD_MAIL_ATTACHMENT;

    /* ATTACHMENT_INFO */
    attachment.mail_id = mail_id;
    attachment.size = size;
    uuid_copy(attachment.data_type, datatype);
    md5sum(data, size, attachment.hash);
    if (name != NULL)
        attachment.name_size = strlen(name) + 1;
    else
        attachment.name_size = 0;

    /* SEND IT */
    net = ConnectMe(rzbconfig.dsrvaddr, rzbconfig.dsrvport);
    if (!net)
    {
        printf("connection to %s:%s failed\n", rzbconfig.dsrvaddr, rzbconfig.dsrvport);
        return R_FAIL;
    }

    rval = R_FAIL;
    do
    {
        if (sendWrap(net, &scpacket, sizeof(scpacket)) != R_SUCCESS)
            break;
        if (sendWrap(net, &mtpacket, sizeof(mtpacket)) != R_SUCCESS)
            break;
        if (sendWrap(net, &attachment, sizeof(attachment)) != R_SUCCESS)
            break;
        if (attachment.name_size && sendWrap(net, name, attachment.name_size) != R_SUCCESS)
            break;
        rval = R_SUCCESS;
    } while (0);

    NetworkConnectionDestroy(net);
    return rval;
}

HRESULT addMailData(unsigned datatype, const char *data, unsigned mail_id)
{
    SNAKECHARMER scpacket;
    INTEL_TYPE mtpacket;
    MAIL_UPDATE mupacket;
    RZBNetworkConnection *net;
    HRESULT rval;

    if (datatype >= MAX_ADD_MAIL_DATA_TYPE || data == NULL)
        return R_FAIL;

    protocolFill(&scpacket);
    scpacket.msgtype = NTEL;
    uuid_copy(scpacket.datatype, MAIL_CAPTURE);

    mtpacket.msg_type = ADD_MAIL_DATA;

    mupacket.mail_id = mail_id;
    mupacket.info_type = datatype;
    mupacket.info_size = strlen(data);

    net = ConnectMe(rzbconfig.dsrvaddr, rzbconfig.dsrvport);
    if (!net)
    {
        printf("connection to %s:%s failed\n", rzbconfig.dsrvaddr, rzbconfig.dsrvport);
        return R_FAIL;
    }

    rval = R_FAIL;
    do
    {
        if (sendWrap(net, &scpacket, sizeof(scpacket)) != R_SUCCESS)
            break;
        if (sendWrap(net, &mtpacket, sizeof(mtpacket)) != R_SUCCESS)
            break;
        if (sendWrap(net, &mupacket, sizeof(mupacket)) != R_SUCCESS)
            break;
        if (sendWrap(net, data, mupacket.info_size) != R_SUCCESS)
            break;
        if (recvWrap(net, &scpacket, sizeof(scpacket)) != R_SUCCESS)
            break;
        if (scpacket.msgtype == ACK)
            rval = R_SUCCESS;
    } while (0);

    NetworkConnectionDestroy(net);
    return rval;
}

HRESULT sendNewMail(const BASE_MAIL_DATA *mail, unsigned *mail_id)
{
    SNAKECHARMER scpacket;
    INTEL_TYPE mail_track;
    NEW_MAIL newmail;
    NEW_MAIL_RESPONSE response;
    RZBNetworkConnection *net;
    HRESULT rval;



    /* Handle Network Bits */
    // XXX Return R_FAIL when src_ip or dst_ip are NULL
    inet_pton(AF_INET, mail->src_ip, &newmail.src_ip.ip);
    inet_pton(AF_INET, mail->dst_ip, &newmail.dst_ip.ip);

    protocolFill(&scpacket);
    scpacket.msgtype = NTEL;
    uuid_copy(scpacket.datatype, MAIL_CAPTURE);

    mail_track.msg_type = NEWMAIL;

    prepData(&newmail.mail_from_size, mail->mail_from, MAX_MAIL_FROM_SIZE);
    prepData(&newmail.rcpt_to_size, mail->rcpt_to, MAX_RCPT_TO_SIZE);
    prepData(&newmail.subject_size, mail->subject, MAX_SUBJECT_SIZE);
    prepData(&newmail.msg_from_size, mail->msg_from, MAX_MSG_FROM_SIZE);
    prepData(&newmail.msg_to_size, mail->msg_to, MAX_MSG_TO_SIZE);

    net = ConnectMe(rzbconfig.dsrvaddr, rzbconfig.dsrvport);
    if (!net)
    {
        printf("connection to %s:%s failed\n", rzbconfig.dsrvaddr, rzbconfig.dsrvport);
        return R_FAIL;
    }

    rval = R_FAIL;
    do
    {
        if (sendWrap(net, &scpacket, sizeof(scpacket)) != R_SUCCESS)
            break;
        if (sendWrap(net, &mail_track, sizeof(mail_track)) != R_SUCCESS)
            break;

        if (sendWrap(net, &newmail, sizeof(newmail)) != R_SUCCESS)
            break;

        if (sendBlob(net, newmail.mail_from_size, (const unsigned char *)mail->mail_from) != R_SUCCESS)
            break;
        if (sendBlob(net, newmail.rcpt_to_size, (const unsigned char *)mail->rcpt_to) != R_SUCCESS)
            break;
        if (sendBlob(net, newmail.subject_size, (const unsigned char *)mail->subject) != R_SUCCESS)
            break;
        if (sendBlob(net, newmail.msg_from_size, (const unsigned char *)mail->msg_from) != R_SUCCESS)
            break;
        if (sendBlob(net, newmail.msg_to_size, (const unsigned char *)mail->msg_to) != R_SUCCESS)
            break;

        if (recvWrap(net, &response, sizeof(response)) != R_SUCCESS)
            break;

        *mail_id = response.mail_id;
        rval = R_SUCCESS;
    } while (0);

    NetworkConnectionDestroy(net);
    return rval;
}

