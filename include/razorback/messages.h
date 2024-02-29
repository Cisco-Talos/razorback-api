/** @file messages.h
 * Razorback API messages.
 */
#ifndef RAZORBACK_MESSAGES_H
#define RAZORBACK_MESSAGES_H

#include <razorback/types.h>

#define MESSAGE_MODE_BIN 1
#define MESSAGE_MODE_JSON 2

/** Message Groups
 * @{
 */
#define MESSAGE_GROUP_C_AND_C   0x10000000  ///< Command And Control
#define MESSAGE_GROUP_CACHE     0x20000000  ///< Global Cache
#define MESSAGE_GROUP_SUBMIT    0x40000000  ///< Submission
/// @}
/** Command And Control Messages
 * @{
 */
#define MESSAGE_TYPE_HELLO          ( MESSAGE_GROUP_C_AND_C | 1 )   ///< Hello Message
#define MESSAGE_TYPE_REG_REQ        ( MESSAGE_GROUP_C_AND_C | 2 )   ///< Registration Request
#define MESSAGE_TYPE_REG_RESP       ( MESSAGE_GROUP_C_AND_C | 3 )   ///< Registration Response
#define MESSAGE_TYPE_REG_ERR        ( MESSAGE_GROUP_C_AND_C | 4 )   ///< Registration Error
#define MESSAGE_TYPE_CONFIG_UPDATE  ( MESSAGE_GROUP_C_AND_C | 5 )   ///< Configuration Update Notice
#define MESSAGE_TYPE_CONFIG_ACK     ( MESSAGE_GROUP_C_AND_C | 6 )   ///< Configuration Updated OK
#define MESSAGE_TYPE_CONFIG_ERR     ( MESSAGE_GROUP_C_AND_C | 7 )   ///< Configuration Update Failed
#if 0
#define MESSAGE_TYPE_STATS_REQ      ( MESSAGE_GROUP_C_AND_C | 8 )   ///< Statistics Request
#define MESSAGE_TYPE_STATS_RESP     ( MESSAGE_GROUP_C_AND_C | 9 )   ///< Statistics Response (With Data)
#define MESSAGE_TYPE_STATS_ERR      ( MESSAGE_GROUP_C_AND_C | 10 )  ///< Statistics Error
#endif
#define MESSAGE_TYPE_PAUSE          ( MESSAGE_GROUP_C_AND_C | 11 )  ///< Pause Processing
#define MESSAGE_TYPE_PAUSED         ( MESSAGE_GROUP_C_AND_C | 12 )  ///< Pause Confirmation
#define MESSAGE_TYPE_GO             ( MESSAGE_GROUP_C_AND_C | 13 )  ///< Unpause Processing
#define MESSAGE_TYPE_RUNNING        ( MESSAGE_GROUP_C_AND_C | 14 )  ///< Unpuase Confirmation
#define MESSAGE_TYPE_TERM           ( MESSAGE_GROUP_C_AND_C | 15 )  ///< Terminate Processing
#define MESSAGE_TYPE_BYE            ( MESSAGE_GROUP_C_AND_C | 16 )  ///< Terminate Processing
#define MESSAGE_TYPE_CLEAR          ( MESSAGE_GROUP_C_AND_C | 17 )  ///< Clear Local Cache
/// @}

/** Global Cache Messages
 * @{
 */
#define MESSAGE_TYPE_REQ            ( MESSAGE_GROUP_CACHE | 1 ) ///< Global Cache Request
#define MESSAGE_TYPE_RESP           ( MESSAGE_GROUP_CACHE | 2 ) ///< Global Cache Response
/// @}

/** Submission Messages
 * @{
 */
#define MESSAGE_TYPE_BLOCK          ( MESSAGE_GROUP_SUBMIT | 1 )    ///< Data Block Submission
#define MESSAGE_TYPE_JUDGMENT       ( MESSAGE_GROUP_SUBMIT | 2 )    ///< Judgment Submission
#define MESSAGE_TYPE_INSPECTION     ( MESSAGE_GROUP_SUBMIT | 3 )    ///< Inspection Submission
#define MESSAGE_TYPE_LOG            ( MESSAGE_GROUP_SUBMIT | 4 )    ///< Log Message
#define MESSAGE_TYPE_ALERT          ( MESSAGE_GROUP_SUBMIT | 5 )    ///< Alert Message
/// @}

