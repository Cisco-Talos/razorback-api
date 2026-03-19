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

#include <razorback/block_id.h>
#include <razorback/hash.h>
#include <razorback/log.h>
#include <razorback/messages.h>

#include "init.h"
#include "messages/core.h"
#include "test_json_buffer_support.h"

#include <json.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <uuid/uuid.h>

void MessageBlockSubmission_Init(void) {}
void MessageCacheResp_Init(void) {}
void MessageInspectionSubmission_Init(void) {}
void MessageJudgmentSubmission_Init(void) {}
void MessageLogSubmission_Init(void) {}
void MessageAlertPrimary_Init(void) {}
void MessageAlertChild_Init(void) {}
void MessageOutputLog_Init(void) {}
void MessageOutputEvent_Init(void) {}
void Message_CnC_Bye_Init(void) {}
void Message_CnC_CacheClear_Init(void) {}
void Message_CnC_ConfigUpdate_Init(void) {}
void Message_CnC_Error_Init(void) {}
void Message_CnC_Go_Init(void) {}
void Message_CnC_Hello_Init(void) {}
void Message_CnC_Pause_Init(void) {}
void Message_CnC_Paused_Init(void) {}
void Message_CnC_RegReq_Init(void) {}
void Message_CnC_RegResp_Init(void) {}
void Message_CnC_Running_Init(void) {}
void Message_CnC_Term_Init(void) {}
void Message_CnC_ReReg_Init(void) {}

static void
ensure_message_handlers_ready(void)
{
    static bool initialized = false;

    if (!initialized) {
        ck_assert(Message_Init());
        initialized = true;
    }
}

static struct BlockId *
create_block_id(const char *uuid_text, uint64_t length, uint32_t hash_type,
                const char *hash_text)
{
    struct BlockId *block_id;

    block_id = calloc(1, sizeof(*block_id));
    ck_assert_ptr_ne(block_id, NULL);
    ck_assert_int_eq(uuid_parse(uuid_text, block_id->uuidDataType), 0);
    block_id->iLength = length;
    block_id->pHash = Hash_Create_From_String(hash_type, hash_text);
    ck_assert_ptr_ne(block_id->pHash, NULL);
    return block_id;
}

static struct Message *
create_received_message(uint32_t type, bool directed, const uuid_t source,
                        const uuid_t dest, const uint8_t *serialized,
                        size_t length)
{
    struct Message *message;

    ck_assert_ptr_ne(serialized, NULL);

    if (directed)
        message = Message_Create_Directed(type, MESSAGE_VERSION_1, 0, source, dest);
    else
        message = Message_Create(type, MESSAGE_VERSION_1, 0);

    ck_assert_ptr_ne(message, NULL);
    message->serialized = calloc(length + 1, sizeof(uint8_t));
    ck_assert_ptr_ne(message->serialized, NULL);
    memcpy(message->serialized, serialized, length + 1);
    message->length = length;
    ck_assert(Message_Setup(message));
    ck_assert(message->deserialize(message));
    return message;
}

static void
set_serialized_body(struct Message *message, const char *body)
{
    size_t length;

    ck_assert_ptr_ne(message, NULL);
    ck_assert_ptr_ne(body, NULL);

    length = strlen(body);
    message->serialized = calloc(length + 1, sizeof(uint8_t));
    ck_assert_ptr_ne(message->serialized, NULL);
    memcpy(message->serialized, body, length + 1);
    message->length = length;
}

START_TEST(test_message_config_ack_round_trips_and_matches_embedded_schemas)
{
    struct Message *message;
    struct Message *received;
    struct MessageConfigurationAck *payload;
    json_object *body;
    uuid_t source;
    uuid_t dest;
    uuid_t nugget_type;
    uuid_t app_type;
    uuid_t parsed_source;
    uuid_t parsed_dest;

    ensure_message_handlers_ready();

    ck_assert_int_eq(uuid_parse("00112233-4455-6677-8899-aabbccddeeff", source), 0);
    ck_assert_int_eq(uuid_parse("12345678-1234-5678-9abc-def012345678", dest), 0);
    ck_assert_int_eq(uuid_parse("aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee", nugget_type), 0);
    ck_assert_int_eq(uuid_parse("ffffffff-1111-2222-3333-444444444444", app_type), 0);

    message = MessageConfigurationAck_Initialize(source, dest, nugget_type, app_type);
    ck_assert_ptr_ne(message, NULL);
    ck_assert(message->serialize(message));
    ck_assert(Message_Get_Nuggets(message, parsed_source, parsed_dest));
    ck_assert_int_eq(uuid_compare(parsed_source, source), 0);
    ck_assert_int_eq(uuid_compare(parsed_dest, dest), 0);

    body = json_tokener_parse((const char *)message->serialized);
    ck_assert_ptr_ne(body, NULL);
    ck_assert_uint_eq((unsigned int)json_object_object_length(body), 2U);
    json_buffer_assert_field_matches_schema(body, "Nugget_Type", "uuid.schema.json");
    json_buffer_assert_field_matches_schema(body, "App_Type", "uuid.schema.json");

    received = create_received_message(MESSAGE_TYPE_CONFIG_ACK, true, source, dest,
                                       message->serialized, message->length);
    payload = received->message;
    ck_assert_ptr_ne(payload, NULL);
    ck_assert_int_eq(uuid_compare(payload->uuidNuggetType, nugget_type), 0);
    ck_assert_int_eq(uuid_compare(payload->uuidApplicationType, app_type), 0);

    json_object_put(body);
    received->destroy(received);
    message->destroy(message);
}
END_TEST

