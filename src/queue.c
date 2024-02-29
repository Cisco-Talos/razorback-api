#include "config.h"
#include <razorback/debug.h>
#include <razorback/queue.h>
#include <razorback/log.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include "runtime_config.h"

#define MBUF_SIZE 1024
struct StompHeader 
{
    char *sName;
    char *sValue;
    struct StompHeader *pNext;
};
struct StompMessage {
    char * sVerb;
    struct StompHeader *pHead;
    struct BinaryBuffer *pBody;
};

static void 
Queue_Destroy_Stomp_Message (struct StompMessage *p_pMessage) 
{
    struct StompHeader *l_pCurrent;

    if (p_pMessage->sVerb != NULL)
        free(p_pMessage->sVerb);
    
    l_pCurrent = p_pMessage->pHead;
    while (l_pCurrent != NULL) 
    {
        p_pMessage->pHead = l_pCurrent->pNext;
        if (l_pCurrent->sName != NULL)
            free(l_pCurrent->sName);
        if (l_pCurrent->sValue != NULL)
            free(l_pCurrent->sValue);
        free(l_pCurrent);
        l_pCurrent = p_pMessage->pHead;
    }

    if (p_pMessage->pBody != NULL)
        BinaryBuffer_Destroy(p_pMessage->pBody);

    free(p_pMessage);
}

