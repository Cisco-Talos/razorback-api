#include "config.h"
#include <unistd.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <razorback/debug.h>
#include <razorback/socket.h>
#include <razorback/log.h>

#include <openssl/ssl.h>

/* controls the amount sent per call to read or write */
#define	MAXRWSIZE	1024
static void
Socket_Destroy (struct Socket *p_pSocket)
{
    if (p_pSocket->pAddressInfo != NULL)
        freeaddrinfo (p_pSocket->pAddressInfo);
    free(p_pSocket);
}

static bool
Socket_CopyAddress (struct Socket *p_pDest, const struct Socket *p_pSource)
{
    ASSERT (p_pDest->pAddressInfo == NULL);
    ASSERT (p_pSource->pAddressInfo != NULL);

    if ((p_pDest->pAddressInfo = calloc(1,sizeof(struct addrinfo))) == NULL)
    {
        rzb_log(LOG_ERR, "%s: Failed to allocate new address info", __func__);
        return false;
    }

    p_pDest->pAddressInfo->ai_flags= p_pSource->pAddressInfo->ai_flags;
    p_pDest->pAddressInfo->ai_family= p_pSource->pAddressInfo->ai_family;
    p_pDest->pAddressInfo->ai_socktype= p_pSource->pAddressInfo->ai_socktype;
    p_pDest->pAddressInfo->ai_protocol= p_pSource->pAddressInfo->ai_protocol;
    p_pDest->pAddressInfo->ai_addrlen= p_pSource->pAddressInfo->ai_addrlen;
    
    p_pDest->pAddressInfo->ai_next= NULL; // No next when we clone it.

    p_pDest->pAddressInfo->ai_canonname = NULL;
    

    if ((p_pDest->pAddressInfo->ai_addr =
         malloc (p_pSource->pAddressInfo->ai_addrlen)) == NULL)
    {
        rzb_log(LOG_ERR, "%s: Failed to allocate new address", __func__);
        return false;
    }

    memcpy (p_pDest->pAddressInfo->ai_addr, p_pSource->pAddressInfo->ai_addr,
            p_pSource->pAddressInfo->ai_addrlen);

    return true;
}

static bool
SocketAddress_Initialize (struct Socket *p_pSocket,
                          const uint8_t * p_sAddress, uint16_t p_iPort)
{
    ASSERT (p_pSocket->pAddressInfo == NULL);

    if (p_pSocket->pAddressInfo != NULL)
    {
        rzb_log(LOG_ERR, "%s: Double address init", __func__);
        return false;
    }
    
    
    struct addrinfo l_aiHints;
    uint8_t l_sPortAsString[32];
    sprintf ((char *) l_sPortAsString, "%i", p_iPort);
    memset (&l_aiHints, 0, sizeof (struct addrinfo));
    
    l_aiHints.ai_family = AF_UNSPEC;
    l_aiHints.ai_socktype = SOCK_STREAM;
    l_aiHints.ai_flags = AI_PASSIVE | AI_NUMERICSERV;
        
    if (getaddrinfo
        ((const char *) p_sAddress, (const char *) l_sPortAsString,
         &l_aiHints, &p_pSocket->pAddressInfo) != 0)
    {
        rzb_perror
            ("Failed to get address information in SocketAddress_Initialize: %s");
        p_pSocket->pAddressInfo = NULL;
        return false;
    }

    return true;
}

SO_PUBLIC struct Socket *
Socket_Listen (const unsigned char *p_sSourceAddress, uint16_t p_iPort)
{
    ASSERT (p_sSourceAddress != NULL);
    struct Socket *l_pSocket;
    int l_iOn = 1;
    if ((l_pSocket = calloc (1, sizeof (struct Socket))) == NULL) 
    {
        rzb_log(LOG_ERR, "%s: Failed to allocate new socket", __func__);
        return NULL;
    }
    
    if (!SocketAddress_Initialize
        (l_pSocket, p_sSourceAddress, p_iPort))
    {
        rzb_log (LOG_ERR,
                 "%s: failed due to failure of SocketAddress_Initialize", __func__);
        Socket_Destroy(l_pSocket);
        return NULL;
    }

    if ((l_pSocket->iSocket =
         socket (l_pSocket->pAddressInfo->ai_family, l_pSocket->pAddressInfo->ai_socktype,
                 l_pSocket->pAddressInfo->ai_protocol)) == -1)
    {
        Socket_Destroy (l_pSocket);
        rzb_perror ("Socket_Listen failed due to failure of socket call: %s");
        return NULL;
    }

    if (setsockopt( l_pSocket->iSocket, SOL_SOCKET, SO_REUSEADDR, &l_iOn, sizeof(l_iOn) ) == -1)
    {
        Socket_Destroy (l_pSocket);
        rzb_perror ("Socket_Listen failed due to failure of setsockopt: %s");
        return NULL;
    }
    if (bind
        (l_pSocket->iSocket,
         l_pSocket->pAddressInfo->ai_addr,
         l_pSocket->pAddressInfo->ai_addrlen) == -1)
    {
        Socket_Destroy (l_pSocket);
        rzb_perror ("Socket_Listen failed due to failure of bind call: ");
        return NULL;
    }

    if (listen (l_pSocket->iSocket, SOMAXCONN) == -1)
    {
        Socket_Destroy (l_pSocket);
        rzb_perror ("Socket_Listen failed due to failure of listen call: %s");
        return NULL;
    }

    // done
    return l_pSocket;
}

