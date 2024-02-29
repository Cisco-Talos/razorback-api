/** @file storage.h
 * Block storage functions
 */
#ifndef RZB_STORAGE_H
#define RZB_STORAGE_H

#include <razorback/log.h>
#include <razorback/hash.h>
#include <razorback/types.h>
#include <razorback/debug.h>

#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#define g_StorageThreshold 0

extern struct TransferTicket * StoreDataBlock (struct Block *block);
extern uint32_t RetrieveDataBlock (struct Block *block, struct TransferTicket *ticket);
extern bool Data_Storage_Initialize();
extern uint32_t ticketSize(struct TransferTicket *ticket);
extern void Storage_TicketDestroy(struct TransferTicket *ticket);
//extern void StorageTestFunction();

#endif
