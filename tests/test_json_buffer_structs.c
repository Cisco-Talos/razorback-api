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
#include <razorback/json_buffer.h>
#include <razorback/judgment.h>
#include <razorback/list.h>
#include <razorback/nugget.h>
#include <razorback/string_list.h>
#include <razorback/ntlv.h>
#include <razorback/uuids.h>

#include "test_json_buffer_support.h"

#include <json.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <uuid/uuid.h>

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

static struct EventId *
create_event_id(const char *uuid_text, uint64_t seconds, uint64_t nanos)
{
    struct EventId *event_id;

    event_id = calloc(1, sizeof(*event_id));
    ck_assert_ptr_ne(event_id, NULL);
    ck_assert_int_eq(uuid_parse(uuid_text, event_id->uuidNuggetId), 0);
    event_id->iSeconds = seconds;
    event_id->iNanoSecs = nanos;
    return event_id;
}

static int
count_list_items(void *item, void *user_data)
{
    size_t *count = user_data;

    (void)item;
    (*count)++;
    return LIST_EACH_OK;
}

struct StringListState {
    bool saw_alpha;
    bool saw_beta;
};

static int
check_string_list_item(void *item, void *user_data)
{
    char *string = item;
    struct StringListState *state = user_data;

    if (strcmp(string, "alpha") == 0)
        state->saw_alpha = true;
    else if (strcmp(string, "beta") == 0)
        state->saw_beta = true;
    else
        ck_abort_msg("unexpected string list entry: %s", string);

    return LIST_EACH_OK;
}

struct UUIDListState {
    bool saw_first;
    bool saw_second;
};

struct UUIDListOptionalDescriptionState {
    bool saw_entry;
};

static int
check_uuid_list_item(void *item, void *user_data)
{
    struct UUIDListNode *node = item;
    struct UUIDListState *state = user_data;
    uuid_t uuid;

    ck_assert_ptr_ne(node->sName, NULL);
    ck_assert_ptr_ne(node->sDescription, NULL);

    ck_assert_int_eq(uuid_parse("00112233-4455-6677-8899-aabbccddeeff", uuid), 0);
    if (uuid_compare(node->uuid, uuid) == 0) {
        ck_assert_str_eq(node->sName, "alpha");
        ck_assert_str_eq(node->sDescription, "first");
        state->saw_first = true;
        return LIST_EACH_OK;
    }

    ck_assert_int_eq(uuid_parse("12345678-1234-5678-9abc-def012345678", uuid), 0);
    if (uuid_compare(node->uuid, uuid) == 0) {
        ck_assert_str_eq(node->sName, "beta");
        ck_assert_str_eq(node->sDescription, "second");
        state->saw_second = true;
        return LIST_EACH_OK;
    }

    ck_abort_msg("unexpected UUID list entry");
    return LIST_EACH_ERROR;
}

static int
check_uuid_list_item_without_description(void *item, void *user_data)
{
    struct UUIDListNode *node = item;
    struct UUIDListOptionalDescriptionState *state = user_data;
    uuid_t uuid;

    ck_assert_ptr_ne(node->sName, NULL);
    ck_assert_ptr_eq(node->sDescription, NULL);
    ck_assert_int_eq(uuid_parse("00112233-4455-6677-8899-aabbccddeeff", uuid), 0);
    ck_assert_int_eq(uuid_compare(node->uuid, uuid), 0);
    ck_assert_str_eq(node->sName, "alpha");
    state->saw_entry = true;
    return LIST_EACH_OK;
}

struct NTLVListState {
    bool saw_item;
    uuid_t expected_name;
    uuid_t expected_type;
    uint8_t expected_data[4];
};