static struct StompMessage *
Queue_Read_Message(struct Socket *p_pSocket) 
{
    ASSERT (p_pSocket !=NULL);
    struct StompMessage *l_pMessage;
    struct StompHeader *l_pHeader;
    char *l_sBuffer;
    size_t l_iMessageLen;
    uint32_t l_iBodySize =0;

    char *l_pHeaderItem;
//    char *l_pHeaderTokenizer;

    if ((l_sBuffer = calloc(MBUF_SIZE, sizeof(char))) == NULL )
    {
        rzb_log(LOG_ERR, "%s: Failed to allocate message buffer", __func__);
        return NULL;
    }

    if ((l_pMessage = calloc (1, sizeof (struct StompMessage))) == NULL)
    {
        rzb_log(LOG_ERR, "%s: Failed to allocate message struct", __func__);
        free(l_sBuffer);
        return NULL;
    }
   
readverb:
    // Read the VERB
    memset (l_sBuffer, 0, MBUF_SIZE);
    if ((l_iMessageLen = Socket_Rx_Until (p_pSocket, (uint8_t*)l_sBuffer, MBUF_SIZE, '\n')) == 0)
    {
        if (errno != EINTR)
            rzb_log (LOG_ERR, "%s: failed due to failure of Socket_Rx_Until", __func__);

        Queue_Destroy_Stomp_Message(l_pMessage);
        free(l_sBuffer);
        return NULL;
    }
    if (l_iMessageLen == 1 && l_sBuffer[0] == '\n')
    {
        goto readverb;
    }
    if ((l_pMessage->sVerb = calloc(l_iMessageLen, sizeof(char))) == NULL)
    {
        rzb_log(LOG_ERR, "%s: failed due to failure of calloc", __func__);
        Queue_Destroy_Stomp_Message(l_pMessage);
        free(l_sBuffer);
        return NULL;
    }
    l_sBuffer[l_iMessageLen -1] = '\0';
    // Copy the verb
    strncpy(l_pMessage->sVerb, l_sBuffer, l_iMessageLen);
#ifdef STOMP_DEBUG
    rzb_log(LOG_DEBUG, "%s: Message Verb: %s - %u", __func__, l_pMessage->sVerb, l_iMessageLen);
#endif
    memset (l_sBuffer, 0, MBUF_SIZE);
    if ((l_iMessageLen = Socket_Rx_Until (p_pSocket, (uint8_t*)l_sBuffer, MBUF_SIZE, '\n')) == 0)
    {
        rzb_log (LOG_ERR,
                 "%s: failed due to failure of Socket_Rx_Until", __func__);
        Queue_Destroy_Stomp_Message(l_pMessage);
        free(l_sBuffer);
        return NULL;
    }
    

    while (l_iMessageLen != 1 && l_sBuffer[0] != '\n') // End of headers
    {
        l_sBuffer[l_iMessageLen -1] = '\0';
        l_pHeaderItem = strchr(l_sBuffer, ':');
        *l_pHeaderItem = '\0';
        if ((l_pHeader = calloc(1,sizeof(struct StompHeader))) == NULL)
        {
            rzb_log(LOG_ERR, "%s: Failed to allocate new header", __func__);
            Queue_Destroy_Stomp_Message(l_pMessage);
            free(l_sBuffer);
            return NULL;
        }
        if ((l_pHeader->sName = calloc(strlen(l_sBuffer)+1, sizeof(char))) == NULL)
        {
            rzb_log(LOG_ERR, "%s: Failed to allocate new header name", __func__);
            Queue_Destroy_Stomp_Message(l_pMessage);
            free(l_pHeader);
            free(l_sBuffer);
            return NULL;
        }
        strcpy(l_pHeader->sName, l_sBuffer);
        l_pHeaderItem++;

        if ((l_pHeader->sValue = calloc(strlen(l_pHeaderItem)+1, sizeof(char))) == NULL)
        {
            rzb_log(LOG_ERR, "%s: Failed to allocate new header value", __func__);
            Queue_Destroy_Stomp_Message(l_pMessage);
            free(l_pHeader->sName);
            free(l_pHeader);
            free(l_sBuffer);
            return NULL;
        }
        strcpy(l_pHeader->sValue, l_pHeaderItem);

        // Add the item to the list
        l_pHeader->pNext = l_pMessage->pHead;
        l_pMessage->pHead = l_pHeader;

#ifdef STOMP_DEBUG
        rzb_log(LOG_DEBUG, "%s: Message Header: %s:%s", __func__, l_pHeader->sName, l_pHeader->sValue);
#endif

        if (strcasecmp(l_pHeader->sName, "content-length") == 0) 
        {
            l_iBodySize =strtoul(l_pHeader->sValue, NULL, 10);
            if (l_iBodySize == 0)
            {
                rzb_log(LOG_ERR, "%s: Failed to parse message lenght: %s", __func__, l_pHeader->sValue);
                Queue_Destroy_Stomp_Message(l_pMessage);
                free(l_sBuffer);
                return NULL;
            }
#ifdef STOMP_DEBUG
            rzb_log(LOG_DEBUG, "%s: Found content length: %d", __func__, l_iBodySize);
#endif
        }

        // Read the next line
        memset (l_sBuffer, 0, MBUF_SIZE);
        if ((l_iMessageLen = Socket_Rx_Until (p_pSocket, (uint8_t*)l_sBuffer, MBUF_SIZE, '\n')) == 0)
        {
            rzb_log (LOG_ERR,
                     "%s failed due to failure of Socket_Rx_Until", __func__);
            Queue_Destroy_Stomp_Message(l_pMessage);
            free(l_sBuffer);
            return NULL;
        }
    }

    if (l_iBodySize != 0) // Read the message body
    {
		if (l_iBodySize > g_storagethreshold) {
			//Need to store it
		}
		else if ((l_pMessage->pBody = BinaryBuffer_Create(l_iBodySize)) == NULL) 
        {
			//Need to store it
            rzb_log (LOG_ERR,
                     "%s: failed to allocate binary buffer", __func__);
            Queue_Destroy_Stomp_Message(l_pMessage);
            free(l_sBuffer);
            return NULL;
        }
        if (!Socket_Rx(p_pSocket, l_iBodySize, l_pMessage->pBody->pBuffer)) 
        {
            rzb_log (LOG_ERR,
                     "%s: failed to read message body", __func__);
            Queue_Destroy_Stomp_Message(l_pMessage);
            free(l_sBuffer);
            return NULL;
        }
        // read the final '\0'
        if (!Socket_Rx (p_pSocket, 1, (uint8_t *)l_sBuffer))
        {
            rzb_log (LOG_ERR, "%s: failed due to Socket_Rx", __func__);
            Queue_Destroy_Stomp_Message(l_pMessage);
            free(l_sBuffer);
            return NULL;
        }

        // some unaccountable error here
        if (l_sBuffer[0] != '\0')
        {
            rzb_log (LOG_ERR, "%s: failed due to unknown error", __func__);
            Queue_Destroy_Stomp_Message(l_pMessage);
            free(l_sBuffer);
            return NULL;
        }

    }
    else
    {
        // Read until we get a null
        l_sBuffer[0] = 'A'; //< Any value not \0
        while (l_sBuffer[0] != '\0') 
        {
            if (!Socket_Rx (p_pSocket, 1, (uint8_t *)l_sBuffer))
            {
                rzb_log (LOG_ERR, "%s: failed due to Socket_Rx", __func__);
                Queue_Destroy_Stomp_Message(l_pMessage);
                free(l_sBuffer);
                return NULL;
            }
        }
    }
    free(l_sBuffer); 
    return l_pMessage;
}

