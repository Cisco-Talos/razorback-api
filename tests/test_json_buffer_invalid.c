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
#include <razorback/json_buffer.h>
#include <razorback/ntlv.h>
#include <razorback/string_list.h>
#include <razorback/uuids.h>

#include "init.h"
#include "test_json_buffer_support.h"

#include <json.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <uuid/uuid.h>

static json_object *
new_uuid_object(const char *id_text)
{
    json_object *object;

    object = json_object_new_object();
    ck_assert_ptr_ne(object, NULL);
    json_object_object_add(object, "id", json_object_new_string(id_text));
    return object;
}

static json_object *
new_hash_object(const char *type_name, const char *hash_text)
{
    json_object *object;

    object = json_object_new_object();
    ck_assert_ptr_ne(object, NULL);
    json_object_object_add(object, "Type", json_object_new_string(type_name));
    json_object_object_add(object, "Value", json_object_new_string(hash_text));
    return object;
}

static void
add_uuid_field(json_object *parent, const char *field_name, const char *id_text)
{
    ck_assert_ptr_ne(parent, NULL);
    ck_assert_ptr_ne(field_name, NULL);
    json_object_object_add(parent, field_name, new_uuid_object(id_text));
}

static void
ensure_test_ntlv_type_registered(const char *type_name)
{
    static bool initialized = false;
    static const struct {
        const char *name;
        const char *uuid_text;
    } ntlv_types[] = {
        { NTLV_TYPE_STRING, "0d2e9e86-fb73-4d16-81ea-20175ab6c781" },
        { NTLV_TYPE_JSON, "7b2fe6f4-1202-4dc8-9fb6-09f5b4f62c21" }
    };
    size_t i;
    List_t *list;
    uuid_t uuid;

    ck_assert_ptr_ne(type_name, NULL);

    if (!initialized) {
        initUuids();
        initialized = true;
    }

    if (UUID_Get_UUID(type_name, UUID_TYPE_NTLV_TYPE, uuid))
        return;

    list = UUID_Get_List(UUID_TYPE_NTLV_TYPE);
    ck_assert_ptr_ne(list, NULL);

    for (i = 0; i < (sizeof(ntlv_types) / sizeof(ntlv_types[0])); i++) {
        if (strcmp(type_name, ntlv_types[i].name) == 0) {
            ck_assert_int_eq(uuid_parse(ntlv_types[i].uuid_text, uuid), 0);
            ck_assert(UUID_Add_List_Entry(list, uuid, type_name, NULL));
            return;
        }
    }

    ck_abort_msg("missing test UUID mapping for NTLV type %s", type_name);
}

static void
add_ntlv_type_field(json_object *parent, const char *field_name, const char *type_name)
{
    char uuid_text[UUID_STRING_LENGTH];
    uuid_t uuid;

    ck_assert_ptr_ne(parent, NULL);
    ck_assert_ptr_ne(field_name, NULL);
    ck_assert_ptr_ne(type_name, NULL);
    ensure_test_ntlv_type_registered(type_name);
    ck_assert(UUID_Get_UUID(type_name, UUID_TYPE_NTLV_TYPE, uuid));
    uuid_unparse(uuid, uuid_text);
    add_uuid_field(parent, field_name, uuid_text);
}

START_TEST(test_json_buffer_get_uuid_rejects_malformed_uuid_text)
{
    json_object *parent;
    uuid_t value;

    parent = json_buffer_test_parent_object();
    json_object_object_add(parent, "uuid", new_uuid_object("not-a-uuid"));

    ck_assert(!JsonBuffer_Get_UUID(parent, "uuid", value));

    json_object_put(parent);
}
END_TEST

START_TEST(test_json_buffer_get_hash_rejects_unknown_hash_type)
{
    json_object *parent;
    struct Hash *hash = NULL;

    parent = json_buffer_test_parent_object();
    json_object_object_add(parent, "hash",
                           new_hash_object("SHA999",
                                           "ba7816bf8f01cfea414140de5dae2223"
                                           "b00361a396177a9cb410ff61f20015ad"));

    ck_assert(!JsonBuffer_Get_Hash(parent, "hash", &hash));
    ck_assert_ptr_eq(hash, NULL);

    json_object_put(parent);
}
END_TEST

START_TEST(test_json_buffer_get_hash_rejects_malformed_hash_text)
{
    json_object *parent;
    struct Hash *hash = NULL;

    parent = json_buffer_test_parent_object();
    json_object_object_add(parent, "hash",
                           new_hash_object("SHA256", "xyz"));

    ck_assert(!JsonBuffer_Get_Hash(parent, "hash", &hash));
    ck_assert_ptr_eq(hash, NULL);

    json_object_put(parent);
}
END_TEST