static int
check_ntlv_item(void *item, void *user_data)
{
    struct NTLVItem *ntlv = item;
    struct NTLVListState *state = user_data;

    ck_assert_int_eq(uuid_compare(ntlv->uuidName, state->expected_name), 0);
    ck_assert_int_eq(uuid_compare(ntlv->uuidType, state->expected_type), 0);
    ck_assert_uint_eq(ntlv->iLength, sizeof(state->expected_data));
    ck_assert_mem_eq(ntlv->pData, state->expected_data,
                     sizeof(state->expected_data));
    state->saw_item = true;
    return LIST_EACH_OK;
}

START_TEST(test_json_buffer_round_trips_block_id_and_matches_schema)
{
    json_object *parent;
    struct BlockId *expected;
    struct BlockId *actual = NULL;

    expected = create_block_id("00112233-4455-6677-8899-aabbccddeeff", 42U,
                               HASH_TYPE_SHA256,
                               "ba7816bf8f01cfea414140de5dae2223"
                               "b00361a396177a9cb410ff61f20015ad");
    parent = json_buffer_test_parent_object();

    ck_assert(JsonBuffer_Put_BlockId(parent, "block_id", expected));
    json_buffer_assert_field_matches_schema(parent, "block_id",
                                            "block-id.schema.json");
    ck_assert(JsonBuffer_Get_BlockId(parent, "block_id", &actual));
    ck_assert(BlockId_IsEqual(expected, actual));

    BlockId_Destroy(actual);
    BlockId_Destroy(expected);
    json_object_put(parent);
}
END_TEST

START_TEST(test_json_buffer_round_trips_event_id_and_matches_schema)
{
    json_object *parent;
    struct EventId *expected;
    struct EventId *actual = NULL;

    expected = create_event_id("00112233-4455-6677-8899-aabbccddeeff",
                               123456789U, 42U);
    parent = json_buffer_test_parent_object();

    ck_assert(JsonBuffer_Put_EventId(parent, "event_id", expected));
    json_buffer_assert_field_matches_schema(parent, "event_id",
                                            "event-id.schema.json");
    ck_assert(JsonBuffer_Get_EventId(parent, "event_id", &actual));
    ck_assert_int_eq(uuid_compare(expected->uuidNuggetId, actual->uuidNuggetId), 0);
    ck_assert_uint_eq(expected->iSeconds, actual->iSeconds);
    ck_assert_uint_eq(expected->iNanoSecs, actual->iNanoSecs);

    EventId_Destroy(actual);
    EventId_Destroy(expected);
    json_object_put(parent);
}
END_TEST

START_TEST(test_json_buffer_round_trips_block_and_matches_schema)
{
    json_object *parent;
    struct Block *expected;
    struct Block *actual = NULL;

    expected = calloc(1, sizeof(*expected));
    ck_assert_ptr_ne(expected, NULL);
    expected->pId = create_block_id("00112233-4455-6677-8899-aabbccddeeff", 64U,
                                    HASH_TYPE_SHA1,
                                    "a94a8fe5ccb19ba61c4c0873d391e987982fbbd3");
    expected->pParentId = create_block_id("12345678-1234-5678-9abc-def012345678", 32U,
                                          HASH_TYPE_MD5,
                                          "900150983cd24fb0d6963f7d28e17f72");
    expected->pMetaDataList = NTLVList_Create();
    ck_assert_ptr_ne(expected->pMetaDataList, NULL);
    parent = json_buffer_test_parent_object();

    ck_assert(JsonBuffer_Put_Block(parent, "block", expected));
    json_buffer_assert_field_matches_schema(parent, "block", "block.schema.json");
    ck_assert(JsonBuffer_Get_Block(parent, "block", &actual));
    ck_assert(BlockId_IsEqual(expected->pId, actual->pId));
    ck_assert(BlockId_IsEqual(expected->pParentId, actual->pParentId));
    ck_assert_ptr_eq(actual->pParentBlock, NULL);
    ck_assert_uint_eq(List_Length(actual->pMetaDataList), 0U);

    Block_Destroy(actual);
    Block_Destroy(expected);
    json_object_put(parent);
}
END_TEST

