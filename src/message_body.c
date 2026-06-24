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

#include "config.h"

#include <razorback/message_body.h>

#include <json.h>
#include <openssl/evp.h>
#include <zlib.h>

#include <ctype.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *
MessageBody_Strdup(const char *value)
{
    char *copy;
    size_t length;

    if (value == NULL)
        return NULL;

    length = strlen(value) + 1;
    copy = malloc(length);
    if (copy == NULL)
        return NULL;
    memcpy(copy, value, length);
    return copy;
}

static bool
MessageBody_StringIsEmpty(const char *value)
{
    return value == NULL || value[0] == '\0';
}

static bool
MessageBody_IsSha256Hex(const char *value)
{
    size_t index;

    if (value == NULL || strlen(value) != 64)
        return false;
    for (index = 0; index < 64; index++) {
        if (!isdigit((unsigned char)value[index]) &&
            (value[index] < 'a' || value[index] > 'f')) {
            return false;
        }
    }
    return true;
}

static bool
MessageBody_SchemaNameIsValid(const char *value)
{
    const char prefix[] = "razorback.";
    size_t index;

    if (value == NULL || strncmp(value, prefix, sizeof(prefix) - 1) != 0)
        return false;
    for (index = sizeof(prefix) - 1; value[index] != '\0'; index++) {
        if (!islower((unsigned char)value[index]) &&
            !isdigit((unsigned char)value[index]) &&
            value[index] != '_' &&
            value[index] != '.' &&
            value[index] != '-') {
            return false;
        }
    }
    return value[sizeof(prefix) - 1] != '\0';
}

static char *
MessageBody_Sha256Hex(const uint8_t *body, size_t bodySize)
{
    EVP_MD *md;
    EVP_MD_CTX *ctx;
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digestLength = 0;
    char *hex;
    unsigned int index;

    md = EVP_MD_fetch(NULL, "SHA256", NULL);
    if (md == NULL)
        return NULL;
    ctx = EVP_MD_CTX_new();
    if (ctx == NULL) {
        EVP_MD_free(md);
        return NULL;
    }
    if (EVP_DigestInit_ex(ctx, md, NULL) != 1 ||
        EVP_DigestUpdate(ctx, body, bodySize) != 1 ||
        EVP_DigestFinal_ex(ctx, digest, &digestLength) != 1) {
        EVP_MD_CTX_free(ctx);
        EVP_MD_free(md);
        return NULL;
    }
    EVP_MD_CTX_free(ctx);
    EVP_MD_free(md);

    if (digestLength != 32)
        return NULL;
    hex = calloc(65, sizeof(char));
    if (hex == NULL)
        return NULL;
    for (index = 0; index < digestLength; index++)
        snprintf(hex + (index * 2), 3, "%02x", digest[index]);
    return hex;
}

static bool
MessageBody_ZlibCompress(const uint8_t *body, size_t bodySize,
                         uint8_t **compressed, size_t *compressedSize)
{
    uLongf destinationSize;
    uint8_t *destination;
    int result;

    if (bodySize > ULONG_MAX)
        return false;

    destinationSize = compressBound((uLong)bodySize);
    destination = malloc(destinationSize == 0 ? 1 : destinationSize);
    if (destination == NULL)
        return false;

    result = compress2(destination, &destinationSize, body, (uLong)bodySize,
                       Z_DEFAULT_COMPRESSION);
    if (result != Z_OK) {
        free(destination);
        return false;
    }

    *compressed = destination;
    *compressedSize = (size_t)destinationSize;
    return true;
}

