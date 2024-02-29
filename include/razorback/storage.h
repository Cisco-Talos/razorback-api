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

extern uint32_t StoreDataBlock (struct Block *block);
extern uint32_t RetrieveDataBlock (struct Block *block);
extern bool Data_Storage_Initialize();
extern bool Data_Storage_Curl_Initialize();
extern uint32_t binaryTicketSize(char *data);
//extern void StorageTestFunction();

#endif
