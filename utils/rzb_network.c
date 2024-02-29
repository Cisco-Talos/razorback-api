#include "config.h"

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <signal.h>
#include <arpa/inet.h>

#include "rzb_network.h"
#include "rzb_thread.h"
#include "rzb_conf.h"
#include "rzb_utils.h"

#define BACKLOG 10

typedef struct _RZBNetworkConnection
{
    int sock;
    unsigned long bytes_in;
    unsigned long bytes_out;
} RZBNC;

/*
 *
 * Wrapper for establishing a local listener
 *
 */
static int MakeListener(const char * port_str)
{
    struct sockaddr_in serveraddr;
    int one = 1;
    struct linger linger = {1, 1};
    int sock;
    char *p;
    unsigned long tmp;
    uint16_t port;

    tmp = strtoul(port_str, &p, 10);
    if (!*port_str || *p || tmp < 1 || tmp > 65535)
    {
        fprintf(stderr, "Invalid port, %s, specified\n", port_str);
        exit(-1);
    }
    port = (uint16_t)tmp;

    //Create Socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("Socket create error");
        exit(-1);
    }
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    setsockopt(sock, SOL_SOCKET, SO_LINGER, &linger, sizeof(linger));

    memset(&serveraddr, 0, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(port);
    serveraddr.sin_addr.s_addr = INADDR_ANY;

    //Bind to local port
    if (bind(sock, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0)
    {
        perror("Bind error");
        exit(-1);
    }

    //Listen for connections
    if (listen(sock, BACKLOG) == -1)
    {
        perror("Listen error");
        exit(-1);
    }

    return sock;
}

static RZBNC *MakeConnection(const int sock)
{
    RZBNetworkConnection *net;

    if ((net = calloc(1, sizeof(*net))) == NULL)
    {
        perror("failed to allocate a network connection");
        exit(-1);
    }
    net->sock = sock;
    return net;
}

/*
 *
 * Wrapper for establishing a connection with a remote interface
 *
 */
SO_PUBLIC RZBNC *ConnectMe(const char *ipaddr, const char *port)
{
    struct addrinfo hints;
    struct addrinfo *servinfo;
    RZBNetworkConnection *net;
    int rval;
    unsigned to_secs = rzbconfig.network_to_secs;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if (getaddrinfo(ipaddr, port, &hints, &servinfo) != 0)
    {
        perror("getaddrinfo error 2");
        return NULL;
    }

    if ((net = calloc(1, sizeof(*net))) == NULL)
    {
        perror("failed to allocate a network connection");
        freeaddrinfo(servinfo);
        exit(-1);
    }

    //Create Socket
    if ((net->sock = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol)) == -1)
    {
        free(net);
        freeaddrinfo(servinfo);
        perror("Socket create error");
        return NULL;
    }

    //Establish a connection
    do
    {
        if ((rval = connect(net->sock, servinfo->ai_addr, servinfo->ai_addrlen)))
        {
            fprintf(stderr, "Error connecting to %s:%s: (%d) %s\n", ipaddr, port, errno, strerror(errno));
            if (to_secs)
            {
                to_secs--;
                sleep(1);
            }
            else
                break;
        }
        if (rzb_debug)
            printf("Connected to %s:%s\n", ipaddr, port);
    } while (rval);

    freeaddrinfo(servinfo);

    if (rval)
    {
        free(net);
        return NULL;
    }

    return net;
}


SO_PUBLIC void NetworkConnectionDestroy(RZBNC *net)
{
    if (net)
    {
        if (net->sock != -1)
        {
            close(net->sock);
        }
        free(net);
    }
}

SO_PUBLIC void NetworkConnectionGetStats(RZBNC *net, unsigned long *bytes_in, unsigned long *bytes_out)
{
    *bytes_in = net->bytes_in;
    *bytes_out = net->bytes_out;
}

/*
 *
 * Wrapper functions for TCPSocket send/recv calls
 *
 */
SO_PUBLIC HRESULT sendWrap(RZBNC *net, const void *buf, unsigned bufsize)
{
    int bytessofar;
    unsigned totalbytes;
    int sock = net->sock;
    uint8_t *buffer = (uint8_t *)buf;

    if (sock == -1)
        return R_FAIL;

    totalbytes = 0;
    while (totalbytes < bufsize)
    {
        bytessofar = send(sock, buffer+totalbytes, bufsize-totalbytes, 0);
        if (bytessofar < 1)
        {
            fprintf(stderr, "Could not send data: (%d) %s\n", errno, strerror(errno));
            return R_FAIL;
        }
        totalbytes += (unsigned)bytessofar;
    }

    net->bytes_out += (unsigned long)totalbytes;
    return R_SUCCESS;
}