START_TEST(test_json_buffer_round_trips_event_and_matches_schema)
{
    json_object *parent;
    struct Event *expected;
    struct Event *actual = NULL;

    expected = calloc(1, sizeof(*expected));
    ck_assert_ptr_ne(expected, NULL);
    expected->pId = create_event_id("00112233-4455-6677-8899-aabbccddeeff",
                                    100U, 10U);
    expected->pParentId = create_event_id("12345678-1234-5678-9abc-def012345678",
                                          200U, 20U);
    expected->pMetaDataList = NTLVList_Create();
    ck_assert_ptr_ne(expected->pMetaDataList, NULL);
    expected->pBlock = calloc(1, sizeof(*expected->pBlock));
    ck_assert_ptr_ne(expected->pBlock, NULL);
    expected->pBlock->pId = create_block_id("aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee", 128U,
                                            HASH_TYPE_SHA256,
                                            "248d6a61d20638b8e5c026930c3e6039"
                                            "a33ce45964ff2167f6ecedd419db06c1");
    expected->pBlock->pMetaDataList = NTLVList_Create();
    ck_assert_ptr_ne(expected->pBlock->pMetaDataList, NULL);
    parent = json_buffer_test_parent_object();

    ck_assert(JsonBuffer_Put_Event(parent, "event", expected));
    json_buffer_assert_field_matches_schema(parent, "event", "event.schema.json");
    ck_assert(JsonBuffer_Get_Event(parent, "event", &actual));
    ck_assert_int_eq(uuid_compare(expected->pId->uuidNuggetId,
                                  actual->pId->uuidNuggetId), 0);
    ck_assert_uint_eq(expected->pId->iSeconds, actual->pId->iSeconds);
    ck_assert_uint_eq(expected->pParentId->iSeconds, actual->pParentId->iSeconds);
    ck_assert(BlockId_IsEqual(expected->pBlock->pId, actual->pBlock->pId));
    ck_assert_uint_eq(List_Length(actual->pMetaDataList), 0U);
    ck_assert_uint_eq(List_Length(actual->pBlock->pMetaDataList), 0U);

    Event_Destroy(actual);
    Event_Destroy(expected);
    json_object_put(parent);
}
END_TEST

START_TEST(test_json_buffer_round_trips_judgment_and_matches_schema)
{
    json_object *parent;
    struct Judgment *expected;
    struct Judgment *actual = NULL;

    expected = calloc(1, sizeof(*expected));
    ck_assert_ptr_ne(expected, NULL);
    ck_assert_int_eq(uuid_parse("00112233-4455-6677-8899-aabbccddeeff",
                                expected->uuidNuggetId), 0);
    expected->iSeconds = 111U;
    expected->iNanoSecs = 222U;
    expected->pEventId = create_event_id("12345678-1234-5678-9abc-def012345678",
                                         333U, 444U);
    expected->pBlockId = create_block_id("aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee", 555U,
                                         HASH_TYPE_SHA224,
                                         "23097d223405d8228642a477bda255b32aadbce4"
                                         "bda0b3f7e36c9da7");
    expected->iPriority = 7U;
    expected->pMetaDataList = NTLVList_Create();
    ck_assert_ptr_ne(expected->pMetaDataList, NULL);
    expected->iGID = 99U;
    expected->iSID = 100U;
    expected->Set_SfFlags = 1U;
    expected->Set_EntFlags = 2U;
    expected->Unset_SfFlags = 3U;
    expected->Unset_EntFlags = 4U;
    expected->sMessage = (uint8_t *)strdup("verdict");
    ck_assert_ptr_ne(expected->sMessage, NULL);
    parent = json_buffer_test_parent_object();

    ck_assert(JsonBuffer_Put_Judgment(parent, "judgment", expected));
    json_buffer_assert_field_matches_schema(parent, "judgment",
                                            "judgment.schema.json");
    ck_assert(JsonBuffer_Get_Judgment(parent, "judgment", &actual));
    ck_assert_int_eq(uuid_compare(expected->uuidNuggetId, actual->uuidNuggetId), 0);
    ck_assert_uint_eq(expected->iSeconds, actual->iSeconds);
    ck_assert_uint_eq(expected->iNanoSecs, actual->iNanoSecs);
    ck_assert_uint_eq(expected->iPriority, actual->iPriority);
    ck_assert(BlockId_IsEqual(expected->pBlockId, actual->pBlockId));
    ck_assert_str_eq((char *)expected->sMessage, (char *)actual->sMessage);
    ck_assert_uint_eq(List_Length(actual->pMetaDataList), 0U);

    Judgment_Destroy(actual);
    Judgment_Destroy(expected);
    json_object_put(parent);
}
END_TEST