/** Message Versions
 * @{
 */
#define MESSAGE_VERSION_1 1
/// @}

#define MSG_CNC_HEADER_SOURCE   "Source_Nugget"
#define MSG_CNC_HEADER_DEST     "Dest_Nugget"

struct MessageHeader 
{
    char *sName;
    char *sValue;
};

struct Message
{
    uint32_t type;             ///< Message type
    size_t length;           ///< Message length
    uint32_t version;          ///< Message version
    struct List *headers;
    void *message;
    uint8_t *serialized;
    bool (*serialize)(struct Message *, int);
    bool (*deserialize)(struct Message *, int);
    void (*destroy)(struct Message *);
};



/** Command and Control Messages
 * @{
 */


/** Error Message
 */
struct MessageError
{
    uint8_t *sMessage;          ///< Error Message Text
};

/** Hello Message
 * This message is a broadcast message.
 */
struct MessageHello
{
    uuid_t uuidNuggetType;      ///< Nugget Type
    uuid_t uuidApplicationType; ///< Type of nugget sending the hello.
};

/** Registration Request Message
 * This message is a broadcase message.
 */
struct MessageRegistrationRequest
{
    uuid_t uuidNuggetType;      ///< Nugget Type
    uuid_t uuidApplicationType; ///< Application Type
    uint32_t iDataTypeCount;    ///< Number of supported data types.
    uuid_t *pDataTypeList;      ///< Supported data type list.
};

/** Configuration Update Message
 */
struct MessageConfigurationUpdate
{
    uint32_t ntlvTypesCount;
    uuid_t *ntlvTypesUuids;
    uint32_t ntlvTypesNamesSize;
    char *ntlvTypesNames;
    uint32_t ntlvNamesCount;
    uuid_t *ntlvNamesUuids;
    uint32_t ntlvNamesNamesSize;
    char *ntlvNamesNames;
    uint32_t dataTypesCount;
    uuid_t *dataTypesUuids;
    uint32_t dataTypesNamesSize;
    char *dataTypesNames;
};

/** Configuration Update Success
 */
struct MessageConfigurationAck
{
    uuid_t uuidNuggetType;      ///< Nugget Type
    uuid_t uuidApplicationType; ///< Type of nugget sending the config ack.
};

/** Terminate Message
 */
struct MessageTerminate
{
    uint8_t *sTerminateReason;  ///< String with termination reason in.
};
/// @}
//
// End of Command and Control Messages

/** Cache Control Messages
 * @{
 */

/** Glocal Cache Request Message
 */
struct MessageCacheReq
{
    uuid_t uuidRequestor;       ///< UUID of the nugget requesting the data.
    struct BlockId *pId;    ///< Data Block ID
};

/** Global Cache Response Message
 */
struct MessageCacheResp
{
    struct BlockId *pId;    ///< Data Block ID
    uint32_t iSfFlags;             ///< Data block code
    uint32_t iEntFlags;             ///< Data block code
};


/// @}
// End Cache Control Messages

/** Submission Messages
 * @{
 */

/** Block Submission Message
 */
struct MessageBlockSubmission
{
    uint32_t iReason;           ///< Submisson Reason
    struct Event *pEvent;
    struct TransferTicket *ticket;
};


/** Judgment Submission Message
 */
struct MessageJudgmentSubmission
{
    uint8_t iReason;                ///< Alert, Error, Done, Log
    struct Judgment *pJudgment;
};

/** Log Submission Message
 */
struct MessageLogSubmission
{
    uuid_t uuidNuggetId;            ///< who wrote it
    uint8_t iPriority;              ///< Meh, Dodgy, YF, YRF
    struct EventId *pEventId;       ///< The event id.
    uint8_t *sMessage;              ///< The message.
};

