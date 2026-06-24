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

#include <razorback/messages_next.h>

#include <ctype.h>
#include <json.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct KnownSchema
{
    const char *name;
    uint32_t version;
};

static const struct KnownSchema KnownSchemas[] = {
    { RZB_NEXT_SCHEMA_CLAIM_CHECK_REFERENCE, RZB_NEXT_SCHEMA_VERSION },
    { RZB_NEXT_SCHEMA_CNC_REGISTRATION_REQUEST, RZB_NEXT_SCHEMA_VERSION },
    { RZB_NEXT_SCHEMA_CNC_REGISTRATION_ACCEPTED, RZB_NEXT_SCHEMA_VERSION },
    { RZB_NEXT_SCHEMA_CNC_REGISTRATION_REJECTED, RZB_NEXT_SCHEMA_VERSION },
    { RZB_NEXT_SCHEMA_CNC_LIVENESS, RZB_NEXT_SCHEMA_VERSION },
    { RZB_NEXT_SCHEMA_CNC_BYE, RZB_NEXT_SCHEMA_VERSION },
    { RZB_NEXT_SCHEMA_CNC_DIRECTED_COMMAND, RZB_NEXT_SCHEMA_VERSION },
    { RZB_NEXT_SCHEMA_CNC_DISPATCHER_HELLO, RZB_NEXT_SCHEMA_VERSION },
    { RZB_NEXT_SCHEMA_BLOCK_SUBMISSION, RZB_NEXT_SCHEMA_VERSION },
    { RZB_NEXT_SCHEMA_BLOCK_UPDATE, RZB_NEXT_SCHEMA_VERSION },
    { RZB_NEXT_SCHEMA_INSPECTION_WORK, RZB_NEXT_SCHEMA_VERSION },
    { RZB_NEXT_SCHEMA_ANALYSIS_RESULT, RZB_NEXT_SCHEMA_VERSION },
    { RZB_NEXT_SCHEMA_CACHE_REQUEST, RZB_NEXT_SCHEMA_VERSION },
    { RZB_NEXT_SCHEMA_CACHE_RESPONSE, RZB_NEXT_SCHEMA_VERSION },
    { RZB_NEXT_SCHEMA_CATALOG_INVALIDATION, RZB_NEXT_SCHEMA_VERSION },
    { RZB_NEXT_SCHEMA_FILE_REMOVE_REQUEST, RZB_NEXT_SCHEMA_VERSION },
    { RZB_NEXT_SCHEMA_FILE_REMOVE_RESULT, RZB_NEXT_SCHEMA_VERSION },
    { RZB_NEXT_SCHEMA_SEARCH_EXPORT, RZB_NEXT_SCHEMA_VERSION }
};

static const char * RzbNext_GetString(json_object *object, const char *field);

static char *
RzbNext_Strdup(const char *value)
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

static char *
RzbNext_Format2(const char *prefix, const char *value)
{
    char *formatted;
    size_t length;

    if (prefix == NULL || value == NULL)
        return NULL;
    length = strlen(prefix) + 1 + strlen(value) + 1;
    formatted = malloc(length);
    if (formatted == NULL)
        return NULL;
    snprintf(formatted, length, "%s.%s", prefix, value);
    return formatted;
}

static char *
RzbNext_Format3(const char *prefix, const char *middle, const char *suffix)
{
    char *formatted;
    size_t length;

    if (prefix == NULL || middle == NULL || suffix == NULL)
        return NULL;
    length = strlen(prefix) + 1 + strlen(middle) + 1 + strlen(suffix) + 1;
    formatted = malloc(length);
    if (formatted == NULL)
        return NULL;
    snprintf(formatted, length, "%s.%s.%s", prefix, middle, suffix);
    return formatted;
}

static bool
RzbNext_IsLowerHex(char value)
{
    return (value >= '0' && value <= '9') || (value >= 'a' && value <= 'f');
}

static bool
RzbNext_IsUuid(const char *value)
{
    size_t index;

    if (value == NULL || strlen(value) != 36)
        return false;
    for (index = 0; index < 36; index++) {
        if (index == 8 || index == 13 || index == 18 || index == 23) {
            if (value[index] != '-')
                return false;
        } else if (!RzbNext_IsLowerHex(value[index])) {
            return false;
        }
    }
    return true;
}

static bool
RzbNext_IsSafeKey(const char *value)
{
    size_t index;

    if (value == NULL || value[0] == '\0' || strlen(value) > 128)
        return false;
    if (!islower((unsigned char)value[0]) && !isdigit((unsigned char)value[0]))
        return false;
    for (index = 1; value[index] != '\0'; index++) {
        if (!islower((unsigned char)value[index]) &&
            !isdigit((unsigned char)value[index]) &&
            value[index] != '_' &&
            value[index] != '-') {
            return false;
        }
    }
    return true;
}

static bool
RzbNext_IsDataTypeName(const char *value)
{
    size_t index;

    if (value == NULL)
        return false;
    if (strcmp(value, "ANY_DATA") == 0)
        return true;
    if (value[0] == '\0' || strlen(value) > 128)
        return false;
    if (!islower((unsigned char)value[0]) && !isdigit((unsigned char)value[0]))
        return false;
    for (index = 1; value[index] != '\0'; index++) {
        if (!islower((unsigned char)value[index]) &&
            !isdigit((unsigned char)value[index]) &&
            value[index] != '_' &&
            value[index] != '.' &&
            value[index] != '/' &&
            value[index] != '+' &&
            value[index] != '-') {
            return false;
        }
    }
    return strchr(value, '~') == NULL;
}

static bool
RzbNext_IsSha256(const char *value)
{
    size_t index;

    if (value == NULL || strlen(value) != 64)
        return false;
    for (index = 0; index < 64; index++) {
        if (!RzbNext_IsLowerHex(value[index]))
            return false;
    }
    return true;
}

static bool
RzbNext_IsTimestamp(const char *value)
{
    size_t index;

    if (value == NULL || strlen(value) != 24)
        return false;
    for (index = 0; index < 24; index++) {
        switch (index) {
        case 4:
        case 7:
            if (value[index] != '-')
                return false;
            break;
        case 10:
            if (value[index] != 'T')
                return false;
            break;
        case 13:
        case 16:
            if (value[index] != ':')
                return false;
            break;
        case 19:
            if (value[index] != '.')
                return false;
            break;
        case 23:
            if (value[index] != 'Z')
                return false;
            break;
        default:
            if (!isdigit((unsigned char)value[index]))
                return false;
            break;
        }
    }
    return true;
}

static bool
RzbNext_IsSchemaName(const char *value)
{
    size_t index;

    if (value == NULL || strncmp(value, "razorback.", 10) != 0 ||
        value[10] == '\0') {
        return false;
    }
    for (index = 0; value[index] != '\0'; index++) {
        if (!islower((unsigned char)value[index]) &&
            !isdigit((unsigned char)value[index]) &&
            value[index] != '.' &&
            value[index] != '_' &&
            value[index] != '-') {
            return false;
        }
    }
    return true;
}

static bool
RzbNext_IsPrintableId(const char *value)
{
    size_t index;
    size_t length;

    if (value == NULL)
        return false;
    length = strlen(value);
    if (length == 0 || length > 512)
        return false;
    for (index = 0; index < length; index++) {
        if ((unsigned char)value[index] < 33 ||
            (unsigned char)value[index] > 126) {
            return false;
        }
    }
    return true;
}

static bool
RzbNext_StringIn(const char *value, const char * const *allowed, size_t count)
{
    size_t index;

    if (value == NULL)
        return false;
    for (index = 0; index < count; index++) {
        if (strcmp(value, allowed[index]) == 0)
            return true;
    }
    return false;
}

static bool
RzbNext_TextBounded(const char *value, size_t maxLength, bool trimCheck)
{
    size_t length;

    if (value == NULL)
        return false;
    length = strlen(value);
    if (length == 0 || length > maxLength)
        return false;
    if (trimCheck &&
        (isspace((unsigned char)value[0]) ||
         isspace((unsigned char)value[length - 1]))) {
        return false;
    }
    return true;
}

static bool
RzbNext_ObjectHasField(json_object *object, const char *field)
{
    json_object *value;

    return json_object_object_get_ex(object, field, &value);
}

static size_t
RzbNext_ObjectFieldCount(json_object *object)
{
    size_t count = 0;

    if (object == NULL || json_object_get_type(object) != json_type_object)
        return 0;
    json_object_object_foreach(object, key, value) {
        (void)key;
        (void)value;
        count++;
    }
    return count;
}

