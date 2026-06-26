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

/** @file messages_next.h
 * Dispatcher-next message identity and routing helpers.
 */
#ifndef RAZORBACK_MESSAGES_NEXT_H
#define RAZORBACK_MESSAGES_NEXT_H

#include <razorback/types.h>
#include <razorback/visibility.h>
#include <razorback/message_body.h>

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RZB_NEXT_SCHEMA_VERSION 1U

#define RZB_NEXT_SCHEMA_CLAIM_CHECK_REFERENCE "razorback.messages.claim_check_reference"
#define RZB_NEXT_SCHEMA_CNC_REGISTRATION_REQUEST "razorback.cnc.registration_request"
#define RZB_NEXT_SCHEMA_CNC_REGISTRATION_ACCEPTED "razorback.cnc.registration_accepted"
#define RZB_NEXT_SCHEMA_CNC_REGISTRATION_REJECTED "razorback.cnc.registration_rejected"
#define RZB_NEXT_SCHEMA_CNC_LIVENESS "razorback.cnc.liveness"
#define RZB_NEXT_SCHEMA_CNC_BYE "razorback.cnc.bye"
#define RZB_NEXT_SCHEMA_CNC_DIRECTED_COMMAND "razorback.cnc.directed_command"
#define RZB_NEXT_SCHEMA_CNC_DISPATCHER_HELLO "razorback.cnc.dispatcher_hello"
#define RZB_NEXT_SCHEMA_BLOCK_SUBMISSION "razorback.messages.block_submission"
#define RZB_NEXT_SCHEMA_BLOCK_UPDATE "razorback.messages.block_update"
#define RZB_NEXT_SCHEMA_INSPECTION_WORK "razorback.messages.inspection_work"
#define RZB_NEXT_SCHEMA_ANALYSIS_RESULT "razorback.analysis_result.envelope"
#define RZB_NEXT_SCHEMA_CACHE_REQUEST "razorback.cache.request"
#define RZB_NEXT_SCHEMA_CACHE_RESPONSE "razorback.cache.response"
#define RZB_NEXT_SCHEMA_CATALOG_INVALIDATION "razorback.catalog_invalidation.event"
#define RZB_NEXT_SCHEMA_FILE_REMOVE_REQUEST "razorback.file_remove.request"
#define RZB_NEXT_SCHEMA_FILE_REMOVE_RESULT "razorback.file_remove.result"
#define RZB_NEXT_SCHEMA_SEARCH_EXPORT "razorback.search_export.record"

#define RZB_NEXT_QUEUE_COMMAND "COMMAND"
#define RZB_NEXT_QUEUE_DIRECTED_COMMAND_PREFIX "COMMAND"
#define RZB_NEXT_EXCHANGE_DISPATCHER_HELLO "DISPATCHER.HELLO"
#define RZB_NEXT_QUEUE_INPUT "INPUT"
#define RZB_NEXT_QUEUE_BLOCK_UPDATE "BLOCK_UPDATE"
#define RZB_NEXT_QUEUE_INSPECTOR_PREFIX "INSPECTOR"
#define RZB_NEXT_QUEUE_ANALYSIS_RESULT "ANALYSIS_RESULT"
#define RZB_NEXT_QUEUE_CACHE_REQUEST "REQUEST"
#define RZB_NEXT_QUEUE_CACHE_RESPONSE_PREFIX "CACHE_RESPONSE"
#define RZB_NEXT_EXCHANGE_CATALOG_INVALIDATION "CATALOG.INVALIDATION"
#define RZB_NEXT_EXCHANGE_FILE_REMOVE "FILE_REMOVE"
#define RZB_NEXT_QUEUE_FILE_REMOVE_RESULT "FILE_REMOVE.RESULT"

#define RZB_NEXT_HEADER_SCHEMA_NAME "rzb-schema-name"
#define RZB_NEXT_HEADER_SCHEMA_VERSION "rzb-schema-version"
#define RZB_NEXT_HEADER_BODY_MODE "rzb-body-mode"
#define RZB_NEXT_HEADER_CONTENT_ENCODING "content-encoding"

enum RzbNextTransport
{
    RZB_NEXT_TRANSPORT_RABBITMQ = 0,
    RZB_NEXT_TRANSPORT_KAFKA = 1
};

struct RzbNextRoute
{
    enum RzbNextTransport transport;
    char *exchange;
    char *routingKey;
};

