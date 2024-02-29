/** @file messages.h
 * Razorback API messages.
 */
#ifndef RAZORBACK_MESSAGES_H
#define RAZORBACK_MESSAGES_H

#include <razorback/types.h>

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
#define MESSAGE_TYPE_STATS_REQ      ( MESSAGE_GROUP_C_AND_C | 8 )   ///< Statistics Request
#define MESSAGE_TYPE_STATS_RESP     ( MESSAGE_GROUP_C_AND_C | 9 )   ///< Statistics Response (With Data)
#define MESSAGE_TYPE_STATS_ERR      ( MESSAGE_GROUP_C_AND_C | 10 )  ///< Statistics Error
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
#define MESSAGE_TYPE_TICKET         ( MESSAGE_GROUP_SUBMIT | 6 )    ///< Submission Ticket
/// @}

/** Message Versions
 * @{
 */
#define MESSAGE_VERSION_1 1
/// @}

/** Message Header
 */
struct MessageHeader
{
    uint32_t iType;             ///< Message type
    uint32_t iLength;           ///< Message length
    uint32_t iVersion;          ///< Message version
};



/** Command and Control Messages
 * @{
 */

/** Command and Control Message Header
 * If uuidDestNugget is all 0's then the message is a broadcast message.
 */
struct CcMessageHeader
{
    struct MessageHeader mhHeader;  ///< Message Header
    uuid_t uuidSourceNugget;    ///< Source Nugget UUID
    uuid_t uuidDestNugget;      ///< Destination Nugget UUID
};

/** Error Message
 */
struct MessageError
{
    struct CcMessageHeader ccmhHeader;  ///< Message Header 
    uint8_t *sMessage;          ///< Error Message Text
};

/** Hello Message
 * This message is a broadcast message.
 */
struct MessageHello
{
    struct CcMessageHeader ccmhHeader;  ///< Command and Control Message Header.
    uuid_t uuidNuggetType;      ///< Nugget Type
    uuid_t uuidApplicationType; ///< Type of nugget sending the hello.
};

/** Registration Request Message
 * This message is a broadcase message.
 */
struct MessageRegistrationRequest
{
    struct CcMessageHeader ccmhMessageHeader;   ///< Command and Control Message Header
    uuid_t uuidNuggetType;      ///< Nugget Type
    uuid_t uuidApplicationType; ///< Application Type
    uint32_t iDataTypeCount;    ///< Number of supported data types.
    uuid_t *pDataTypeList;      ///< Supported data type list.
};

/** Registration Response Message
 */
struct MessageRegistrationResponse
{
    struct CcMessageHeader ccmhMessageHeader;   ///< Command and Control Message Header
};

/** Configuration Update Message
 */
struct MessageConfigurationUpdate
{
    struct CcMessageHeader ccmhMessageHeader;   ///< Command and Control Message Header
    uint32_t ntlvTypesCount;
    uuid_t *ntlvTypesUuids;
    uint32_t ntlvTypesNamesSize;
    char *ntlvTypesNames;
    uint32_t ntlvNamesCount;
    uuid_t *ntlvNamesUuids;
    uint32_t ntlvNamesNamesSize;
    char *ntlvNamesNames;
};

/** Configuration Update Success
 */
struct MessageConfigurationAck
{
    struct CcMessageHeader ccmhMessageHeader;   ///< Command and Control Message Header
    uuid_t uuidNuggetType;      ///< Nugget Type
    uuid_t uuidApplicationType; ///< Type of nugget sending the config ack.
};
/** Configuration Update Error
 */
struct MessageConfigurationErr
{
    struct CcMessageHeader ccmhMessageHeader;   ///< Command and Control Message Header
};

/** Statistics Request Message
 */
struct MessageStatsRequest
{
    struct CcMessageHeader ccmhMessageHeader;   ///< Command and Control Message Header
};

/** Statistics Response Message
 */
struct MessageStatsResponse
{
    struct CcMessageHeader ccmhMessageHeader;   ///< Command and Control Message Header
    struct NTLVList *ntlvStatsList;  ///< Statistics Data
};

/** Pause Message
 */
struct MessagePause
{
    struct CcMessageHeader ccmhMessageHeader;   ///< Command and Control Message Header
};

/** Paused Message
 */
struct MessagePaused
{
    struct CcMessageHeader ccmhMessageHeader;   ///< Command and Control Message Header
};

/** Go Message
 */
struct MessageGo
{
    struct CcMessageHeader ccmhMessageHeader;   ///< Command and Control Message Header
};

/** Running Message
 */
struct MessageRunning
{
    struct CcMessageHeader ccmhMessageHeader;   ///< Command and Control Message Header
};

/** Terminate Message
 */
