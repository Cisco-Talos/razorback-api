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

#include <check.h>

#include <razorback/messages_next.h>

#include <json-c/json.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct RouteFixture
{
    const char *directory;
    const char *name;
    const char *cacheRequestorUuid;
    enum RzbNextTransport transport;
    const char *exchange;
    const char *routingKey;
};

struct MessageFixture
{
    const char *directory;
    const char *name;
};

static char *
read_fixture(const char *directory, const char *name)
{
    char path[4096];
    FILE *file;
    long size;
    char *contents;

    snprintf(path, sizeof(path), "%s/%s/%s", RAZORBACK_SCHEMA_FIXTURE_ROOT,
             directory, name);
    file = fopen(path, "rb");
    ck_assert_msg(file != NULL, "failed to open fixture %s", path);
    ck_assert_int_eq(fseek(file, 0, SEEK_END), 0);
    size = ftell(file);
    ck_assert_int_ge(size, 0);
    ck_assert_int_eq(fseek(file, 0, SEEK_SET), 0);

    contents = calloc((size_t)size + 1, sizeof(char));
    ck_assert_ptr_ne(contents, NULL);
    ck_assert_uint_eq(fread(contents, 1, (size_t)size, file), (size_t)size);
    fclose(file);
    return contents;
}

static char *
copy_json_string(json_object *object)
{
    const char *jsonText;
    char *copy;
    size_t length;

    jsonText = json_object_to_json_string_ext(object, JSON_C_TO_STRING_PLAIN);
    ck_assert_ptr_ne(jsonText, NULL);
    length = strlen(jsonText) + 1;
    copy = malloc(length);
    ck_assert_ptr_ne(copy, NULL);
    memcpy(copy, jsonText, length);
    return copy;
}

static char *
mutate_fixture_version(const char *fixture)
{
    json_object *object;
    json_object *value;
    char *mutated;
    const char *field = "schema_version";

    object = json_tokener_parse(fixture);
    ck_assert_ptr_ne(object, NULL);
    if (json_object_object_get_ex(object, "reference_schema_version", &value))
        field = "reference_schema_version";
    json_object_object_add(object, field, json_object_new_int(2));
    mutated = copy_json_string(object);
    json_object_put(object);
    return mutated;
}

static char *
mutate_fixture_schema(const char *fixture)
{
    json_object *object;
    json_object *value;
    const char *field = "schema_name";
    const char *wrongSchema = RZB_NEXT_SCHEMA_CACHE_REQUEST;
    const char *currentSchema;
    char *mutated;

    object = json_tokener_parse(fixture);
    ck_assert_ptr_ne(object, NULL);
    if (json_object_object_get_ex(object, "reference_schema_name", &value)) {
        field = "reference_schema_name";
        currentSchema = json_object_get_string(value);
    } else {
        ck_assert(json_object_object_get_ex(object, "schema_name", &value));
        currentSchema = json_object_get_string(value);
    }
    if (currentSchema != NULL &&
        strcmp(currentSchema, RZB_NEXT_SCHEMA_CACHE_REQUEST) == 0) {
        wrongSchema = RZB_NEXT_SCHEMA_CACHE_RESPONSE;
    }
    json_object_object_add(object, field, json_object_new_string(wrongSchema));
    mutated = copy_json_string(object);
    json_object_put(object);
    return mutated;
}

static const char *
header_value(const struct RzbNextMessageHeader *headers, size_t headerCount,
             const char *name)
{
    size_t index;

    for (index = 0; index < headerCount; index++) {
        if (strcmp(headers[index].name, name) == 0)
            return headers[index].value;
    }
    return NULL;
}

static const char *
json_string_field(json_object *object, const char *field)
{
    json_object *value;

    ck_assert(json_object_object_get_ex(object, field, &value));
    ck_assert_int_eq(json_object_get_type(value), json_type_string);
    return json_object_get_string(value);
}

