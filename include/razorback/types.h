/** @file types.h
 * Razorback API data types.
 */
#ifndef RAZORBACK_TYPES_H
#define RAZORBACK_TYPES_H

#include <openssl/evp.h>
#include <uuid/uuid.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>
#define UUID_STRING_LENGTH 37   ///< The size of a UUID String including the null
#define g_storagethreshold 0x10000000

typedef enum 
{
    R_SUCCESS = 0,
    R_ERROR = 1,
    R_FOUND = 2,
    R_NOT_FOUND = 3,
} Lookup_Result;
/** Name Type Length Block
 */
struct NTLVItem
{
    uuid_t uuidName;            ///< The UUID of the data type name.
    uuid_t uuidType;            ///< The UUID of the data type in this block
    uint32_t iLength;           ///< The length of the data in this block
    uint8_t *pData;             ///< The data
};

/** Name Type Length List Item
 */
struct NTLVListItem
{
    struct NTLVItem *pItem;     ///< Data Block
    struct NTLVListItem *pNext; ///< Next data time.
};

/** Name Type Lenght List
 */
struct NTLVList
{
    struct NTLVListItem *pHead; ///< List head
    struct NTLVListItem *pTail; ///< List tail
    uint32_t iCount;            ///< List length
};

/** Hash types
 * @{
 */
#define HASH_TYPE_MD5 1         ///< MD5 Hash
#define HASH_TYPE_SHA1 2        ///< SHA-1 Hash
#define HASH_TYPE_SHA224 3      ///< SHA224 Hash
#define HASH_TYPE_SHA256 4      ///< SHA256 Hash
#define HASH_TYPE_SHA512 5      ///< SHA512 Hash
/// @}

/** Hash Flags
 * @{
 */
#define HASH_FLAG_FINAL 0x00000001  ///< Hash has been finalized.
/// @}

/** Block Hash
 * utilize various algorithms, eg. MD5, SHA256, etc. to uniquely identify block of data.
 */
struct Hash
{
    uint32_t iType;             ///< The hash Type.
    uint32_t iSize;             ///< size of the data stored, must be the same for all hashes in system
    uint8_t *pData;             ///< actual data of the hash
    EVP_MD_CTX CTX;         ///< Private hash data.
    uint32_t iFlags;            ///< Hash Flags.
};

/** Data Block ID
 * If iLength is zero we dont have the block just the hash.
 */
struct BlockId
{
    struct Hash *pHash;         ///< The hash of the block
    uuid_t uuidDataType;        ///< The UUID of the data type in the block
    uint64_t iLength;           ///< The length of the data in the block
};

/** Block Pool Item Data
 */
struct BlockPoolData
{
    uint32_t iLength;           ///< Size of data block
    int iFlags;                 ///< Data Block Flags
    uint8_t *pData;             ///< Data block
    struct BlockPoolData *pNext;    ///< Next item in the chain
};

// Predeclare Block
struct Block;
struct Event;

/** Block Pool Item
 */
struct BlockPoolItem
{
    pthread_mutex_t mutex;                              ///< Item lock <- Why is it brown.
    uint32_t iStatus;                                   ///< Status Flags
    struct BlockPoolData *pDataHead;                    ///< Head Item
    struct BlockPoolData *pDataTail;                    ///< Tail Item
    void (*submittedCallback) (struct BlockPoolItem *); ///< Post submission callback
    struct Event *pEvent;
    void *userData;
};

/** Data Block
 */
struct Block
{
    struct BlockId *pId;       ///< Block ID
    struct BlockId *pParent;  ///< Parent Block ID
	uint8_t isStored;
    uint8_t *pData;
	uint16_t ticketSize;
	uint8_t *pTicket;         ///< Data for the Block
    struct BlockPoolItem *pPoolItem; ///< Data for the Block
    struct NTLVList *pMetaDataList;  ///< Meta Data List
};

struct EventId
{
    uuid_t uuidNuggetId;            ///< Id of the nugget creating the event
    uint64_t iSeconds;              ///< Time Stamp
    uint64_t iNanoSecs;             ///< Time Stamp

};

/** Event
 */
struct Event
{
    struct EventId *pId;            ///< The event id.
    uuid_t uuidApplicationType;     ///< Application Type
    uint64_t iSeconds;              ///< Time Stamp
    uint64_t iNanoSecs;             ///< Time Stamp
    struct Block *pBlock;           ///< The data block
    struct NTLVList *pMetaDataList; ///< Meta Data List
};

struct Judgment
{
    uuid_t uuidNuggetId;            ///< The nugget submitting
    uint64_t iSeconds;              ///< Time Stamp
    uint64_t iNanoSecs;             ///< Time Stamp
    struct EventId *pEventId;       ///< Event Id
    struct BlockId *pBlockId;       ///< Block Id
    uint8_t iPriority;              ///< Meh, Dodgy, YF, YRF
    struct NTLVList *pMetaDataList; ///< Meta Data List
    uint32_t iGID;                  ///< The GID
    uint32_t iSID;                  ///< The SID
    uint32_t Set_SfFlags;           ///< The blocks Sourcefire flags
    uint32_t Set_EntFlags;          ///< The blocks enterprise flags
    uint32_t Unset_SfFlags;         ///< The blocks Sourcefire flags
    uint32_t Unset_EntFlags;        ///< The blocks enterprise flags
    uint8_t *sMessage;              ///< The message

};

struct Alert
{
    uuid_t uuidNuggetId;            ///< The nugget generating the alert.
    uint64_t iSeconds;              ///< Time Stamp
    uint64_t iNanoSecs;             ///< Time Stamp
    struct EventId *pEventId;       ///< Event Id
    uint8_t iPriority;              ///< Meh, Dodgy, YF, YRF
    struct NTLVList *pMetaDataList; ///< Meta Data List
    uint32_t iGID;                  ///< The GID
    uint32_t iSID;                  ///< The SID
    uint32_t iSfFlags;              ///< The blocks Sourcefire flags
    uint32_t iEntFlags;             ///< The blocks enterprise flags
};
/** Defered Data Block List
 */
struct DeferredList
{
    uint8_t stuff;
};


#define SF_FLAG_GOOD        0x00000001
#define SF_FLAG_BAD         0x00000002
#define SF_FLAG_WHITE_LIST  0x00000004
#define SF_FLAG_BLACK_LIST  0x00000008
#define SF_FLAG_DIRTY       0x00000010
#define SF_FLAG_CANHAZ      0x00000020
#define SF_FLAG_PROCESSING  0x00000040
// Duplication Intended
#define SF_FLAG_DODGY       0x00000080
#define SF_FLAG_SUSPICIOUS  0x00000080

#define SF_FLAG_ALL         0xffffffff


#define JUDGMENT_REASON_DONE 0
#define JUDGMENT_REASON_ALERT 1
#define JUDGMENT_REASON_ERROR 2
#define JUDGMENT_REASON_DEFERRED 3
#define JUDGMENT_REASON_PENDING 4


#endif //RAZORBACK_TYPES_H