START_TEST(test_json_buffer_get_block_id_rejects_invalid_data_type_uuid)
{
    json_object *parent;
    json_object *block_id;
    struct BlockId *value = NULL;

    parent = json_buffer_test_parent_object();
    block_id = json_object_new_object();
    ck_assert_ptr_ne(block_id, NULL);
    json_object_object_add(parent, "block_id", block_id);
    json_object_object_add(block_id, "Hash",
                           new_hash_object("SHA256",
                                           "ba7816bf8f01cfea414140de5dae2223"
                                           "b00361a396177a9cb410ff61f20015ad"));
    json_object_object_add(block_id, "Size", json_object_new_uint64(42U));
    json_object_object_add(block_id, "Data_Type", new_uuid_object("invalid-uuid"));

    ck_assert(!JsonBuffer_Get_BlockId(parent, "block_id", &value));
    ck_assert_ptr_eq(value, NULL);

    json_object_put(parent);
}
END_TEST

START_TEST(test_json_buffer_get_event_id_rejects_invalid_nugget_uuid)
{
    json_object *parent;
    json_object *event_id;
    json_object *nugget;
    struct EventId *value = NULL;

    parent = json_buffer_test_parent_object();
    event_id = json_object_new_object();
    nugget = json_object_new_object();
    ck_assert_ptr_ne(event_id, NULL);
    ck_assert_ptr_ne(nugget, NULL);
    json_object_object_add(parent, "event_id", event_id);
    json_object_object_add(event_id, "Nugget", nugget);
    json_object_object_add(nugget, "Id", json_object_new_string("invalid-uuid"));
    json_object_object_add(event_id, "Seconds", json_object_new_uint64(100U));
    json_object_object_add(event_id, "Nano_Seconds", json_object_new_uint64(200U));

    ck_assert(!JsonBuffer_Get_EventId(parent, "event_id", &value));
    ck_assert_ptr_eq(value, NULL);

    json_object_put(parent);
}
END_TEST

START_TEST(test_json_buffer_get_ntlv_list_rejects_non_object_entries)
{
    json_object *parent;
    json_object *metadata;
    List_t *list = NULL;

    parent = json_buffer_test_parent_object();
    metadata = json_object_new_array();
    ck_assert_ptr_ne(metadata, NULL);
    json_object_array_add(metadata, json_object_new_string("bad"));
    json_object_object_add(parent, "metadata", metadata);

    ck_assert(!JsonBuffer_Get_NTLVList(parent, "metadata", &list));
    ck_assert_ptr_eq(list, NULL);

    json_object_put(parent);
}
END_TEST

START_TEST(test_json_buffer_get_ntlv_list_rejects_invalid_type_uuid)
{
    json_object *parent;
    json_object *metadata;
    json_object *entry;
    List_t *list = NULL;

    parent = json_buffer_test_parent_object();
    metadata = json_object_new_array();
    entry = json_object_new_object();
    ck_assert_ptr_ne(metadata, NULL);
    ck_assert_ptr_ne(entry, NULL);
    json_object_array_add(metadata, entry);
    json_object_object_add(parent, "metadata", metadata);

    add_uuid_field(entry, "Name", "00112233-4455-6677-8899-aabbccddeeff");
    json_object_object_add(entry, "Type", new_uuid_object("invalid-uuid"));
    json_object_object_add(entry, "String_Value", json_object_new_string("value"));

    ck_assert(!JsonBuffer_Get_NTLVList(parent, "metadata", &list));
    ck_assert_ptr_eq(list, NULL);

    json_object_put(parent);
}
END_TEST

START_TEST(test_json_buffer_get_ntlv_list_rejects_multiple_value_fields)
{
    json_object *parent;
    json_object *metadata;
    json_object *entry;
    List_t *list = NULL;

    parent = json_buffer_test_parent_object();
    metadata = json_object_new_array();
    entry = json_object_new_object();
    ck_assert_ptr_ne(metadata, NULL);
    ck_assert_ptr_ne(entry, NULL);
    json_object_array_add(metadata, entry);
    json_object_object_add(parent, "metadata", metadata);

    add_uuid_field(entry, "Name", "00112233-4455-6677-8899-aabbccddeeff");
    add_ntlv_type_field(entry, "Type", NTLV_TYPE_STRING);
    json_object_object_add(entry, "String_Value", json_object_new_string("value"));
    json_object_object_add(entry, "Bin_Value", json_object_new_string("dmFsdWU="));

    ck_assert(!JsonBuffer_Get_NTLVList(parent, "metadata", &list));
    ck_assert_ptr_eq(list, NULL);

    json_object_put(parent);
}
END_TEST

