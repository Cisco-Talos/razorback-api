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

/** @file message_formats.h
 * Message format structures.
 */
#ifndef RAZORBACK_MESSAGES_FORMATS_H
#define RAZORBACK_MESSAGES_FORMATS_H

#include <razorback/visibility.h>
#include <razorback/types.h>


/**
 * Message container.
 */
struct Message
{
    uint32_t type;                          ///< Message type
    size_t length;                          ///< Message length
    uint32_t version;                       ///< Message version
    uint64_t brokerDeliveryTag;             ///< Broker delivery tag for deferred ack handling
    TelemetryContextCarrier_t *telemetryContext; ///< Cross-thread telemetry context for deferred processing
    List_t *headers;                        ///< Message headers list
    void *message;                          ///< Message structure
    uint8_t *serialized;                    ///< Serialized message string
    bool brokerAckPending;                  ///< True when the broker delivery still needs to be acked or rejected
    bool (*serialize)(struct Message *);    ///< Pointer to message serialization function
    bool (*deserialize)(struct Message *);  ///< Pointer to message deserialization function
    void (*destroy)(struct Message *);      ///< Pointer to message destructor
};

/** Message header
 */
struct MessageHeader
{
    char *sName;     ///< Header name
    char *sValue;    ///< Header value
    void *pMessage;  ///< Message this header is for
};


/** Command and Control Messages
 * @{
 */


/** Error Message
 */
struct MessageError
{
    uint8_t *sMessage;  ///< Error Message Text
};

/** Hello Message
 * This message is a broadcast message.
 */
struct MessageHello
{
    uuid_t uuidNuggetType;       ///< Nugget Type
    uuid_t uuidApplicationType;  ///< Type of nugget sending the hello.
    uint8_t locality;            ///< Nugget Locality
    uint8_t priority;            ///< Dispatcher priority
    uint32_t flags;              ///< Dispatcher flags
    List_t *addressList;         ///< Dispatcher address list.
    uint8_t protocol;            ///< Dispatcher transfer protocol.
    uint16_t port;               ///< Dispatcher transfer server port.
};

/** Registration Request Message
 * This message is a broadcast message.
 */
struct MessageRegistrationRequest
{
    uuid_t uuidNuggetType;       ///< Nugget Type
    uuid_t uuidApplicationType;  ///< Application Type
    uint32_t iDataTypeCount;     ///< Number of supported data types.
    uuid_t *pDataTypeList;       ///< Supported data type list.
};

/** Configuration Update Message
 */
struct MessageConfigurationUpdate
{
    List_t *ntlvTypes;  ///< List of NTLV Type UUIDs
    List_t *ntlvNames;  ///< List of NTLV Name UUIDs
    List_t *dataTypes;  ///< List of data type UUIDs
};

/** Configuration Update Success
 */
struct MessageConfigurationAck
{
    uuid_t uuidNuggetType;       ///< Nugget Type
    uuid_t uuidApplicationType;  ///< Type of nugget sending the config ack.
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

/** Global Cache Request Message
 */
struct MessageCacheReq
{
    uuid_t uuidRequestor;  ///< UUID of the nugget requesting the data.
    struct BlockId *pId;   ///< Data Block ID
};

/** Global Cache Response Message
 */
struct MessageCacheResp
{
    struct BlockId *pId;  ///< Data Block ID
    uint32_t iSfFlags;    ///< Data block code
    uint32_t iEntFlags;   ///< Data block code
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
    uint32_t iReason;        ///< Submission Reason
    struct Event *pEvent;    ///< Event data
    uint8_t storedLocality;  ///< Locality the block was stored in.
};


/** Judgment Submission Message
 */
struct MessageJudgmentSubmission
{
    uint8_t iReason;             ///< Alert, Error, Done, Log
    struct Judgment *pJudgment;  ///< Judgment data
};

/** Inspection Submission Message
 */
struct MessageInspectionSubmission
{
    uint32_t iReason;         ///< Submission Reason
    struct Block *pBlock;     ///< Data block
    struct EventId *eventId;  ///< Triggering event id
    List_t *pEventMetadata;   ///< Event metadata list
    uint32_t localityCount;   ///< Number of localities the block is stored in
    uint8_t *localityList;    ///< Array of localities that block is stored in
};
/// @}
// End Submission Messages

/** Output Messages
 * @{
 */

/** Primary Alert Message
 */
struct MessageAlertPrimary
{
    struct Nugget *nugget;   ///< Generating nugget
    struct Block *block;     ///< The block
    struct Event *event;     ///< The event
    uint32_t gid;            ///< Alert GID
    uint32_t sid;            ///< Alert SID
    List_t *metadata;        ///< Alert Metadata
    uint8_t priority;        ///< Alert Priority
    char *message;           ///< Alert message string
    uint64_t seconds;        ///< Time stamp (Seconds)
    uint64_t nanosecs;       ///< Time stamp (Nano Seconds)
    uint32_t SF_Flags;       ///< Current SF Flags
    uint32_t Ent_Flags;      ///< Current Enterprise Flags
    uint32_t Old_SF_Flags;   ///< Previous SF Flags
    uint32_t Old_Ent_Flags;  ///< Previous Enterprise Flags
};

/** Primary Alert Message
 */
struct MessageAlertChild
{
    struct Nugget *nugget;   ///< Generating nugget
    struct Block *block;     ///< The block
    struct Block *child;     ///< The child block which caused this alert
    uint64_t eventCount;     ///< Number of events for this block
    uint64_t parentCount;    ///< Number of parents of this block
    uint32_t SF_Flags;       ///< Current SF Flags
    uint32_t Ent_Flags;      ///< Current Enterprise Flags
    uint32_t Old_SF_Flags;   ///< Previous SF Flags
    uint32_t Old_Ent_Flags;  ///< Previous Enterprise Flags
};

/** Event Output
 */
struct MessageOutputEvent
{
    struct Nugget *nugget;  ///< Generating nugget
    struct Event *event;    ///< Event data
};

/** Inspection Output
 */
struct MessageOutputInspection
{
    struct Nugget *nugget;    ///< Generating nugget
    uint64_t seconds;         ///< Time stamp (Seconds)
    uint64_t nanosecs;        ///< Time stamp (Nano Seconds)
    struct BlockId *blockId;  ///< Block ID
    uint8_t status;           ///< Inspection Status
    bool final;               ///< Final inspection notice.
};


#endif