static bool
RzbNext_ObjectOnlyHasFields(json_object *object,
                            const char * const *allowed,
                            size_t count)
{
    json_object_object_foreach(object, key, value) {
        (void)value;
        if (!RzbNext_StringIn(key, allowed, count))
            return false;
    }
    return true;
}

static json_object *
RzbNext_GetTyped(json_object *object, const char *field, enum json_type type)
{
    json_object *value;

    if (!json_object_object_get_ex(object, field, &value))
        return NULL;
    if (json_object_get_type(value) != type)
        return NULL;
    return value;
}

static bool
RzbNext_RequireString(json_object *object, const char *field,
                      bool (*validator)(const char *))
{
    const char *value = RzbNext_GetString(object, field);

    return value != NULL && (validator == NULL || validator(value));
}

static bool
RzbNext_RequireEnum(json_object *object, const char *field,
                    const char * const *allowed, size_t count)
{
    return RzbNext_StringIn(RzbNext_GetString(object, field), allowed, count);
}

static bool
RzbNext_RequireBool(json_object *object, const char *field)
{
    return RzbNext_GetTyped(object, field, json_type_boolean) != NULL;
}

static bool
RzbNext_RequireIntRange(json_object *object, const char *field,
                        int64_t minimum, int64_t maximum)
{
    json_object *value = RzbNext_GetTyped(object, field, json_type_int);
    int64_t integer;

    if (value == NULL)
        return false;
    integer = json_object_get_int64(value);
    return integer >= minimum && integer <= maximum;
}

static bool
RzbNext_RequirePositiveInt(json_object *object, const char *field)
{
    return RzbNext_RequireIntRange(object, field, 1, INT64_MAX);
}

static bool
RzbNext_OptionalTimestamp(json_object *object, const char *field)
{
    return !RzbNext_ObjectHasField(object, field) ||
           RzbNext_RequireString(object, field, RzbNext_IsTimestamp);
}

static bool
RzbNext_StringArrayValid(json_object *array, bool (*validator)(const char *),
                         size_t minimum, bool unique)
{
    size_t count;
    size_t index;
    size_t other;

    if (array == NULL || json_object_get_type(array) != json_type_array)
        return false;
    count = json_object_array_length(array);
    if (count < minimum)
        return false;
    for (index = 0; index < count; index++) {
        json_object *item = json_object_array_get_idx(array, index);
        const char *value;

        if (item == NULL || json_object_get_type(item) != json_type_string)
            return false;
        value = json_object_get_string(item);
        if (validator != NULL && !validator(value))
            return false;
        if (!unique)
            continue;
        for (other = index + 1; other < count; other++) {
            json_object *otherItem = json_object_array_get_idx(array, other);

            if (otherItem != NULL &&
                json_object_get_type(otherItem) == json_type_string &&
                strcmp(value, json_object_get_string(otherItem)) == 0) {
                return false;
            }
        }
    }
    return true;
}

static bool
RzbNext_StringArrayFieldValid(json_object *object, const char *field,
                              bool (*validator)(const char *), size_t minimum,
                              bool unique)
{
    return RzbNext_StringArrayValid(RzbNext_GetTyped(object, field,
                                                    json_type_array),
                                    validator, minimum, unique);
}

static bool
RzbNext_IsSystemTag(const char *value)
{
    static const char * const tags[] = {
        "GOOD", "BAD", "SUSPICIOUS", "ALLOW_LIST", "BLOCK_LIST",
        "DIRTY", "NOT_STORED", "PROCESSING", "REMOVED"
    };

    return RzbNext_StringIn(value, tags, sizeof(tags) / sizeof(tags[0]));
}

static bool
RzbNext_ValidateBlock(json_object *object, bool storageOnly)
{
    static const char * const blockFields[] = {
        "sha256", "size", "data_type"
    };
    static const char * const storageFields[] = {
        "sha256", "size"
    };

    if (object == NULL || json_object_get_type(object) != json_type_object)
        return false;
    if (storageOnly) {
        if (!RzbNext_ObjectOnlyHasFields(
                object, storageFields,
                sizeof(storageFields) / sizeof(storageFields[0]))) {
            return false;
        }
    } else if (!RzbNext_ObjectOnlyHasFields(
                   object, blockFields,
                   sizeof(blockFields) / sizeof(blockFields[0]))) {
        return false;
    }

    if (!RzbNext_RequireString(object, "sha256", RzbNext_IsSha256) ||
        !RzbNext_RequirePositiveInt(object, "size")) {
        return false;
    }
    return storageOnly ||
           RzbNext_RequireString(object, "data_type", RzbNext_IsDataTypeName);
}

static bool
RzbNext_ValidateMetadataArray(json_object *array, size_t minimum, bool updates)
{
    static const char * const recordFields[] = {
        "name", "type", "value"
    };
    static const char * const updateFields[] = {
        "name", "type", "created_at", "value"
    };
    size_t count;
    size_t index;

    if (array == NULL || json_object_get_type(array) != json_type_array)
        return false;
    count = json_object_array_length(array);
    if (count < minimum)
        return false;
    for (index = 0; index < count; index++) {
        json_object *record = json_object_array_get_idx(array, index);
        json_object *value;

        if (record == NULL ||
            json_object_get_type(record) != json_type_object) {
            return false;
        }
        if (updates) {
            if (!RzbNext_ObjectOnlyHasFields(
                    record, updateFields,
                    sizeof(updateFields) / sizeof(updateFields[0])) ||
                !RzbNext_RequireString(record, "created_at",
                                       RzbNext_IsTimestamp)) {
                return false;
            }
        } else if (!RzbNext_ObjectOnlyHasFields(
                       record, recordFields,
                       sizeof(recordFields) / sizeof(recordFields[0]))) {
            return false;
        }
        if (!RzbNext_RequireString(record, "name", RzbNext_IsSafeKey) ||
            !RzbNext_RequireString(record, "type", RzbNext_IsSafeKey) ||
            !json_object_object_get_ex(record, "value", &value) ||
            json_object_get_type(value) == json_type_null) {
            return false;
        }
    }
    return true;
}

static bool
RzbNext_OptionalMetadataArray(json_object *object, const char *field,
                              size_t minimum, bool updates)
{
    if (!RzbNext_ObjectHasField(object, field))
        return true;
    return RzbNext_ValidateMetadataArray(
        RzbNext_GetTyped(object, field, json_type_array), minimum, updates);
}

static bool
RzbNext_ValidateTagMutations(json_object *object)
{
    static const char * const fields[] = {
        "set_system_tags", "unset_system_tags",
        "set_enterprise_tags", "unset_enterprise_tags"
    };
    bool seen = false;

    if (object == NULL || json_object_get_type(object) != json_type_object)
        return false;
    if (!RzbNext_ObjectOnlyHasFields(object, fields,
                                    sizeof(fields) / sizeof(fields[0]))) {
        return false;
    }
    if (RzbNext_ObjectHasField(object, "set_system_tags")) {
        seen = true;
        if (!RzbNext_StringArrayFieldValid(object, "set_system_tags",
                                           RzbNext_IsSystemTag, 1, true))
            return false;
    }
    if (RzbNext_ObjectHasField(object, "unset_system_tags")) {
        seen = true;
        if (!RzbNext_StringArrayFieldValid(object, "unset_system_tags",
                                           RzbNext_IsSystemTag, 1, true))
            return false;
    }
    if (RzbNext_ObjectHasField(object, "set_enterprise_tags")) {
        seen = true;
        if (!RzbNext_StringArrayFieldValid(object, "set_enterprise_tags",
                                           RzbNext_IsSafeKey, 1, true))
            return false;
    }
    if (RzbNext_ObjectHasField(object, "unset_enterprise_tags")) {
        seen = true;
        if (!RzbNext_StringArrayFieldValid(object, "unset_enterprise_tags",
                                           RzbNext_IsSafeKey, 1, true))
            return false;
    }
    return seen;
}

static bool
RzbNext_OptionalTagMutations(json_object *object, const char *field)
{
    if (!RzbNext_ObjectHasField(object, field))
        return true;
    return RzbNext_ValidateTagMutations(RzbNext_GetTyped(object, field,
                                                        json_type_object));
}

static bool
RzbNext_ValidateTextMessage(json_object *object, const char *field,
                            size_t maxLength, bool trimCheck)
{
    return RzbNext_TextBounded(RzbNext_GetString(object, field), maxLength,
                               trimCheck);
}