/* Returns: 0 = timeout, -1 = error, 1 = ok */
SO_PUBLIC int
Socket_Accept (struct Socket **p_pSocket,
               const struct Socket *p_sListeningSocket)
{
    ASSERT (p_pSocket != NULL);
    ASSERT (p_sListeningSocket != NULL);

    struct Socket *l_pSocket;
    fd_set l_fdsRead;
    struct timeval l_tTimeout;

    if ((l_pSocket = calloc(1, sizeof (struct Socket))) == NULL)
    {
        rzb_log(LOG_ERR, "%s: Failed to allocate new socket", __func__);
        return -1;
    }

    Socket_CopyAddress (l_pSocket, p_sListeningSocket);

    l_tTimeout.tv_sec = 0;
    l_tTimeout.tv_usec = 10000;
    FD_ZERO (&l_fdsRead);
    FD_SET (p_sListeningSocket->iSocket, &l_fdsRead);
    if (select (p_sListeningSocket->iSocket +1, &l_fdsRead, NULL, NULL, &l_tTimeout) < 0)
    {
        Socket_Destroy (l_pSocket);
        rzb_perror
            ("Socket_Accept failed due to failure of accept call: %s");
        return -1;
    }
    
    {
        // check for error
        if ((l_pSocket->iSocket =
             accept (p_sListeningSocket->iSocket,
                     l_pSocket->pAddressInfo->ai_addr,
                     &l_pSocket->pAddressInfo->ai_addrlen)) == -1)
        {
            Socket_Destroy (l_pSocket);
            rzb_perror
                ("Socket_Accept failed due to failure of accept call: %s");
            return -1;
        }
        *p_pSocket = l_pSocket;
        // ok
        return 1;
    }
    // timeout
    return 0;
}

SO_PUBLIC struct Socket *
Socket_Connect (const unsigned char *p_sDestinationAddress, uint16_t p_iPort)
{
    ASSERT (p_sDestinationAddress != NULL);
    struct addrinfo *cur = NULL;

    struct Socket *l_pSocket;
    if ((l_pSocket = calloc (1, sizeof (struct Socket))) == NULL) 
    {
        rzb_log(LOG_ERR, "%s: Failed to allocate new socket", __func__);
        return NULL;
    }
    l_pSocket->ssl =false;

    if (!SocketAddress_Initialize
        (l_pSocket, p_sDestinationAddress, p_iPort))
    {
        rzb_log (LOG_ERR,
                 "%s: failed due to failure of SocketAddress_Initialize", __func__);
        return NULL;
    }
    
    cur = l_pSocket->pAddressInfo;
    while (cur != NULL)
    {
        if ((l_pSocket->iSocket = socket (cur->ai_family, cur->ai_socktype, cur->ai_protocol)) == -1)
        {
            rzb_perror
                ("Socket_Connect failed due to failure of socket call: %s");
            cur = cur->ai_next;
            continue;
        }

        if (connect(l_pSocket->iSocket, cur->ai_addr, cur->ai_addrlen) == -1)
        {
            rzb_perror
                ("Socket_Connect failed due to failure of connect call: %s");
            cur = cur->ai_next;
            close(l_pSocket->iSocket);
            continue;
        }
        return l_pSocket;
    }

    rzb_log(LOG_ERR,"%s: All possible hosts exhausted", __func__);
    Socket_Close(l_pSocket);
    return NULL;
}

SO_PUBLIC struct Socket * 
SSL_Socket_Connect ( const uint8_t * destination, uint16_t port)
{
    struct Socket *socket;
    if ((socket = Socket_Connect(destination, port)) == NULL)
    {
        return NULL;
    }
    socket->ssl =true;
    if ((socket->sslContext = SSL_CTX_new(SSLv23_client_method())) == NULL)
    {
        Socket_Close(socket);
        return NULL;
    }
    if ((socket->sslHandle = SSL_new (socket->sslContext)) == NULL) 
    {
        Socket_Close(socket);
        return NULL;
    }
    
    if (!SSL_set_fd (socket->sslHandle, socket->iSocket))
    {
        Socket_Close(socket);
        return NULL;
    }

    // Initiate SSL handshake
    if (SSL_connect (socket->sslHandle) != 1)
    {
        Socket_Close(socket);
        return NULL;
    }
    return socket;
    
}
SO_PUBLIC void
Socket_Close (struct Socket *p_pSocket)
{
    ASSERT (p_pSocket != NULL);

    close (p_pSocket->iSocket);
    if (p_pSocket->ssl)
    {
        if (p_pSocket->sslHandle)
        {
            SSL_shutdown (p_pSocket->sslHandle);
            SSL_free (p_pSocket->sslHandle);
        }
        if (p_pSocket->sslContext)
            SSL_CTX_free (p_pSocket->sslContext);
    }
    Socket_Destroy (p_pSocket);

    // done
}