static bool 
Queue_Send_Message(struct Socket *p_pSocket, struct StompMessage *p_pMessage)
{
    char *l_sLine;
    struct StompHeader *l_pHeader;

    if (asprintf(&l_sLine, "%s\n", p_pMessage->sVerb) == -1)
    {
        rzb_log(LOG_ERR, "%s: Failed to allocate verb", __func__);
        return false;
    }
    if (!Socket_Tx(p_pSocket, strlen(l_sLine), (uint8_t *)l_sLine)) {
        rzb_log(LOG_ERR, "%s: Failed to send verb", __func__);
        free(l_sLine);
        return false;
    }
    free(l_sLine);
    l_pHeader = p_pMessage->pHead;
    while (l_pHeader != NULL)
    {
        if (asprintf(&l_sLine, "%s:%s\n", l_pHeader->sName, l_pHeader->sValue) == -1)
        {
            rzb_log(LOG_ERR, "%s: Failed to alloc header", __func__);
            return false;
        }
        if (!Socket_Tx(p_pSocket, strlen(l_sLine), (uint8_t *)l_sLine)) 
        {
            rzb_log(LOG_ERR, "%s: Failed to send header", __func__);
            free(l_sLine);
            return false;
        }
        free(l_sLine);
        l_pHeader = l_pHeader->pNext;
    }
    if (!Socket_Tx(p_pSocket, 1, (uint8_t *)"\n"))
    {
        rzb_log(LOG_ERR, "%s: Failed to send end of header", __func__);
        return false;
    }
    if (p_pMessage->pBody != NULL)
    {
        if (!Socket_Tx(p_pSocket, p_pMessage->pBody->iLength, p_pMessage->pBody->pBuffer))
        {
            rzb_log(LOG_ERR, "%s: Failed to send message body", __func__);
            return false;
        }
    }
    if (!Socket_Tx(p_pSocket, 1, (uint8_t *)"\0"))
    {
        rzb_log(LOG_ERR, "%s: Failed to send end of message", __func__);
        return false;
    }
    return true;
}

static struct StompMessage *
Queue_Message_Create(const char * p_sVerb)
{
    struct StompMessage *l_pMessage;
    if ((l_pMessage = calloc(1, sizeof(struct StompMessage))) == NULL)
    {
        rzb_log(LOG_ERR, "%s: Failed to alloc message", __func__);
        return NULL;
    }
    if ((l_pMessage->sVerb = calloc(strlen(p_sVerb)+1, sizeof(char))) == NULL)
    {
        rzb_log(LOG_ERR, "%s: Failed to allocate verb", __func__);
        free(l_pMessage);
        return NULL;
    }
    strcpy(l_pMessage->sVerb, p_sVerb);
    return l_pMessage;
}

static bool
Queue_Message_Add_Header(struct StompMessage *p_pMessage, const char *p_sName, const char *p_sValue)
{
    struct StompHeader *l_pHeader;
    if ((l_pHeader = calloc(1, sizeof (struct StompHeader))) == NULL)
    {
        rzb_log(LOG_ERR, "%s: Failed to alloc new header", __func__);
        return false;
    }
    if ((l_pHeader->sName = calloc(1, strlen(p_sName)+1)) == NULL)
    {
        rzb_log(LOG_ERR, "%s: Failed to alloc new header name", __func__);
        free(l_pHeader);
        return false;
    }
    if ((l_pHeader->sValue = calloc(1, strlen(p_sValue)+1)) == NULL)
    {
        rzb_log(LOG_ERR, "%s: Failed to alloc new header value", __func__);
        free(l_pHeader->sName);
        free(l_pHeader);
        return false;
    }
    strcpy(l_pHeader->sName, p_sName);
    strcpy(l_pHeader->sValue, p_sValue);
    l_pHeader->pNext = p_pMessage->pHead;
    p_pMessage->pHead = l_pHeader;
    return true;
}