static bool
RzbNext_ValidateClaimCheck(json_object *object)
{
    static const char * const fields[] = {
        "reference_schema_name", "reference_schema_version", "signed_url",
        "expires_at", "bucket", "object_key", "sha256", "uncompressed_size",
        "stored_compressed_size", "content_type", "schema_name",
        "schema_version", "content_encoding"
    };

    return RzbNext_ObjectOnlyHasFields(object, fields,
                                      sizeof(fields) / sizeof(fields[0])) &&
           RzbNext_RequireString(object, "reference_schema_name", NULL) &&
           strcmp(RzbNext_GetString(object, "reference_schema_name"),
                  RZB_NEXT_SCHEMA_CLAIM_CHECK_REFERENCE) == 0 &&
           RzbNext_RequireIntRange(object, "reference_schema_version", 1, 1) &&
           RzbNext_ValidateTextMessage(object, "signed_url", 4096, false) &&
           strstr(RzbNext_GetString(object, "signed_url"), "://") != NULL &&
           RzbNext_RequireString(object, "expires_at", RzbNext_IsTimestamp) &&
           RzbNext_ValidateTextMessage(object, "bucket", 1024, false) &&
           RzbNext_ValidateTextMessage(object, "object_key", 4096, false) &&
           RzbNext_RequireString(object, "sha256", RzbNext_IsSha256) &&
           RzbNext_RequirePositiveInt(object, "uncompressed_size") &&
           RzbNext_RequirePositiveInt(object, "stored_compressed_size") &&
           RzbNext_ValidateTextMessage(object, "content_type", 256, false) &&
           RzbNext_RequireString(object, "schema_name", RzbNext_IsSchemaName) &&
           RzbNext_RequirePositiveInt(object, "schema_version") &&
           RzbNext_RequireString(object, "content_encoding", NULL) &&
           strcmp(RzbNext_GetString(object, "content_encoding"), "zlib") == 0;
}

static bool
RzbNext_ValidateCapabilities(json_object *object)
{
    static const char * const fields[] = {
        "component_version", "sdk_name", "sdk_version",
        "supported_message_body_modes", "supports_deferred_results"
    };
    static const char * const modes[] = {
        "inline", "zlib", "claim_check"
    };
    json_object *array;
    size_t count;
    size_t index;
    bool hasInline = false;

    if (object == NULL || json_object_get_type(object) != json_type_object)
        return false;
    if (!RzbNext_ObjectOnlyHasFields(object, fields,
                                    sizeof(fields) / sizeof(fields[0])) ||
        !RzbNext_ValidateTextMessage(object, "component_version", 128, true) ||
        !RzbNext_ValidateTextMessage(object, "sdk_name", 128, true) ||
        !RzbNext_ValidateTextMessage(object, "sdk_version", 128, true) ||
        !RzbNext_RequireBool(object, "supports_deferred_results")) {
        return false;
    }

    array = RzbNext_GetTyped(object, "supported_message_body_modes",
                             json_type_array);
    if (!RzbNext_StringArrayValid(array, NULL, 1, true))
        return false;
    count = json_object_array_length(array);
    for (index = 0; index < count; index++) {
        const char *mode =
            json_object_get_string(json_object_array_get_idx(array, index));

        if (!RzbNext_StringIn(mode, modes, sizeof(modes) / sizeof(modes[0])))
            return false;
        if (strcmp(mode, "inline") == 0)
            hasInline = true;
    }
    return hasInline;
}

static bool
RzbNext_ValidateDataTypes(json_object *object)
{
    json_object *array;
    size_t count;
    size_t index;
    bool hasAnyData = false;

    if (!RzbNext_ObjectHasField(object, "data_types"))
        return true;
    array = RzbNext_GetTyped(object, "data_types", json_type_array);
    if (!RzbNext_StringArrayValid(array, RzbNext_IsDataTypeName, 1, true))
        return false;
    count = json_object_array_length(array);
    for (index = 0; index < count; index++) {
        const char *dataType =
            json_object_get_string(json_object_array_get_idx(array, index));

        if (strcmp(dataType, "ANY_DATA") == 0)
            hasAnyData = true;
    }
    return !hasAnyData || count == 1;
}

static bool
RzbNext_ValidateCncRegistrationRequest(json_object *object)
{
    static const char * const fields[] = {
        "schema_name", "schema_version", "request_id", "nugget_uuid",
        "process_uuid", "nugget_type", "app_type", "data_types",
        "capabilities", "desired_runtime_policy", "created_at"
    };
    static const char * const runtimePolicies[] = { "running", "paused" };

    return RzbNext_ObjectOnlyHasFields(object, fields,
                                      sizeof(fields) / sizeof(fields[0])) &&
           RzbNext_RequireString(object, "request_id", RzbNext_IsUuid) &&
           RzbNext_RequireString(object, "nugget_uuid", RzbNext_IsUuid) &&
           RzbNext_RequireString(object, "process_uuid", RzbNext_IsUuid) &&
           RzbNext_RequireString(object, "nugget_type", RzbNext_IsSafeKey) &&
           RzbNext_RequireString(object, "app_type", RzbNext_IsSafeKey) &&
           RzbNext_ValidateDataTypes(object) &&
           RzbNext_ValidateCapabilities(RzbNext_GetTyped(object, "capabilities",
                                                        json_type_object)) &&
           RzbNext_RequireEnum(object, "desired_runtime_policy",
                               runtimePolicies,
                               sizeof(runtimePolicies) /
                               sizeof(runtimePolicies[0])) &&
           RzbNext_RequireString(object, "created_at", RzbNext_IsTimestamp);
}

static bool
RzbNext_ValidateCncRegistrationAccepted(json_object *object)
{
    static const char * const fields[] = {
        "schema_name", "schema_version", "request_id", "nugget_uuid",
        "registration_generation", "effective_runtime_policy",
        "liveness_interval", "liveness_freshness_window",
        "liveness_clock_skew_tolerance", "created_at"
    };
    static const char * const runtimePolicies[] = { "running", "paused" };

    return RzbNext_ObjectOnlyHasFields(object, fields,
                                      sizeof(fields) / sizeof(fields[0])) &&
           RzbNext_RequireString(object, "request_id", RzbNext_IsUuid) &&
           RzbNext_RequireString(object, "nugget_uuid", RzbNext_IsUuid) &&
           RzbNext_RequireString(object, "registration_generation",
                                 RzbNext_IsUuid) &&
           RzbNext_RequireEnum(object, "effective_runtime_policy",
                               runtimePolicies,
                               sizeof(runtimePolicies) /
                               sizeof(runtimePolicies[0])) &&
           RzbNext_RequirePositiveInt(object, "liveness_interval") &&
           RzbNext_RequirePositiveInt(object, "liveness_freshness_window") &&
           RzbNext_RequirePositiveInt(object,
                                      "liveness_clock_skew_tolerance") &&
           RzbNext_RequireString(object, "created_at", RzbNext_IsTimestamp);
}

static bool
RzbNext_ValidateCncRegistrationRejected(json_object *object)
{
    static const char * const fields[] = {
        "schema_name", "schema_version", "request_id", "nugget_uuid",
        "reason_code", "message", "retryable", "retry_after", "created_at"
    };
    static const char * const reasons[] = {
        "invalid_schema", "unsupported_schema_version", "invalid_uuid",
        "invalid_catalog_reference", "retired_catalog_reference",
        "unauthorized", "auto_provision_disabled",
        "auto_provision_not_allowed", "topology_error", "duplicate_identity",
        "dependency_unavailable", "rate_limited", "internal_error"
    };
    json_object *retryable = RzbNext_GetTyped(object, "retryable",
                                             json_type_boolean);

    if (!RzbNext_ObjectOnlyHasFields(object, fields,
                                    sizeof(fields) / sizeof(fields[0])) ||
        !RzbNext_RequireString(object, "request_id", RzbNext_IsUuid) ||
        !RzbNext_RequireString(object, "nugget_uuid", RzbNext_IsUuid) ||
        !RzbNext_RequireEnum(object, "reason_code", reasons,
                             sizeof(reasons) / sizeof(reasons[0])) ||
        !RzbNext_ValidateTextMessage(object, "message", 4096, true) ||
        retryable == NULL ||
        !RzbNext_RequireString(object, "created_at", RzbNext_IsTimestamp)) {
        return false;
    }
    if (!RzbNext_ObjectHasField(object, "retry_after"))
        return true;
    return json_object_get_boolean(retryable) &&
           RzbNext_RequirePositiveInt(object, "retry_after");
}