START_TEST(test_messages_next_accepts_known_schema_identities)
{
    static const struct MessageFixture fixtures[] = {
        { "messages", "claim_check_reference.valid.json" },
        { "messages", "cnc_registration_request.valid.json" },
        { "messages", "cnc_registration_accepted.valid.json" },
        { "messages", "cnc_registration_rejected.valid.json" },
        { "messages", "cnc_liveness.valid.json" },
        { "messages", "cnc_bye.valid.json" },
        { "messages", "cnc_directed_command.valid.json" },
        { "messages", "cnc_dispatcher_hello.valid.json" },
        { "messages", "block_submission.valid.json" },
        { "messages", "block_update.valid.json" },
        { "messages", "inspection_work.valid.json" },
        { "messages", "analysis_result_envelope.valid.json" },
        { "messages", "cache_request.valid.json" },
        { "messages", "cache_response.valid.json" },
        { "messages", "catalog_invalidation_event.valid.json" },
        { "messages", "file_remove_request.valid.json" },
        { "messages", "file_remove_result.valid.json" },
        { "search_export", "search_export_record.valid.json" }
    };
    size_t index;

    for (index = 0; index < sizeof(fixtures) / sizeof(fixtures[0]); index++) {
        char *fixture = read_fixture(fixtures[index].directory,
                                     fixtures[index].name);

        ck_assert_msg(RzbNextMessage_ValidateIdentity(fixture),
                      "fixture identity should be valid: %s",
                      fixtures[index].name);
        ck_assert_msg(RzbNextMessage_Validate(fixture),
                      "fixture structure should be valid: %s",
                      fixtures[index].name);
        free(fixture);
    }

    ck_assert(RzbNextMessage_IsKnownSchema(RZB_NEXT_SCHEMA_ANALYSIS_RESULT));
    ck_assert(!RzbNextMessage_IsKnownSchema("razorback.messages.analysis_result"));
}
END_TEST

START_TEST(test_analysis_result_helpers_build_from_inspection_work)
{
    char *work;
    char *completed;
    char *error;
    char *deferred;
    struct RzbNextRoute *route = NULL;
    json_object *completedObject;
    json_object *errorObject;
    json_object *deferredObject;
    const char *inspectorUuid = "22222222-2222-4222-8222-222222222222";
    const char *createdAt = "2026-06-17T21:06:00.000Z";

    work = read_fixture("messages", "inspection_work.valid.json");
    completed = RzbNextAnalysisResult_BuildCompleted(
        work, inspectorUuid, createdAt, NULL, NULL,
        "{\"set_system_tags\":[\"SUSPICIOUS\"]}", NULL);
    ck_assert_ptr_ne(completed, NULL);
    ck_assert(RzbNextMessage_Validate(completed));
    ck_assert(RzbNextMessage_Route(completed, NULL, &route));
    ck_assert_str_eq(route->routingKey, RZB_NEXT_QUEUE_ANALYSIS_RESULT);
    RzbNextRoute_Destroy(route);
    route = NULL;

    completedObject = json_tokener_parse(completed);
    ck_assert_ptr_ne(completedObject, NULL);
    ck_assert_str_eq(json_string_field(completedObject, "schema_name"),
                     RZB_NEXT_SCHEMA_ANALYSIS_RESULT);
    ck_assert_str_eq(json_string_field(completedObject, "inspection_id"),
                     "aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa");
    ck_assert_str_eq(json_string_field(completedObject, "event_id"),
                     "88888888-8888-4888-8888-888888888888");
    ck_assert_str_eq(json_string_field(completedObject, "inspector_id"),
                     inspectorUuid);
    ck_assert_str_eq(json_string_field(completedObject, "app_type"),
                     "pdf_inspector");
    ck_assert_str_eq(json_string_field(completedObject, "result_status"),
                     "completed");
    json_object_put(completedObject);

    error = RzbNextAnalysisResult_BuildError(
        work, inspectorUuid, "inspector_failed", "inspector_failed",
        "inspector failed", "{\"retryable\":false}", createdAt);
    ck_assert_ptr_ne(error, NULL);
    ck_assert(RzbNextMessage_Validate(error));
    errorObject = json_tokener_parse(error);
    ck_assert_ptr_ne(errorObject, NULL);
    ck_assert_str_eq(json_string_field(errorObject, "result_status"),
                     "error");
    json_object_put(errorObject);

    deferred = RzbNextAnalysisResult_BuildDeferred(
        work, inspectorUuid, "external_scan_pending",
        "external scan pending", "2026-06-17T21:07:00.000Z", NULL,
        createdAt);
    ck_assert_ptr_ne(deferred, NULL);
    ck_assert(RzbNextMessage_Validate(deferred));
    deferredObject = json_tokener_parse(deferred);
    ck_assert_ptr_ne(deferredObject, NULL);
    ck_assert_str_eq(json_string_field(deferredObject, "result_status"),
                     "deferred");
    json_object_put(deferredObject);

    ck_assert_ptr_eq(RzbNextAnalysisResult_BuildCompleted(
                         work, "not-a-uuid", createdAt, NULL, NULL, NULL,
                         NULL),
                     NULL);

    RzbNext_FreeString(deferred);
    RzbNext_FreeString(error);
    RzbNext_FreeString(completed);
    free(work);
}
END_TEST

