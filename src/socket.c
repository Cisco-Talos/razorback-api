/*
 * Copyright (c) 2011-2026 Cisco Systems, Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *  MA 02110-1301, USA.
 */

#include "config.h"
#include <razorback/debug.h>
#include <razorback/lock.h>
#include <razorback/socket.h>
#include <razorback/log.h>

#ifdef _MSC_VER
#include <WinSock2.h>
#include <Ws2tcpip.h>
#include <io.h>
#pragma comment(lib, "Ws2_32.lib")
#define OPT_CAST const char *
#else
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <netdb.h>
#define OPT_CAST void *
#define closesocket(x) close(x)
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#endif

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>


/* controls the amount sent per call to read or write */
#define MAXRWSIZE   1024

/*
 * Shared client contexts are configured once per mode and then treated as
 * immutable. Each connection takes an extra reference to the selected context.
 */
static Mutex_t *sg_pTlsContextMutex = NULL;
static SSL_CTX *sg_pTlsSecureClientContext = NULL;
static SSL_CTX *sg_pTlsInsecureClientContext = NULL;

static SSL_CTX *Socket_TLS_GetSharedContextLocked(bool insecureMode);
static bool Socket_TLS_CreateHandle(struct Socket *sock, bool insecureMode);

static bool
Socket_TLS_ConfigureContext(SSL_CTX *context, bool insecureMode)
{
    ASSERT(context != NULL);
    if (context == NULL)
        return false;

    if (SSL_CTX_set_min_proto_version(context, TLS1_2_VERSION) != 1)
    {
        rzb_log(LOG_ERR, LOG_C_NETWORK, "%s: Failed to enforce minimum TLS version", __func__);
        return false;
    }

    if (insecureMode)
    {
        SSL_CTX_set_verify(context, SSL_VERIFY_NONE, NULL);
        return true;
    }

    if (SSL_CTX_set_default_verify_paths(context) != 1)
    {
        rzb_log(LOG_ERR, LOG_C_NETWORK, "%s: Failed to load default certificate authorities", __func__);
        return false;
    }

    SSL_CTX_set_verify(context, SSL_VERIFY_PEER, NULL);
    return true;
}

bool
Socket_TLS_InitializeSharedState(void)
{
    if (sg_pTlsContextMutex != NULL)
        return true;

    if ((sg_pTlsContextMutex = Mutex_Create(MUTEX_MODE_NORMAL)) == NULL)
    {
        rzb_log(LOG_ERR, LOG_C_NETWORK, "%s: Failed to create TLS context mutex", __func__);
        return false;
    }

    return true;
}

static SSL_CTX *
Socket_TLS_GetSharedContextLocked(bool insecureMode)
{
    SSL_CTX **sharedContext;
    SSL_CTX *context;

    sharedContext = insecureMode ? &sg_pTlsInsecureClientContext : &sg_pTlsSecureClientContext;
    if (*sharedContext == NULL)
    {
        if ((context = SSL_CTX_new(TLS_client_method())) == NULL)
        {
            rzb_log(LOG_ERR, LOG_C_NETWORK, "%s: Failed to allocate shared SSL context", __func__);
            return NULL;
        }

        if (!Socket_TLS_ConfigureContext(context, insecureMode))
        {
            SSL_CTX_free(context);
            return NULL;
        }

        *sharedContext = context;
    }

    return *sharedContext;
}