/** Inspection Submission Message
 */
struct MessageInspectionSubmission
{
    uint32_t iReason;           ///< Submisson Reason
	struct Block *pBlock;        ///< Datablock
    struct EventId *eventId;
    struct List *pEventMetadata;
    struct TransferTicket *ticket;
};

/** Alert Message
 */
struct MessageAlert
{
    uint32_t iDisposition;      ///< the disposition
    uuid_t uuidInspectorId;     ///< which inspector
    struct Block *pBlock;        ///< The block
};

/// @}
// End Submission Messages

extern struct List * Message_Header_List_Create(void);
extern bool Message_Add_Header(struct List *headers, const char *p_sName, const char *p_sValue);
extern bool Message_CnC_Get_Nuggets(struct Message *message, uuid_t source, uuid_t dest);


/** initializes message cache req
 * @param p_pMessage the message
 * @param p_uuidRequestor the requestor
 * @param p_pBlockId the blockid
 * @return true if ok, false otherwise
 */
extern struct Message*  MessageCacheReq_Initialize (
                                        const uuid_t p_uuidRequestor,
                                        const struct BlockId *p_pBlockId);


/** initializes message cache resp
 * @param p_pMessage the message
 * @param p_pBlockId the blockid
 * @param p_iSfFlags  the code
 * @param p_iEntFlags the code
 * @return true if ok, false otherwise
 */
extern struct Message* MessageCacheResp_Initialize (
                                         const struct BlockId *p_pBlockId,
                                         uint32_t p_iSfFlags, uint32_t p_iEntFlags);


/** initializes message block submission
 * @param p_pMessage the message
 * @param p_pEvent the event\
 * @param p_iReason the submission reason
 * @return true if ok, false otherwise
 */
extern struct Message*  MessageBlockSubmission_Initialize (
                                               struct Event *p_pEvent,
                                               uint32_t p_iReason);
/** initializes message block submission
 * @param p_pMessage the message
 * @param p_pBlockIthe block
 * @param p_iReason the reason for the submission
 * @param p_iPriority the priority
 * @param p_uuidInspectorId the submitting nugget
 * @param p_uuidApplicationId the application type of the block
 * @return true if ok, false otherwise
 */
extern struct Message* MessageJudgmentSubmission_Initialize (
                                                  uint8_t p_iReason,
                                                  struct Judgment* p_pJudgment);

/** initializes message inspection submission
 * @param p_pMessage the message
 * @param p_pEvent The event
 * @param p_iReason the reason
 * @return true if ok, false otherwise
 */
extern struct Message* MessageInspectionSubmission_Initialize (
                                                    const struct Event
                                                    *p_pEvent,
                                                    uint32_t p_iReason,
                                                    struct TransferTicket *ticket
                                                    );

/** initializes message alert
 * @param p_pMessage the message
 * @param p_pBlock the block
 * @param p_iDisposition the disposition
 * @param p_uuidInspectorId the submitting nugget
 * @return true if ok, false otherwise
 */
extern struct Message* MessageAlert_Initialize (
                                     const struct Block *p_pBlock,
                                     uint32_t p_iDisposition,
                                     const uuid_t p_uuidInspectorId);


/** initializes a hello message
 * @param p_pMessage the message
 * @param p_uuidSourceNugget the source
 * @param p_uuidDestNugget the dest
 * @param p_uuidApplicationType the app type
 */
extern struct Message* MessageHello_Initialize (
                                     const uuid_t p_uuidSourceNugget,
                                     const uuid_t p_uuidNuggetType,
                                     const uuid_t p_uuidApplicationType);


/** initializes a registration request message
 * @param p_pMessage the message
 * @param p_uuidSourceNugget the source
 * @param p_uuidNuggetTye the nugget type
 * @param p_uuidApplicationType the app type
 * @param p_iDataTypeCount the number of data types
 * @param p_pDataTypeList the data types
 */
extern struct Message* MessageRegistrationRequest_Initialize (
                                                   const uuid_t
                                                   p_uuidSourceNugget,
                                                   const uuid_t
                                                   p_uuidNuggetType,
                                                   const uuid_t
                                                   p_uuidApplicationType,
                                                   uint32_t p_iDataTypeCount,
                                                   uuid_t * p_pDataTypeList);