START_TEST(test_json_buffer_get_ntlv_list_rejects_non_container_json_value)
{
    json_object *parent;
    json_object *metadata;
    json_object *entry;
    List_t *list = NULL;

    parent = json_buffer_test_parent_object();
    metadata = json_object_new_array();
    entry = json_object_new_object();
    ck_assert_ptr_ne(metadata, NULL);
    ck_assert_ptr_ne(entry, NULL);
    json_object_array_add(metadata, entry);
    json_object_object_add(parent, "metadata", metadata);

    add_uuid_field(entry, "Name", "00112233-4455-6677-8899-aabbccddeeff");
    add_ntlv_type_field(entry, "Type", NTLV_TYPE_JSON);
    json_object_object_add(entry, "Json_Value", json_object_new_string("value"));

    ck_assert(!JsonBuffer_Get_NTLVList(parent, "metadata", &list));
    ck_assert_ptr_eq(list, NULL);

    json_object_put(parent);
}
END_TEST

START_TEST(test_json_buffer_get_uuid_list_rejects_malformed_uuid_entries)
{
    json_object *parent;
    json_object *uuids;
    json_object *entry;
    List_t *list = NULL;

    parent = json_buffer_test_parent_object();
    uuids = json_object_new_array();
    entry = json_object_new_object();
    ck_assert_ptr_ne(uuids, NULL);
    ck_assert_ptr_ne(entry, NULL);
    json_object_array_add(uuids, entry);
    json_object_object_add(parent, "uuids", uuids);
    json_object_object_add(entry, "id", json_object_new_string("not-a-uuid"));
    json_object_object_add(entry, "name", json_object_new_string("alpha"));

    ck_assert(!JsonBuffer_Get_UUIDList(parent, "uuids", &list));
    ck_assert_ptr_eq(list, NULL);

    json_object_put(parent);
}
END_TEST

START_TEST(test_json_buffer_get_uuid_list_rejects_missing_name)
{
    json_object *parent;
    json_object *uuids;
    json_object *entry;
    List_t *list = NULL;

    parent = json_buffer_test_parent_object();
    uuids = json_object_new_array();
    entry = json_object_new_object();
    ck_assert_ptr_ne(uuids, NULL);
    ck_assert_ptr_ne(entry, NULL);
    json_object_array_add(uuids, entry);
    json_object_object_add(parent, "uuids", uuids);
    json_object_object_add(entry, "id",
                           json_object_new_string("00112233-4455-6677-8899-aabbccddeeff"));

    ck_assert(!JsonBuffer_Get_UUIDList(parent, "uuids", &list));
    ck_assert_ptr_eq(list, NULL);

    json_object_put(parent);
}
END_TEST

START_TEST(test_json_buffer_get_uuid_list_rejects_empty_name)
{
    json_object *parent;
    json_object *uuids;
    json_object *entry;
    List_t *list = NULL;

    parent = json_buffer_test_parent_object();
    uuids = json_object_new_array();
    entry = json_object_new_object();
    ck_assert_ptr_ne(uuids, NULL);
    ck_assert_ptr_ne(entry, NULL);
    json_object_array_add(uuids, entry);
    json_object_object_add(parent, "uuids", uuids);
    json_object_object_add(entry, "id",
                           json_object_new_string("00112233-4455-6677-8899-aabbccddeeff"));
    json_object_object_add(entry, "name", json_object_new_string(""));

    ck_assert(!JsonBuffer_Get_UUIDList(parent, "uuids", &list));
    ck_assert_ptr_eq(list, NULL);

    json_object_put(parent);
}
END_TEST

START_TEST(test_json_buffer_get_string_list_rejects_non_string_entries)
{
    json_object *parent;
    json_object *strings;
    List_t *list = NULL;

    parent = json_buffer_test_parent_object();
    strings = json_object_new_array();
    ck_assert_ptr_ne(strings, NULL);
    json_object_array_add(strings, json_object_new_string("alpha"));
    json_object_array_add(strings, json_object_new_int(7));
    json_object_object_add(parent, "strings", strings);

    ck_assert(!JsonBuffer_Get_StringList(parent, "strings", &list));
    ck_assert_ptr_eq(list, NULL);

    json_object_put(parent);
}
END_TEST

START_TEST(test_json_buffer_get_uint8_list_rejects_non_integer_entries)
{
    json_object *parent;
    json_object *values;
    uint8_t *list = NULL;
    uint32_t count = 0;

    parent = json_buffer_test_parent_object();
    values = json_object_new_array();
    ck_assert_ptr_ne(values, NULL);
    json_object_array_add(values, json_object_new_int(1));
    json_object_array_add(values, json_object_new_string("bad"));
    json_object_object_add(parent, "values", values);

    ck_assert(!JsonBuffer_Get_uint8List(parent, "values", &list, &count));
    ck_assert_ptr_eq(list, NULL);
    ck_assert_uint_eq(count, 0U);

    json_object_put(parent);
}
END_TEST