static bool
Socket_TLS_CreateHandle(struct Socket *sock, bool insecureMode)
{
    SSL_CTX *sharedContext;

    ASSERT(sock != NULL);
    if (sock == NULL)
        return false;

    ASSERT(sg_pTlsContextMutex != NULL);
    if (sg_pTlsContextMutex == NULL)
    {
        rzb_log(LOG_ERR, LOG_C_NETWORK, "%s: Shared TLS state is not initialized", __func__);
        return false;
    }

    if (!Mutex_Lock(sg_pTlsContextMutex))
    {
        rzb_log(LOG_ERR, LOG_C_NETWORK, "%s: Failed to lock TLS context mutex", __func__);
        return false;
    }

    if ((sharedContext = Socket_TLS_GetSharedContextLocked(insecureMode)) == NULL)
    {
        Mutex_Unlock(sg_pTlsContextMutex);
        return false;
    }

    if (SSL_CTX_up_ref(sharedContext) != 1)
    {
        rzb_log(LOG_ERR, LOG_C_NETWORK, "%s: Failed to reference shared SSL context", __func__);
        Mutex_Unlock(sg_pTlsContextMutex);
        return false;
    }

    if ((sock->sslHandle = SSL_new(sharedContext)) == NULL)
    {
        rzb_log(LOG_ERR, LOG_C_NETWORK, "%s: Failed to allocate SSL handle", __func__);
        SSL_CTX_free(sharedContext);
        Mutex_Unlock(sg_pTlsContextMutex);
        return false;
    }

    sock->sslContext = sharedContext;
    Mutex_Unlock(sg_pTlsContextMutex);
    return true;
}

static bool
Socket_TLS_SetExpectedPeer(SSL *handle, const char *destination, bool insecureMode)
{
    unsigned char ipv4[sizeof(struct in_addr)];
    unsigned char ipv6[sizeof(struct in6_addr)];
    X509_VERIFY_PARAM *verifyParams;

    ASSERT(handle != NULL);
    if (handle == NULL)
        return false;

    ASSERT(destination != NULL);
    if (destination == NULL)
        return false;

    if ((inet_pton(AF_INET, destination, ipv4) == 1) ||
        (inet_pton(AF_INET6, destination, ipv6) == 1))
    {
        if (insecureMode)
            return true;

        verifyParams = SSL_get0_param(handle);
        if (verifyParams == NULL)
        {
            rzb_log(LOG_ERR, LOG_C_NETWORK, "%s: Failed to retrieve verification parameters", __func__);
            return false;
        }

        if (X509_VERIFY_PARAM_set1_ip_asc(verifyParams, destination) != 1)
        {
            rzb_log(LOG_ERR, LOG_C_NETWORK, "%s: Failed to configure peer IP verification", __func__);
            return false;
        }
        return true;
    }

    if (SSL_set_tlsext_host_name(handle, destination) != 1)
    {
        rzb_log(LOG_ERR, LOG_C_NETWORK, "%s: Failed to configure TLS SNI", __func__);
        return false;
    }

    if (insecureMode)
        return true;

    verifyParams = SSL_get0_param(handle);
    if (verifyParams == NULL)
    {
        rzb_log(LOG_ERR, LOG_C_NETWORK, "%s: Failed to retrieve verification parameters", __func__);
        return false;
    }

    if (X509_VERIFY_PARAM_set1_host(verifyParams, destination, 0) != 1)
    {
        rzb_log(LOG_ERR, LOG_C_NETWORK, "%s: Failed to configure peer hostname verification", __func__);
        return false;
    }

    return true;
}

static void
Socket_Destroy (struct Socket *sock)
{
    if (sock->pAddressInfo != NULL)
        freeaddrinfo (sock->pAddressInfo);
    free(sock);
}