static bool
MessageBody_ZlibDecompressUnknown(const uint8_t *body, size_t bodySize,
                                  uint8_t **decoded, size_t *decodedSize)
{
    z_stream stream;
    uint8_t *output;
    size_t outputSize;
    int result;

    if (bodySize > UINT_MAX)
        return false;

    memset(&stream, 0, sizeof(stream));
    if (inflateInit(&stream) != Z_OK)
        return false;

    outputSize = (bodySize * 4) + 1024;
    if (outputSize < 1024)
        outputSize = 1024;
    output = malloc(outputSize);
    if (output == NULL) {
        inflateEnd(&stream);
        return false;
    }

    stream.next_in = (Bytef *)body;
    stream.avail_in = (uInt)bodySize;

    for (;;) {
        if (stream.total_out == outputSize) {
            uint8_t *grown;
            size_t grownSize = outputSize * 2;

            if (grownSize <= outputSize) {
                free(output);
                inflateEnd(&stream);
                return false;
            }
            grown = realloc(output, grownSize);
            if (grown == NULL) {
                free(output);
                inflateEnd(&stream);
                return false;
            }
            output = grown;
            outputSize = grownSize;
        }

        stream.next_out = output + stream.total_out;
        stream.avail_out = (uInt)(outputSize - stream.total_out);
        result = inflate(&stream, Z_NO_FLUSH);
        if (result == Z_STREAM_END)
            break;
        if (result != Z_OK) {
            free(output);
            inflateEnd(&stream);
            return false;
        }
    }

    *decodedSize = (size_t)stream.total_out;
    *decoded = output;
    inflateEnd(&stream);
    return true;
}

static bool
MessageBody_ZlibDecompressExpected(const uint8_t *body, size_t bodySize,
                                   uint64_t expectedSize, uint8_t **decoded,
                                   size_t *decodedSize)
{
    uint8_t *output;
    uLongf outputSize;
    int result;

    if (bodySize > ULONG_MAX || expectedSize > ULONG_MAX ||
        expectedSize > SIZE_MAX)
        return false;

    outputSize = (uLongf)expectedSize;
    output = malloc(outputSize == 0 ? 1 : (size_t)outputSize);
    if (output == NULL)
        return false;

    result = uncompress(output, &outputSize, body, (uLong)bodySize);
    if (result != Z_OK || outputSize != expectedSize) {
        free(output);
        return false;
    }

    *decoded = output;
    *decodedSize = (size_t)outputSize;
    return true;
}

static bool
ClaimCheckReference_AssignString(char **field, const char *value)
{
    char *copy;

    copy = MessageBody_Strdup(value);
    if (copy == NULL)
        return false;
    free(*field);
    *field = copy;
    return true;
}

SO_PUBLIC struct MessageBodyPolicy
MessageBodyPolicy_Default(void)
{
    struct MessageBodyPolicy policy;

    policy.maxInlineBytes = MESSAGE_BODY_DEFAULT_MAX_INLINE_BYTES;
    return policy;
}

SO_PUBLIC const char *
MessageBodyMode_ToString(enum MessageBodyMode mode)
{
    switch (mode) {
    case MESSAGE_BODY_MODE_INLINE:
        return "inline";
    case MESSAGE_BODY_MODE_ZLIB:
        return "zlib";
    case MESSAGE_BODY_MODE_CLAIM_CHECK:
        return "claim_check";
    default:
        return NULL;
    }
}

SO_PUBLIC struct ClaimCheckReference *
ClaimCheckReference_Create(const char *signedUrl, const char *expiresAt,
                           const char *bucket, const char *objectKey,
                           const char *contentType, const char *schemaName,
                           uint32_t schemaVersion)
{
    struct ClaimCheckReference *reference;

    if (MessageBody_StringIsEmpty(signedUrl) ||
        MessageBody_StringIsEmpty(expiresAt) ||
        MessageBody_StringIsEmpty(bucket) ||
        MessageBody_StringIsEmpty(objectKey) ||
        MessageBody_StringIsEmpty(contentType) ||
        MessageBody_StringIsEmpty(schemaName) ||
        schemaVersion == 0) {
        return NULL;
    }

    reference = calloc(1, sizeof(*reference));
    if (reference == NULL)
        return NULL;

    reference->referenceSchemaName =
        MessageBody_Strdup(MESSAGE_BODY_CLAIM_CHECK_REFERENCE_SCHEMA_NAME);
    reference->referenceSchemaVersion =
        MESSAGE_BODY_CLAIM_CHECK_REFERENCE_SCHEMA_VERSION;
    reference->signedUrl = MessageBody_Strdup(signedUrl);
    reference->expiresAt = MessageBody_Strdup(expiresAt);
    reference->bucket = MessageBody_Strdup(bucket);
    reference->objectKey = MessageBody_Strdup(objectKey);
    reference->sha256 = MessageBody_Strdup("");
    reference->contentType = MessageBody_Strdup(contentType);
    reference->schemaName = MessageBody_Strdup(schemaName);
    reference->schemaVersion = schemaVersion;
    reference->contentEncoding =
        MessageBody_Strdup(MESSAGE_BODY_CONTENT_ENCODING_ZLIB);

    if (reference->referenceSchemaName == NULL || reference->signedUrl == NULL ||
        reference->expiresAt == NULL || reference->bucket == NULL ||
        reference->objectKey == NULL || reference->sha256 == NULL ||
        reference->contentType == NULL || reference->schemaName == NULL ||
        reference->contentEncoding == NULL) {
        ClaimCheckReference_Destroy(reference);
        return NULL;
    }
    return reference;
}