START_TEST(test_messages_next_rejects_identity_mutations_for_all_fixtures)
{
    static const struct MessageFixture fixtures[] = {
        { "messages", "claim_check_reference.valid.json" },
        { "messages", "cnc_registration_request.valid.json" },
        { "messages", "cnc_registration_accepted.valid.json" },
        { "messages", "cnc_registration_rejected.valid.json" },
        { "messages", "cnc_liveness.valid.json" },
        { "messages", "cnc_bye.valid.json" },
        { "messages", "cnc_directed_command.valid.json" },
        { "messages", "cnc_dispatcher_hello.valid.json" },
        { "messages", "block_submission.valid.json" },
        { "messages", "block_update.valid.json" },
        { "messages", "inspection_work.valid.json" },
        { "messages", "analysis_result_envelope.valid.json" },
        { "messages", "cache_request.valid.json" },
        { "messages", "cache_response.valid.json" },
        { "messages", "catalog_invalidation_event.valid.json" },
        { "messages", "file_remove_request.valid.json" },
        { "messages", "file_remove_result.valid.json" },
        { "search_export", "search_export_record.valid.json" }
    };
    size_t index;

    for (index = 0; index < sizeof(fixtures) / sizeof(fixtures[0]); index++) {
        char *fixture = read_fixture(fixtures[index].directory,
                                     fixtures[index].name);
        char *badVersion = mutate_fixture_version(fixture);
        char *badSchema = mutate_fixture_schema(fixture);

        ck_assert_msg(!RzbNextMessage_ValidateIdentity(badVersion),
                      "unsupported schema version should fail identity: %s",
                      fixtures[index].name);
        ck_assert_msg(!RzbNextMessage_Validate(badVersion),
                      "unsupported schema version should fail validation: %s",
                      fixtures[index].name);

        ck_assert_msg(RzbNextMessage_ValidateIdentity(badSchema),
                      "known-but-wrong schema identity should still parse: %s",
                      fixtures[index].name);
        ck_assert_msg(!RzbNextMessage_Validate(badSchema),
                      "known-but-wrong schema identity should fail validation: %s",
                      fixtures[index].name);

        free(badSchema);
        free(badVersion);
        free(fixture);
    }
}
END_TEST

START_TEST(test_messages_next_rejects_unknown_or_bad_identity)
{
    static const char bad_version[] =
        "{\"schema_name\":\"razorback.cache.request\",\"schema_version\":2}";
    static const char bad_schema[] =
        "{\"schema_name\":\"razorback.unknown\",\"schema_version\":1}";
    static const char bad_reference[] =
        "{\"reference_schema_name\":\"razorback.messages.wrong_reference\","
        "\"reference_schema_version\":1}";

    ck_assert(!RzbNextMessage_ValidateIdentity(NULL));
    ck_assert(!RzbNextMessage_ValidateIdentity("[]"));
    ck_assert(!RzbNextMessage_ValidateIdentity("{bad json"));
    ck_assert(!RzbNextMessage_ValidateIdentity(bad_version));
    ck_assert(!RzbNextMessage_ValidateIdentity(bad_schema));
    ck_assert(!RzbNextMessage_ValidateIdentity(bad_reference));

    ck_assert(!RzbNextMessage_Validate(NULL));
    ck_assert(!RzbNextMessage_Validate("[]"));
    ck_assert(!RzbNextMessage_Validate("{bad json"));
    ck_assert(!RzbNextMessage_Validate(bad_version));
    ck_assert(!RzbNextMessage_Validate(bad_schema));
    ck_assert(!RzbNextMessage_Validate(bad_reference));
}
END_TEST