static bool
Socket_CopyAddress (struct Socket *dest, const struct Socket *source)
{
    ASSERT (dest->pAddressInfo == NULL);
    if (dest->pAddressInfo != NULL)
        return false;

    ASSERT (source->pAddressInfo != NULL);
    if (source->pAddressInfo == NULL)
        return false;

    if ((dest->pAddressInfo = (struct addrinfo *)calloc(1,sizeof(struct addrinfo))) == NULL)
    {
        rzb_log(LOG_ERR, LOG_C_NETWORK, "%s: Failed to allocate new address info", __func__);
        return false;
    }

    dest->pAddressInfo->ai_flags= source->pAddressInfo->ai_flags;
    dest->pAddressInfo->ai_family= source->pAddressInfo->ai_family;
    dest->pAddressInfo->ai_socktype= source->pAddressInfo->ai_socktype;
    dest->pAddressInfo->ai_protocol= source->pAddressInfo->ai_protocol;
    dest->pAddressInfo->ai_addrlen= source->pAddressInfo->ai_addrlen;

    dest->pAddressInfo->ai_next= NULL; // No next when we clone it.

    dest->pAddressInfo->ai_canonname = NULL;


    if ((dest->pAddressInfo->ai_addr = (struct sockaddr *)
         malloc (source->pAddressInfo->ai_addrlen)) == NULL)
    {
        rzb_log(LOG_ERR,LOG_C_NETWORK, "%s: Failed to allocate new address", __func__);
        return false;
    }

    memcpy (dest->pAddressInfo->ai_addr, source->pAddressInfo->ai_addr,
            source->pAddressInfo->ai_addrlen);

    return true;
}

static bool
SocketAddress_Initialize (struct Socket *sock,
                          const char * address, uint16_t port)
{
    struct addrinfo aiHints;
    char portAsString[32];
    int ret;

    ASSERT (sock->pAddressInfo == NULL);
    if (sock->pAddressInfo != NULL)
    {
        rzb_log(LOG_ERR,LOG_C_NETWORK, "%s: Double address init", __func__);
        return false;
    }

    sprintf (portAsString, "%i", port);
    memset (&aiHints, 0, sizeof (struct addrinfo));

    aiHints.ai_family = AF_UNSPEC;
    aiHints.ai_socktype = SOCK_STREAM;
    aiHints.ai_flags = AI_PASSIVE | AI_NUMERICSERV;

    aiHints.ai_protocol = IPPROTO_TCP;
    ret = getaddrinfo(
            address,
            portAsString,
            &aiHints,
            &sock->pAddressInfo);

    if (ret != 0)
    {

#ifdef _MSC_VER
        rzb_log(LOG_ERR,LOG_C_NETWORK, "Failed to get address info: %S, %d, %S", address, ret, gai_strerror(ret));
#else
        rzb_perror
            (LOG_C_NETWORK,"Failed to get address information in SocketAddress_Initialize: %s");
#endif
        sock->pAddressInfo = NULL;

        return false;
    }

    return true;
}

SO_PUBLIC struct Socket *
Socket_Listen (const char *sourceAddress, uint16_t port)
{
    struct Socket *sock;
    int on = 1;

    ASSERT (sourceAddress != NULL);
    if (sourceAddress == NULL)
        return NULL;

    ASSERT (port > 0);
    if (port <= 0)
        return  NULL;

    if ((sock = (struct Socket *)calloc (1, sizeof (struct Socket))) == NULL)
    {
        rzb_log(LOG_ERR,LOG_C_NETWORK, "%s: Failed to allocate new socket", __func__);
        return NULL;
    }

    if (!SocketAddress_Initialize
        (sock, sourceAddress, port))
    {
        rzb_log (LOG_ERR,LOG_C_NETWORK,
                 "%s: failed due to failure of SocketAddress_Initialize", __func__);
        Socket_Destroy(sock);
        return NULL;
    }

    if ((sock->iSocket =
         socket (sock->pAddressInfo->ai_family, sock->pAddressInfo->ai_socktype,
                 sock->pAddressInfo->ai_protocol)) == -1)
    {
        Socket_Destroy (sock);
        rzb_perror (LOG_C_NETWORK,"Socket_Listen failed due to failure of socket call: %s");
        return NULL;
    }

    if (setsockopt( sock->iSocket, SOL_SOCKET, SO_REUSEADDR, (OPT_CAST)&on, sizeof(on) ) == -1)
    {
        Socket_Destroy (sock);
        rzb_perror (LOG_C_NETWORK,"Socket_Listen failed due to failure of setsockopt: %s");
        return NULL;
    }
    if (bind
        (sock->iSocket,
         sock->pAddressInfo->ai_addr,
         sock->pAddressInfo->ai_addrlen) == -1)
    {
        Socket_Destroy (sock);
        rzb_perror (LOG_C_NETWORK,"Socket_Listen failed due to failure of bind call: %s");
        return NULL;
    }

    if (listen (sock->iSocket, SOMAXCONN) == -1)
    {
        Socket_Destroy (sock);
        rzb_perror (LOG_C_NETWORK,"Socket_Listen failed due to failure of listen call: %s");
        return NULL;
    }

    return sock;
}