static bool
RzbNext_ValidateCncLiveness(json_object *object)
{
    static const char * const fields[] = {
        "schema_name", "schema_version", "nugget_uuid",
        "registration_generation", "runtime_policy", "availability",
        "created_at"
    };
    static const char * const runtimePolicies[] = { "running", "paused" };
    static const char * const availability[] = {
        "ready", "registration_gated", "dependency_paused",
        "draining", "failed"
    };

    return RzbNext_ObjectOnlyHasFields(object, fields,
                                      sizeof(fields) / sizeof(fields[0])) &&
           RzbNext_RequireString(object, "nugget_uuid", RzbNext_IsUuid) &&
           RzbNext_RequireString(object, "registration_generation",
                                 RzbNext_IsUuid) &&
           RzbNext_RequireEnum(object, "runtime_policy", runtimePolicies,
                               sizeof(runtimePolicies) /
                               sizeof(runtimePolicies[0])) &&
           RzbNext_RequireEnum(object, "availability", availability,
                               sizeof(availability) / sizeof(availability[0])) &&
           RzbNext_RequireString(object, "created_at", RzbNext_IsTimestamp);
}

static bool
RzbNext_ValidateCncBye(json_object *object)
{
    static const char * const fields[] = {
        "schema_name", "schema_version", "nugget_uuid",
        "registration_generation", "reason", "created_at"
    };
    static const char * const reasons[] = {
        "shutdown", "restart", "operator_requested", "terminate_command",
        "fatal_error", "dependency_failure"
    };

    return RzbNext_ObjectOnlyHasFields(object, fields,
                                      sizeof(fields) / sizeof(fields[0])) &&
           RzbNext_RequireString(object, "nugget_uuid", RzbNext_IsUuid) &&
           RzbNext_RequireString(object, "registration_generation",
                                 RzbNext_IsUuid) &&
           RzbNext_RequireEnum(object, "reason", reasons,
                               sizeof(reasons) / sizeof(reasons[0])) &&
           RzbNext_RequireString(object, "created_at", RzbNext_IsTimestamp);
}

static bool
RzbNext_DirectedReasonAllowed(const char *command, const char *reason)
{
    static const char * const pauseReasons[] = {
        "operator_requested", "policy_change", "internal_recovery"
    };
    static const char * const goReasons[] = {
        "operator_requested", "policy_change", "dependency_recovery",
        "internal_recovery"
    };
    static const char * const terminateReasons[] = {
        "operator_requested", "shutdown", "internal_recovery"
    };
    static const char * const reregisterReasons[] = {
        "stale_presence", "generation_mismatch", "catalog_invalidation",
        "dependency_recovery", "internal_recovery"
    };
    static const char * const cacheReasons[] = {
        "operator_requested", "policy_change", "catalog_invalidation",
        "internal_recovery"
    };

    if (strcmp(command, "pause") == 0)
        return RzbNext_StringIn(reason, pauseReasons,
                                sizeof(pauseReasons) /
                                sizeof(pauseReasons[0]));
    if (strcmp(command, "go") == 0)
        return RzbNext_StringIn(reason, goReasons,
                                sizeof(goReasons) / sizeof(goReasons[0]));
    if (strcmp(command, "terminate") == 0)
        return RzbNext_StringIn(reason, terminateReasons,
                                sizeof(terminateReasons) /
                                sizeof(terminateReasons[0]));
    if (strcmp(command, "re_register") == 0)
        return RzbNext_StringIn(reason, reregisterReasons,
                                sizeof(reregisterReasons) /
                                sizeof(reregisterReasons[0]));
    if (strcmp(command, "cache_invalidate") == 0)
        return RzbNext_StringIn(reason, cacheReasons,
                                sizeof(cacheReasons) /
                                sizeof(cacheReasons[0]));
    return false;
}

static bool
RzbNext_ValidateCncDirectedCommand(json_object *object)
{
    static const char * const fields[] = {
        "schema_name", "schema_version", "command_id", "target_nugget_uuid",
        "registration_generation", "command", "reason_code", "invalidation_id",
        "created_at"
    };
    const char *command;
    const char *reason;
    bool hasInvalidation;

    if (!RzbNext_ObjectOnlyHasFields(object, fields,
                                    sizeof(fields) / sizeof(fields[0])) ||
        !RzbNext_RequireString(object, "command_id", RzbNext_IsUuid) ||
        !RzbNext_RequireString(object, "target_nugget_uuid", RzbNext_IsUuid) ||
        !RzbNext_RequireString(object, "registration_generation",
                               RzbNext_IsUuid) ||
        !RzbNext_RequireString(object, "created_at", RzbNext_IsTimestamp)) {
        return false;
    }
    command = RzbNext_GetString(object, "command");
    reason = RzbNext_GetString(object, "reason_code");
    hasInvalidation = RzbNext_ObjectHasField(object, "invalidation_id");
    if (command == NULL || reason == NULL ||
        !RzbNext_DirectedReasonAllowed(command, reason)) {
        return false;
    }
    if (strcmp(command, "cache_invalidate") == 0) {
        return hasInvalidation &&
               RzbNext_RequireString(object, "invalidation_id",
                                     RzbNext_IsUuid);
    }
    return !hasInvalidation;
}

static bool
RzbNext_ValidateDispatcherHelloReasons(json_object *object, bool ready,
                                       const char *availability)
{
    static const char * const reasonValues[] = {
        "starting", "draining", "rabbitmq_unavailable", "mysql_unavailable",
        "etcd_unavailable", "cassandra_unavailable", "kafka_unavailable",
        "claim_check_unavailable", "catalog_cache_stale", "internal_error"
    };
    json_object *array = RzbNext_GetTyped(object, "dependency_reason_codes",
                                         json_type_array);
    size_t count;
    size_t index;

    if (!RzbNext_StringArrayValid(array, NULL, 0, true))
        return false;
    count = json_object_array_length(array);
    for (index = 0; index < count; index++) {
        const char *reason =
            json_object_get_string(json_object_array_get_idx(array, index));

        if (!RzbNext_StringIn(reason, reasonValues,
                              sizeof(reasonValues) /
                              sizeof(reasonValues[0]))) {
            return false;
        }
    }

    if (ready && strcmp(availability, "ready") == 0)
        return count == 0;
    if (ready && strcmp(availability, "degraded") == 0)
        return count > 0;
    if (!ready && strcmp(availability, "starting") == 0)
        return count == 1 &&
               strcmp(json_object_get_string(
                          json_object_array_get_idx(array, 0)),
                      "starting") == 0;
    if (!ready && strcmp(availability, "draining") == 0)
        return count == 1 &&
               strcmp(json_object_get_string(
                          json_object_array_get_idx(array, 0)),
                      "draining") == 0;
    if (!ready && strcmp(availability, "failed") == 0)
        return count > 0;
    return false;
}

static bool
RzbNext_ValidateCncDispatcherHello(json_object *object)
{
    static const char * const fields[] = {
        "schema_name", "schema_version", "dispatcher_id", "created_at",
        "started_at", "ready", "availability", "dependency_reason_codes"
    };
    static const char * const availabilityValues[] = {
        "starting", "ready", "degraded", "draining", "failed"
    };
    json_object *ready;
    const char *availability;

    if (!RzbNext_ObjectOnlyHasFields(object, fields,
                                    sizeof(fields) / sizeof(fields[0])) ||
        !RzbNext_RequireString(object, "dispatcher_id", RzbNext_IsUuid) ||
        !RzbNext_RequireString(object, "created_at", RzbNext_IsTimestamp) ||
        !RzbNext_RequireString(object, "started_at", RzbNext_IsTimestamp) ||
        !RzbNext_RequireEnum(object, "availability", availabilityValues,
                             sizeof(availabilityValues) /
                             sizeof(availabilityValues[0]))) {
        return false;
    }
    ready = RzbNext_GetTyped(object, "ready", json_type_boolean);
    availability = RzbNext_GetString(object, "availability");
    return ready != NULL &&
           RzbNext_ValidateDispatcherHelloReasons(
               object, json_object_get_boolean(ready), availability);
}

static bool
RzbNext_ValidateBlockSubmission(json_object *object)
{
    static const char * const fields[] = {
        "schema_name", "schema_version", "event_id", "source_nugget_uuid",
        "block", "stored", "event_metadata", "parent_event_id",
        "parent_block", "created_at"
    };
    bool hasParentEvent;
    bool hasParentBlock;

    if (!RzbNext_ObjectOnlyHasFields(object, fields,
                                    sizeof(fields) / sizeof(fields[0])) ||
        !RzbNext_RequireString(object, "event_id", RzbNext_IsUuid) ||
        !RzbNext_RequireString(object, "source_nugget_uuid",
                               RzbNext_IsUuid) ||
        !RzbNext_ValidateBlock(RzbNext_GetTyped(object, "block",
                                               json_type_object), false) ||
        !RzbNext_RequireBool(object, "stored") ||
        !RzbNext_OptionalMetadataArray(object, "event_metadata", 1, false) ||
        !RzbNext_RequireString(object, "created_at", RzbNext_IsTimestamp)) {
        return false;
    }
    hasParentEvent = RzbNext_ObjectHasField(object, "parent_event_id");
    hasParentBlock = RzbNext_ObjectHasField(object, "parent_block");
    if (hasParentEvent != hasParentBlock)
        return false;
    if (hasParentEvent &&
        (!RzbNext_RequireString(object, "parent_event_id", RzbNext_IsUuid) ||
         !RzbNext_ValidateBlock(RzbNext_GetTyped(object, "parent_block",
                                                json_type_object), false))) {
        return false;
    }
    return true;
}

