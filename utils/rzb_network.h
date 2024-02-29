#ifndef RZB_NETWORK_H
#define RZB_NETWORK_H

#include "rzb_utils_types.h"

/*
 *
 * NETWORK INFO
 *
 */
typedef enum
{
    REQ,
    ACK,
    RDR,
    REG,
    POLL,
    RST,
    MD5CHECK,
    MD5RESPONSE,
    META,
    REFERRAL,
    ALRT,
    DETECTDONE,
    NEWCAP,
    ERR,
    REQBLOB,
    SENDBLOB,
    NTEL,
    WKST,
    VERDICT
} MESSAGE;

typedef struct _SNAKECHARMER
{
    char magic[3];
    uint8_t version;
    MESSAGE msgtype;
    uuid_t datatype;
} SNAKECHARMER;

struct _RZBNetworkConnection;
typedef struct _RZBNetworkConnection RZBNetworkConnection;

RZBNetworkConnection *ConnectMe(const char * ipaddr, const char * port);
void NetworkConnectionDestroy(RZBNetworkConnection *net);
HRESULT sendWrap(RZBNetworkConnection *net, const void *buf, unsigned bufsize);
HRESULT recvWrap(RZBNetworkConnection *net, void *buf, unsigned bufsize);
void protocolFill(SNAKECHARMER *scpacket);
HRESULT protocolCheck(SNAKECHARMER *scpacket);
HRESULT sendFailure(RZBNetworkConnection *net, SNAKECHARMER *scpacket);
HRESULT acknowledge(RZBNetworkConnection *net, SNAKECHARMER *scpacket);
void NetworkConnectionGetStats(RZBNetworkConnection *net, unsigned long *bytes_in, unsigned long *bytes_out);

#endif /* RZB_NETWORK_H */

