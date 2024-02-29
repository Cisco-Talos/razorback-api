#include "config.h"

#include <uuid/uuid.h>
#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
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
#include <pthread.h>
#include <dlfcn.h>
#include <dirent.h>
#include <arpa/inet.h>
#include <signal.h>

#include "rzb_api_types.h"
#include "rzb_client.h"
#include "rzb_utils.h"
#include "rzb_api.h"
#include "rzb_network.h"
#include "rzb_thread.h"
#include "rzb_conf.h"
#include "rzb_detection_api.h"

/*
 *
 * UTILITY FUNCTIONS
 *
 */

#if 0
HRESULT writeToFile(int fd, uint8_t *file, unsigned size)
{
    int bytessofar;
    unsigned totalbytes;

    totalbytes = 0;
    while (totalbytes < size)
    {
        bytessofar = write(fd, file+totalbytes, size-totalbytes);
        if (bytessofar == -1)
        {
            perror("Could not send data");
            return R_FAIL;
        }
        totalbytes += (unsigned)bytessofar;
    }

    return R_SUCCESS;
}
#endif

//static 
HRESULT callByType(BLOCK_META_DATA *metaData)
{
    unsigned i;
    handlerNode *temp = head;
    uuid_t *supportedtype;

    do
    {
        supportedtype = temp->acceptedTypes;
        for (i = 0; i < temp->numTypes; i++)
        {
            if (!uuid_compare(supportedtype[i], metaData->datatype))
            {
                temp->fp(metaData);
                //head = temp->next;
                //return R_SUCCESS;
            }
        }
        temp = temp->next;
    } while (temp != NULL);

    return R_SUCCESS;
}

static void *masterOfNuggets(THREADARGS *threadargs)
{
    RZBNetworkConnection *net = threadargs->net;
    SNAKECHARMER scpacket;
    REQPACKET reqpacket;
    BLOCK_META_DATA metaData;
    ALERT_HEADER header;

    if (recvWrap(net, &scpacket, sizeof(scpacket)) != R_SUCCESS)
    {
        NetworkConnectionDestroy(net);
        unthreadme(threadargs);
        return NULL;
    }

    if (protocolCheck(&scpacket) != R_SUCCESS)
    {
        NetworkConnectionDestroy(net);
        unthreadme(threadargs);
        return NULL;
    }

    switch (scpacket.msgtype)
    {
        case ALRT:
            if (recvWrap(net, &header, sizeof(header)) != R_SUCCESS)
                break;
            acknowledge(net, &scpacket);
            metaData.data = (uint8_t *)&header;
            metaData.size = sizeof(header);
            uuid_copy(metaData.datatype, ALERT_OUTPUT);
            callByType(&metaData);
            break;

        case REQ:
            if (recvWrap(net, &reqpacket, sizeof(reqpacket)) != R_SUCCESS)
                break;

            if (recvWrap(net, &metaData, sizeof(metaData)) != R_SUCCESS)
                break;

            if ((metaData.data = malloc(reqpacket.filesize)) == NULL)
                break;

            metaData.parent_data = NULL;

            scpacket.msgtype = ACK;

            if (sendWrap(net, &scpacket, sizeof(scpacket)) != R_SUCCESS)
            {
                free(metaData.data);
                break;
            }

            if (recvWrap(net, metaData.data, reqpacket.filesize) != R_SUCCESS)
            {
                free(metaData.data);
                break;
            }

            //This is where you would call detection function
            callByType(&metaData);
            free(metaData.data);
            break;

        default:
            printf("\nInvalid Message Type\n");
            scpacket.msgtype = RST;
            sendWrap(net, &scpacket, sizeof(scpacket));
            break;
    }

    NetworkConnectionDestroy(net);
    threadargs->net = NULL;
    unthreadme(threadargs);
    return NULL;
}