START_TEST(test_json_buffer_round_trips_judgment_without_message)
{
    json_object *parent;
    json_object *judgment_object;
    json_object *field = NULL;
    struct Judgment *expected;
    struct Judgment *actual = NULL;

    expected = calloc(1, sizeof(*expected));
    ck_assert_ptr_ne(expected, NULL);
    ck_assert_int_eq(uuid_parse("00112233-4455-6677-8899-aabbccddeeff",
                                expected->uuidNuggetId), 0);
    expected->iSeconds = 111U;
    expected->iNanoSecs = 222U;
    expected->pEventId = create_event_id("12345678-1234-5678-9abc-def012345678",
                                         333U, 444U);
    expected->pBlockId = create_block_id("aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee", 555U,
                                         HASH_TYPE_SHA224,
                                         "23097d223405d8228642a477bda255b32aadbce4"
                                         "bda0b3f7e36c9da7");
    expected->iPriority = 7U;
    expected->pMetaDataList = NTLVList_Create();
    ck_assert_ptr_ne(expected->pMetaDataList, NULL);
    expected->iGID = 99U;
    expected->iSID = 100U;
    expected->Set_SfFlags = 1U;
    expected->Set_EntFlags = 2U;
    expected->Unset_SfFlags = 3U;
    expected->Unset_EntFlags = 4U;
    parent = json_buffer_test_parent_object();

    ck_assert(JsonBuffer_Put_Judgment(parent, "judgment", expected));
    json_buffer_assert_field_matches_schema(parent, "judgment",
                                            "judgment.schema.json");
    judgment_object = json_object_object_get(parent, "judgment");
    ck_assert_ptr_ne(judgment_object, NULL);
    ck_assert(!json_object_object_get_ex(judgment_object, "Message", &field));
    ck_assert(JsonBuffer_Get_Judgment(parent, "judgment", &actual));
    ck_assert_int_eq(uuid_compare(expected->uuidNuggetId, actual->uuidNuggetId), 0);
    ck_assert_uint_eq(expected->iSeconds, actual->iSeconds);
    ck_assert_uint_eq(expected->iNanoSecs, actual->iNanoSecs);
    ck_assert_uint_eq(expected->iPriority, actual->iPriority);
    ck_assert(BlockId_IsEqual(expected->pBlockId, actual->pBlockId));
    ck_assert_ptr_eq(actual->sMessage, NULL);
    ck_assert_uint_eq(List_Length(actual->pMetaDataList), 0U);

    Judgment_Destroy(actual);
    Judgment_Destroy(expected);
    json_object_put(parent);
}
END_TEST