SO_PUBLIC struct ClaimCheckReference *
ClaimCheckReference_Clone(const struct ClaimCheckReference *source)
{
    struct ClaimCheckReference *copy;

    if (source == NULL)
        return NULL;

    copy = ClaimCheckReference_Create(source->signedUrl, source->expiresAt,
                                      source->bucket, source->objectKey,
                                      source->contentType, source->schemaName,
                                      source->schemaVersion);
    if (copy == NULL)
        return NULL;
    copy->referenceSchemaVersion = source->referenceSchemaVersion;
    copy->uncompressedSize = source->uncompressedSize;
    copy->storedCompressedSize = source->storedCompressedSize;

    if (!ClaimCheckReference_AssignString(&copy->referenceSchemaName,
                                          source->referenceSchemaName) ||
        !ClaimCheckReference_AssignString(&copy->sha256, source->sha256) ||
        !ClaimCheckReference_AssignString(&copy->contentEncoding,
                                          source->contentEncoding)) {
        ClaimCheckReference_Destroy(copy);
        return NULL;
    }
    return copy;
}

SO_PUBLIC void
ClaimCheckReference_Destroy(struct ClaimCheckReference *reference)
{
    if (reference == NULL)
        return;
    free(reference->referenceSchemaName);
    free(reference->signedUrl);
    free(reference->expiresAt);
    free(reference->bucket);
    free(reference->objectKey);
    free(reference->sha256);
    free(reference->contentType);
    free(reference->schemaName);
    free(reference->contentEncoding);
    free(reference);
}

static bool
ClaimCheckReference_Validate(const struct ClaimCheckReference *reference)
{
    if (reference == NULL)
        return false;
    if (reference->referenceSchemaName == NULL ||
        reference->contentEncoding == NULL ||
        reference->sha256 == NULL ||
        reference->schemaName == NULL)
        return false;
    if (strcmp(reference->referenceSchemaName,
               MESSAGE_BODY_CLAIM_CHECK_REFERENCE_SCHEMA_NAME) != 0)
        return false;
    if (reference->referenceSchemaVersion !=
        MESSAGE_BODY_CLAIM_CHECK_REFERENCE_SCHEMA_VERSION)
        return false;
    if (strcmp(reference->contentEncoding, MESSAGE_BODY_CONTENT_ENCODING_ZLIB) != 0)
        return false;
    if (MessageBody_StringIsEmpty(reference->signedUrl) ||
        MessageBody_StringIsEmpty(reference->expiresAt) ||
        MessageBody_StringIsEmpty(reference->bucket) ||
        MessageBody_StringIsEmpty(reference->objectKey) ||
        MessageBody_StringIsEmpty(reference->contentType))
        return false;
    if (!MessageBody_SchemaNameIsValid(reference->schemaName))
        return false;
    if (!MessageBody_IsSha256Hex(reference->sha256))
        return false;
    return reference->schemaVersion > 0 && reference->uncompressedSize > 0 &&
           reference->storedCompressedSize > 0;
}