struct MessageTerminate
{
    struct CcMessageHeader ccmhMessageHeader;   ///< Command and Control Message Header
    uint8_t *sTerminateReason;  ///< String with termination reason in.
};

/** Bye Message
 */
struct MessageBye
{
    struct CcMessageHeader ccmhMessageHeader;   ///< Command and Controll Message Header
};

/** Local Cache Clear Message
 */
struct MessageCacheClear
{
    struct CcMessageHeader mhHeader;  ///< Message Header
};

/** Union of Command and Control messages
 */
union CcMessageUnion
{
    struct CcMessageHeader mchHeader;   ///< C and C Message Header
    struct MessageHello mhHello;    ///< Hello Message
    struct MessageRegistrationRequest mrrRegReq;    ///< Registration Request Message
    struct MessageRegistrationResponse mrrRegResp;  ///< Registration Response Message
    struct MessageConfigurationUpdate mcuConfigUpdate;  ///< Config Update Message
    struct MessageConfigurationAck mcaConfigAck;    ///< Config Update ACK Message
    struct MessageStatsRequest msrStatsReq; ///< Stats Request Message
    struct MessageStatsResponse msrStatsResp;   ///< Status Response Message
    struct MessagePause mpPause;    ///< Pause Message
    struct MessagePaused mpPaused;  ///< Paused Message
    struct MessageGo mgGo;      ///< Go Message
    struct MessageRunning mrRunning;    ///< Running Message
    struct MessageTerminate mtTerminate;    ///< Terminate Message
    struct MessageBye mbBye;    ///< Bye Message
    struct MessageError meError;    ///< Error Message
    struct MessageCacheClear mccCacheClear;    ///< Error Message
};

/// @}
// End of Command and Control Messages

/** Cache Control Messages
 * @{
 */

/** Glocal Cache Request Message
 */
struct MessageCacheReq
{
    struct MessageHeader mhHeader;  ///< Message Header
    uuid_t uuidRequestor;       ///< UUID of the nugget requesting the data.
    struct BlockId *pId;    ///< Data Block ID
};

/** Global Cache Response Message
 */
struct MessageCacheResp
{
    struct MessageHeader mhHeader;  ///< Message Header
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
    struct MessageHeader mhHeader;  ///< Message Header
    uint32_t iReason;           ///< Submisson Reason
    struct Event *pEvent;
};

/** Data Block Submission Stub
 */ 
struct MessageInspectionTicket
{
	uint64_t filesize;
	uint32_t localityId;
	uint32_t retrievalType;
	uint16_t port;
	char *hostName;
	char *basePath;
};

/** Judgment Submission Message
 */
struct MessageJudgmentSubmission
{
    struct MessageHeader mhHeader;  ///< Message Header
    uint8_t iReason;                ///< Alert, Error, Done, Log
    struct Judgment *pJudgment;
};

/** Log Submission Message
 */
struct MessageLogSubmission
{
    struct MessageHeader mhHeader;  ///< Message Header
    uuid_t uuidNuggetId;            ///< who wrote it
    uint8_t iPriority;              ///< Meh, Dodgy, YF, YRF
    struct EventId *pEventId;       ///< The event id.
    uint8_t *sMessage;              ///< The message.
};

/** Inspection Submission Message
 */
struct MessageInspectionSubmission
{
    struct MessageHeader mhHeader;  ///< Message Header
    uint32_t iReason;           ///< Submisson Reason
	struct Block *pBlock;        ///< Datablock
    struct EventId *eventId;
    struct NTLVList *pEventMetadata;
};

/** Alert Message
 */
struct MessageAlert
{
    struct MessageHeader mhHeader;  ///< Message Header
    uint32_t iDisposition;      ///< the disposition
    uuid_t uuidInspectorId;     ///< which inspector
    struct Block *pBlock;        ///< The block
};

/// @}
// End Submission Messages

/** gets length of message header
 * @param p_pHeader the header
 * @return the size when written into binary buffer
 */
extern uint32_t MessageHeader_BinaryLength (const struct MessageHeader
                                            *p_pHeader);

/** initializes message cache req
 * @param p_pMessage the message
 * @param p_uuidRequestor the requestor
 * @param p_pBlockId the blockid
 * @return true if ok, false otherwise
 */
extern bool MessageCacheReq_Initialize (struct MessageCacheReq *p_pMessage,
                                        const uuid_t p_uuidRequestor,
                                        const struct BlockId *p_pBlockId);

/** destroys message cache req
 * @param p_pMessage the message
 */
extern void MessageCacheReq_Destroy (struct MessageCacheReq *p_pMessage);

/** initializes message cache resp
 * @param p_pMessage the message
 * @param p_pBlockId the blockid
 * @param p_iSfFlags  the code
 * @param p_iEntFlags the code
 * @return true if ok, false otherwise
 */