START_TEST(test_messages_next_rejects_invalid_schema_fixtures)
{
    static const struct MessageFixture fixtures[] = {
        { "messages", "claim_check_reference.invalid.json" },
        { "messages", "cnc_registration_request.invalid.json" },
        { "messages", "cnc_registration_accepted.invalid.json" },
        { "messages", "cnc_registration_rejected.invalid.json" },
        { "messages", "cnc_liveness.invalid.json" },
        { "messages", "cnc_bye.invalid.json" },
        { "messages", "cnc_directed_command.invalid.json" },
        { "messages", "cnc_dispatcher_hello.invalid.json" },
        { "messages", "block_submission.invalid.json" },
        { "messages", "block_update.invalid.json" },
        { "messages", "inspection_work.invalid.json" },
        { "messages", "analysis_result_envelope.invalid.json" },
        { "messages", "cache_request.invalid.json" },
        { "messages", "cache_response.invalid.json" },
        { "messages", "catalog_invalidation_event.invalid.json" },
        { "messages", "file_remove_request.invalid.json" },
        { "messages", "file_remove_result.invalid.json" },
        { "search_export", "search_export_record.invalid.json" }
    };
    size_t index;

    for (index = 0; index < sizeof(fixtures) / sizeof(fixtures[0]); index++) {
        char *fixture = read_fixture(fixtures[index].directory,
                                     fixtures[index].name);

        ck_assert_msg(!RzbNextMessage_Validate(fixture),
                      "fixture structure should be invalid: %s",
                      fixtures[index].name);
        free(fixture);
    }
}
END_TEST