SO_PUBLIC struct Socket *
Socket_Listen_Unix (const char *path)
{
#ifdef _MSC_VER
    ASSERT(false);
    return NULL;
#else
    struct Socket *sock;
    struct sockaddr_un *server;

    ASSERT (path != NULL);
    if (path == NULL)
        return NULL;

    if((server = calloc(1, sizeof(struct sockaddr_un))) == NULL)
        return NULL;

    server->sun_family = AF_UNIX;
    strncpy (server->sun_path, path, sizeof (server->sun_path));
    server->sun_path[sizeof (server->sun_path) - 1] = '\0';

    if ((sock = (struct Socket *)calloc (1, sizeof (struct Socket))) == NULL)
    {
        rzb_log(LOG_ERR,LOG_C_NETWORK, "%s: Failed to allocate new socket", __func__);\
        free(server);
        return NULL;
    }
    if ((sock->pAddressInfo = (struct addrinfo *)calloc(1,sizeof(struct addrinfo))) == NULL)
    {
        rzb_log(LOG_ERR,LOG_C_NETWORK, "%s: Failed to allocate new address info", __func__);
        Socket_Destroy(sock);
        free(server);
        return false;
    }

    sock->pAddressInfo->ai_family=AF_UNIX;
    sock->pAddressInfo->ai_next= NULL;
    sock->pAddressInfo->ai_canonname = NULL;
    sock->pAddressInfo->ai_addrlen= sizeof(struct sockaddr_un);
    sock->pAddressInfo->ai_addr = (struct sockaddr *)server;

    if ((sock->iSocket = socket (AF_UNIX, SOCK_STREAM, 0)) == -1)
    {
        Socket_Destroy (sock);
        rzb_perror (LOG_C_NETWORK,"Socket_Listen failed due to failure of socket call: %s");
        return NULL;
    }

    if (bind
        (sock->iSocket,
         sock->pAddressInfo->ai_addr,
         sock->pAddressInfo->ai_addrlen) == -1)
    {
        Socket_Destroy (sock);
        rzb_perror (LOG_C_NETWORK,"Socket_Listen_Unix failed due to failure of bind call: %s");
        return NULL;
    }

    if (listen (sock->iSocket, SOMAXCONN) == -1)
    {
        Socket_Destroy (sock);
        rzb_perror (LOG_C_NETWORK,"Socket_Listen failed due to failure of listen call: %s");
        return NULL;
    }

    return sock;
#endif
}

/* Returns: 0 = timeout, -1 = error, 1 = ok */
SO_PUBLIC int
Socket_Accept (struct Socket **retSock,
               const struct Socket *listeningSocket)
{
    struct Socket *sock = NULL;
    fd_set fdSet;
    struct timeval timeout;

    ASSERT (retSock != NULL);
    if (retSock == NULL)
        return -1;

    ASSERT (listeningSocket != NULL);
    if (listeningSocket == NULL)
        return -1;

    if ((sock = (struct Socket *)calloc(1, sizeof (struct Socket))) == NULL)
    {
        rzb_log(LOG_ERR,LOG_C_NETWORK, "%s: Failed to allocate new socket", __func__);
        return -1;
    }

    Socket_CopyAddress (sock, listeningSocket);

    timeout.tv_sec = 0;
    timeout.tv_usec = 10000;
    FD_ZERO (&fdSet);
    FD_SET (listeningSocket->iSocket, &fdSet);
    if (select (listeningSocket->iSocket +1, &fdSet, NULL, NULL, &timeout) < 0)
    {
        Socket_Destroy (sock);
        rzb_perror
            (LOG_C_NETWORK,"Socket_Accept failed due to failure of accept call: %s");
        return -1;
    }

    {
        // check for error
        if ((sock->iSocket =
             accept (listeningSocket->iSocket,
                     sock->pAddressInfo->ai_addr,
                     &sock->pAddressInfo->ai_addrlen)) == -1)
        {
            Socket_Destroy (sock);
            rzb_perror
                (LOG_C_NETWORK,"Socket_Accept failed due to failure of accept call: %s");
            return -1;
        }
        *retSock = sock;
        return 1;
    }
    return 0;
}