SO_PUBLIC HRESULT recvWrap(RZBNC *net, void *buf, unsigned bufsize)
{
    int sock = net->sock;
    int bytessofar;
    unsigned totalbytes;
    uint8_t *buffer = (uint8_t *)buf;

    totalbytes = 0;
    while (totalbytes < bufsize)
    {
        bytessofar = recv(sock, buffer+totalbytes, bufsize-totalbytes, 0);
        if (bytessofar < 1)
        {
            fprintf(stderr, "Could not receive data: (%d) %s\n", errno, strerror(errno));
            return R_FAIL;
        }
        totalbytes += (unsigned)bytessofar;
    }

    net->bytes_in += (unsigned long)totalbytes;
    return R_SUCCESS;
}

/*
 *
 * Protocol sanity checks
 *
 */
SO_PUBLIC void protocolFill(SNAKECHARMER *scpacket)
{
    memset(scpacket, 0, sizeof(*scpacket));
    memcpy(scpacket->magic, "NRT", 3);
    scpacket->version = 1;
}

SO_PUBLIC HRESULT acknowledge(RZBNC *net, SNAKECHARMER *scpacket)
{
    protocolFill(scpacket);
    scpacket->msgtype = ACK;

    if ((sendWrap(net, scpacket, sizeof(*scpacket))) != R_SUCCESS)
        return R_FAIL;

    return R_SUCCESS;
}

SO_PUBLIC HRESULT sendFailure(RZBNC *net, SNAKECHARMER *scpacket)
{
    protocolFill(scpacket);
    scpacket->msgtype = RST;

    if ((sendWrap(net, scpacket, sizeof(*scpacket))) != R_SUCCESS)
        return R_FAIL;

    return R_SUCCESS;
}


SO_PUBLIC HRESULT protocolCheck(SNAKECHARMER *scpacket)
{
    if (!memcmp(scpacket->magic, "NRT", 3) && scpacket->version == 1)
        return R_SUCCESS;
    else
        return R_FAIL;
}

static volatile sig_atomic_t stop_processing = 0;

static void handleSignal(int sig)
{
    stop_processing = 1;
}

SO_PUBLIC HRESULT rzbServer(const char *port, rzb_thread_func_t func, unsigned to_secs, const char *description)
{
    int sock, conn;
    struct sockaddr_in dest_addr;
    socklen_t sockaddrsize;
    HRESULT retval;
    THREADARGS *threadargs;
    SNAKECHARMER scpacket;
    struct sigaction sa;
    RZBNetworkConnection *net;

    sock = MakeListener(port);

    printf("Now listening on port %s\n", port);

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = &handleSignal;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGQUIT, &sa, NULL);

    retval = R_SUCCESS;
    while (!stop_processing)
    {
        sockaddrsize = sizeof(dest_addr);
        if ((conn = accept(sock, (struct sockaddr *)&dest_addr, &sockaddrsize)) == -1)
        {
            if (errno != EINTR)
            {
                retval = R_FAIL;
                fprintf(stderr, "Accept error: (%d) %s\n", errno, strerror(errno));
                perror("Accept error");
                break;
            }
        }
        else
        {
            if (rzb_debug)
            {
                char ipstr[INET_ADDRSTRLEN];
                inet_ntop(dest_addr.sin_family, &dest_addr.sin_addr, ipstr, sizeof(ipstr));
                printf("Accepted %u connection from %s\n", dest_addr.sin_family, ipstr);
            }
            net = MakeConnection(conn);
            if (net)
            {
                threadargs = calloc(1, sizeof(*threadargs));
                if (threadargs)
                {
                    threadargs->net = net;
                    retval = threadme(func, threadargs, description);
                }
                else
                    retval = R_FAIL;
            }
            else
            {
                threadargs = NULL;
                retval = R_FAIL;
            }
            if (retval == R_FAIL)
            {
                printf("\nThread dropped!\n");
                if (net)
                    sendFailure(net, &scpacket);
                NetworkConnectionDestroy(net);
                if (threadargs)
                    free(threadargs);
            }
        }
    }
    waitForIdle(to_secs);
    close(sock);
    return retval;
}