extern bool MessageCacheResp_Initialize (struct MessageCacheResp *p_pMessage,
                                         const struct BlockId *p_pBlockId,
                                         uint32_t p_iSfFlags, uint32_t p_iEntFlags);

/** destroys message cache resp
 * @param p_pMessage the message
 */
extern void MessageCacheResp_Destroy (struct MessageCacheResp *p_pMessage);

/** initializes message block submission
 * @param p_pMessage the message
 * @param p_pEvent the event\
 * @param p_iReason the submission reason
 * @return true if ok, false otherwise
 */
extern bool MessageBlockSubmission_Initialize (struct MessageBlockSubmission
                                               *p_pMessage,
                                               struct Event *p_pEvent,
                                               uint32_t p_iReason);

/** destroys message block submission
 * @param p_pMessage the message
 */
extern void MessageBlockSubmission_Destroy (struct MessageBlockSubmission
                                            *p_pMessage);

/** initializes message block submission
 * @param p_pMessage the message
 * @param p_pBlockIthe block
 * @param p_iReason the reason for the submission
 * @param p_iPriority the priority
 * @param p_uuidInspectorId the submitting nugget
 * @param p_uuidApplicationId the application type of the block
 * @return true if ok, false otherwise
 */
extern bool MessageJudgmentSubmission_Initialize (struct
                                                  MessageJudgmentSubmission
                                                  *p_pMessage,
                                                  uint8_t p_iReason,
                                                  struct Judgment* p_pJudgment);

/** destroys message judgment submission
 * @param p_pMessage the message
 */
extern void MessageJudgmentSubmission_Destroy (struct
                                               MessageJudgmentSubmission
                                               *p_pMessage);

/** initializes message inspection submission
 * @param p_pMessage the message
 * @param p_pEvent The event
 * @param p_iReason the reason
 * @return true if ok, false otherwise
 */
extern bool MessageInspectionSubmission_Initialize (struct
                                                    MessageInspectionSubmission
                                                    *p_pMessage,
                                                    const struct Event
                                                    *p_pEvent,
                                                    uint32_t p_iReason);

/** destroys message inspection submission
 * @param p_pMessage the message
 */
extern void MessageInspectionSubmission_Destroy (struct
                                                 MessageInspectionSubmission
                                                 *p_pMessage);

/** initializes message alert
 * @param p_pMessage the message
 * @param p_pBlock the block
 * @param p_iDisposition the disposition
 * @param p_uuidInspectorId the submitting nugget
 * @return true if ok, false otherwise
 */
extern bool MessageAlert_Initialize (struct MessageAlert *p_pMessage,
                                     const struct Block *p_pBlock,
                                     uint32_t p_iDisposition,
                                     const uuid_t p_uuidInspectorId);

/** destroys message alert
 * @param p_pMessage the message
 */
extern void MessageAlert_Destroy (struct MessageAlert *p_pMessage);

/** initializes a hello message
 * @param p_pMessage the message
 * @param p_uuidSourceNugget the source
 * @param p_uuidDestNugget the dest
 * @param p_uuidApplicationType the app type
 */