SO_PUBLIC struct Socket *
Socket_Connect (const char *destinationAddress, uint16_t port)
{
    struct addrinfo *cur = NULL;
    struct Socket *sock = NULL;

#ifdef _MSC_VER
    DWORD nTimeout = 5000;
#endif

    ASSERT (destinationAddress != NULL);

    if ((sock = (struct Socket *)calloc (1, sizeof (struct Socket))) == NULL)
    {
        rzb_log(LOG_ERR,LOG_C_NETWORK, "%s: Failed to allocate new socket", __func__);
        return NULL;
    }
    sock->ssl =false;

    if (!SocketAddress_Initialize
        (sock, destinationAddress, port))
    {
        rzb_log (LOG_ERR,LOG_C_NETWORK,
                 "%s: failed due to failure of SocketAddress_Initialize", __func__);
        return NULL;
    }

    cur = sock->pAddressInfo;
    while (cur != NULL)
    {
        if ((sock->iSocket = socket (cur->ai_family, cur->ai_socktype, cur->ai_protocol)) ==INVALID_SOCKET)
        {
            rzb_perror
                (LOG_C_NETWORK,"Socket_Connect failed due to failure of socket call: %s");
            cur = cur->ai_next;
            continue;
        }
        int flag = 1;
        setsockopt(sock->iSocket, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int));

#ifdef _MSC_VER
        setsockopt(sock->iSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&nTimeout, sizeof(int));
#endif

        if (connect(sock->iSocket, cur->ai_addr, cur->ai_addrlen) == SOCKET_ERROR)
        {
            rzb_perror
                (LOG_C_NETWORK,"Socket_Connect failed due to failure of connect call: %s");
            cur = cur->ai_next;
            closesocket(sock->iSocket);
            continue;
        }
        return sock;
    }

    rzb_log(LOG_ERR,LOG_C_NETWORK,"%s: All possible hosts exhausted", __func__);
    Socket_Close(sock);
    return NULL;
}