static HRESULT initHandlers(RZBConfig *rzbconfig)
{
    char soname[PATH_MAX];
    void *dlHandle;
    DIR *dp;
    struct dirent *ep;
    initNugFunc init;

    char *errstr;

    dp = opendir(rzbconfig->handlerdir);
    if (dp == NULL)
    {
        printf("Unable to open directory %s\n", rzbconfig->handlerdir);
        exit (-1);
    }

    while ((ep = readdir(dp)) != NULL)
    {

        if (ep->d_type != DT_REG || strcmp(ep->d_name, ".") == 0 || strcmp(ep->d_name, "..") == 0)
            continue;

        snprintf(soname, sizeof(soname), "%s/%s", rzbconfig->handlerdir, ep->d_name);

        dlHandle = dlopen(soname, RTLD_LOCAL | RTLD_NOW);
        if (dlHandle == NULL)
        {
            errstr = dlerror();
            if (rzb_debug)
                printf("Failed to open %s \n", errstr);
            continue;
        }

        *(void **)&init = dlsym(dlHandle, "initNug");
        if (init == NULL)
        {
            errstr = dlerror();
            if (rzb_debug)
                printf("Failed to resolve initNug() for %s.\n", errstr);
            continue;
        }

        if (init(&rzb_detection) != R_SUCCESS)
            printf("Failed to register functions for %s\n", ep->d_name);
        else
            printf("Functions registered for %s\n", ep->d_name);
    }

    return R_SUCCESS;

}

SO_PUBLIC HRESULT nuggetServer(const char *configDir)
{
    HRESULT retval;

    readApiConfig(configDir);

    initHandlers(&rzbconfig);

    retval = rzbServer(rzbconfig.nugport, &masterOfNuggets, 30, "nuggetServer");
    return retval;
}


HRESULT remoteMD5CacheCheck(unsigned *eventid, unsigned char *hash, unsigned length, uuid_t datatype,
                            NetworkCacheResponse *response)
{
    SNAKECHARMER scpacket;
    MD5CACHEREQUEST request;
    MD5CACHERESPONSE resp;

    RZBNetworkConnection *net = ConnectMe(rzbconfig.dsrvaddr, rzbconfig.dsrvport);
    if (!net)
    {
        printf("connection to %s:%s failed\n", rzbconfig.dsrvaddr, rzbconfig.dsrvport);
        return R_FAIL;
    }

    protocolFill(&scpacket);
    scpacket.msgtype = MD5CHECK;

    request.filesize = length;
    memcpy(request.hash, hash, RZB_HASH_SIZE);
    uuid_copy(request.datatype, datatype);


    //Send packet
    if (sendWrap(net, &scpacket, sizeof(scpacket)) != R_SUCCESS)
    {
        NetworkConnectionDestroy(net);
        return R_FAIL;
    }

    if (sendWrap(net, &request, sizeof(request)) != R_SUCCESS)
    {
        NetworkConnectionDestroy(net);
        return R_FAIL;
    }

    //Recv ACK (hopefully)
    if (recvWrap(net, &scpacket, sizeof(scpacket)) != R_SUCCESS)
    {
        NetworkConnectionDestroy(net);
        return R_FAIL;
    }

    if (scpacket.msgtype != MD5RESPONSE)
    {
        printf("Unknown response code, unable to proceed\n");
        NetworkConnectionDestroy(net);
        return R_FAIL;
    }

    if (recvWrap(net, &resp, sizeof(resp)) != R_SUCCESS)
    {
        NetworkConnectionDestroy(net);
        return R_FAIL;
    }

    switch (resp.result)
    {
        case KNOWN_BAD_RESPONSE:
            *response = KNOWN_BAD_RESPONSE;
            printf("File is known to be bad\n");
            break;

        case KNOWN_GOOD_RESPONSE:
            *response = KNOWN_GOOD_RESPONSE;
            printf("File is known to be good\n");
            break;

        case UNKNOWN_MD5_RESPONSE:
            *response = UNKNOWN_MD5_RESPONSE;
            *eventid = resp.detectionID;
            break;

        default:
            printf("Bad response, FAILURE\n");
            NetworkConnectionDestroy(net);
            return R_FAIL;

    }
    NetworkConnectionDestroy(net);
    return R_SUCCESS;
}