START_TEST(test_json_buffer_round_trips_nugget_and_matches_schema)
{
    json_object *parent;
    struct Nugget *expected;
    struct Nugget *actual = NULL;

    expected = calloc(1, sizeof(*expected));
    ck_assert_ptr_ne(expected, NULL);
    ck_assert_int_eq(uuid_parse("00112233-4455-6677-8899-aabbccddeeff",
                                expected->uuidNuggetId), 0);
    ck_assert_int_eq(uuid_parse("12345678-1234-5678-9abc-def012345678",
                                expected->uuidApplicationType), 0);
    ck_assert_int_eq(uuid_parse("aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee",
                                expected->uuidNuggetType), 0);
    expected->sName = strdup("collector");
    expected->sLocation = strdup("lab");
    expected->sContact = strdup("ops@example.test");
    ck_assert_ptr_ne(expected->sName, NULL);
    ck_assert_ptr_ne(expected->sLocation, NULL);
    ck_assert_ptr_ne(expected->sContact, NULL);
    parent = json_buffer_test_parent_object();

    ck_assert(JsonBuffer_Put_Nugget(parent, "nugget", expected));
    json_buffer_assert_field_matches_schema(parent, "nugget",
                                            "nugget.schema.json");
    ck_assert(JsonBuffer_Get_Nugget(parent, "nugget", &actual));
    ck_assert_int_eq(uuid_compare(expected->uuidNuggetId, actual->uuidNuggetId), 0);
    ck_assert_int_eq(uuid_compare(expected->uuidApplicationType,
                                  actual->uuidApplicationType), 0);
    ck_assert_int_eq(uuid_compare(expected->uuidNuggetType,
                                  actual->uuidNuggetType), 0);
    ck_assert_str_eq(expected->sName, actual->sName);
    ck_assert_str_eq(expected->sLocation, actual->sLocation);
    ck_assert_str_eq(expected->sContact, actual->sContact);

    Nugget_Destroy(actual);
    Nugget_Destroy(expected);
    json_object_put(parent);
}
END_TEST

START_TEST(test_json_buffer_round_trips_nugget_without_optional_fields)
{
    json_object *parent;
    json_object *nugget_object;
    json_object *field = NULL;
    struct Nugget *expected;
    struct Nugget *actual = NULL;

    expected = calloc(1, sizeof(*expected));
    ck_assert_ptr_ne(expected, NULL);
    ck_assert_int_eq(uuid_parse("00112233-4455-6677-8899-aabbccddeeff",
                                expected->uuidNuggetId), 0);
    ck_assert_int_eq(uuid_parse("12345678-1234-5678-9abc-def012345678",
                                expected->uuidApplicationType), 0);
    ck_assert_int_eq(uuid_parse("aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee",
                                expected->uuidNuggetType), 0);
    parent = json_buffer_test_parent_object();

    ck_assert(JsonBuffer_Put_Nugget(parent, "nugget", expected));
    json_buffer_assert_field_matches_schema(parent, "nugget",
                                            "nugget.schema.json");
    nugget_object = json_object_object_get(parent, "nugget");
    ck_assert_ptr_ne(nugget_object, NULL);
    ck_assert(!json_object_object_get_ex(nugget_object, "Name", &field));
    ck_assert(!json_object_object_get_ex(nugget_object, "Location", &field));
    ck_assert(!json_object_object_get_ex(nugget_object, "Contact", &field));
    ck_assert(JsonBuffer_Get_Nugget(parent, "nugget", &actual));
    ck_assert_int_eq(uuid_compare(expected->uuidNuggetId, actual->uuidNuggetId), 0);
    ck_assert_int_eq(uuid_compare(expected->uuidApplicationType,
                                  actual->uuidApplicationType), 0);
    ck_assert_int_eq(uuid_compare(expected->uuidNuggetType,
                                  actual->uuidNuggetType), 0);
    ck_assert_ptr_eq(actual->sName, NULL);
    ck_assert_ptr_eq(actual->sLocation, NULL);
    ck_assert_ptr_eq(actual->sContact, NULL);

    Nugget_Destroy(actual);
    Nugget_Destroy(expected);
    json_object_put(parent);
}
END_TEST