extern void MessageHello_Initialize (struct MessageHello *p_pMessage,
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
extern bool MessageRegistrationRequest_Initialize (struct
                                                   MessageRegistrationRequest
                                                   *p_pMessage,
                                                   const uuid_t
                                                   p_uuidSourceNugget,
                                                   const uuid_t
                                                   p_uuidNuggetType,
                                                   const uuid_t
                                                   p_uuidApplicationType,
                                                   uint32_t p_iDataTypeCount,
                                                   uuid_t * p_pDataTypeList);

/** destroys a registration request message
 * @param p_pMessage the message
 */
extern void MessageRegistrationRequest_Destroy (struct
                                                MessageRegistrationRequest
                                                *p_pMessage);

/** initializes a registration response message
 * @param p_pMessage the message
 * @param p_uuidSourceNugget the source
 * @param p_uuidDestNugget the dest
 */
extern void MessageRegistrationResponse_Initialize (struct
                                                    MessageRegistrationResponse
                                                    *p_pMessage,
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
extern bool MessageConfigurationUpdate_Initialize (struct
                                                   MessageConfigurationUpdate
                                                   *p_pMessage,
                                                   const uuid_t
                                                   p_uuidSourceNugget,
                                                   const uuid_t
                                                   p_uuidDestNugget);

/** destroys a configuration update message
 * @param p_pMessage the message
 */
extern void MessageConfigurationUpdate_Destroy (struct
                                                MessageConfigurationUpdate
                                                *p_pMessage);

/** initializes a configuration ack message
 * @param p_pMessage the message
 * @param p_uuidSourceNugget the source
 * @param p_uuidDestNugget the dest
 * @param p_uuidNuggetType the type of nugget
 * @param p_uuidApplicationType the application type
 */
extern void MessageConfigurationAck_Initialize (struct MessageConfigurationAck
                                                *p_pMessage,
                                                const uuid_t
                                                p_uuidSourceNugget,
                                                const uuid_t
                                                p_uuidDestNugget,
                                                const uuid_t p_uuidNuggetType,
                                                const uuid_t
                                                p_uuidApplicationType);

/** initializes a stats request message
 * @param p_pMessage the message
 * @param p_uuidSourceNugget the source
 * @param p_uuidDestNugget the dest
 */
extern void MessageStatsRequest_Initialize (struct MessageStatsRequest
                                            *p_pMessage,
                                            const uuid_t p_uuidSourceNugget,
                                            const uuid_t p_uuidDestNugget);

/** initializes a stats response message
 * @param p_pMessage the message
 * @param p_uuidSourceNugget the source
 * @param p_uuidDestNugget the dest
 * @param p_pStatsList the stats
 */
extern void MessageStatsResponse_Initialize (struct MessageStatsResponse
                                             *p_pMessage,
                                             const uuid_t p_uuidSourceNugget,
                                             const uuid_t p_uuidDestNugget,
                                             const struct NTLVList
                                             *p_pStatsList);

/** destroys a stats response message
 * @param p_pMessage the message
 */
extern void MessageStatsResponse_Destroy (struct MessageStatsResponse
                                          *p_pMessage);

/** initializes a pause message
 * @param p_pMessage the message
 * @param p_uuidSourceNugget the source
 * @param p_uuidDestNugget the dest
 */
extern void MessagePause_Initialize (struct MessagePause *p_pMessage,
                                     const uuid_t p_uuidSourceNugget,
                                     const uuid_t p_uuidDestNugget);

/** initializes a paused message
 * @param p_pMessage the message
 * @param p_uuidSourceNugget the source
 * @param p_uuidDestNugget the dest
 */
extern void MessagePaused_Initialize (struct MessagePaused *p_pMessage,
                                      const uuid_t p_uuidSourceNugget,
                                      const uuid_t p_uuidDestNugget);

/** initializes a go message
 * @param p_pMessage the message
 * @param p_uuidSourceNugget the source
 * @param p_uuidDestNugget the dest
 */
extern void MessageGo_Initialize (struct MessageGo *p_pMessage,
                                  const uuid_t p_uuidSourceNugget,
                                  const uuid_t p_uuidDestNugget);

/** initializes a running message
 * @param p_pMessage the message
 * @param p_uuidSourceNugget the source
 * @param p_uuidDestNugget the dest
 */
extern void MessageRunning_Initialize (struct MessageRunning *p_pMessage,
                                       const uuid_t p_uuidSourceNugget,
                                       const uuid_t p_uuidDestNugget);

/** initializes a terminate message
 * @param p_pMessage the message
 * @param p_uuidSourceNugget the source
 * @param p_uuidDestNugget the dest
 * @param p_sTerminateReason the reason
 */
extern bool MessageTerminate_Initialize (struct MessageTerminate *p_pMessage,
                                         const uuid_t p_uuidSourceNugget,
                                         const uuid_t p_uuidDestNugget,
                                         const uint8_t * p_sTerminateReason);

/** destroys a terminate message
 * @param p_pMessage the message
 */
extern void MessageTerminate_Destroy (struct MessageTerminate *p_pMessage);

/** initializes a bye message
 * @param p_pMessage the message
 * @param p_uuidSourceNugget the source
 * @param p_uuidDestNugget the dest
 */
extern void MessageBye_Initialize (struct MessageBye *p_pMessage,
                                   const uuid_t p_uuidSourceNugget,
                                   const uuid_t p_uuidDestNugget);

extern void MessageCacheClear_Initialize (struct MessageCacheClear *p_pMessage,
                                   const uuid_t p_uuidSourceNugget);


extern void MessageError_Initialize (struct MessageError *p_pMessage,
                                    uint32_t p_iErrorCode,
                                    const char *p_sMessage,
                                    const uuid_t p_uuidSourceNugget,
                                    const uuid_t p_uuidDestNugget);


/** destroys a command and control message
 * @param p_pHeader the message
 */
extern void MessageCC_Destroy (union CcMessageUnion *p_pHeader);


extern bool
MessageLog_Initialize (struct MessageLogSubmission *p_pMessage,
                         const uuid_t p_uuidNuggetId,
                         uint8_t p_iPriority,
                         char *p_sMessage,
                         struct EventId *p_pEventId);
extern void
MessageLog_Destroy (struct MessageLogSubmission *p_pMessage);


#endif //RAZORBACK_MESSAGES_H