static char *
Queue_Message_Get_Header(struct StompMessage *p_pMessage, const char *p_sName)
{
    char * l_sReturn = NULL;
    struct StompHeader *l_pHeader;
    l_pHeader = p_pMessage->pHead;
    while (l_pHeader != NULL) 
    {
        if (strcasecmp(l_pHeader->sName, p_sName) == 0)
        {
            l_sReturn = l_pHeader->sValue;
            break;
        }
        l_pHeader = l_pHeader->pNext;
    }
    return l_sReturn;
}



static struct Socket *
Queue_Connect_Socket( const char * p_sAddress,
                        int16_t p_iPort, const char * p_sUsername,
                        const char * p_sPassword, bool useSSL)
{
    ASSERT (p_sAddress != NULL);
    ASSERT (p_sUsername != NULL);
    ASSERT (p_sPassword != NULL);

    struct Socket *l_pSocket;
    struct StompMessage *l_pMessage;

    // open the socket
    if (useSSL)
        l_pSocket = SSL_Socket_Connect ((uint8_t *)p_sAddress, p_iPort);
    else
        l_pSocket = Socket_Connect ((uint8_t *)p_sAddress, p_iPort);

    if (l_pSocket == NULL )
    {
        if (useSSL)
            rzb_log (LOG_ERR,
                     "%s: failed due to failure of SSL_Socket_Connect", __func__);
        else 
            rzb_log (LOG_ERR,
                     "%s: failed due to failure of Socket_Connect", __func__);
        return NULL;
    }

    if ((l_pMessage= Queue_Message_Create("CONNECT")) == NULL) 
    {
        rzb_log(LOG_ERR, "%s: Failed to create connect message", __func__);
        Socket_Close(l_pSocket);
        return NULL;
    }

    // Put headers in backwards 
    if (!Queue_Message_Add_Header(l_pMessage, "passcode", p_sPassword) || 
            !Queue_Message_Add_Header(l_pMessage, "login", p_sUsername))
    {
        rzb_log(LOG_ERR, "%s: Failed to add auth headers", __func__);
        Queue_Destroy_Stomp_Message(l_pMessage);
        Socket_Close(l_pSocket);
        return NULL;
    }

    // send the Connect message
    if (!Queue_Send_Message(l_pSocket, l_pMessage))
    {
        rzb_log(LOG_ERR, "%s: Failed to send connect message", __func__);
        Socket_Close(l_pSocket);
        Queue_Destroy_Stomp_Message(l_pMessage);
        return NULL;
    }
    Queue_Destroy_Stomp_Message(l_pMessage); //< The Connect message

    if ((l_pMessage = Queue_Read_Message(l_pSocket)) == NULL)
    {
        rzb_log(LOG_ERR, "%s: Failed to read connection response", __func__);
        Socket_Close(l_pSocket);
        return false;
    }
    if (strcasecmp(l_pMessage->sVerb, "CONNECTED") != 0)
    {
        rzb_log (LOG_ERR,
                 "%s: failed due to failure of strncasecmp ( CONNECTED )", __func__);
        return NULL;
    }

    Queue_Destroy_Stomp_Message(l_pMessage);

    // done
    return l_pSocket;
}

static bool
Queue_BeginReading (struct Queue *p_pQ)
{
    ASSERT (p_pQ != NULL);
    struct StompMessage *l_pMessage;


    // send the subscribe message

    if ((l_pMessage= Queue_Message_Create("SUBSCRIBE")) == NULL) 
    {
        rzb_log(LOG_ERR, "%s: Failed to create subscribe message", __func__);
        return false;
    }

    if (!Queue_Message_Add_Header(l_pMessage, "destination", p_pQ->sName) ||
            !Queue_Message_Add_Header(l_pMessage, "ack", "client"))
    {
        rzb_log(LOG_ERR, "%s: Failed to add destination headers", __func__);
        Queue_Destroy_Stomp_Message(l_pMessage);
        return false;
    }

    // send the Connect message
    if (!Queue_Send_Message(p_pQ->pReadSocket, l_pMessage))
    {
        rzb_log(LOG_ERR, "%s: Failed to send subscribe message", __func__);
        Queue_Destroy_Stomp_Message(l_pMessage);
        return false;
    }

    Queue_Destroy_Stomp_Message(l_pMessage);

    return true;
}