static bool
ClaimCheckReference_JsonAddString(json_object *object, const char *name,
                                  const char *value)
{
    json_object *stringObject;

    if (value == NULL)
        return false;
    stringObject = json_object_new_string(value);
    if (stringObject == NULL)
        return false;
    json_object_object_add(object, name, stringObject);
    return true;
}

static bool
ClaimCheckReference_JsonAddUint64(json_object *object, const char *name,
                                  uint64_t value)
{
    json_object *intObject;

    if (value > INT64_MAX)
        return false;
    intObject = json_object_new_int64((int64_t)value);
    if (intObject == NULL)
        return false;
    json_object_object_add(object, name, intObject);
    return true;
}

SO_PUBLIC char *
ClaimCheckReference_ToJson(const struct ClaimCheckReference *reference)
{
    json_object *object;
    const char *json;
    char *copy;

    if (reference == NULL)
        return NULL;

    object = json_object_new_object();
    if (object == NULL)
        return NULL;

    if (!ClaimCheckReference_JsonAddString(object, "reference_schema_name",
                                           reference->referenceSchemaName) ||
        !ClaimCheckReference_JsonAddUint64(object, "reference_schema_version",
                                           reference->referenceSchemaVersion) ||
        !ClaimCheckReference_JsonAddString(object, "signed_url",
                                           reference->signedUrl) ||
        !ClaimCheckReference_JsonAddString(object, "expires_at",
                                           reference->expiresAt) ||
        !ClaimCheckReference_JsonAddString(object, "bucket",
                                           reference->bucket) ||
        !ClaimCheckReference_JsonAddString(object, "object_key",
                                           reference->objectKey) ||
        !ClaimCheckReference_JsonAddString(object, "sha256",
                                           reference->sha256) ||
        !ClaimCheckReference_JsonAddUint64(object, "uncompressed_size",
                                           reference->uncompressedSize) ||
        !ClaimCheckReference_JsonAddUint64(object, "stored_compressed_size",
                                           reference->storedCompressedSize) ||
        !ClaimCheckReference_JsonAddString(object, "content_type",
                                           reference->contentType) ||
        !ClaimCheckReference_JsonAddString(object, "schema_name",
                                           reference->schemaName) ||
        !ClaimCheckReference_JsonAddUint64(object, "schema_version",
                                           reference->schemaVersion) ||
        !ClaimCheckReference_JsonAddString(object, "content_encoding",
                                           reference->contentEncoding)) {
        json_object_put(object);
        return NULL;
    }

    json = json_object_to_json_string_ext(object, JSON_C_TO_STRING_PLAIN);
    copy = MessageBody_Strdup(json);
    json_object_put(object);
    return copy;
}

static bool
ClaimCheckReference_ReadString(json_object *object, const char *name,
                               char **value)
{
    json_object *field = NULL;

    if (!json_object_object_get_ex(object, name, &field) ||
        json_object_get_type(field) != json_type_string) {
        return false;
    }
    *value = MessageBody_Strdup(json_object_get_string(field));
    return *value != NULL;
}

static bool
ClaimCheckReference_ReadUint64(json_object *object, const char *name,
                               uint64_t *value)
{
    json_object *field = NULL;
    int64_t parsed;

    if (!json_object_object_get_ex(object, name, &field) ||
        json_object_get_type(field) != json_type_int) {
        return false;
    }
    parsed = json_object_get_int64(field);
    if (parsed < 0)
        return false;
    *value = (uint64_t)parsed;
    return true;
}