START_TEST(test_json_buffer_round_trips_uuid_list_and_matches_schema)
{
    json_object *parent;
    List_t *expected;
    List_t *actual = NULL;
    uuid_t uuid;
    struct UUIDListState state = { false, false };

    expected = UUID_Create_List();
    ck_assert_ptr_ne(expected, NULL);
    ck_assert_int_eq(uuid_parse("00112233-4455-6677-8899-aabbccddeeff", uuid), 0);
    ck_assert(UUID_Add_List_Entry(expected, uuid, "alpha", "first"));
    ck_assert_int_eq(uuid_parse("12345678-1234-5678-9abc-def012345678", uuid), 0);
    ck_assert(UUID_Add_List_Entry(expected, uuid, "beta", "second"));
    parent = json_buffer_test_parent_object();

    ck_assert(JsonBuffer_Put_UUIDList(parent, "uuids", expected));
    json_buffer_assert_field_matches_schema(parent, "uuids",
                                            "uuid-list.schema.json");
    ck_assert(JsonBuffer_Get_UUIDList(parent, "uuids", &actual));
    ck_assert_uint_eq(List_Length(actual), 2U);
    ck_assert(List_ForEach(actual, check_uuid_list_item, &state));
    ck_assert(state.saw_first);
    ck_assert(state.saw_second);

    List_Destroy(actual);
    List_Destroy(expected);
    json_object_put(parent);
}
END_TEST

START_TEST(test_json_buffer_round_trips_empty_uuid_list_and_matches_schema)
{
    json_object *parent;
    List_t *expected;
    List_t *actual = NULL;

    expected = UUID_Create_List();
    ck_assert_ptr_ne(expected, NULL);
    parent = json_buffer_test_parent_object();

    ck_assert(JsonBuffer_Put_UUIDList(parent, "uuids", expected));
    json_buffer_assert_field_matches_schema(parent, "uuids",
                                            "uuid-list.schema.json");
    ck_assert(JsonBuffer_Get_UUIDList(parent, "uuids", &actual));
    ck_assert_uint_eq(List_Length(actual), 0U);

    List_Destroy(actual);
    List_Destroy(expected);
    json_object_put(parent);
}
END_TEST

START_TEST(test_json_buffer_round_trips_uuid_list_without_description_and_matches_schema)
{
    json_object *parent;
    json_object *uuids_object;
    json_object *entry_object;
    json_object *field = NULL;
    List_t *expected;
    List_t *actual = NULL;
    uuid_t uuid;
    struct UUIDListOptionalDescriptionState state = { false };

    expected = UUID_Create_List();
    ck_assert_ptr_ne(expected, NULL);
    ck_assert_int_eq(uuid_parse("00112233-4455-6677-8899-aabbccddeeff", uuid), 0);
    ck_assert(UUID_Add_List_Entry(expected, uuid, "alpha", NULL));
    parent = json_buffer_test_parent_object();

    ck_assert(JsonBuffer_Put_UUIDList(parent, "uuids", expected));
    json_buffer_assert_field_matches_schema(parent, "uuids",
                                            "uuid-list.schema.json");
    uuids_object = json_object_object_get(parent, "uuids");
    ck_assert_ptr_ne(uuids_object, NULL);
    entry_object = json_object_array_get_idx(uuids_object, 0);
    ck_assert_ptr_ne(entry_object, NULL);
    ck_assert(!json_object_object_get_ex(entry_object, "description", &field));
    ck_assert(JsonBuffer_Get_UUIDList(parent, "uuids", &actual));
    ck_assert_uint_eq(List_Length(actual), 1U);
    ck_assert(List_ForEach(actual, check_uuid_list_item_without_description, &state));
    ck_assert(state.saw_entry);

    List_Destroy(actual);
    List_Destroy(expected);
    json_object_put(parent);
}
END_TEST

START_TEST(test_json_buffer_round_trips_string_list_and_matches_schema)
{
    json_object *parent;
    List_t *expected;
    List_t *actual = NULL;
    struct StringListState state = { false, false };

    expected = StringList_Create();
    ck_assert_ptr_ne(expected, NULL);
    ck_assert(StringList_Add(expected, "alpha"));
    ck_assert(StringList_Add(expected, "beta"));
    parent = json_buffer_test_parent_object();

    ck_assert(JsonBuffer_Put_StringList(parent, "strings", expected));
    json_buffer_assert_field_matches_schema(parent, "strings",
                                            "string-list.schema.json");
    ck_assert(JsonBuffer_Get_StringList(parent, "strings", &actual));
    ck_assert_uint_eq(List_Length(actual), 2U);
    ck_assert(List_ForEach(actual, check_string_list_item, &state));
    ck_assert(state.saw_alpha);
    ck_assert(state.saw_beta);

    List_Destroy(actual);
    List_Destroy(expected);
    json_object_put(parent);
}
END_TEST