static bool
RzbNext_ValidateReinspectionRequest(json_object *object)
{
    static const char * const fields[] = { "reason_code" };
    static const char * const reasons[] = {
        "operator_requested", "dirty_block", "dependency_recovery",
        "state_reconciliation"
    };

    return object != NULL &&
           json_object_get_type(object) == json_type_object &&
           RzbNext_ObjectOnlyHasFields(object, fields,
                                      sizeof(fields) / sizeof(fields[0])) &&
           RzbNext_RequireEnum(object, "reason_code", reasons,
                               sizeof(reasons) / sizeof(reasons[0]));
}

static bool
RzbNext_OptionalReinspectionRequest(json_object *object)
{
    if (!RzbNext_ObjectHasField(object, "reinspection_request"))
        return true;
    return RzbNext_ValidateReinspectionRequest(
        RzbNext_GetTyped(object, "reinspection_request", json_type_object));
}

static bool
RzbNext_ValidateBlockUpdate(json_object *object)
{
    static const char * const fields[] = {
        "schema_name", "schema_version", "update_id", "source_nugget_uuid",
        "block", "metadata_updates", "tag_mutations",
        "reinspection_request", "created_at"
    };
    bool hasMutation;

    if (!RzbNext_ObjectOnlyHasFields(object, fields,
                                    sizeof(fields) / sizeof(fields[0])) ||
        !RzbNext_RequireString(object, "update_id", RzbNext_IsUuid) ||
        !RzbNext_RequireString(object, "source_nugget_uuid",
                               RzbNext_IsUuid) ||
        !RzbNext_ValidateBlock(RzbNext_GetTyped(object, "block",
                                               json_type_object), false) ||
        !RzbNext_OptionalMetadataArray(object, "metadata_updates", 1, true) ||
        !RzbNext_OptionalTagMutations(object, "tag_mutations") ||
        !RzbNext_OptionalReinspectionRequest(object) ||
        !RzbNext_RequireString(object, "created_at", RzbNext_IsTimestamp)) {
        return false;
    }
    hasMutation = RzbNext_ObjectHasField(object, "metadata_updates") ||
                  RzbNext_ObjectHasField(object, "tag_mutations") ||
                  RzbNext_ObjectHasField(object, "reinspection_request");
    return hasMutation;
}

static bool
RzbNext_ValidateInspectionEvent(json_object *object)
{
    static const char * const fields[] = {
        "event_id", "event_kind", "created_at", "source_nugget_uuid",
        "stored", "parent_event_id", "parent_block", "update_id", "metadata"
    };
    const char *eventKind;
    bool hasParentEvent;
    bool hasParentBlock;

    if (object == NULL || json_object_get_type(object) != json_type_object ||
        !RzbNext_ObjectOnlyHasFields(object, fields,
                                    sizeof(fields) / sizeof(fields[0])) ||
        !RzbNext_RequireString(object, "event_id", RzbNext_IsUuid) ||
        !RzbNext_RequireString(object, "created_at", RzbNext_IsTimestamp) ||
        !RzbNext_RequireString(object, "source_nugget_uuid",
                               RzbNext_IsUuid) ||
        !RzbNext_OptionalMetadataArray(object, "metadata", 0, false)) {
        return false;
    }

    eventKind = RzbNext_GetString(object, "event_kind");
    if (eventKind == NULL)
        return false;
    if (strcmp(eventKind, "source_submission") == 0) {
        if (!RzbNext_RequireBool(object, "stored") ||
            !RzbNext_ObjectHasField(object, "metadata") ||
            RzbNext_ObjectHasField(object, "update_id")) {
            return false;
        }
        hasParentEvent = RzbNext_ObjectHasField(object, "parent_event_id");
        hasParentBlock = RzbNext_ObjectHasField(object, "parent_block");
        if (hasParentEvent != hasParentBlock)
            return false;
        if (hasParentEvent &&
            (!RzbNext_RequireString(object, "parent_event_id",
                                    RzbNext_IsUuid) ||
             !RzbNext_ValidateBlock(RzbNext_GetTyped(object, "parent_block",
                                                    json_type_object),
                                    false))) {
            return false;
        }
        return true;
    }
    if (strcmp(eventKind, "unsolicited_block_update") == 0) {
        return RzbNext_RequireString(object, "update_id", RzbNext_IsUuid) &&
               RzbNext_ObjectHasField(object, "metadata") &&
               !RzbNext_ObjectHasField(object, "stored") &&
               !RzbNext_ObjectHasField(object, "parent_event_id") &&
               !RzbNext_ObjectHasField(object, "parent_block");
    }
    return false;
}

static bool
RzbNext_ValidateInspectionWork(json_object *object)
{
    static const char * const fields[] = {
        "schema_name", "schema_version", "inspection_id", "block",
        "app_type", "work_kind", "event", "created_at"
    };
    static const char * const kinds[] = { "inspect", "deferred_poll" };

    return RzbNext_ObjectOnlyHasFields(object, fields,
                                      sizeof(fields) / sizeof(fields[0])) &&
           RzbNext_RequireString(object, "inspection_id", RzbNext_IsUuid) &&
           RzbNext_ValidateBlock(RzbNext_GetTyped(object, "block",
                                                 json_type_object), false) &&
           RzbNext_RequireString(object, "app_type", RzbNext_IsSafeKey) &&
           RzbNext_RequireEnum(object, "work_kind", kinds,
                               sizeof(kinds) / sizeof(kinds[0])) &&
           RzbNext_ValidateInspectionEvent(RzbNext_GetTyped(object, "event",
                                                           json_type_object)) &&
           RzbNext_RequireString(object, "created_at", RzbNext_IsTimestamp);
}

static bool
RzbNext_ValidateAnalysisError(json_object *object)
{
    static const char * const fields[] = {
        "category", "code", "message", "details"
    };
    static const char * const categories[] = {
        "input_malformed", "input_unsupported", "dependency_unavailable",
        "timeout", "resource_exhausted", "policy_rejected",
        "inspector_failed", "internal_error"
    };

    return object != NULL &&
           json_object_get_type(object) == json_type_object &&
           RzbNext_ObjectOnlyHasFields(object, fields,
                                      sizeof(fields) / sizeof(fields[0])) &&
           RzbNext_RequireEnum(object, "category", categories,
                               sizeof(categories) / sizeof(categories[0])) &&
           RzbNext_RequireString(object, "code", RzbNext_IsSafeKey) &&
           RzbNext_ValidateTextMessage(object, "message", 4096, false) &&
           (!RzbNext_ObjectHasField(object, "details") ||
            RzbNext_GetTyped(object, "details", json_type_object) != NULL);
}

static bool
RzbNext_ValidateAnalysisDeferred(json_object *object)
{
    static const char * const fields[] = {
        "reason_code", "message", "poll_after", "details"
    };

    return object != NULL &&
           json_object_get_type(object) == json_type_object &&
           RzbNext_ObjectOnlyHasFields(object, fields,
                                      sizeof(fields) / sizeof(fields[0])) &&
           RzbNext_RequireString(object, "reason_code", RzbNext_IsSafeKey) &&
           RzbNext_ValidateTextMessage(object, "message", 4096, false) &&
           RzbNext_OptionalTimestamp(object, "poll_after") &&
           (!RzbNext_ObjectHasField(object, "details") ||
            RzbNext_GetTyped(object, "details", json_type_object) != NULL);
}

static bool
RzbNext_ValidateAlerts(json_object *array)
{
    static const char * const fields[] = {
        "alert_id", "created_at", "priority", "title", "message", "metadata"
    };
    size_t count;
    size_t index;

    if (array == NULL || json_object_get_type(array) != json_type_array)
        return false;
    count = json_object_array_length(array);
    for (index = 0; index < count; index++) {
        json_object *alert = json_object_array_get_idx(array, index);

        if (alert == NULL ||
            json_object_get_type(alert) != json_type_object ||
            !RzbNext_ObjectOnlyHasFields(alert, fields,
                                        sizeof(fields) / sizeof(fields[0])) ||
            !RzbNext_RequireString(alert, "alert_id", RzbNext_IsUuid) ||
            !RzbNext_RequireString(alert, "created_at",
                                   RzbNext_IsTimestamp) ||
            !RzbNext_RequireIntRange(alert, "priority", 0, 255) ||
            !RzbNext_ValidateTextMessage(alert, "title", 256, false) ||
            !RzbNext_ValidateTextMessage(alert, "message", 4096, false) ||
            !RzbNext_OptionalMetadataArray(alert, "metadata", 0, false) ||
            !RzbNext_ObjectHasField(alert, "metadata")) {
            return false;
        }
    }
    return true;
}