START_TEST(test_messages_next_routes_match_dispatcher_next_topology)
{
    static const struct RouteFixture fixtures[] = {
        { "messages", "cnc_registration_request.valid.json", NULL,
          RZB_NEXT_TRANSPORT_RABBITMQ, "", RZB_NEXT_QUEUE_COMMAND },
        { "messages", "cnc_registration_accepted.valid.json", NULL,
          RZB_NEXT_TRANSPORT_RABBITMQ, "",
          "COMMAND.22222222-2222-4222-8222-222222222222" },
        { "messages", "cnc_registration_rejected.valid.json", NULL,
          RZB_NEXT_TRANSPORT_RABBITMQ, "",
          "COMMAND.22222222-2222-4222-8222-222222222222" },
        { "messages", "cnc_liveness.valid.json", NULL,
          RZB_NEXT_TRANSPORT_RABBITMQ, "", RZB_NEXT_QUEUE_COMMAND },
        { "messages", "cnc_bye.valid.json", NULL,
          RZB_NEXT_TRANSPORT_RABBITMQ, "", RZB_NEXT_QUEUE_COMMAND },
        { "messages", "cnc_directed_command.valid.json", NULL,
          RZB_NEXT_TRANSPORT_RABBITMQ, "",
          "COMMAND.22222222-2222-4222-8222-222222222222" },
        { "messages", "cnc_dispatcher_hello.valid.json", NULL,
          RZB_NEXT_TRANSPORT_RABBITMQ, RZB_NEXT_EXCHANGE_DISPATCHER_HELLO, "" },
        { "messages", "block_submission.valid.json", NULL,
          RZB_NEXT_TRANSPORT_RABBITMQ, "", RZB_NEXT_QUEUE_INPUT },
        { "messages", "block_update.valid.json", NULL,
          RZB_NEXT_TRANSPORT_RABBITMQ, "", RZB_NEXT_QUEUE_BLOCK_UPDATE },
        { "messages", "inspection_work.valid.json", NULL,
          RZB_NEXT_TRANSPORT_RABBITMQ, "", "INSPECTOR.pdf_inspector" },
        { "messages", "analysis_result_envelope.valid.json", NULL,
          RZB_NEXT_TRANSPORT_RABBITMQ, "", RZB_NEXT_QUEUE_ANALYSIS_RESULT },
        { "messages", "cache_request.valid.json", NULL,
          RZB_NEXT_TRANSPORT_RABBITMQ, "", RZB_NEXT_QUEUE_CACHE_REQUEST },
        { "messages", "cache_response.valid.json",
          "22222222-2222-4222-8222-222222222222",
          RZB_NEXT_TRANSPORT_RABBITMQ, "",
          "CACHE_RESPONSE.22222222-2222-4222-8222-222222222222" },
        { "messages", "catalog_invalidation_event.valid.json", NULL,
          RZB_NEXT_TRANSPORT_RABBITMQ,
          RZB_NEXT_EXCHANGE_CATALOG_INVALIDATION, "catalog.app_type.updated" },
        { "messages", "file_remove_request.valid.json", NULL,
          RZB_NEXT_TRANSPORT_RABBITMQ, RZB_NEXT_EXCHANGE_FILE_REMOVE,
          RZB_NEXT_QUEUE_FILE_REMOVE_FILE_STORE },
        { "messages", "file_remove_result.valid.json", NULL,
          RZB_NEXT_TRANSPORT_RABBITMQ, "", RZB_NEXT_QUEUE_FILE_REMOVE_RESULT },
        { "search_export", "search_export_record.valid.json", NULL,
          RZB_NEXT_TRANSPORT_KAFKA, "",
          "88888888-8888-4888-8888-888888888888" }
    };
    size_t index;

    for (index = 0; index < sizeof(fixtures) / sizeof(fixtures[0]); index++) {
        char *fixture = read_fixture(fixtures[index].directory,
                                     fixtures[index].name);
        struct RzbNextRoute *route = NULL;

        ck_assert_msg(RzbNextMessage_Route(fixture,
                                           fixtures[index].cacheRequestorUuid,
                                           &route),
                      "fixture should route: %s", fixtures[index].name);
        ck_assert_ptr_ne(route, NULL);
        ck_assert_int_eq(route->transport, fixtures[index].transport);
        ck_assert_str_eq(RzbNextTransport_ToString(route->transport),
                         fixtures[index].transport == RZB_NEXT_TRANSPORT_KAFKA ?
                         "kafka" : "rabbitmq");
        ck_assert_str_eq(route->exchange, fixtures[index].exchange);
        ck_assert_str_eq(route->routingKey, fixtures[index].routingKey);

        RzbNextRoute_Destroy(route);
        free(fixture);
    }
}
END_TEST

START_TEST(test_rabbitmq_prepare_and_decode_inline_message)
{
    char *fixture = read_fixture("messages",
                                 "cnc_registration_request.valid.json");
    struct MessageBodyPolicy policy = MessageBodyPolicy_Default();
    struct RzbNextPreparedRabbitMqMessage *prepared = NULL;
    struct RzbNextDecodedRabbitMqMessage *decoded = NULL;

    policy.maxInlineBytes = 4096;
    ck_assert(RzbNextRabbitMq_PrepareMessage(fixture, NULL, &policy, NULL,
                                             &prepared));
    ck_assert_ptr_ne(prepared, NULL);
    ck_assert_str_eq(prepared->route->exchange, "");
    ck_assert_str_eq(prepared->route->routingKey, RZB_NEXT_QUEUE_COMMAND);
    ck_assert_str_eq(prepared->contentType, "application/json");
    ck_assert_ptr_eq(prepared->contentEncoding, NULL);
    ck_assert_str_eq(header_value(prepared->headers, prepared->headerCount,
                                  RZB_NEXT_HEADER_SCHEMA_NAME),
                     RZB_NEXT_SCHEMA_CNC_REGISTRATION_REQUEST);
    ck_assert_str_eq(header_value(prepared->headers, prepared->headerCount,
                                  RZB_NEXT_HEADER_SCHEMA_VERSION), "1");
    ck_assert_str_eq(header_value(prepared->headers, prepared->headerCount,
                                  RZB_NEXT_HEADER_BODY_MODE), "inline");
    ck_assert_ptr_eq(header_value(prepared->headers, prepared->headerCount,
                                  RZB_NEXT_HEADER_CONTENT_ENCODING), NULL);

    ck_assert(RzbNextRabbitMq_DecodeMessage(prepared->body, prepared->bodySize,
                                            prepared->headers,
                                            prepared->headerCount, NULL, 0,
                                            &decoded));
    ck_assert_ptr_ne(decoded, NULL);
    ck_assert_ptr_eq(decoded->claimCheckReference, NULL);
    ck_assert_str_eq(decoded->jsonMessage, fixture);
    ck_assert(RzbNextMessage_Validate(decoded->jsonMessage));

    RzbNextDecodedRabbitMqMessage_Destroy(decoded);
    RzbNextPreparedRabbitMqMessage_Destroy(prepared);
    free(fixture);
}
END_TEST

