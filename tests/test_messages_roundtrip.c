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

#include <razorback/block.h>
#include <razorback/block_id.h>
#include <razorback/event.h>
#include <razorback/hash.h>
#include <razorback/judgment.h>
#include <razorback/list.h>
#include <razorback/log.h>
#include <razorback/messages.h>
#include <razorback/ntlv.h>
#include <razorback/string_list.h>
#include <razorback/transfer.h>
#include <razorback/uuids.h>

#include "init.h"
#include "messages/core.h"
#include "test_json_buffer_support.h"

#include <json.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <uuid/uuid.h>

static void
parse_uuid_or_fail(const char *uuid_text, uuid_t uuid)
{
    ck_assert_ptr_ne(uuid_text, NULL);
    ck_assert_int_eq(uuid_parse(uuid_text, uuid), 0);
}

static char *
clone_string(const char *value)
{
    char *copy;

    ck_assert_ptr_ne(value, NULL);
    copy = strdup(value);
    ck_assert_ptr_ne(copy, NULL);
    return copy;
}

static void
ensure_uuid_tables_ready(void)
{
    static bool initialized = false;

    if (!initialized) {
        initUuids();
        initialized = true;
    }
}

static void
ensure_message_handlers_ready(void)
{
    static bool initialized = false;

    if (!initialized) {
        ensure_uuid_tables_ready();
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
    parse_uuid_or_fail(uuid_text, block_id->uuidDataType);
    block_id->iLength = length;
    block_id->pHash = Hash_Create_From_String(hash_type, hash_text);
    ck_assert_ptr_ne(block_id->pHash, NULL);
    return block_id;
}

static struct EventId *
create_event_id(const char *uuid_text, uint64_t seconds, uint64_t nanoseconds)
{
    struct EventId *event_id;

    event_id = calloc(1, sizeof(*event_id));
    ck_assert_ptr_ne(event_id, NULL);
    parse_uuid_or_fail(uuid_text, event_id->uuidNuggetId);
    event_id->iSeconds = seconds;
    event_id->iNanoSecs = nanoseconds;
    return event_id;
}

static List_t *
create_empty_ntlv_list(void)
{
    List_t *list;

    list = NTLVList_Create();
    ck_assert_ptr_ne(list, NULL);
    return list;
}

static struct Block *
create_block(const char *data_type_uuid_text, uint64_t length)
{
    struct Block *block;

    block = calloc(1, sizeof(*block));
    ck_assert_ptr_ne(block, NULL);
    block->pId = create_block_id(data_type_uuid_text, length,
                                 HASH_TYPE_SHA256,
                                 "ba7816bf8f01cfea414140de5dae2223"
                                 "b00361a396177a9cb410ff61f20015ad");
    block->pMetaDataList = create_empty_ntlv_list();
    return block;
}

static struct Event *
create_event(void)
{
    struct Event *event;

    event = calloc(1, sizeof(*event));
    ck_assert_ptr_ne(event, NULL);
    event->pId = create_event_id("00112233-4455-6677-8899-aabbccddeeff",
                                 1234U, 5678U);
    event->pBlock = create_block("11111111-2222-3333-4444-555555555555", 42U);
    event->pMetaDataList = create_empty_ntlv_list();
    parse_uuid_or_fail("aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee",
                       event->uuidApplicationType);
    return event;
}

static struct Nugget *
create_nugget(void)
{
    struct Nugget *nugget;

    nugget = calloc(1, sizeof(*nugget));
    ck_assert_ptr_ne(nugget, NULL);
    parse_uuid_or_fail("99999999-8888-7777-6666-555555555555",
                       nugget->uuidNuggetId);
    parse_uuid_or_fail("aaaaaaaa-1111-2222-3333-444444444444",
                       nugget->uuidApplicationType);
    parse_uuid_or_fail("bbbbbbbb-1111-2222-3333-444444444444",
                       nugget->uuidNuggetType);
    nugget->sName = clone_string("sample-nugget");
    nugget->sLocation = clone_string("lab");
    nugget->sContact = clone_string("ops@example.test");
    return nugget;
}

static struct Judgment *
create_judgment(void)
{
    struct Judgment *judgment;

    judgment = calloc(1, sizeof(*judgment));
    ck_assert_ptr_ne(judgment, NULL);
    parse_uuid_or_fail("12345678-9abc-def0-1234-56789abcdef0",
                       judgment->uuidNuggetId);
    judgment->iSeconds = 4321U;
    judgment->iNanoSecs = 8765U;
    judgment->pEventId = create_event_id("0f1e2d3c-4b5a-6978-8796-a5b4c3d2e1f0",
                                         4321U, 8765U);
    judgment->pBlockId = create_block_id("abcdefab-cdef-abcd-efab-cdefabcdefab",
                                         99U, HASH_TYPE_SHA256,
                                         "248d6a61d20638b8e5c026930c3e6039"
                                         "a33ce45964ff2167f6ecedd419db06c1");
    judgment->iPriority = 7U;
    judgment->pMetaDataList = create_empty_ntlv_list();
    judgment->iGID = 1001U;
    judgment->iSID = 2002U;
    judgment->Set_SfFlags = 3U;
    judgment->Set_EntFlags = 4U;
    judgment->Unset_SfFlags = 5U;
    judgment->Unset_EntFlags = 6U;
    return judgment;
}

static List_t *
create_address_list(void)
{
    List_t *addresses;

    addresses = StringList_Create();
    ck_assert_ptr_ne(addresses, NULL);
    ck_assert(StringList_Add(addresses, "dispatcher.example.test"));
    ck_assert(StringList_Add(addresses, "10.0.0.42"));
    return addresses;
}

static void
init_non_dispatcher_context(struct RazorbackContext *context)
{
    memset(context, 0, sizeof(*context));
    parse_uuid_or_fail("fedcba98-7654-3210-fedc-ba9876543210",
                       context->uuidNuggetId);
    parse_uuid_or_fail("99998888-7777-6666-5555-444433332222",
                       context->uuidApplicationType);
    ck_assert(UUID_Get_UUID(NUGGET_TYPE_INSPECTION, UUID_TYPE_NUGGET_TYPE,
                            context->uuidNuggetType));
    context->locality = 9U;
}

static void
init_dispatcher_context(struct RazorbackContext *context)
{
    memset(context, 0, sizeof(*context));
    parse_uuid_or_fail("01234567-89ab-cdef-0123-456789abcdef",
                       context->uuidNuggetId);
    parse_uuid_or_fail("eeee1111-2222-3333-4444-555566667777",
                       context->uuidApplicationType);
    ck_assert(UUID_Get_UUID(NUGGET_TYPE_DISPATCHER, UUID_TYPE_NUGGET_TYPE,
                            context->uuidNuggetType));
    context->locality = 4U;
    context->dispatcher.priority = 5U;
    context->dispatcher.protocol = TRANSFER_MODE_HTTPS;
    context->dispatcher.port = 8443U;
    context->dispatcher.flags = 0x12U;
    context->dispatcher.addressList = create_address_list();
}

static void
cleanup_context(struct RazorbackContext *context)
{
    if (context->dispatcher.addressList != NULL) {
        List_Destroy(context->dispatcher.addressList);
        context->dispatcher.addressList = NULL;
    }
}

static json_object *
serialize_and_parse_body(struct Message *message)
{
    json_object *body;

    ck_assert_ptr_ne(message, NULL);
    ck_assert(message->serialize(message));
    body = json_tokener_parse((const char *)message->serialized);
    ck_assert_ptr_ne(body, NULL);
    return body;
}

static void
destroy_message(struct Message *message)
{
    if (message == NULL)
        return;

    if (message->type == MESSAGE_TYPE_REG_ERR ||
        message->type == MESSAGE_TYPE_CONFIG_ERR) {
        struct MessageError *error = message->message;

        if (error != NULL) {
            free(error->sMessage);
            free(error);
            message->message = NULL;
        }
        Message_Destroy(message);
        return;
    }

    if (message->destroy != NULL) {
        message->destroy(message);
    } else {
        Message_Destroy(message);
    }
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

START_TEST(test_message_config_ack_round_trips_and_matches_schema)
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

    parse_uuid_or_fail("00112233-4455-6677-8899-aabbccddeeff", source);
    parse_uuid_or_fail("12345678-1234-5678-9abc-def012345678", dest);
    parse_uuid_or_fail("aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee", nugget_type);
    parse_uuid_or_fail("ffffffff-1111-2222-3333-444444444444", app_type);

    message = MessageConfigurationAck_Initialize(source, dest, nugget_type, app_type);
    ck_assert_ptr_ne(message, NULL);
    body = serialize_and_parse_body(message);
    ck_assert(Message_Get_Nuggets(message, parsed_source, parsed_dest));
    ck_assert_int_eq(uuid_compare(parsed_source, source), 0);
    ck_assert_int_eq(uuid_compare(parsed_dest, dest), 0);
    json_buffer_assert_matches_schema("message-config-ack.schema.json", body);

    received = create_received_message(MESSAGE_TYPE_CONFIG_ACK, true, source, dest,
                                       message->serialized, message->length);
    payload = received->message;
    ck_assert_ptr_ne(payload, NULL);
    ck_assert_int_eq(uuid_compare(payload->uuidNuggetType, nugget_type), 0);
    ck_assert_int_eq(uuid_compare(payload->uuidApplicationType, app_type), 0);

    json_object_put(body);
    destroy_message(received);
    destroy_message(message);
}
END_TEST

START_TEST(test_message_cache_request_round_trips_and_matches_schema)
{
    struct Message *message;
    struct Message *received;
    struct MessageCacheReq *payload;
    struct BlockId *expected_block_id;
    json_object *body;
    uuid_t requestor;

    ensure_message_handlers_ready();

    parse_uuid_or_fail("fedcba98-7654-3210-fedc-ba9876543210", requestor);
    expected_block_id = create_block_id("00112233-4455-6677-8899-aabbccddeeff", 42U,
                                        HASH_TYPE_SHA256,
                                        "ba7816bf8f01cfea414140de5dae2223"
                                        "b00361a396177a9cb410ff61f20015ad");

    message = MessageCacheReq_Initialize(requestor, expected_block_id);
    ck_assert_ptr_ne(message, NULL);

    body = serialize_and_parse_body(message);
    json_buffer_assert_matches_schema("message-cache-req.schema.json", body);

    received = create_received_message(MESSAGE_TYPE_REQ, false, requestor, requestor,
                                       message->serialized, message->length);
    payload = received->message;
    ck_assert_ptr_ne(payload, NULL);
    ck_assert_int_eq(uuid_compare(payload->uuidRequestor, requestor), 0);
    ck_assert(BlockId_IsEqual(payload->pId, expected_block_id));

    json_object_put(body);
    BlockId_Destroy(expected_block_id);
    destroy_message(received);
    destroy_message(message);
}
END_TEST

START_TEST(test_message_registration_response_serializes_full_empty_json_body)
{
    struct Message *message;
    json_object *body;
    uuid_t source;
    uuid_t dest;
    uuid_t parsed_source;
    uuid_t parsed_dest;

    parse_uuid_or_fail("00112233-4455-6677-8899-aabbccddeeff", source);
    parse_uuid_or_fail("12345678-1234-5678-9abc-def012345678", dest);

    message = MessageRegistrationResponse_Initialize(source, dest);
    ck_assert_ptr_ne(message, NULL);
    body = serialize_and_parse_body(message);
    ck_assert_uint_eq((unsigned int)message->length, 2U);
    ck_assert_str_eq((const char *)message->serialized, "{}");
    ck_assert(Message_Get_Nuggets(message, parsed_source, parsed_dest));
    ck_assert_int_eq(uuid_compare(parsed_source, source), 0);
    ck_assert_int_eq(uuid_compare(parsed_dest, dest), 0);
    json_buffer_assert_matches_schema("message-reg-resp.schema.json", body);

    json_object_put(body);
    destroy_message(message);
}
END_TEST

START_TEST(test_message_empty_body_schemas)
{
    struct Message *message;
    json_object *body;
    uuid_t source;
    uuid_t dest;

    parse_uuid_or_fail("00112233-4455-6677-8899-aabbccddeeff", source);
    parse_uuid_or_fail("12345678-1234-5678-9abc-def012345678", dest);

    message = MessagePause_Initialize(source, dest);
    ck_assert_ptr_ne(message, NULL);
    body = serialize_and_parse_body(message);
    json_buffer_assert_matches_schema("message-pause.schema.json", body);
    json_object_put(body);
    destroy_message(message);

    message = MessagePaused_Initialize(source, dest);
    ck_assert_ptr_ne(message, NULL);
    body = serialize_and_parse_body(message);
    json_buffer_assert_matches_schema("message-paused.schema.json", body);
    json_object_put(body);
    destroy_message(message);

    message = MessageGo_Initialize(source, dest);
    ck_assert_ptr_ne(message, NULL);
    body = serialize_and_parse_body(message);
    json_buffer_assert_matches_schema("message-go.schema.json", body);
    json_object_put(body);
    destroy_message(message);

    message = MessageRunning_Initialize(source, dest);
    ck_assert_ptr_ne(message, NULL);
    body = serialize_and_parse_body(message);
    json_buffer_assert_matches_schema("message-running.schema.json", body);
    json_object_put(body);
    destroy_message(message);

    message = MessageBye_Initialize(source);
    ck_assert_ptr_ne(message, NULL);
    body = serialize_and_parse_body(message);
    json_buffer_assert_matches_schema("message-bye.schema.json", body);
    json_object_put(body);
    destroy_message(message);

    message = MessageCacheClear_Initialize(source);
    ck_assert_ptr_ne(message, NULL);
    body = serialize_and_parse_body(message);
    json_buffer_assert_matches_schema("message-cache-clear.schema.json", body);
    json_object_put(body);
    destroy_message(message);

    message = MessageReReg_Initialize(source, dest);
    ck_assert_ptr_ne(message, NULL);
    body = serialize_and_parse_body(message);
    json_buffer_assert_matches_schema("message-rereg.schema.json", body);
    json_object_put(body);
    destroy_message(message);
}
END_TEST

START_TEST(test_message_control_body_schemas)
{
    struct RazorbackContext context;
    struct Message *message;
    json_object *body;
    json_object *field;
    uuid_t source;
    uuid_t dest;
    uuid_t dispatcher_id;
    uuid_t nugget_type;
    uuid_t app_type;
    uuid_t data_types[2];

    ensure_message_handlers_ready();

    parse_uuid_or_fail("00112233-4455-6677-8899-aabbccddeeff", source);
    parse_uuid_or_fail("12345678-1234-5678-9abc-def012345678", dest);
    parse_uuid_or_fail("22222222-3333-4444-5555-666666666666", dispatcher_id);
    parse_uuid_or_fail("aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee", app_type);
    ck_assert(UUID_Get_UUID(NUGGET_TYPE_OUTPUT, UUID_TYPE_NUGGET_TYPE, nugget_type));
    parse_uuid_or_fail("11111111-2222-3333-4444-555555555555", data_types[0]);
    parse_uuid_or_fail("66666666-7777-8888-9999-aaaaaaaaaaaa", data_types[1]);

    init_non_dispatcher_context(&context);
    message = MessageHello_Initialize(&context);
    ck_assert_ptr_ne(message, NULL);
    body = serialize_and_parse_body(message);
    json_buffer_assert_matches_schema("message-hello.schema.json", body);
    ck_assert(!json_object_object_get_ex(body, "Priority", &field));
    json_object_put(body);
    destroy_message(message);
    cleanup_context(&context);

    init_dispatcher_context(&context);
    message = MessageHello_Initialize(&context);
    ck_assert_ptr_ne(message, NULL);
    body = serialize_and_parse_body(message);
    json_buffer_assert_matches_schema("message-hello.schema.json", body);
    ck_assert(json_object_object_get_ex(body, "Address_List", &field));
    json_object_put(body);
    destroy_message(message);
    cleanup_context(&context);

    message = MessageRegistrationRequest_Initialize(dispatcher_id, source,
                                                   nugget_type, app_type,
                                                   2U, data_types);
    ck_assert_ptr_ne(message, NULL);
    body = serialize_and_parse_body(message);
    json_buffer_assert_matches_schema("message-reg-req.schema.json", body);
    json_object_put(body);
    destroy_message(message);

    message = MessageConfigurationUpdate_Initialize(source, dest);
    ck_assert_ptr_ne(message, NULL);
    body = serialize_and_parse_body(message);
    json_buffer_assert_matches_schema("message-config-update.schema.json", body);
    json_object_put(body);
    destroy_message(message);

    message = MessageError_Initialize(MESSAGE_TYPE_REG_ERR, "registration failed",
                                      source, dest);
    ck_assert_ptr_ne(message, NULL);
    body = serialize_and_parse_body(message);
    json_buffer_assert_matches_schema("message-error.schema.json", body);
    json_buffer_assert_matches_schema("message-reg-error.schema.json", body);
    json_buffer_assert_matches_schema("message-config-error.schema.json", body);
    json_object_put(body);
    destroy_message(message);

    message = MessageTerminate_Initialize(source, dest,
                                         (const uint8_t *)"shutdown");
    ck_assert_ptr_ne(message, NULL);
    body = serialize_and_parse_body(message);
    json_buffer_assert_matches_schema("message-term.schema.json", body);
    json_object_put(body);
    destroy_message(message);
}
END_TEST

START_TEST(test_message_submission_body_schemas)
{
    struct Message *message;
    json_object *body;
    struct BlockId *block_id;
    struct Event *event;
    struct Judgment *judgment;
    struct EventId *log_event_id;
    uint8_t localities[] = { 1U, 7U, 9U };
    char *log_text;
    uuid_t log_nugget_id;

    ensure_message_handlers_ready();

    block_id = create_block_id("00112233-4455-6677-8899-aabbccddeeff", 64U,
                               HASH_TYPE_SHA256,
                               "4bf5122f344554c53bde2ebb8cd2b7e3"
                               "d1600ad631c385a5d7cce23c7785459a");
    message = MessageCacheResp_Initialize(block_id, 10U, 20U);
    ck_assert_ptr_ne(message, NULL);
    body = serialize_and_parse_body(message);
    json_buffer_assert_matches_schema("message-cache-resp.schema.json", body);
    json_object_put(body);
    BlockId_Destroy(block_id);
    destroy_message(message);

    event = create_event();
    message = MessageBlockSubmission_Initialize(event, 3U, 2U);
    ck_assert_ptr_ne(message, NULL);
    body = serialize_and_parse_body(message);
    json_buffer_assert_matches_schema("message-block-submit.schema.json", body);
    json_object_put(body);
    destroy_message(message);

    judgment = create_judgment();
    message = MessageJudgmentSubmission_Initialize(4U, judgment);
    ck_assert_ptr_ne(message, NULL);
    body = serialize_and_parse_body(message);
    json_buffer_assert_matches_schema("message-judgment.schema.json", body);
    json_object_put(body);
    destroy_message(message);

    event = create_event();
    message = MessageInspectionSubmission_Initialize(event, 8U,
                                                    sizeof(localities),
                                                    localities);
    ck_assert_ptr_ne(message, NULL);
    body = serialize_and_parse_body(message);
    json_buffer_assert_matches_schema("message-inspection.schema.json", body);
    json_object_put(body);
    Event_Destroy(event);
    destroy_message(message);

    log_text = clone_string("sample log message");
    parse_uuid_or_fail("11111111-1111-1111-1111-111111111111", log_nugget_id);
    log_event_id = create_event_id("22222222-2222-2222-2222-222222222222",
                                   11U, 22U);
    message = MessageLog_Initialize(log_nugget_id, 6U, log_text, log_event_id);
    ck_assert_ptr_ne(message, NULL);
    body = serialize_and_parse_body(message);
    json_buffer_assert_matches_schema("message-log.schema.json", body);
    json_object_put(body);
    destroy_message(message);
    free(log_text);
}
END_TEST

START_TEST(test_message_output_body_schemas)
{
    struct Message *message;
    json_object *body;
    struct Event *event;
    struct Block *block;
    struct Block *child;
    struct BlockId *inspection_block_id;
    struct Nugget *nugget;
    struct Judgment *judgment;
    struct MessageLogSubmission log = { 0 };

    ensure_message_handlers_ready();

    event = create_event();
    block = create_block("aaaaaaaa-0000-0000-0000-000000000000", 11U);
    nugget = create_nugget();
    judgment = create_judgment();
    message = MessageAlertPrimary_Initialize(event, block, create_empty_ntlv_list(),
                                            nugget, judgment,
                                            1U, 2U, 3U, 4U);
    ck_assert_ptr_ne(message, NULL);
    body = serialize_and_parse_body(message);
    json_buffer_assert_matches_schema("message-alert-primary.schema.json", body);
    json_object_put(body);
    Judgment_Destroy(judgment);
    destroy_message(message);

    block = create_block("bbbbbbbb-0000-0000-0000-000000000000", 12U);
    child = create_block("cccccccc-0000-0000-0000-000000000000", 13U);
    nugget = create_nugget();
    message = MessageAlertChild_Initialize(block, child, nugget,
                                           21U, 34U, 5U, 6U, 7U, 8U);
    ck_assert_ptr_ne(message, NULL);
    body = serialize_and_parse_body(message);
    json_buffer_assert_matches_schema("message-alert-child.schema.json", body);
    json_object_put(body);
    destroy_message(message);

    event = create_event();
    nugget = create_nugget();
    message = MessageOutputEvent_Initialize(event, nugget);
    ck_assert_ptr_ne(message, NULL);
    body = serialize_and_parse_body(message);
    json_buffer_assert_matches_schema("message-output-event.schema.json", body);
    json_object_put(body);
    destroy_message(message);

    log.iPriority = 9U;
    log.pEventId = create_event_id("33333333-3333-3333-3333-333333333333",
                                   100U, 200U);
    log.sMessage = (uint8_t *)clone_string("output log");
    nugget = create_nugget();
    message = MessageOutputLog_Initialize(&log, nugget);
    ck_assert_ptr_ne(message, NULL);
    ((struct MessageOutputLog *)message->message)->seconds = 100U;
    ((struct MessageOutputLog *)message->message)->nanosecs = 200U;
    body = serialize_and_parse_body(message);
    json_buffer_assert_matches_schema("message-output-log.schema.json", body);
    json_object_put(body);
    destroy_message(message);

    nugget = create_nugget();
    inspection_block_id = create_block_id("dddddddd-0000-0000-0000-000000000000",
                                          14U, HASH_TYPE_SHA256,
                                          "5df6e0e2761358cbd7936dcb8ed8f4f8"
                                          "5b8ed0d0dba7a7d87f8d9c6b6fdd6f60");
    message = MessageOutputInspection_Initialize(
        nugget,
        inspection_block_id,
        3U,
        true);
    ck_assert_ptr_ne(message, NULL);
    body = serialize_and_parse_body(message);
    json_buffer_assert_matches_schema("message-output-inspection.schema.json", body);
    json_object_put(body);
    BlockId_Destroy(inspection_block_id);
    destroy_message(message);
}
END_TEST

START_TEST(test_message_setup_rejects_unknown_type)
{
    struct Message *message;
    uuid_t source;
    uuid_t dest;

    ensure_message_handlers_ready();

    parse_uuid_or_fail("00112233-4455-6677-8899-aabbccddeeff", source);
    parse_uuid_or_fail("12345678-1234-5678-9abc-def012345678", dest);

    message = Message_Create_Directed(0xdeadbeefU, MESSAGE_VERSION_1, 0, source, dest);
    ck_assert_ptr_ne(message, NULL);
    ck_assert(!Message_Setup(message));

    Message_Destroy(message);
}
END_TEST

START_TEST(test_message_config_ack_deserialize_rejects_missing_app_type)
{
    struct Message *message;
    uuid_t source;
    uuid_t dest;

    ensure_message_handlers_ready();

    parse_uuid_or_fail("00112233-4455-6677-8899-aabbccddeeff", source);
    parse_uuid_or_fail("12345678-1234-5678-9abc-def012345678", dest);

    message = Message_Create_Directed(MESSAGE_TYPE_CONFIG_ACK, MESSAGE_VERSION_1, 0,
                                      source, dest);
    ck_assert_ptr_ne(message, NULL);
    ck_assert(Message_Setup(message));
    set_serialized_body(message,
                        "{\"Nugget_Type\":{\"id\":\"aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee\"}}");
    ck_assert(!message->deserialize(message));

    destroy_message(message);
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

    destroy_message(message);
}
END_TEST

static Suite *
messages_suite(void)
{
    Suite *suite;
    TCase *testcase;

    suite = suite_create("messages_roundtrip");
    testcase = tcase_create("core");

    tcase_add_test(testcase, test_message_config_ack_round_trips_and_matches_schema);
    tcase_add_test(testcase, test_message_cache_request_round_trips_and_matches_schema);
    tcase_add_test(testcase, test_message_registration_response_serializes_full_empty_json_body);
    tcase_add_test(testcase, test_message_empty_body_schemas);
    tcase_add_test(testcase, test_message_control_body_schemas);
    tcase_add_test(testcase, test_message_submission_body_schemas);
    tcase_add_test(testcase, test_message_output_body_schemas);
    tcase_add_test(testcase, test_message_setup_rejects_unknown_type);
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