static bool
Queue_EndReading (struct Queue *p_pQ)
{
    ASSERT (p_pQ != NULL);
    struct StompMessage *l_pMessage;


    // send the subscribe message

    if ((l_pMessage= Queue_Message_Create("UNSUBSCRIBE")) == NULL) 
    {
        rzb_log(LOG_ERR, "%s: Failed to create unsubscribe message", __func__);
        return false;
    }

    if (!Queue_Message_Add_Header(l_pMessage, "destination", p_pQ->sName))
    {
        rzb_log(LOG_ERR, "%s: Failed to add destination headers", __func__);
        Queue_Destroy_Stomp_Message(l_pMessage);
        return false;
    }

    // send the Connect message
    if (!Queue_Send_Message(p_pQ->pReadSocket, l_pMessage))
    {
        rzb_log(LOG_ERR, "%s: Failed to send unsubscribe message", __func__);
        Queue_Destroy_Stomp_Message(l_pMessage);
        return false;
    }
    Queue_Destroy_Stomp_Message(l_pMessage);

    return true;
}

SO_PUBLIC struct Queue *
Queue_Create (const uint8_t * p_sQueueName, int p_iFlags)
{
    ASSERT (p_sQueueName != NULL);

    struct Queue *l_pQueue;
    if ((l_pQueue = calloc (1, sizeof (struct Queue))) == NULL)
    {
        rzb_log (LOG_ERR, "%s: Failed to alloc new queue", __func__);
        return NULL;
    }

    if ((l_pQueue->sName = calloc(strlen((char *)p_sQueueName)+1, sizeof(char))) == NULL)
    {
        rzb_log (LOG_ERR, "%s: Failed to alloc new queue name", __func__);
        free(l_pQueue);
        return NULL;
    }
    strcpy(l_pQueue->sName, (char *)p_sQueueName);
    pthread_mutex_init (&l_pQueue->mReadMutex, NULL);
    pthread_mutex_init (&l_pQueue->mWriteMutex, NULL);
    l_pQueue->iFlags = p_iFlags;

    if ((p_iFlags & QUEUE_FLAG_RECV) == QUEUE_FLAG_RECV)
    {
        if ((l_pQueue->pReadSocket = 
                    Queue_Connect_Socket(Config_getMqHost (), 
                        Config_getMqPort (), Config_getMqUser (), 
                        Config_getMqPassword (), Config_getMqSSL())) == NULL)
        {
            rzb_log (LOG_ERR,
                     "%s: failed due to failure of Queue_Connect_Socket (Read)", __func__);
            Queue_Terminate(l_pQueue);
            return NULL;
        }
        if (!Queue_BeginReading (l_pQueue))
        {
            rzb_log (LOG_ERR,
                     "%s: failed due to failure of Queue_BeginReading", __func__);
            return NULL;
        }
    }

    if ((p_iFlags & QUEUE_FLAG_SEND) == QUEUE_FLAG_SEND)
    {
        if ((l_pQueue->pWriteSocket = 
                    Queue_Connect_Socket(Config_getMqHost (), 
                        Config_getMqPort (), Config_getMqUser (), 
                        Config_getMqPassword (), Config_getMqSSL())) == NULL)
        {
            rzb_log (LOG_ERR,
                     "%s: failed due to failure of Queue_Connect_Socket (Write)", __func__);
            Queue_Terminate(l_pQueue);
            return NULL;
        }
    }

    return l_pQueue;
}