START_TEST(test_json_buffer_round_trips_empty_string_list_and_matches_schema)
{
    json_object *parent;
    List_t *expected;
    List_t *actual = NULL;

    expected = StringList_Create();
    ck_assert_ptr_ne(expected, NULL);
    parent = json_buffer_test_parent_object();

    ck_assert(JsonBuffer_Put_StringList(parent, "strings", expected));
    json_buffer_assert_field_matches_schema(parent, "strings",
                                            "string-list.schema.json");
    ck_assert(JsonBuffer_Get_StringList(parent, "strings", &actual));
    ck_assert_uint_eq(List_Length(actual), 0U);

    List_Destroy(actual);
    List_Destroy(expected);
    json_object_put(parent);
}
END_TEST

START_TEST(test_json_buffer_round_trips_uint8_list_and_matches_schema)
{
    json_object *parent;
    uint8_t expected[] = { 0U, 7U, 255U };
    uint8_t *actual = NULL;
    uint32_t count = 0;

    parent = json_buffer_test_parent_object();

    ck_assert(JsonBuffer_Put_uint8List(parent, "values", expected,
                                       (uint32_t)(sizeof(expected) / sizeof(expected[0]))));
    json_buffer_assert_field_matches_schema(parent, "values",
                                            "uint8-list.schema.json");
    ck_assert(JsonBuffer_Get_uint8List(parent, "values", &actual, &count));
    ck_assert_uint_eq(count, sizeof(expected) / sizeof(expected[0]));
    ck_assert_mem_eq(actual, expected, sizeof(expected));

    free(actual);
    json_object_put(parent);
}
END_TEST

START_TEST(test_json_buffer_round_trips_empty_uint8_list_and_matches_schema)
{
    json_object *parent;
    uint8_t placeholder = 0U;
    uint8_t *actual = (uint8_t *)&placeholder;
    uint32_t count = 999U;

    parent = json_buffer_test_parent_object();

    ck_assert(JsonBuffer_Put_uint8List(parent, "values", &placeholder, 0U));
    json_buffer_assert_field_matches_schema(parent, "values",
                                            "uint8-list.schema.json");
    ck_assert(JsonBuffer_Get_uint8List(parent, "values", &actual, &count));
    ck_assert_uint_eq(count, 0U);
    ck_assert_ptr_eq(actual, NULL);

    json_object_put(parent);
}
END_TEST

START_TEST(test_json_buffer_round_trips_ntlv_list_and_matches_schema)
{
    json_object *parent;
    List_t *expected;
    List_t *actual = NULL;
    uuid_t name_uuid;
    uuid_t type_uuid;
    struct NTLVListState state = { false };
    size_t item_count = 0;

    expected = NTLVList_Create();
    ck_assert_ptr_ne(expected, NULL);
    ck_assert_int_eq(uuid_parse("00112233-4455-6677-8899-aabbccddeeff", name_uuid), 0);
    ck_assert_int_eq(uuid_parse("12345678-1234-5678-9abc-def012345678", type_uuid), 0);
    state.expected_data[0] = 0x01;
    state.expected_data[1] = 0x02;
    state.expected_data[2] = 0x03;
    state.expected_data[3] = 0x04;
    uuid_copy(state.expected_name, name_uuid);
    uuid_copy(state.expected_type, type_uuid);
    ck_assert(NTLVList_Add(expected, name_uuid, type_uuid,
                           sizeof(state.expected_data), state.expected_data));
    parent = json_buffer_test_parent_object();

    ck_assert(JsonBuffer_Put_NTLVList(parent, "metadata", expected));
    json_buffer_assert_field_matches_schema(parent, "metadata",
                                            "ntlv-list.schema.json");
    ck_assert(JsonBuffer_Get_NTLVList(parent, "metadata", &actual));
    ck_assert(List_ForEach(actual, count_list_items, &item_count));
    ck_assert_uint_eq(item_count, 1U);
    ck_assert(List_ForEach(actual, check_ntlv_item, &state));
    ck_assert(state.saw_item);

    List_Destroy(actual);
    List_Destroy(expected);
    json_object_put(parent);
}
END_TEST