static bool
RzbNext_ValidateAnalysisResult(json_object *object)
{
    static const char * const fields[] = {
        "schema_name", "schema_version", "inspection_id", "event_id",
        "block", "inspector_id", "app_type", "result_status",
        "block_metadata_updates", "metadata", "tag_mutations", "alerts",
        "error", "deferred", "created_at"
    };
    const char *status;

    if (!RzbNext_ObjectOnlyHasFields(object, fields,
                                    sizeof(fields) / sizeof(fields[0])) ||
        !RzbNext_RequireString(object, "inspection_id", RzbNext_IsUuid) ||
        !RzbNext_RequireString(object, "event_id", RzbNext_IsUuid) ||
        !RzbNext_ValidateBlock(RzbNext_GetTyped(object, "block",
                                               json_type_object), false) ||
        !RzbNext_RequireString(object, "inspector_id", RzbNext_IsUuid) ||
        !RzbNext_RequireString(object, "app_type", RzbNext_IsSafeKey) ||
        !RzbNext_OptionalMetadataArray(object, "block_metadata_updates", 1,
                                       true) ||
        !RzbNext_OptionalMetadataArray(object, "metadata", 0, false) ||
        !RzbNext_OptionalTagMutations(object, "tag_mutations") ||
        !RzbNext_RequireString(object, "created_at", RzbNext_IsTimestamp)) {
        return false;
    }
    if (RzbNext_ObjectHasField(object, "alerts") &&
        !RzbNext_ValidateAlerts(RzbNext_GetTyped(object, "alerts",
                                                json_type_array))) {
        return false;
    }
    status = RzbNext_GetString(object, "result_status");
    if (status == NULL)
        return false;
    if (strcmp(status, "completed") == 0)
        return !RzbNext_ObjectHasField(object, "error") &&
               !RzbNext_ObjectHasField(object, "deferred");
    if (strcmp(status, "error") == 0)
        return RzbNext_ValidateAnalysisError(
                   RzbNext_GetTyped(object, "error", json_type_object)) &&
               !RzbNext_ObjectHasField(object, "deferred") &&
               !RzbNext_ObjectHasField(object, "block_metadata_updates") &&
               !RzbNext_ObjectHasField(object, "metadata") &&
               !RzbNext_ObjectHasField(object, "tag_mutations") &&
               !RzbNext_ObjectHasField(object, "alerts");
    if (strcmp(status, "deferred") == 0)
        return RzbNext_ValidateAnalysisDeferred(
                   RzbNext_GetTyped(object, "deferred", json_type_object)) &&
               !RzbNext_ObjectHasField(object, "error") &&
               !RzbNext_ObjectHasField(object, "block_metadata_updates") &&
               !RzbNext_ObjectHasField(object, "metadata") &&
               !RzbNext_ObjectHasField(object, "tag_mutations") &&
               !RzbNext_ObjectHasField(object, "alerts");
    return false;
}

static bool
RzbNext_ValidateCacheRequest(json_object *object)
{
    static const char * const fields[] = {
        "schema_name", "schema_version", "request_id", "requestor_uuid",
        "block", "created_at"
    };

    return RzbNext_ObjectOnlyHasFields(object, fields,
                                      sizeof(fields) / sizeof(fields[0])) &&
           RzbNext_RequireString(object, "request_id", RzbNext_IsUuid) &&
           RzbNext_RequireString(object, "requestor_uuid", RzbNext_IsUuid) &&
           RzbNext_ValidateBlock(RzbNext_GetTyped(object, "block",
                                                 json_type_object), false) &&
           RzbNext_RequireString(object, "created_at", RzbNext_IsTimestamp);
}

static bool
RzbNext_ValidateCacheResponse(json_object *object)
{
    static const char * const fields[] = {
        "schema_name", "schema_version", "request_id", "block", "found",
        "system_tags", "enterprise_tags", "created_at"
    };

    return RzbNext_ObjectOnlyHasFields(object, fields,
                                      sizeof(fields) / sizeof(fields[0])) &&
           RzbNext_RequireString(object, "request_id", RzbNext_IsUuid) &&
           RzbNext_ValidateBlock(RzbNext_GetTyped(object, "block",
                                                 json_type_object), false) &&
           RzbNext_RequireBool(object, "found") &&
           RzbNext_StringArrayFieldValid(object, "system_tags",
                                         RzbNext_IsSystemTag, 0, true) &&
           RzbNext_StringArrayFieldValid(object, "enterprise_tags",
                                         RzbNext_IsSafeKey, 0, true) &&
           RzbNext_RequireString(object, "created_at", RzbNext_IsTimestamp);
}

static bool
RzbNext_ValidateCatalogInvalidation(json_object *object)
{
    static const char * const fields[] = {
        "schema_name", "schema_version", "mutation_id", "resource_type",
        "resource_key", "mutation_action", "invalidation_scope", "created_at"
    };
    static const char * const resourceTypes[] = {
        "nugget_type", "app_type", "data_type", "data_type_alias",
        "metadata_name", "metadata_type", "locality", "nugget",
        "system_tag", "enterprise_tag"
    };
    static const char * const actions[] = {
        "created", "updated", "retired", "deleted"
    };
    static const char * const scopes[] = { "global", "locality", "nugget" };

    return RzbNext_ObjectOnlyHasFields(object, fields,
                                      sizeof(fields) / sizeof(fields[0])) &&
           RzbNext_RequireString(object, "mutation_id", RzbNext_IsUuid) &&
           RzbNext_RequireEnum(object, "resource_type", resourceTypes,
                               sizeof(resourceTypes) /
                               sizeof(resourceTypes[0])) &&
           RzbNext_ValidateTextMessage(object, "resource_key", 512, false) &&
           RzbNext_RequireEnum(object, "mutation_action", actions,
                               sizeof(actions) / sizeof(actions[0])) &&
           RzbNext_RequireEnum(object, "invalidation_scope", scopes,
                               sizeof(scopes) / sizeof(scopes[0])) &&
           RzbNext_RequireString(object, "created_at", RzbNext_IsTimestamp);
}

static bool
RzbNext_ValidateFileRemoveRequest(json_object *object)
{
    static const char * const fields[] = {
        "schema_name", "schema_version", "request_id", "block",
        "reason_code", "created_at"
    };
    static const char * const reasons[] = {
        "administrative_removal", "retention_expired",
        "state_reconciliation", "test_fixture"
    };

    return RzbNext_ObjectOnlyHasFields(object, fields,
                                      sizeof(fields) / sizeof(fields[0])) &&
           RzbNext_RequireString(object, "request_id", RzbNext_IsUuid) &&
           RzbNext_ValidateBlock(RzbNext_GetTyped(object, "block",
                                                 json_type_object), true) &&
           RzbNext_RequireEnum(object, "reason_code", reasons,
                               sizeof(reasons) / sizeof(reasons[0])) &&
           RzbNext_RequireString(object, "created_at", RzbNext_IsTimestamp);
}

static bool
RzbNext_ValidateFileRemoveError(json_object *object)
{
    static const char * const fields[] = {
        "category", "code", "message", "details"
    };
    static const char * const categories[] = {
        "invalid_request", "unsafe_target", "dependency_unavailable",
        "timeout", "permission_denied", "internal_error"
    };

    return object != NULL &&
           json_object_get_type(object) == json_type_object &&
           RzbNext_ObjectOnlyHasFields(object, fields,
                                      sizeof(fields) / sizeof(fields[0])) &&
           RzbNext_RequireEnum(object, "category", categories,
                               sizeof(categories) / sizeof(categories[0])) &&
           RzbNext_RequireString(object, "code", RzbNext_IsSafeKey) &&
           RzbNext_ValidateTextMessage(object, "message", 4096, false) &&
           (!RzbNext_ObjectHasField(object, "details") ||
            RzbNext_GetTyped(object, "details", json_type_object) != NULL);
}

