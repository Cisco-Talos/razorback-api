/** @file socket.h
 * Socket API.
 */

#ifndef	RAZORBACK_SOCKET_H
#define RAZORBACK_SOCKET_H

#include <razorback/types.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <openssl/ssl.h>


/** Socket Structure
 */
struct Socket
{
    int iSocket;           ///< The Socket FD
    struct addrinfo *pAddressInfo;
    bool ssl;
    SSL *sslHandle;
    SSL_CTX *sslContext;
};

/** Starts a socket listening
 * @param *p_sSourceAddress The address
 * @param p_iPort The port
 * @return A new socket or NULL on error.
 */
extern struct Socket * Socket_Listen (const uint8_t * p_sSourceAddress,
                           uint16_t p_iPort);

/** Starts a socket listening
 * @param *p_sSourceAddress The address
 * @param p_iPort The port
 * @return A new socket or NULL on error.
 */
extern struct Socket * SSL_Socket_Connect ( const uint8_t * p_sDestinationAddress,
                            uint16_t p_iPort);

/** Starts a socket from a listening socket
 * @param *p_pSocket the socket
 * @param *p_sListeningSocket the listening socket
 * @return true on success false on error.
 */
extern int Socket_Accept (struct Socket **p_pSocket,
                          const struct Socket *p_sListeningSocket);

/** Starts a connecting socket
 * @param *p_sDestinationAddress the address
 * @param p_iPort the port
 * @return a new socket on success null on failure.
 */
extern struct Socket * Socket_Connect ( const uint8_t * p_sDestinationAddress,
                            uint16_t p_iPort);

/** Close a socket
 * @param *p_pSocket the socket
 */
extern void Socket_Close (struct Socket *p_pSocket);

/** transmits on a socket
 * @param *p_pSocket the socket
 * @param p_iSize the size of data
 * @param *p_sData the data
 * @return true on success false on error
 */
extern bool Socket_Tx (const struct Socket *p_pSocket, uint32_t p_iSize,
                       const uint8_t * p_sData);

/** receives on a socket
 * @param *p_pSocket the socket
 * @param p_iSize the size to read
 * @param *p_sData the data
 * @return true on success false on error.
 */
extern bool Socket_Rx (const struct Socket *p_pSocket, uint32_t p_iSize,
                       uint8_t * p_sData);

/** receives on a socket until the terminator is reached
 * @param p_pS the socket
 * @param p_sData the data to read
 * @param p_cTerminator the terminating character
 * @return true on success, false on error
 */
extern size_t Socket_Rx_Until (const struct Socket *p_pS, uint8_t * p_sData,
                             size_t iSize, uint8_t p_cTerminator);

/** determines whether there is any data available
 * @param p_pS the socket
 * @return true if data available, false otherwise
 */
extern bool Socket_ReadyForRead (const struct Socket *p_pS);


#endif // RAZORBACK_SOCKET_H