SO_PUBLIC bool
Socket_Tx (const struct Socket *p_pSocket, uint32_t p_iSize,
           const uint8_t * p_sData)
{
    ASSERT (p_pSocket != NULL);
    ASSERT (p_sData != NULL);

    int l_iAmountRemaining;
    int l_iAmountSent;
    int l_iAmountToSendThisTime;
    int l_iAmountSentThisTime;

    l_iAmountRemaining = p_iSize;
    l_iAmountSent = 0;

    while (l_iAmountRemaining > 0)
    {
        l_iAmountToSendThisTime = l_iAmountRemaining;
        if (l_iAmountToSendThisTime > MAXRWSIZE)
        {
            l_iAmountToSendThisTime = MAXRWSIZE;
        }
        if (p_pSocket->ssl)
        {
            l_iAmountSentThisTime =
                SSL_write (p_pSocket->sslHandle, p_sData + l_iAmountSent,
                       l_iAmountToSendThisTime);
        }
        else
        {
            l_iAmountSentThisTime =
                write (p_pSocket->iSocket, p_sData + l_iAmountSent,
                       l_iAmountToSendThisTime);
        }
        if (l_iAmountSentThisTime < 1)
        {
            rzb_perror ("Socket_Tx failed due to failure of write call: %s");
            return false;
        };
        l_iAmountSent += l_iAmountSentThisTime;
        l_iAmountRemaining -= l_iAmountSentThisTime;
    }
    // done
    return true;
}


SO_PUBLIC bool
Socket_Rx (const struct Socket * p_pSocket, uint32_t p_iSize,
           uint8_t * p_sData)
{
    ASSERT (p_pSocket != NULL);
    ASSERT (p_iSize > 0);
    ASSERT (p_sData != NULL);

    int l_iAmountRemaining;
    int l_iAmountReceived;
    int l_iAmountToReadThisTime;
    int l_iAmountReadThisTime;

    l_iAmountRemaining = p_iSize;
    l_iAmountReceived = 0;

    while (l_iAmountRemaining > 0)
    {
        l_iAmountToReadThisTime = l_iAmountRemaining;
        if (l_iAmountToReadThisTime > MAXRWSIZE)
            l_iAmountToReadThisTime = MAXRWSIZE;
        if (p_pSocket->ssl)
        {
            l_iAmountReadThisTime =
                SSL_read (p_pSocket->sslHandle, p_sData + l_iAmountReceived,
                  l_iAmountToReadThisTime);
        }
        else
        {
            l_iAmountReadThisTime =
                read (p_pSocket->iSocket, p_sData + l_iAmountReceived,
                      l_iAmountToReadThisTime);
        }
        if (l_iAmountReadThisTime < 1)
        {
            if (errno != EINTR && errno != EAGAIN) 
                rzb_perror ("Socket_Rx failed due to failure of read call: %s");
            if (errno == 0)
                errno = EPIPE;

            return false;
        }
        l_iAmountReceived += l_iAmountReadThisTime;
        l_iAmountRemaining -= l_iAmountReadThisTime;
    }

    // done
    return true;
}

SO_PUBLIC size_t
Socket_Rx_Until (const struct Socket * p_pSocket, uint8_t * p_sData,
                 size_t iSize, uint8_t p_cTerminator)
{
    ASSERT (p_pSocket != NULL);
    ASSERT (p_sData != NULL);

    // counter
    uint32_t l_iOffset = 0;
 
	while (l_iOffset < iSize) 
    {
        if (!Socket_Rx (p_pSocket, 1, &p_sData[l_iOffset]))
        {
            if (errno != EINTR && errno != EAGAIN)
                rzb_log (LOG_ERR, "%s: failed due to failure of Socket_Rx", __func__);

            return 0;
        }
        if (p_sData[l_iOffset++] == p_cTerminator)
            return l_iOffset;
    }

    // done
    return 0;
}

SO_PUBLIC bool
Socket_ReadyForRead (const struct Socket * p_pSocket)
{
    ASSERT (p_pSocket != NULL);

    // temporary variables
    struct timeval l_tvTimeOut;
    fd_set l_fdsReadEvents;
    fd_set l_fdsWriteEvents;
    fd_set l_fdsExceptEvents;

    // setup descriptors
    FD_ZERO (&l_fdsReadEvents);
    FD_SET (p_pSocket->iSocket, &l_fdsReadEvents);
    FD_ZERO (&l_fdsWriteEvents);
    FD_ZERO (&l_fdsExceptEvents);

    // specify timeout
    l_tvTimeOut.tv_sec = 0;
    l_tvTimeOut.tv_usec = 1000; // 1 ms time-out

    // return true if it does not timeout
    return (select
            (p_pSocket->iSocket + 1, &l_fdsReadEvents, &l_fdsWriteEvents,
             &l_fdsExceptEvents, &l_tvTimeOut) != 0);
}