struct RzbNextMessageHeader
{
    char *name;
    char *value;
};

struct RzbNextPreparedRabbitMqMessage
{
    struct RzbNextRoute *route;
    struct RzbNextMessageHeader *headers;
    size_t headerCount;
    uint8_t *body;
    size_t bodySize;
    char *contentType;
    char *contentEncoding;
    uint8_t *claimCheckBody;
    size_t claimCheckBodySize;
    struct ClaimCheckReference *claimCheckReference;
};

struct RzbNextDecodedRabbitMqMessage
{
    char *jsonMessage;
    struct ClaimCheckReference *claimCheckReference;
};

SO_PUBLIC extern const char * RzbNextTransport_ToString(
    enum RzbNextTransport transport
);
SO_PUBLIC extern bool RzbNextMessage_IsKnownSchema(const char *schemaName);
SO_PUBLIC extern bool RzbNextMessage_ValidateIdentity(const char *jsonMessage);
SO_PUBLIC extern bool RzbNextMessage_Validate(const char *jsonMessage);
SO_PUBLIC extern bool RzbNextMessage_Route(
    const char *jsonMessage,
    const char *cacheRequestorUuid,
    struct RzbNextRoute **route
);
SO_PUBLIC extern bool RzbNextRabbitMq_PrepareMessage(
    const char *jsonMessage,
    const char *cacheRequestorUuid,
    const struct MessageBodyPolicy *policy,
    const struct ClaimCheckReference *claimCheckTemplate,
    struct RzbNextPreparedRabbitMqMessage **prepared
);
SO_PUBLIC extern bool RzbNextRabbitMq_DecodeMessage(
    const uint8_t *body,
    size_t bodySize,
    const struct RzbNextMessageHeader *headers,
    size_t headerCount,
    const uint8_t *claimCheckBody,
    size_t claimCheckBodySize,
    struct RzbNextDecodedRabbitMqMessage **decoded
);
SO_PUBLIC extern bool RzbNextRabbitMq_DecodeMessageWithPolicy(
    const struct MessageBodyPolicy *policy,
    const uint8_t *body,
    size_t bodySize,
    const struct RzbNextMessageHeader *headers,
    size_t headerCount,
    const uint8_t *claimCheckBody,
    size_t claimCheckBodySize,
    struct RzbNextDecodedRabbitMqMessage **decoded
);
SO_PUBLIC extern bool RzbNextCnc_IsReadyDispatcherHello(const char *jsonMessage);
SO_PUBLIC extern bool RzbNextCnc_RegistrationAcceptedTiming(
    const char *jsonMessage,
    uint64_t *livenessInterval,
    uint64_t *livenessFreshnessWindow,
    uint64_t *livenessClockSkewTolerance
);
SO_PUBLIC extern char * RzbNextCnc_DirectedCommandQueue(const char *nuggetUuid);
SO_PUBLIC extern char * RzbNextAnalysisResult_BuildCompleted(
    const char *inspectionWorkJson,
    const char *inspectorUuid,
    const char *createdAt,
    const char *blockMetadataUpdatesJson,
    const char *metadataJson,
    const char *tagMutationsJson,
    const char *alertsJson
);
SO_PUBLIC extern char * RzbNextAnalysisResult_BuildError(
    const char *inspectionWorkJson,
    const char *inspectorUuid,
    const char *category,
    const char *code,
    const char *message,
    const char *detailsJson,
    const char *createdAt
);
SO_PUBLIC extern char * RzbNextAnalysisResult_BuildDeferred(
    const char *inspectionWorkJson,
    const char *inspectorUuid,
    const char *reasonCode,
    const char *message,
    const char *pollAfter,
    const char *detailsJson,
    const char *createdAt
);
SO_PUBLIC extern void RzbNext_FreeString(char *value);
SO_PUBLIC extern void RzbNextRoute_Destroy(struct RzbNextRoute *route);
SO_PUBLIC extern void RzbNextPreparedRabbitMqMessage_Destroy(
    struct RzbNextPreparedRabbitMqMessage *prepared
);
SO_PUBLIC extern void RzbNextDecodedRabbitMqMessage_Destroy(
    struct RzbNextDecodedRabbitMqMessage *decoded
);

#ifdef __cplusplus
}
#endif
#endif /* RAZORBACK_MESSAGES_NEXT_H */