START_TEST(test_rabbitmq_prepare_and_decode_zlib_message)
{
    char *fixture = read_fixture("messages",
                                 "analysis_result_envelope.valid.json");
    struct MessageBodyPolicy policy = MessageBodyPolicy_Default();
    struct RzbNextPreparedRabbitMqMessage *prepared = NULL;
    struct RzbNextDecodedRabbitMqMessage *decoded = NULL;

    policy.maxInlineBytes = 900;
    ck_assert(RzbNextRabbitMq_PrepareMessage(fixture, NULL, &policy, NULL,
                                             &prepared));
    ck_assert_ptr_ne(prepared, NULL);
    ck_assert_str_eq(prepared->route->routingKey,
                     RZB_NEXT_QUEUE_ANALYSIS_RESULT);
    ck_assert_str_eq(header_value(prepared->headers, prepared->headerCount,
                                  RZB_NEXT_HEADER_BODY_MODE), "zlib");
    ck_assert_str_eq(header_value(prepared->headers, prepared->headerCount,
                                  RZB_NEXT_HEADER_CONTENT_ENCODING),
                     MESSAGE_BODY_CONTENT_ENCODING_ZLIB);
    ck_assert_str_eq(prepared->contentEncoding,
                     MESSAGE_BODY_CONTENT_ENCODING_ZLIB);

    ck_assert(RzbNextRabbitMq_DecodeMessage(prepared->body, prepared->bodySize,
                                            prepared->headers,
                                            prepared->headerCount, NULL, 0,
                                            &decoded));
    ck_assert_str_eq(decoded->jsonMessage, fixture);
    ck_assert_ptr_eq(decoded->claimCheckReference, NULL);

    RzbNextDecodedRabbitMqMessage_Destroy(decoded);
    RzbNextPreparedRabbitMqMessage_Destroy(prepared);
    free(fixture);
}
END_TEST

START_TEST(test_rabbitmq_prepare_and_decode_claim_check_message)
{
    char *fixture = read_fixture("messages",
                                 "analysis_result_envelope.valid.json");
    struct MessageBodyPolicy policy = MessageBodyPolicy_Default();
    struct ClaimCheckReference *template = NULL;
    struct RzbNextPreparedRabbitMqMessage *prepared = NULL;
    struct RzbNextDecodedRabbitMqMessage *decoded = NULL;

    policy.maxInlineBytes = 32;
    template = ClaimCheckReference_Create(
        "https://objects.example.invalid/messages/payload.zlib?signature=redacted",
        "2026-06-18T00:00:00.000Z",
        "razorback-claim-check",
        "messages/payload.zlib",
        "application/json",
        RZB_NEXT_SCHEMA_ANALYSIS_RESULT,
        RZB_NEXT_SCHEMA_VERSION
    );
    ck_assert_ptr_ne(template, NULL);

    ck_assert(RzbNextRabbitMq_PrepareMessage(fixture, NULL, &policy, template,
                                             &prepared));
    ck_assert_ptr_ne(prepared, NULL);
    ck_assert_str_eq(header_value(prepared->headers, prepared->headerCount,
                                  RZB_NEXT_HEADER_BODY_MODE), "claim_check");
    ck_assert_ptr_eq(header_value(prepared->headers, prepared->headerCount,
                                  RZB_NEXT_HEADER_CONTENT_ENCODING), NULL);
    ck_assert_ptr_ne(prepared->claimCheckBody, NULL);
    ck_assert_ptr_ne(prepared->claimCheckReference, NULL);
    ck_assert_str_eq(prepared->claimCheckReference->schemaName,
                     RZB_NEXT_SCHEMA_ANALYSIS_RESULT);

    ck_assert(RzbNextRabbitMq_DecodeMessage(
        prepared->body, prepared->bodySize, prepared->headers,
        prepared->headerCount, prepared->claimCheckBody,
        prepared->claimCheckBodySize, &decoded));
    ck_assert_str_eq(decoded->jsonMessage, fixture);
    ck_assert_ptr_ne(decoded->claimCheckReference, NULL);
    ck_assert_str_eq(decoded->claimCheckReference->schemaName,
                     RZB_NEXT_SCHEMA_ANALYSIS_RESULT);

    RzbNextDecodedRabbitMqMessage_Destroy(decoded);
    RzbNextPreparedRabbitMqMessage_Destroy(prepared);
    ClaimCheckReference_Destroy(template);
    free(fixture);
}
END_TEST