SO_PUBLIC void
Queue_Terminate (struct Queue *p_pQ)
{
    ASSERT (p_pQ != NULL);
    struct StompMessage *l_pMessage;
    pthread_mutex_lock (&p_pQ->mReadMutex);
    pthread_mutex_lock (&p_pQ->mWriteMutex);
    if ((l_pMessage= Queue_Message_Create("DISCONNECT")) == NULL) 
    {
        rzb_log(LOG_ERR, "%s: Failed to create disconnect message", __func__);
    }
 
    if ((p_pQ->iFlags & QUEUE_FLAG_RECV) == QUEUE_FLAG_RECV &&
            p_pQ->pReadSocket != NULL)
    {
        Queue_EndReading (p_pQ);
        if (l_pMessage != NULL)
            Queue_Send_Message(p_pQ->pReadSocket, l_pMessage);
        Socket_Close (p_pQ->pReadSocket);
    }
    if ((p_pQ->iFlags & QUEUE_FLAG_SEND) == QUEUE_FLAG_SEND &&
            p_pQ->pWriteSocket != NULL)
    {
        if (l_pMessage != NULL)
            Queue_Send_Message(p_pQ->pWriteSocket, l_pMessage);
        Socket_Close (p_pQ->pWriteSocket);
    }

    if (l_pMessage != NULL)
        Queue_Destroy_Stomp_Message(l_pMessage);

    pthread_mutex_unlock (&p_pQ->mReadMutex);
    pthread_mutex_unlock (&p_pQ->mWriteMutex);
    pthread_mutex_destroy (&p_pQ->mReadMutex);
    pthread_mutex_destroy (&p_pQ->mWriteMutex);
    free(p_pQ->sName);
    free(p_pQ);
}

SO_PUBLIC struct BinaryBuffer *
Queue_Get (struct Queue *p_pQ)
{
    ASSERT (p_pQ);
    pthread_mutex_lock (&p_pQ->mReadMutex);

    struct BinaryBuffer *l_pBuffer = NULL;
    struct StompMessage *l_pMessage = NULL;
    struct StompMessage *l_pAck = NULL;
    char * l_sMessageId;

    if (( l_pMessage = Queue_Read_Message (p_pQ->pReadSocket)) == NULL)
    {
        if ( errno != EINTR )
            rzb_log (LOG_ERR, "%s: failed due to failure of Queue_Read_Message", __func__);

        pthread_mutex_unlock (&p_pQ->mReadMutex);
        return NULL;
    }
    if (strcasecmp(l_pMessage->sVerb, "MESSAGE") == 0)
    {
        if ((l_sMessageId = Queue_Message_Get_Header(l_pMessage, "message-id")) == NULL)
        {
            rzb_log(LOG_ERR, "%s: Failed to get message-id", __func__);
            pthread_mutex_unlock (&p_pQ->mReadMutex);
            Queue_Destroy_Stomp_Message(l_pMessage);
            return NULL;
        }
        l_pBuffer = l_pMessage->pBody;
        l_pMessage->pBody = NULL;

        // Send Ack
        if ((l_pAck = Queue_Message_Create("ACK")) == NULL)
        {
            rzb_log(LOG_ERR, "%s: Failed to create ACK", __func__);
            BinaryBuffer_Destroy(l_pBuffer);
            Queue_Destroy_Stomp_Message(l_pMessage);
            pthread_mutex_unlock (&p_pQ->mReadMutex);
            return NULL;
        }
        if (!Queue_Message_Add_Header(l_pAck, "message-id", l_sMessageId))
        {
            rzb_log(LOG_ERR, "%s: Failed to add ack message-id headers", __func__);
            Queue_Destroy_Stomp_Message(l_pAck);
            Queue_Destroy_Stomp_Message(l_pMessage);
            BinaryBuffer_Destroy(l_pBuffer);
            pthread_mutex_unlock (&p_pQ->mReadMutex);
            return NULL;
        }
        if (!Queue_Send_Message(p_pQ->pReadSocket, l_pAck))
        {
            rzb_log(LOG_ERR, "%s: Failed to send ack message", __func__);
            Queue_Destroy_Stomp_Message(l_pAck);
            Queue_Destroy_Stomp_Message(l_pMessage);
            BinaryBuffer_Destroy(l_pBuffer);
            pthread_mutex_unlock (&p_pQ->mReadMutex);
            return NULL;
        }
        Queue_Destroy_Stomp_Message(l_pMessage);
        Queue_Destroy_Stomp_Message(l_pAck);

        pthread_mutex_unlock (&p_pQ->mReadMutex);
        return l_pBuffer;
    }

    errno = EAGAIN;
    Queue_Destroy_Stomp_Message(l_pMessage);
    pthread_mutex_unlock (&p_pQ->mReadMutex);
    return NULL;
}