/** initializes a registration response message
 * @param p_pMessage the message
 * @param p_uuidSourceNugget the source
 * @param p_uuidDestNugget the dest
 */
extern struct Message* MessageRegistrationResponse_Initialize (
                                                    const uuid_t
                                                    p_uuidSourceNugget,
                                                    const uuid_t
                                                    p_uuidDestNugget);

/** initializes a configuration update message
 * @param p_pMessage the message
 * @param p_uuidSourceNugget the source
 * @param p_uuidDestNugget the dest
 * @param p_ntlvConfigurationList the config list
 */
extern struct Message * MessageConfigurationUpdate_Initialize (
                                                   const uuid_t
                                                   p_uuidSourceNugget,
                                                   const uuid_t
                                                   p_uuidDestNugget);


/** initializes a configuration ack message
 * @param p_pMessage the message
 * @param p_uuidSourceNugget the source
 * @param p_uuidDestNugget the dest
 * @param p_uuidNuggetType the type of nugget
 * @param p_uuidApplicationType the application type
 */
extern struct Message* MessageConfigurationAck_Initialize (
                                                const uuid_t
                                                p_uuidSourceNugget,
                                                const uuid_t
                                                p_uuidDestNugget,
                                                const uuid_t p_uuidNuggetType,
                                                const uuid_t
                                                p_uuidApplicationType);


/** initializes a pause message
 * @param p_pMessage the message
 * @param p_uuidSourceNugget the source
 * @param p_uuidDestNugget the dest
 */
extern struct Message * MessagePause_Initialize (
                                     const uuid_t p_uuidSourceNugget,
                                     const uuid_t p_uuidDestNugget);


/** initializes a paused message
 * @param p_pMessage the message
 * @param p_uuidSourceNugget the source
 * @param p_uuidDestNugget the dest
 */
extern struct Message* MessagePaused_Initialize (
                                      const uuid_t p_uuidSourceNugget,
                                      const uuid_t p_uuidDestNugget);


/** initializes a go message
 * @param p_pMessage the message
 * @param p_uuidSourceNugget the source
 * @param p_uuidDestNugget the dest
 */
extern struct Message * MessageGo_Initialize (
                                  const uuid_t p_uuidSourceNugget,
                                  const uuid_t p_uuidDestNugget);



/** initializes a running message
 * @param p_pMessage the message
 * @param p_uuidSourceNugget the source
 * @param p_uuidDestNugget the dest
 */
extern struct Message * MessageRunning_Initialize (
                                       const uuid_t p_uuidSourceNugget,
                                       const uuid_t p_uuidDestNugget);


/** initializes a terminate message
 * @param p_pMessage the message
 * @param p_uuidSourceNugget the source
 * @param p_uuidDestNugget the dest
 * @param p_sTerminateReason the reason
 */
extern struct Message * MessageTerminate_Initialize (
                                         const uuid_t p_uuidSourceNugget,
                                         const uuid_t p_uuidDestNugget,
                                         const uint8_t * p_sTerminateReason);


/** initializes a bye message
 * @param p_pMessage the message
 * @param p_uuidSourceNugget the source
 * @param p_uuidDestNugget the dest
 */
extern struct Message * MessageBye_Initialize (
                                   const uuid_t p_uuidSourceNugget);


extern struct Message * MessageCacheClear_Initialize (
                                   const uuid_t p_uuidSourceNugget);


extern struct Message * MessageError_Initialize (
                                    uint32_t p_iErrorCode,
                                    const char *p_sMessage,
                                    const uuid_t p_uuidSourceNugget,
                                    const uuid_t p_uuidDestNugget);


extern struct Message *
MessageLog_Initialize (
                         const uuid_t p_uuidNuggetId,
                         uint8_t p_iPriority,
                         char *p_sMessage,
                         struct EventId *p_pEventId);


#endif //RAZORBACK_MESSAGES_H