START_TEST(test_rabbitmq_prepare_rejects_kafka_routed_message)
{
    char *fixture = read_fixture("search_export",
                                 "search_export_record.valid.json");
    struct MessageBodyPolicy policy = MessageBodyPolicy_Default();
    struct RzbNextPreparedRabbitMqMessage *prepared = NULL;

    policy.maxInlineBytes = 4096;
    ck_assert(!RzbNextRabbitMq_PrepareMessage(fixture, NULL, &policy, NULL,
                                              &prepared));
    ck_assert_ptr_eq(prepared, NULL);

    free(fixture);
}
END_TEST

START_TEST(test_cnc_helpers_gate_registration_and_extract_timing)
{
    char *hello;
    char *accepted;
    char *directedQueue;
    uint64_t interval = 0;
    uint64_t freshness = 0;
    uint64_t skew = 0;
    static const char notReadyHello[] =
        "{"
        "\"schema_name\":\"razorback.cnc.dispatcher_hello\","
        "\"schema_version\":1,"
        "\"dispatcher_id\":\"aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa\","
        "\"created_at\":\"2026-06-17T21:00:00.000Z\","
        "\"started_at\":\"2026-06-17T20:59:00.000Z\","
        "\"ready\":false,"
        "\"availability\":\"starting\","
        "\"dependency_reason_codes\":[\"starting\"]"
        "}";

    hello = read_fixture("messages", "cnc_dispatcher_hello.valid.json");
    accepted = read_fixture("messages", "cnc_registration_accepted.valid.json");

    ck_assert(RzbNextCnc_IsReadyDispatcherHello(hello));
    ck_assert(!RzbNextCnc_IsReadyDispatcherHello(notReadyHello));
    ck_assert(RzbNextCnc_RegistrationAcceptedTiming(accepted, &interval,
                                                    &freshness, &skew));
    ck_assert_uint_eq(interval, 10);
    ck_assert_uint_eq(freshness, 30);
    ck_assert_uint_eq(skew, 5);

    directedQueue = RzbNextCnc_DirectedCommandQueue(
        "22222222-2222-4222-8222-222222222222");
    ck_assert_ptr_ne(directedQueue, NULL);
    ck_assert_str_eq(directedQueue,
                     "COMMAND.22222222-2222-4222-8222-222222222222");
    RzbNext_FreeString(directedQueue);
    ck_assert_ptr_eq(RzbNextCnc_DirectedCommandQueue("not-a-uuid"), NULL);

    free(accepted);
    free(hello);
}
END_TEST

START_TEST(test_messages_next_cache_response_route_requires_requestor)
{
    char *fixture;
    struct RzbNextRoute *route = NULL;

    fixture = read_fixture("messages", "cache_response.valid.json");
    ck_assert(!RzbNextMessage_Route(fixture, NULL, &route));
    ck_assert_ptr_eq(route, NULL);
    ck_assert(!RzbNextMessage_Route(fixture, "not-a-uuid", &route));
    ck_assert_ptr_eq(route, NULL);
    free(fixture);
}
END_TEST