SO_PUBLIC struct ClaimCheckReference *
ClaimCheckReference_FromJson(const char *json)
{
    json_object *object;
    struct ClaimCheckReference *reference;
    uint64_t referenceVersion;
    uint64_t schemaVersion;

    if (json == NULL)
        return NULL;

    object = json_tokener_parse(json);
    if (object == NULL || json_object_get_type(object) != json_type_object) {
        if (object != NULL)
            json_object_put(object);
        return NULL;
    }

    reference = calloc(1, sizeof(*reference));
    if (reference == NULL) {
        json_object_put(object);
        return NULL;
    }

    if (!ClaimCheckReference_ReadString(object, "reference_schema_name",
                                        &reference->referenceSchemaName) ||
        !ClaimCheckReference_ReadUint64(object, "reference_schema_version",
                                        &referenceVersion) ||
        referenceVersion > UINT32_MAX ||
        !ClaimCheckReference_ReadString(object, "signed_url",
                                        &reference->signedUrl) ||
        !ClaimCheckReference_ReadString(object, "expires_at",
                                        &reference->expiresAt) ||
        !ClaimCheckReference_ReadString(object, "bucket", &reference->bucket) ||
        !ClaimCheckReference_ReadString(object, "object_key",
                                        &reference->objectKey) ||
        !ClaimCheckReference_ReadString(object, "sha256", &reference->sha256) ||
        !ClaimCheckReference_ReadUint64(object, "uncompressed_size",
                                        &reference->uncompressedSize) ||
        !ClaimCheckReference_ReadUint64(object, "stored_compressed_size",
                                        &reference->storedCompressedSize) ||
        !ClaimCheckReference_ReadString(object, "content_type",
                                        &reference->contentType) ||
        !ClaimCheckReference_ReadString(object, "schema_name",
                                        &reference->schemaName) ||
        !ClaimCheckReference_ReadUint64(object, "schema_version",
                                        &schemaVersion) ||
        schemaVersion > UINT32_MAX ||
        !ClaimCheckReference_ReadString(object, "content_encoding",
                                        &reference->contentEncoding)) {
        json_object_put(object);
        ClaimCheckReference_Destroy(reference);
        return NULL;
    }

    reference->referenceSchemaVersion = (uint32_t)referenceVersion;
    reference->schemaVersion = (uint32_t)schemaVersion;

    json_object_put(object);
    if (!ClaimCheckReference_Validate(reference)) {
        ClaimCheckReference_Destroy(reference);
        return NULL;
    }
    return reference;
}

static bool
EncodedMessageBody_SetTransport(struct EncodedMessageBody *encoded,
                                const uint8_t *body, size_t bodySize)
{
    encoded->transportBody = malloc(bodySize == 0 ? 1 : bodySize);
    if (encoded->transportBody == NULL)
        return false;
    if (bodySize > 0)
        memcpy(encoded->transportBody, body, bodySize);
    encoded->transportBodySize = bodySize;
    return true;
}

SO_PUBLIC bool
MessageBody_Encode(const struct MessageBodyPolicy *policy, const uint8_t *body,
                   size_t bodySize,
                   const struct ClaimCheckReference *claimCheckTemplate,
                   struct EncodedMessageBody **encoded)
{
    struct MessageBodyPolicy effectivePolicy;
    struct EncodedMessageBody *result;
    uint8_t *compressed = NULL;
    size_t compressedSize = 0;
    char *referenceJson;

    if (encoded == NULL || (body == NULL && bodySize > 0))
        return false;
    *encoded = NULL;

    effectivePolicy = (policy != NULL) ? *policy : MessageBodyPolicy_Default();
    result = calloc(1, sizeof(*result));
    if (result == NULL)
        return false;

    if (bodySize <= effectivePolicy.maxInlineBytes) {
        result->mode = MESSAGE_BODY_MODE_INLINE;
        if (!EncodedMessageBody_SetTransport(result, body, bodySize)) {
            EncodedMessageBody_Destroy(result);
            return false;
        }
        *encoded = result;
        return true;
    }

    if (!MessageBody_ZlibCompress(body, bodySize, &compressed, &compressedSize)) {
        EncodedMessageBody_Destroy(result);
        return false;
    }

    if (compressedSize <= effectivePolicy.maxInlineBytes) {
        result->mode = MESSAGE_BODY_MODE_ZLIB;
        result->transportBody = compressed;
        result->transportBodySize = compressedSize;
        result->contentEncoding =
            MessageBody_Strdup(MESSAGE_BODY_CONTENT_ENCODING_ZLIB);
        if (result->contentEncoding == NULL) {
            EncodedMessageBody_Destroy(result);
            return false;
        }
        *encoded = result;
        return true;
    }