START_TEST(test_json_buffer_round_trips_empty_ntlv_list_and_matches_schema)
{
    json_object *parent;
    List_t *expected;
    List_t *actual = NULL;
    size_t item_count = 0;

    expected = NTLVList_Create();
    ck_assert_ptr_ne(expected, NULL);
    parent = json_buffer_test_parent_object();

    ck_assert(JsonBuffer_Put_NTLVList(parent, "metadata", expected));
    json_buffer_assert_field_matches_schema(parent, "metadata",
                                            "ntlv-list.schema.json");
    ck_assert(JsonBuffer_Get_NTLVList(parent, "metadata", &actual));
    ck_assert(List_ForEach(actual, count_list_items, &item_count));
    ck_assert_uint_eq(item_count, 0U);

    List_Destroy(actual);
    List_Destroy(expected);
    json_object_put(parent);
}
END_TEST

static Suite *
json_buffer_structs_suite(void)
{
    Suite *suite;
    TCase *testcase;

    suite = suite_create("json_buffer_structs");
    testcase = tcase_create("core");

    tcase_add_test(testcase, test_json_buffer_round_trips_block_id_and_matches_schema);
    tcase_add_test(testcase, test_json_buffer_round_trips_event_id_and_matches_schema);
    tcase_add_test(testcase, test_json_buffer_round_trips_block_and_matches_schema);
    tcase_add_test(testcase, test_json_buffer_round_trips_event_and_matches_schema);
    tcase_add_test(testcase, test_json_buffer_round_trips_judgment_and_matches_schema);
    tcase_add_test(testcase, test_json_buffer_round_trips_judgment_without_message);
    tcase_add_test(testcase, test_json_buffer_round_trips_nugget_and_matches_schema);
    tcase_add_test(testcase, test_json_buffer_round_trips_nugget_without_optional_fields);
    tcase_add_test(testcase, test_json_buffer_round_trips_uuid_list_and_matches_schema);
    tcase_add_test(testcase, test_json_buffer_round_trips_empty_uuid_list_and_matches_schema);
    tcase_add_test(testcase, test_json_buffer_round_trips_uuid_list_without_description_and_matches_schema);
    tcase_add_test(testcase, test_json_buffer_round_trips_string_list_and_matches_schema);
    tcase_add_test(testcase, test_json_buffer_round_trips_empty_string_list_and_matches_schema);
    tcase_add_test(testcase, test_json_buffer_round_trips_uint8_list_and_matches_schema);
    tcase_add_test(testcase, test_json_buffer_round_trips_empty_uint8_list_and_matches_schema);
    tcase_add_test(testcase, test_json_buffer_round_trips_ntlv_list_and_matches_schema);
    tcase_add_test(testcase, test_json_buffer_round_trips_empty_ntlv_list_and_matches_schema);

    suite_add_tcase(suite, testcase);
    return suite;
}

int
main(void)
{
    Suite *suite;
    SRunner *runner;
    int failed;

    suite = json_buffer_structs_suite();
    runner = srunner_create(suite);
    srunner_set_fork_status(runner, CK_NOFORK);
    srunner_run_all(runner, CK_NORMAL);
    failed = srunner_ntests_failed(runner);
    srunner_free(runner);

    return (failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