SO_PUBLIC bool
Queue_Put (struct Queue * p_pQueue,  struct BinaryBuffer * p_pBuffer)
{
    ASSERT (p_pQueue != NULL);
    ASSERT (p_pBuffer != NULL);
    
    pthread_mutex_lock (&p_pQueue->mWriteMutex);

    struct StompMessage *l_pMessage;
    char l_sMessageId[MBUF_SIZE];
    char l_sMessageLen[MBUF_SIZE];
    char *l_pReceiptId = NULL;

    time_t l_tTime = time(NULL);

    snprintf(l_sMessageId, MBUF_SIZE, "message-%ju", (uintmax_t)l_tTime);
    snprintf(l_sMessageLen, MBUF_SIZE, "%i", p_pBuffer->iLength);

    if (p_pBuffer->iLength != p_pBuffer->iOffset) 
    {
        rzb_log(LOG_ERR, "%s: Message Head Calculation error: Len: %d, Offset: %d", __func__, p_pBuffer->iLength, p_pBuffer->iOffset);
        return false;
    }

    if ((l_pMessage = Queue_Message_Create("SEND")) == NULL)
    {
        rzb_log(LOG_ERR, "%s: Failed to create SEND", __func__);
        pthread_mutex_unlock (&p_pQueue->mWriteMutex);
        return false;
    }

    l_pMessage->pBody = p_pBuffer;

    if (!Queue_Message_Add_Header(l_pMessage, "receipt", l_sMessageId) ||
            !Queue_Message_Add_Header(l_pMessage, "destination", p_pQueue->sName) ||
            !Queue_Message_Add_Header(l_pMessage, "amq-msg-type", "bytes") ||
            !Queue_Message_Add_Header(l_pMessage, "content-length", l_sMessageLen))
    {
        rzb_log(LOG_ERR, "%s: Failed to add ack message-id headers", __func__);
        l_pMessage->pBody = NULL; // or it will be destroyed by Queue_Destroy_Stomp_Message()
        Queue_Destroy_Stomp_Message(l_pMessage);
        pthread_mutex_unlock (&p_pQueue->mWriteMutex);
        return false;
    }

    if (!Queue_Send_Message(p_pQueue->pWriteSocket, l_pMessage))
    {
        rzb_log(LOG_ERR, "%s: Failed to send message", __func__);
        l_pMessage->pBody = NULL; // or it will be destroyed by Queue_Destroy_Stomp_Message()
        Queue_Destroy_Stomp_Message(l_pMessage);
        pthread_mutex_unlock (&p_pQueue->mWriteMutex);
        return false;
    }
    l_pMessage->pBody = NULL; // or it will be destroyed by Queue_Destroy_Stomp_Message()
    Queue_Destroy_Stomp_Message(l_pMessage);

    if (( l_pMessage = Queue_Read_Message (p_pQueue->pWriteSocket)) == NULL)
    {
        rzb_log (LOG_ERR,
                 "%s: failed due to failure of Queue_Read_Message", __func__);
        pthread_mutex_unlock (&p_pQueue->mWriteMutex);
        return false;
    }
    if (strcasecmp(l_pMessage->sVerb, "RECEIPT") == 0)
    {
        if ((l_pReceiptId = Queue_Message_Get_Header(l_pMessage, "receipt-id")) == NULL)
        {
            rzb_log(LOG_ERR, "%s: Failed to get receipt-id", __func__);
            pthread_mutex_unlock (&p_pQueue->mWriteMutex);
            Queue_Destroy_Stomp_Message(l_pMessage);
            return false;
        }
        if (strcmp(l_pReceiptId, l_sMessageId) != 0)
        {
            rzb_log(LOG_ERR, "%s: receipt-id did not match sent message: %s, %s", __func__, l_pReceiptId, l_sMessageId);
            pthread_mutex_unlock (&p_pQueue->mWriteMutex);
            Queue_Destroy_Stomp_Message(l_pMessage);
            return false;
        }
    }
    Queue_Destroy_Stomp_Message(l_pMessage);
 
    pthread_mutex_unlock (&p_pQueue->mWriteMutex);
    return true;
}

SO_PUBLIC void
Queue_GetQueueName (const uint8_t * p_sLeading, uuid_t p_pId,
                    uint8_t * p_sQueueName)
{
    char l_sUUID[UUID_STRING_LENGTH];

    uuid_unparse (p_pId, l_sUUID);
    sprintf ((char *) p_sQueueName, "%s.%s", (const char *) p_sLeading,
             (const char *) l_sUUID);
}