START_TEST(test_messages_next_cache_submit_decision_maps_response_tags)
{
    char *fixture;
    json_object *object;
    json_object *tags;
    char *mutated;

    fixture = read_fixture("messages", "cache_response.valid.json");
    ck_assert_int_eq(RzbNextCache_SubmitDecision("not-json"),
                     RZB_NEXT_CACHE_INVALID_REQUEST);
    ck_assert_int_eq(RzbNextCache_SubmitDecision(fixture),
                     RZB_NEXT_CACHE_SKIP_KNOWN);

    object = json_tokener_parse(fixture);
    ck_assert_ptr_ne(object, NULL);
    json_object_object_add(object, "found", json_object_new_boolean(false));
    tags = json_object_new_array();
    json_object_object_add(object, "system_tags", tags);
    mutated = copy_json_string(object);
    ck_assert_int_eq(RzbNextCache_SubmitDecision(mutated),
                     RZB_NEXT_CACHE_SUBMIT_NEW);
    free(mutated);

    json_object_object_add(object, "found", json_object_new_boolean(true));
    tags = json_object_new_array();
    json_object_array_add(tags, json_object_new_string("DIRTY"));
    json_object_object_add(object, "system_tags", tags);
    mutated = copy_json_string(object);
    ck_assert_int_eq(RzbNextCache_SubmitDecision(mutated),
                     RZB_NEXT_CACHE_SUBMIT_FOR_REINSPECTION);
    free(mutated);

    tags = json_object_new_array();
    json_object_array_add(tags, json_object_new_string("DIRTY"));
    json_object_array_add(tags, json_object_new_string("NOT_STORED"));
    json_object_object_add(object, "system_tags", tags);
    mutated = copy_json_string(object);
    ck_assert_int_eq(RzbNextCache_SubmitDecision(mutated),
                     RZB_NEXT_CACHE_RESTORE_AND_SUBMIT_FOR_REINSPECTION);
    free(mutated);

    json_object_put(object);
    free(fixture);
}
END_TEST

START_TEST(test_messages_next_route_rejects_structurally_invalid_messages)
{
    char *fixture;
    struct RzbNextRoute *route = NULL;

    fixture = read_fixture("messages", "cnc_directed_command.invalid.json");
    ck_assert(RzbNextMessage_ValidateIdentity(fixture));
    ck_assert(!RzbNextMessage_Validate(fixture));
    ck_assert(!RzbNextMessage_Route(fixture, NULL, &route));
    ck_assert_ptr_eq(route, NULL);
    free(fixture);
}
END_TEST

static Suite *
messages_next_suite(void)
{
    Suite *suite;
    TCase *testcase;

    suite = suite_create("messages_next");
    testcase = tcase_create("core");
    tcase_add_test(testcase, test_messages_next_accepts_known_schema_identities);
    tcase_add_test(testcase,
                   test_messages_next_rejects_identity_mutations_for_all_fixtures);
    tcase_add_test(testcase, test_messages_next_rejects_unknown_or_bad_identity);
    tcase_add_test(testcase, test_messages_next_rejects_invalid_schema_fixtures);
    tcase_add_test(testcase, test_messages_next_routes_match_dispatcher_next_topology);
    tcase_add_test(testcase,
                   test_analysis_result_helpers_build_from_inspection_work);
    tcase_add_test(testcase, test_rabbitmq_prepare_and_decode_inline_message);
    tcase_add_test(testcase, test_rabbitmq_prepare_and_decode_zlib_message);
    tcase_add_test(testcase, test_rabbitmq_prepare_and_decode_claim_check_message);
    tcase_add_test(testcase, test_rabbitmq_prepare_rejects_kafka_routed_message);
    tcase_add_test(testcase,
                   test_cnc_helpers_gate_registration_and_extract_timing);
    tcase_add_test(testcase,
                   test_messages_next_cache_response_route_requires_requestor);
    tcase_add_test(testcase,
                   test_messages_next_cache_submit_decision_maps_response_tags);
    tcase_add_test(testcase,
                   test_messages_next_route_rejects_structurally_invalid_messages);
    suite_add_tcase(suite, testcase);
    return suite;
}

int
main(void)
{
    Suite *suite;
    SRunner *runner;
    int failed;

    suite = messages_next_suite();
    runner = srunner_create(suite);
    srunner_run_all(runner, CK_NORMAL);
    failed = srunner_ntests_failed(runner);
    srunner_free(runner);
    return failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