SO_PUBLIC struct Socket *
SSL_Socket_Connect ( const char * destination, uint16_t port, bool insecureMode)
{
    struct Socket *sock;
    const char *peerName = destination;
    long verifyResult;

    ASSERT (destination != NULL);
    if (destination == NULL)
        return NULL;

    ASSERT(port > 0);
    if (port <= 0)
        return NULL;

    if ((sock = Socket_Connect(destination, port)) == NULL)
    {
        return NULL;
    }

    if (insecureMode)
    {
        rzb_log(LOG_WARNING, LOG_C_NETWORK,
                "%s: TLS certificate verification disabled for %s:%u",
                __func__, peerName, port);
    }

    sock->ssl =true;
    if (!Socket_TLS_CreateHandle(sock, insecureMode))
    {
        Socket_Close(sock);
        return NULL;
    }

    if (!SSL_set_fd (sock->sslHandle, sock->iSocket))
    {
        rzb_log(LOG_ERR, LOG_C_NETWORK, "%s: Failed to attach socket to SSL handle", __func__);
        Socket_Close(sock);
        return NULL;
    }

    if (!Socket_TLS_SetExpectedPeer(sock->sslHandle, peerName, insecureMode))
    {
        Socket_Close(sock);
        return NULL;
    }

    // Initiate SSL handshake
    {
        int rc = SSL_connect(sock->sslHandle);
        if (rc != 1)
        {
            int ssl_err = SSL_get_error(sock->sslHandle, rc);
            char err_buf[256];
            unsigned long e;

            rzb_log(LOG_ERR, LOG_C_NETWORK,
                    "%s: TLS handshake failed (SSL_connect rc=%d, SSL_get_error=%d)",
                    __func__, rc, ssl_err);

            while ((e = ERR_get_error()) != 0)
            {
                ERR_error_string_n(e, err_buf, sizeof(err_buf));
                rzb_log(LOG_ERR, LOG_C_NETWORK,
                        "%s: OpenSSL error: %s",
                        __func__, err_buf);
            }

            Socket_Close(sock);
            return NULL;
        }
    }

    if (insecureMode)
        return sock;

    if (SSL_get0_peer_certificate(sock->sslHandle) == NULL)
    {
        rzb_log(LOG_ERR, LOG_C_NETWORK, "%s: Server did not provide a certificate", __func__);
        Socket_Close(sock);
        return NULL;
    }

    verifyResult = SSL_get_verify_result(sock->sslHandle);
    if (verifyResult != X509_V_OK)
    {
        rzb_log(LOG_ERR, LOG_C_NETWORK, "%s: TLS certificate verification failed: %s",
                __func__, X509_verify_cert_error_string(verifyResult));
        Socket_Close(sock);
        return NULL;
    }
    return sock;
}

SO_PUBLIC void
Socket_Close (struct Socket *sock)
{
    ASSERT (sock != NULL);
    if (sock == NULL)
        return;

    closesocket(sock->iSocket);

    if (sock->ssl)
    {
        if (sock->sslHandle)
        {
            SSL_shutdown (sock->sslHandle);
            SSL_free (sock->sslHandle);
        }
        if (sock->sslContext)
        {
            if (sg_pTlsContextMutex != NULL)
            {
                if (Mutex_Lock(sg_pTlsContextMutex))
                {
                    SSL_CTX_free (sock->sslContext);
                    Mutex_Unlock(sg_pTlsContextMutex);
                }
                else
                {
                    SSL_CTX_free (sock->sslContext);
                }
            }
            else
            {
                SSL_CTX_free (sock->sslContext);
            }
        }
    }
    Socket_Destroy (sock);
}

SO_PUBLIC ssize_t
Socket_Tx (const struct Socket *sock, size_t size,
           const uint8_t * buffer)
{
    ssize_t amountSent = 0;
    ssize_t sendThisTime = 0;
    ssize_t sentThisTime = 0;

    ASSERT (sock != NULL);
    if (sock == NULL)
        return -1;

    ASSERT (buffer != NULL);
    if (buffer == NULL)
        return -1;

    ASSERT (size > 0);
    if (size <= 0)
        return -1;

    while ((size - amountSent )> 0)
    {
        sendThisTime = size - amountSent;
        if (sendThisTime > MAXRWSIZE)
        {
            sendThisTime = MAXRWSIZE;
        }
        if (sock->ssl)
        {
            sentThisTime =
                SSL_write (sock->sslHandle, buffer + amountSent,
                       sendThisTime);
        }
        else
        {
            sentThisTime =
                send (sock->iSocket, (const char *)buffer + amountSent,
                       sendThisTime, 0);
        }
        if (sentThisTime == SOCKET_ERROR)
        {
#ifdef _MSC_VER
            if (WSAGetLastError() == WSAETIMEDOUT)
                errno = EINTR;
#endif
            if (errno != EINTR && errno != EAGAIN)
                rzb_perror (LOG_C_NETWORK,"Socket_Tx failed due to failure of read call: %s");

            return -1;
        }
        else if (sentThisTime == 0)
            return amountSent;
        else
            amountSent += sentThisTime;
    }

    return amountSent;
}