static bool
RzbNext_ValidateFileRemoveResult(json_object *object)
{
    static const char * const fields[] = {
        "schema_name", "schema_version", "request_id", "service", "status",
        "error", "created_at"
    };
    static const char * const services[] = { "file_store", "varnish" };
    static const char * const statuses[] = { "success", "failed" };
    const char *status;

    if (!RzbNext_ObjectOnlyHasFields(object, fields,
                                    sizeof(fields) / sizeof(fields[0])) ||
        !RzbNext_RequireString(object, "request_id", RzbNext_IsUuid) ||
        !RzbNext_RequireEnum(object, "service", services,
                             sizeof(services) / sizeof(services[0])) ||
        !RzbNext_RequireEnum(object, "status", statuses,
                             sizeof(statuses) / sizeof(statuses[0])) ||
        !RzbNext_RequireString(object, "created_at", RzbNext_IsTimestamp)) {
        return false;
    }
    status = RzbNext_GetString(object, "status");
    if (strcmp(status, "success") == 0)
        return !RzbNext_ObjectHasField(object, "error");
    return RzbNext_ValidateFileRemoveError(
        RzbNext_GetTyped(object, "error", json_type_object));
}

static bool
RzbNext_ValidateSearchExport(json_object *object)
{
    static const char * const fields[] = {
        "schema_name", "schema_version", "export_event_id",
        "aggregate_type", "aggregate_id", "event_type", "occurred_at",
        "target_index", "document_id", "document"
    };
    static const char * const aggregateTypes[] = {
        "block", "event", "analysis_result", "alert", "audit"
    };
    static const char * const eventTypes[] = {
        "event.upserted", "block.seen", "block.updated",
        "event.inspection_status_updated", "analysis_result.completed",
        "analysis_result.error", "analysis_result.deferred", "alert.created",
        "block.finalized", "audit.recorded"
    };
    static const char * const targetIndexes[] = {
        "razorback-blocks", "razorback-events",
        "razorback-analysis-results", "razorback-alerts",
        "razorback-admin-audit"
    };
    json_object *document;

    document = RzbNext_GetTyped(object, "document", json_type_object);
    return RzbNext_ObjectOnlyHasFields(object, fields,
                                      sizeof(fields) / sizeof(fields[0])) &&
           RzbNext_RequireString(object, "export_event_id",
                                 RzbNext_IsPrintableId) &&
           RzbNext_RequireEnum(object, "aggregate_type", aggregateTypes,
                               sizeof(aggregateTypes) /
                               sizeof(aggregateTypes[0])) &&
           RzbNext_RequireString(object, "aggregate_id",
                                 RzbNext_IsPrintableId) &&
           RzbNext_RequireEnum(object, "event_type", eventTypes,
                               sizeof(eventTypes) / sizeof(eventTypes[0])) &&
           RzbNext_RequireString(object, "occurred_at", RzbNext_IsTimestamp) &&
           RzbNext_RequireEnum(object, "target_index", targetIndexes,
                               sizeof(targetIndexes) /
                               sizeof(targetIndexes[0])) &&
           RzbNext_RequireString(object, "document_id",
                                 RzbNext_IsPrintableId) &&
           document != NULL &&
           RzbNext_ObjectFieldCount(document) > 0;
}

static bool
RzbNext_ValidateObjectForSchema(json_object *object, const char *schemaName)
{
    if (strcmp(schemaName, RZB_NEXT_SCHEMA_CLAIM_CHECK_REFERENCE) == 0)
        return RzbNext_ValidateClaimCheck(object);
    if (strcmp(schemaName, RZB_NEXT_SCHEMA_CNC_REGISTRATION_REQUEST) == 0)
        return RzbNext_ValidateCncRegistrationRequest(object);
    if (strcmp(schemaName, RZB_NEXT_SCHEMA_CNC_REGISTRATION_ACCEPTED) == 0)
        return RzbNext_ValidateCncRegistrationAccepted(object);
    if (strcmp(schemaName, RZB_NEXT_SCHEMA_CNC_REGISTRATION_REJECTED) == 0)
        return RzbNext_ValidateCncRegistrationRejected(object);
    if (strcmp(schemaName, RZB_NEXT_SCHEMA_CNC_LIVENESS) == 0)
        return RzbNext_ValidateCncLiveness(object);
    if (strcmp(schemaName, RZB_NEXT_SCHEMA_CNC_BYE) == 0)
        return RzbNext_ValidateCncBye(object);
    if (strcmp(schemaName, RZB_NEXT_SCHEMA_CNC_DIRECTED_COMMAND) == 0)
        return RzbNext_ValidateCncDirectedCommand(object);
    if (strcmp(schemaName, RZB_NEXT_SCHEMA_CNC_DISPATCHER_HELLO) == 0)
        return RzbNext_ValidateCncDispatcherHello(object);
    if (strcmp(schemaName, RZB_NEXT_SCHEMA_BLOCK_SUBMISSION) == 0)
        return RzbNext_ValidateBlockSubmission(object);
    if (strcmp(schemaName, RZB_NEXT_SCHEMA_BLOCK_UPDATE) == 0)
        return RzbNext_ValidateBlockUpdate(object);
    if (strcmp(schemaName, RZB_NEXT_SCHEMA_INSPECTION_WORK) == 0)
        return RzbNext_ValidateInspectionWork(object);
    if (strcmp(schemaName, RZB_NEXT_SCHEMA_ANALYSIS_RESULT) == 0)
        return RzbNext_ValidateAnalysisResult(object);
    if (strcmp(schemaName, RZB_NEXT_SCHEMA_CACHE_REQUEST) == 0)
        return RzbNext_ValidateCacheRequest(object);
    if (strcmp(schemaName, RZB_NEXT_SCHEMA_CACHE_RESPONSE) == 0)
        return RzbNext_ValidateCacheResponse(object);
    if (strcmp(schemaName, RZB_NEXT_SCHEMA_CATALOG_INVALIDATION) == 0)
        return RzbNext_ValidateCatalogInvalidation(object);
    if (strcmp(schemaName, RZB_NEXT_SCHEMA_FILE_REMOVE_REQUEST) == 0)
        return RzbNext_ValidateFileRemoveRequest(object);
    if (strcmp(schemaName, RZB_NEXT_SCHEMA_FILE_REMOVE_RESULT) == 0)
        return RzbNext_ValidateFileRemoveResult(object);
    if (strcmp(schemaName, RZB_NEXT_SCHEMA_SEARCH_EXPORT) == 0)
        return RzbNext_ValidateSearchExport(object);
    return false;
}

static json_object *
RzbNext_ParseJsonObject(const char *jsonMessage)
{
    json_tokener *tokener;
    json_object *object;
    enum json_tokener_error error;
    int parseEnd;
    size_t length;

    if (jsonMessage == NULL)
        return NULL;
    length = strlen(jsonMessage);
    tokener = json_tokener_new();
    if (tokener == NULL)
        return NULL;
    object = json_tokener_parse_ex(tokener, jsonMessage, (int)length);
    error = json_tokener_get_error(tokener);
    parseEnd = json_tokener_get_parse_end(tokener);
    json_tokener_free(tokener);
    if (object == NULL || error != json_tokener_success) {
        if (object != NULL)
            json_object_put(object);
        return NULL;
    }
    while (parseEnd < (int)length &&
           isspace((unsigned char)jsonMessage[parseEnd])) {
        parseEnd++;
    }
    if (parseEnd != (int)length ||
        json_object_get_type(object) != json_type_object) {
        json_object_put(object);
        return NULL;
    }
    return object;
}

static const char *
RzbNext_GetString(json_object *object, const char *field)
{
    json_object *value;

    if (!json_object_object_get_ex(object, field, &value))
        return NULL;
    if (json_object_get_type(value) != json_type_string)
        return NULL;
    return json_object_get_string(value);
}

static bool
RzbNext_GetVersion(json_object *object, const char *field, uint32_t *version)
{
    json_object *value;

    if (!json_object_object_get_ex(object, field, &value))
        return false;
    if (json_object_get_type(value) != json_type_int)
        return false;
    if (json_object_get_int64(value) < 0 ||
        json_object_get_int64(value) > UINT32_MAX) {
        return false;
    }
    *version = (uint32_t)json_object_get_int64(value);
    return true;
}

static bool
RzbNext_GetIdentity(json_object *object, const char **schemaName,
                    uint32_t *schemaVersion)
{
    const char *referenceSchemaName;

    referenceSchemaName = RzbNext_GetString(object, "reference_schema_name");
    if (referenceSchemaName != NULL) {
        *schemaName = referenceSchemaName;
        return RzbNext_GetVersion(object, "reference_schema_version",
                                  schemaVersion);
    }
    *schemaName = RzbNext_GetString(object, "schema_name");
    if (*schemaName == NULL)
        return false;
    return RzbNext_GetVersion(object, "schema_version", schemaVersion);
}