    result->mode = MESSAGE_BODY_MODE_CLAIM_CHECK;
    result->claimCheckReference = ClaimCheckReference_Clone(claimCheckTemplate);
    if (result->claimCheckReference == NULL) {
        free(compressed);
        EncodedMessageBody_Destroy(result);
        return false;
    }

    result->claimCheckBody = compressed;
    result->claimCheckBodySize = compressedSize;
    result->claimCheckReference->referenceSchemaVersion =
        MESSAGE_BODY_CLAIM_CHECK_REFERENCE_SCHEMA_VERSION;
    result->claimCheckReference->uncompressedSize = bodySize;
    result->claimCheckReference->storedCompressedSize = compressedSize;
    if (!ClaimCheckReference_AssignString(
            &result->claimCheckReference->referenceSchemaName,
            MESSAGE_BODY_CLAIM_CHECK_REFERENCE_SCHEMA_NAME) ||
        !ClaimCheckReference_AssignString(
            &result->claimCheckReference->contentEncoding,
            MESSAGE_BODY_CONTENT_ENCODING_ZLIB)) {
        EncodedMessageBody_Destroy(result);
        return false;
    }

    free(result->claimCheckReference->sha256);
    result->claimCheckReference->sha256 =
        MessageBody_Sha256Hex(result->claimCheckBody, result->claimCheckBodySize);
    if (result->claimCheckReference->sha256 == NULL) {
        EncodedMessageBody_Destroy(result);
        return false;
    }

    referenceJson = ClaimCheckReference_ToJson(result->claimCheckReference);
    if (referenceJson == NULL) {
        EncodedMessageBody_Destroy(result);
        return false;
    }
    result->transportBodySize = strlen(referenceJson);
    result->transportBody = (uint8_t *)referenceJson;
    *encoded = result;
    return true;
}

SO_PUBLIC bool
MessageBody_DecodeInline(const uint8_t *body, size_t bodySize,
                         const char *contentEncoding, uint8_t **decoded,
                         size_t *decodedSize)
{
    if (decoded == NULL || decodedSize == NULL || (body == NULL && bodySize > 0))
        return false;
    *decoded = NULL;
    *decodedSize = 0;

    if (contentEncoding == NULL || contentEncoding[0] == '\0') {
        *decoded = malloc(bodySize == 0 ? 1 : bodySize);
        if (*decoded == NULL)
            return false;
        if (bodySize > 0)
            memcpy(*decoded, body, bodySize);
        *decodedSize = bodySize;
        return true;
    }

    if (strcmp(contentEncoding, MESSAGE_BODY_CONTENT_ENCODING_ZLIB) != 0)
        return false;
    return MessageBody_ZlibDecompressUnknown(body, bodySize, decoded, decodedSize);
}

SO_PUBLIC bool
MessageBody_DecodeClaimCheck(const uint8_t *compressedBody,
                             size_t compressedBodySize,
                             const struct ClaimCheckReference *reference,
                             uint8_t **decoded, size_t *decodedSize)
{
    char *sha256;
    bool ok;

    if (decoded == NULL || decodedSize == NULL ||
        (compressedBody == NULL && compressedBodySize > 0))
        return false;
    *decoded = NULL;
    *decodedSize = 0;

    if (!ClaimCheckReference_Validate(reference) ||
        reference->storedCompressedSize != compressedBodySize)
        return false;

    sha256 = MessageBody_Sha256Hex(compressedBody, compressedBodySize);
    if (sha256 == NULL)
        return false;
    ok = strcmp(sha256, reference->sha256) == 0;
    free(sha256);
    if (!ok)
        return false;

    return MessageBody_ZlibDecompressExpected(compressedBody, compressedBodySize,
                                              reference->uncompressedSize,
                                              decoded, decodedSize);
}

SO_PUBLIC void
EncodedMessageBody_Destroy(struct EncodedMessageBody *encoded)
{
    if (encoded == NULL)
        return;
    free(encoded->transportBody);
    free(encoded->contentEncoding);
    free(encoded->claimCheckBody);
    ClaimCheckReference_Destroy(encoded->claimCheckReference);
    free(encoded);
}