SO_PUBLIC PRINTF_FUNC(2,3) bool
Socket_Printf (const struct Socket *sock, const char *fmt, ...)
{
    char *buffer = NULL;
    va_list argp;

    ASSERT(sock != NULL);
    if (sock == NULL)
        return false;

    ASSERT(fmt != NULL);
    if (fmt == NULL)
        return false;

    va_start(argp, fmt);

    if(vasprintf(&buffer, fmt, argp) == -1)
    {
        va_end(argp);
        return false;
    }

    va_end(argp);

    if (Socket_Tx(sock, strlen(buffer), (uint8_t *)buffer) != (ssize_t)strlen(buffer))
    {
        free(buffer);
        return true;
    }
    else
    {
        free(buffer);
        return false;
    }
}


SO_PUBLIC ssize_t
Socket_Rx (const struct Socket * sock, size_t len,
           uint8_t * buffer)
{
    ssize_t received = 0;
    ssize_t toReadThisTime = 0;
    ssize_t readThisTime = 0;

    ASSERT (sock != NULL);
    if (sock == NULL)
        return false;

    ASSERT (len > 0);
    if (len <= 0)
        return false;

    ASSERT (buffer != NULL);
    if (buffer == NULL)
        return false;

    while ((len - received) > 0)
    {
        toReadThisTime = len - received;
        if (toReadThisTime > MAXRWSIZE)
            toReadThisTime = MAXRWSIZE;
        if (sock->ssl)
        {
            readThisTime =
                SSL_read (sock->sslHandle, buffer + received,
                  toReadThisTime);
        }
        else
        {
            readThisTime =
                recv (sock->iSocket, (char *)buffer + received,
                      toReadThisTime, 0);
        }

        if (readThisTime == SOCKET_ERROR)
        {
#ifdef _MSC_VER
            if (WSAGetLastError() == WSAETIMEDOUT)
                errno = EINTR;
#endif
            if (errno != EINTR && errno != EAGAIN)
                rzb_perror (LOG_C_NETWORK,"Socket_Rx failed due to failure of read call: %s");

            return -1;
        }
        else if (readThisTime == 0)
            return received;
        else
            received += readThisTime;
    }

    return received;
}

SO_PUBLIC ssize_t
Socket_Rx_Until (const struct Socket * sock, uint8_t ** r_buffer,
                 uint8_t terminator)
{
    ssize_t now = 0;
    ssize_t total = 0;
    ssize_t bufSize = MAXRWSIZE;
    uint8_t *buffer = NULL;
    uint8_t *tmp = NULL;

    ASSERT(sock != NULL);
    if (sock == NULL)
        return -1;

    ASSERT(r_buffer != NULL);
    if (r_buffer == NULL)
        return -1;

    if ((buffer = calloc(MAXRWSIZE, sizeof(uint8_t))) == NULL)
        return -1;

    do {
        now = Socket_Rx(sock, 1, &buffer[total]);
        if (now == -1) {
            free(buffer);
            if (errno != EINTR && errno != EAGAIN) {
                rzb_perror(LOG_C_NETWORK, "Socket_Rx failed: %s");
            }

            return -1;
        } else if (now == 0) {
            rzb_log(LOG_DEBUG, LOG_C_NETWORK, "%s: Socket_Rx returned 0 bytes", __func__);
            free(buffer);
            return 0;
        }
        total++;
        if (buffer[total - 1] == terminator) {
            *r_buffer = buffer;
            return total;
        }
        if (total == bufSize) {
            if ((tmp = realloc(buffer, bufSize + MAXRWSIZE)) == NULL) {
                rzb_log(LOG_ERR, LOG_C_NETWORK, "%s: Failed to realloc buffer", __func__);
                free(buffer);
                return -1;
            }
            buffer = tmp;
            bufSize += MAXRWSIZE;
        }

    } while (now > 0);
    // Unreachable
    return -1;
}