START_TEST(test_message_cache_request_round_trips_and_matches_embedded_schemas)
{
    struct Message *message;
    struct Message *received;
    struct MessageCacheReq *payload;
    struct BlockId *expected_block_id;
    json_object *body;
    uuid_t requestor;

    ensure_message_handlers_ready();

    ck_assert_int_eq(uuid_parse("fedcba98-7654-3210-fedc-ba9876543210", requestor), 0);
    expected_block_id = create_block_id("00112233-4455-6677-8899-aabbccddeeff", 42U,
                                        HASH_TYPE_SHA256,
                                        "ba7816bf8f01cfea414140de5dae2223"
                                        "b00361a396177a9cb410ff61f20015ad");

    message = MessageCacheReq_Initialize(requestor, expected_block_id);
    ck_assert_ptr_ne(message, NULL);
    ck_assert(message->serialize(message));

    body = json_tokener_parse((const char *)message->serialized);
    ck_assert_ptr_ne(body, NULL);
    ck_assert_uint_eq((unsigned int)json_object_object_length(body), 2U);
    json_buffer_assert_field_matches_schema(body, "Requestor", "uuid.schema.json");
    json_buffer_assert_field_matches_schema(body, "Block_ID", "block-id.schema.json");

    received = create_received_message(MESSAGE_TYPE_REQ, false, requestor, requestor,
                                       message->serialized, message->length);
    payload = received->message;
    ck_assert_ptr_ne(payload, NULL);
    ck_assert_int_eq(uuid_compare(payload->uuidRequestor, requestor), 0);
    ck_assert(BlockId_IsEqual(payload->pId, expected_block_id));

    json_object_put(body);
    BlockId_Destroy(expected_block_id);
    received->destroy(received);
    message->destroy(message);
}
END_TEST

START_TEST(test_message_setup_rejects_unknown_type)
{
    struct Message *message;
    uuid_t source;
    uuid_t dest;

    ensure_message_handlers_ready();

    ck_assert_int_eq(uuid_parse("00112233-4455-6677-8899-aabbccddeeff", source), 0);
    ck_assert_int_eq(uuid_parse("12345678-1234-5678-9abc-def012345678", dest), 0);

    message = Message_Create_Directed(0xdeadbeefU, MESSAGE_VERSION_1, 0, source, dest);
    ck_assert_ptr_ne(message, NULL);
    ck_assert(!Message_Setup(message));

    Message_Destroy(message);
}
END_TEST

START_TEST(test_message_set_serialized_json_rejects_replacing_existing_body)
{
    struct Message *message;

    message = Message_Create(MESSAGE_TYPE_REQ, MESSAGE_VERSION_1, 0);
    ck_assert_ptr_ne(message, NULL);
    ck_assert(Message_SetSerializedJson(message, "{\"ok\":true}", LOG_C_CORE, __func__));
    ck_assert(!Message_SetSerializedJson(message, "{\"again\":true}", LOG_C_CORE, __func__));

    Message_Destroy(message);
}
END_TEST

START_TEST(test_message_config_ack_deserialize_rejects_missing_app_type)
{
    struct Message *message;
    uuid_t source;
    uuid_t dest;

    ensure_message_handlers_ready();

    ck_assert_int_eq(uuid_parse("00112233-4455-6677-8899-aabbccddeeff", source), 0);
    ck_assert_int_eq(uuid_parse("12345678-1234-5678-9abc-def012345678", dest), 0);

    message = Message_Create_Directed(MESSAGE_TYPE_CONFIG_ACK, MESSAGE_VERSION_1, 0,
                                      source, dest);
    ck_assert_ptr_ne(message, NULL);
    ck_assert(Message_Setup(message));
    set_serialized_body(message,
                        "{\"Nugget_Type\":{\"id\":\"aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee\"}}");
    ck_assert(!message->deserialize(message));

    message->destroy(message);
}
END_TEST

START_TEST(test_message_cache_request_deserialize_rejects_missing_block_id)
{
    struct Message *message;

    ensure_message_handlers_ready();

    message = Message_Create(MESSAGE_TYPE_REQ, MESSAGE_VERSION_1, 0);
    ck_assert_ptr_ne(message, NULL);
    ck_assert(Message_Setup(message));
    set_serialized_body(message,
                        "{\"Requestor\":{\"id\":\"fedcba98-7654-3210-fedc-ba9876543210\"}}");
    ck_assert(!message->deserialize(message));

    message->destroy(message);
}
END_TEST

static Suite *
messages_suite(void)
{
    Suite *suite;
    TCase *testcase;

    suite = suite_create("messages_roundtrip");
    testcase = tcase_create("core");

    tcase_add_test(testcase, test_message_config_ack_round_trips_and_matches_embedded_schemas);
    tcase_add_test(testcase, test_message_cache_request_round_trips_and_matches_embedded_schemas);
    tcase_add_test(testcase, test_message_setup_rejects_unknown_type);
    tcase_add_test(testcase, test_message_set_serialized_json_rejects_replacing_existing_body);
    tcase_add_test(testcase, test_message_config_ack_deserialize_rejects_missing_app_type);
    tcase_add_test(testcase, test_message_cache_request_deserialize_rejects_missing_block_id);

    suite_add_tcase(suite, testcase);
    return suite;
}

int
main(void)
{
    Suite *suite;
    SRunner *runner;
    int failed;

    suite = messages_suite();
    runner = srunner_create(suite);
    srunner_set_fork_status(runner, CK_NOFORK);
    srunner_run_all(runner, CK_NORMAL);
    failed = srunner_ntests_failed(runner);
    srunner_free(runner);

    return (failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