START_TEST(test_json_buffer_get_uint8_rejects_negative_and_overflow_values)
{
    json_object *parent;
    uint8_t value = 0U;

    parent = json_buffer_test_parent_object();
    json_object_object_add(parent, "negative", json_object_new_int(-1));
    json_object_object_add(parent, "overflow", json_object_new_int(256));

    ck_assert(!JsonBuffer_Get_uint8_t(parent, "negative", &value));
    ck_assert(!JsonBuffer_Get_uint8_t(parent, "overflow", &value));

    json_object_put(parent);
}
END_TEST

START_TEST(test_json_buffer_get_uint16_rejects_negative_and_overflow_values)
{
    json_object *parent;
    uint16_t value = 0U;

    parent = json_buffer_test_parent_object();
    json_object_object_add(parent, "negative", json_object_new_int(-1));
    json_object_object_add(parent, "overflow", json_object_new_int(65536));

    ck_assert(!JsonBuffer_Get_uint16_t(parent, "negative", &value));
    ck_assert(!JsonBuffer_Get_uint16_t(parent, "overflow", &value));

    json_object_put(parent);
}
END_TEST

START_TEST(test_json_buffer_get_uint32_rejects_negative_and_overflow_values)
{
    json_object *parent;
    uint32_t value = 0U;

    parent = json_buffer_test_parent_object();
    json_object_object_add(parent, "negative_int", json_object_new_int(-1));
    json_object_object_add(parent, "negative_string", json_object_new_string("-1"));
    json_object_object_add(parent, "overflow_int",
                           json_object_new_uint64((uint64_t)UINT32_MAX + 1U));
    json_object_object_add(parent, "overflow_string",
                           json_object_new_string("4294967296"));

    ck_assert(!JsonBuffer_Get_uint32_t(parent, "negative_int", &value));
    ck_assert(!JsonBuffer_Get_uint32_t(parent, "negative_string", &value));
    ck_assert(!JsonBuffer_Get_uint32_t(parent, "overflow_int", &value));
    ck_assert(!JsonBuffer_Get_uint32_t(parent, "overflow_string", &value));

    json_object_put(parent);
}
END_TEST

static Suite *
json_buffer_invalid_suite(void)
{
    Suite *suite;
    TCase *testcase;

    suite = suite_create("json_buffer_invalid");
    testcase = tcase_create("core");

    tcase_add_test(testcase, test_json_buffer_get_uuid_rejects_malformed_uuid_text);
    tcase_add_test(testcase, test_json_buffer_get_hash_rejects_unknown_hash_type);
    tcase_add_test(testcase, test_json_buffer_get_hash_rejects_malformed_hash_text);
    tcase_add_test(testcase, test_json_buffer_get_block_id_rejects_invalid_data_type_uuid);
    tcase_add_test(testcase, test_json_buffer_get_event_id_rejects_invalid_nugget_uuid);
    tcase_add_test(testcase, test_json_buffer_get_ntlv_list_rejects_non_object_entries);
    tcase_add_test(testcase, test_json_buffer_get_ntlv_list_rejects_invalid_type_uuid);
    tcase_add_test(testcase, test_json_buffer_get_ntlv_list_rejects_multiple_value_fields);
    tcase_add_test(testcase, test_json_buffer_get_ntlv_list_rejects_non_container_json_value);
    tcase_add_test(testcase, test_json_buffer_get_uuid_list_rejects_malformed_uuid_entries);
    tcase_add_test(testcase, test_json_buffer_get_uuid_list_rejects_missing_name);
    tcase_add_test(testcase, test_json_buffer_get_uuid_list_rejects_empty_name);
    tcase_add_test(testcase, test_json_buffer_get_string_list_rejects_non_string_entries);
    tcase_add_test(testcase, test_json_buffer_get_uint8_list_rejects_non_integer_entries);
    tcase_add_test(testcase, test_json_buffer_get_uint8_rejects_negative_and_overflow_values);
    tcase_add_test(testcase, test_json_buffer_get_uint16_rejects_negative_and_overflow_values);
    tcase_add_test(testcase, test_json_buffer_get_uint32_rejects_negative_and_overflow_values);

    suite_add_tcase(suite, testcase);
    return suite;
}

int
main(void)
{
    Suite *suite;
    SRunner *runner;
    int failed;

    suite = json_buffer_invalid_suite();
    runner = srunner_create(suite);
    srunner_set_fork_status(runner, CK_NOFORK);
    srunner_run_all(runner, CK_NORMAL);
    failed = srunner_ntests_failed(runner);
    srunner_free(runner);

    return (failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