static bool
RzbNext_IsKnownSchemaVersion(const char *schemaName, uint32_t schemaVersion)
{
    size_t index;

    if (schemaName == NULL)
        return false;
    for (index = 0; index < sizeof(KnownSchemas) / sizeof(KnownSchemas[0]);
         index++) {
        if (strcmp(schemaName, KnownSchemas[index].name) == 0)
            return schemaVersion == KnownSchemas[index].version;
    }
    return false;
}

static struct RzbNextRoute *
RzbNextRoute_Create(enum RzbNextTransport transport, const char *exchange,
                    char *routingKey)
{
    struct RzbNextRoute *route;

    if (routingKey == NULL)
        return NULL;
    route = calloc(1, sizeof(*route));
    if (route == NULL) {
        free(routingKey);
        return NULL;
    }
    route->transport = transport;
    route->exchange = RzbNext_Strdup(exchange == NULL ? "" : exchange);
    route->routingKey = routingKey;
    if (route->exchange == NULL) {
        RzbNextRoute_Destroy(route);
        return NULL;
    }
    return route;
}

SO_PUBLIC const char *
RzbNextTransport_ToString(enum RzbNextTransport transport)
{
    switch (transport) {
    case RZB_NEXT_TRANSPORT_RABBITMQ:
        return "rabbitmq";
    case RZB_NEXT_TRANSPORT_KAFKA:
        return "kafka";
    default:
        return "unknown";
    }
}

SO_PUBLIC bool
RzbNextMessage_IsKnownSchema(const char *schemaName)
{
    return RzbNext_IsKnownSchemaVersion(schemaName, RZB_NEXT_SCHEMA_VERSION);
}

SO_PUBLIC bool
RzbNextMessage_ValidateIdentity(const char *jsonMessage)
{
    json_object *object;
    const char *schemaName;
    uint32_t schemaVersion;
    bool valid;

    object = RzbNext_ParseJsonObject(jsonMessage);
    if (object == NULL)
        return false;
    valid = RzbNext_GetIdentity(object, &schemaName, &schemaVersion) &&
            RzbNext_IsKnownSchemaVersion(schemaName, schemaVersion);
    json_object_put(object);
    return valid;
}

SO_PUBLIC bool
RzbNextMessage_Validate(const char *jsonMessage)
{
    json_object *object;
    const char *schemaName;
    uint32_t schemaVersion;
    bool valid;

    object = RzbNext_ParseJsonObject(jsonMessage);
    if (object == NULL)
        return false;
    valid = RzbNext_GetIdentity(object, &schemaName, &schemaVersion) &&
            RzbNext_IsKnownSchemaVersion(schemaName, schemaVersion) &&
            RzbNext_ValidateObjectForSchema(object, schemaName);
    json_object_put(object);
    return valid;
}

SO_PUBLIC bool
RzbNextMessage_Route(const char *jsonMessage, const char *cacheRequestorUuid,
                     struct RzbNextRoute **route)
{
    json_object *object;
    const char *schemaName;
    uint32_t schemaVersion;
    char *routingKey = NULL;
    enum RzbNextTransport transport = RZB_NEXT_TRANSPORT_RABBITMQ;
    const char *exchange = "";
    const char *field;

    if (route == NULL)
        return false;
    *route = NULL;
    object = RzbNext_ParseJsonObject(jsonMessage);
    if (object == NULL)
        return false;
    if (!RzbNext_GetIdentity(object, &schemaName, &schemaVersion) ||
        !RzbNext_IsKnownSchemaVersion(schemaName, schemaVersion) ||
        !RzbNext_ValidateObjectForSchema(object, schemaName)) {
        json_object_put(object);
        return false;
    }

    if (strcmp(schemaName, RZB_NEXT_SCHEMA_CNC_REGISTRATION_REQUEST) == 0 ||
        strcmp(schemaName, RZB_NEXT_SCHEMA_CNC_LIVENESS) == 0 ||
        strcmp(schemaName, RZB_NEXT_SCHEMA_CNC_BYE) == 0) {
        routingKey = RzbNext_Strdup(RZB_NEXT_QUEUE_COMMAND);
    } else if (strcmp(schemaName,
                      RZB_NEXT_SCHEMA_CNC_REGISTRATION_ACCEPTED) == 0 ||
               strcmp(schemaName,
                      RZB_NEXT_SCHEMA_CNC_REGISTRATION_REJECTED) == 0) {
        field = RzbNext_GetString(object, "nugget_uuid");
        if (RzbNext_IsUuid(field))
            routingKey = RzbNext_Format2(RZB_NEXT_QUEUE_DIRECTED_COMMAND_PREFIX,
                                         field);
    } else if (strcmp(schemaName, RZB_NEXT_SCHEMA_CNC_DIRECTED_COMMAND) == 0) {
        field = RzbNext_GetString(object, "target_nugget_uuid");
        if (RzbNext_IsUuid(field))
            routingKey = RzbNext_Format2(RZB_NEXT_QUEUE_DIRECTED_COMMAND_PREFIX,
                                         field);
    } else if (strcmp(schemaName, RZB_NEXT_SCHEMA_CNC_DISPATCHER_HELLO) == 0) {
        exchange = RZB_NEXT_EXCHANGE_DISPATCHER_HELLO;
        routingKey = RzbNext_Strdup("");
    } else if (strcmp(schemaName, RZB_NEXT_SCHEMA_BLOCK_SUBMISSION) == 0) {
        routingKey = RzbNext_Strdup(RZB_NEXT_QUEUE_INPUT);
    } else if (strcmp(schemaName, RZB_NEXT_SCHEMA_BLOCK_UPDATE) == 0) {
        routingKey = RzbNext_Strdup(RZB_NEXT_QUEUE_BLOCK_UPDATE);
    } else if (strcmp(schemaName, RZB_NEXT_SCHEMA_INSPECTION_WORK) == 0) {
        field = RzbNext_GetString(object, "app_type");
        if (RzbNext_IsSafeKey(field))
            routingKey = RzbNext_Format2(RZB_NEXT_QUEUE_INSPECTOR_PREFIX, field);
    } else if (strcmp(schemaName, RZB_NEXT_SCHEMA_ANALYSIS_RESULT) == 0) {
        routingKey = RzbNext_Strdup(RZB_NEXT_QUEUE_ANALYSIS_RESULT);
    } else if (strcmp(schemaName, RZB_NEXT_SCHEMA_CACHE_REQUEST) == 0) {
        routingKey = RzbNext_Strdup(RZB_NEXT_QUEUE_CACHE_REQUEST);
    } else if (strcmp(schemaName, RZB_NEXT_SCHEMA_CACHE_RESPONSE) == 0) {
        if (RzbNext_IsUuid(cacheRequestorUuid))
            routingKey = RzbNext_Format2(RZB_NEXT_QUEUE_CACHE_RESPONSE_PREFIX,
                                         cacheRequestorUuid);
    } else if (strcmp(schemaName, RZB_NEXT_SCHEMA_CATALOG_INVALIDATION) == 0) {
        const char *resourceType = RzbNext_GetString(object, "resource_type");
        const char *mutationAction = RzbNext_GetString(object, "mutation_action");

        if (RzbNext_IsSafeKey(resourceType) &&
            RzbNext_IsSafeKey(mutationAction)) {
            exchange = RZB_NEXT_EXCHANGE_CATALOG_INVALIDATION;
            routingKey = RzbNext_Format3("catalog", resourceType,
                                         mutationAction);
        }
    } else if (strcmp(schemaName, RZB_NEXT_SCHEMA_FILE_REMOVE_REQUEST) == 0) {
        exchange = RZB_NEXT_EXCHANGE_FILE_REMOVE;
        routingKey = RzbNext_Strdup("");
    } else if (strcmp(schemaName, RZB_NEXT_SCHEMA_FILE_REMOVE_RESULT) == 0) {
        routingKey = RzbNext_Strdup(RZB_NEXT_QUEUE_FILE_REMOVE_RESULT);
    } else if (strcmp(schemaName, RZB_NEXT_SCHEMA_SEARCH_EXPORT) == 0) {
        field = RzbNext_GetString(object, "aggregate_id");
        if (field != NULL && field[0] != '\0') {
            transport = RZB_NEXT_TRANSPORT_KAFKA;
            routingKey = RzbNext_Strdup(field);
        }
    }

    json_object_put(object);
    if (routingKey == NULL)
        return false;
    *route = RzbNextRoute_Create(transport, exchange, routingKey);
    return *route != NULL;
}

SO_PUBLIC void
RzbNextRoute_Destroy(struct RzbNextRoute *route)
{
    if (route == NULL)
        return;
    free(route->exchange);
    free(route->routingKey);
    free(route);
}
