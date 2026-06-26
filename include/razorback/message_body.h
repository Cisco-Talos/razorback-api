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

/** @file message_body.h
 * Dispatcher-next message body encoding helpers.
 */
#ifndef RAZORBACK_MESSAGE_BODY_H
#define RAZORBACK_MESSAGE_BODY_H

#include <razorback/types.h>
#include <razorback/visibility.h>

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MESSAGE_BODY_DEFAULT_MAX_INLINE_BYTES 921600U
#define MESSAGE_BODY_DEFAULT_MAX_EXPANDED_BYTES 268435456U
#define MESSAGE_BODY_CONTENT_ENCODING_ZLIB "zlib"
#define MESSAGE_BODY_CLAIM_CHECK_REFERENCE_SCHEMA_NAME "razorback.messages.claim_check_reference"
#define MESSAGE_BODY_CLAIM_CHECK_REFERENCE_SCHEMA_VERSION 1U

enum MessageBodyMode
{
    MESSAGE_BODY_MODE_INLINE = 0,
    MESSAGE_BODY_MODE_ZLIB = 1,
    MESSAGE_BODY_MODE_CLAIM_CHECK = 2
};

struct MessageBodyPolicy
{
    size_t maxInlineBytes;
    size_t maxExpandedBytes;
};

struct ClaimCheckReference
{
    char *referenceSchemaName;
    uint32_t referenceSchemaVersion;
    char *signedUrl;
    char *expiresAt;
    char *bucket;
    char *objectKey;
    char *sha256;
    uint64_t uncompressedSize;
    uint64_t storedCompressedSize;
    char *contentType;
    char *schemaName;
    uint32_t schemaVersion;
    char *contentEncoding;
};

struct EncodedMessageBody
{
    enum MessageBodyMode mode;
    uint8_t *transportBody;
    size_t transportBodySize;
    char *contentEncoding;
    uint8_t *claimCheckBody;
    size_t claimCheckBodySize;
    struct ClaimCheckReference *claimCheckReference;
};

SO_PUBLIC extern struct MessageBodyPolicy MessageBodyPolicy_Default(void);
SO_PUBLIC extern const char * MessageBodyMode_ToString(enum MessageBodyMode mode);

SO_PUBLIC extern struct ClaimCheckReference * ClaimCheckReference_Create(
    const char *signedUrl,
    const char *expiresAt,
    const char *bucket,
    const char *objectKey,
    const char *contentType,
    const char *schemaName,
    uint32_t schemaVersion
);
SO_PUBLIC extern struct ClaimCheckReference * ClaimCheckReference_Clone(
    const struct ClaimCheckReference *source
);
SO_PUBLIC extern void ClaimCheckReference_Destroy(struct ClaimCheckReference *reference);
SO_PUBLIC extern char * ClaimCheckReference_ToJson(
    const struct ClaimCheckReference *reference
);
SO_PUBLIC extern struct ClaimCheckReference * ClaimCheckReference_FromJson(
    const char *json
);

SO_PUBLIC extern bool MessageBody_Encode(
    const struct MessageBodyPolicy *policy,
    const uint8_t *body,
    size_t bodySize,
    const struct ClaimCheckReference *claimCheckTemplate,
    struct EncodedMessageBody **encoded
);
SO_PUBLIC extern bool MessageBody_DecodeInline(
    const uint8_t *body,
    size_t bodySize,
    const char *contentEncoding,
    uint8_t **decoded,
    size_t *decodedSize
);
SO_PUBLIC extern bool MessageBody_DecodeInlineWithPolicy(
    const struct MessageBodyPolicy *policy,
    const uint8_t *body,
    size_t bodySize,
    const char *contentEncoding,
    uint8_t **decoded,
    size_t *decodedSize
);
SO_PUBLIC extern bool MessageBody_DecodeClaimCheck(
    const uint8_t *compressedBody,
    size_t compressedBodySize,
    const struct ClaimCheckReference *reference,
    uint8_t **decoded,
    size_t *decodedSize
);
SO_PUBLIC extern bool MessageBody_DecodeClaimCheckWithPolicy(
    const struct MessageBodyPolicy *policy,
    const uint8_t *compressedBody,
    size_t compressedBodySize,
    const struct ClaimCheckReference *reference,
    uint8_t **decoded,
    size_t *decodedSize
);
SO_PUBLIC extern void EncodedMessageBody_Destroy(struct EncodedMessageBody *encoded);

#ifdef __cplusplus
}
#endif
#endif /* RAZORBACK_MESSAGE_BODY_H */
