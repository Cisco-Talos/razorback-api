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

#include <razorback/hash.h>
#include <razorback/json_buffer.h>

#include "test_json_buffer_support.h"

#include <json.h>
#include <stdlib.h>
#include <string.h>
#include <uuid/uuid.h>

static void
assert_json_string_matches(const char *actual, const char *expected)
{
    json_object *actual_object;
    json_object *expected_object;

    ck_assert_ptr_ne(actual, NULL);
    ck_assert_ptr_ne(expected, NULL);

    actual_object = json_tokener_parse(actual);
    expected_object = json_tokener_parse(expected);
    ck_assert_ptr_ne(actual_object, NULL);
    ck_assert_ptr_ne(expected_object, NULL);
    ck_assert_str_eq(json_object_to_json_string_ext(actual_object, JSON_C_TO_STRING_PLAIN),
                     json_object_to_json_string_ext(expected_object, JSON_C_TO_STRING_PLAIN));

    json_object_put(expected_object);
    json_object_put(actual_object);
}

static struct Hash *
create_final_hash(uint32_t type, const uint8_t *data, uint32_t length)
{
    struct Hash *hash;

    hash = Hash_Create_Type(type);
    ck_assert_ptr_ne(hash, NULL);
    ck_assert(Hash_Update(hash, (uint8_t *)data, length));
    ck_assert(Hash_Finalize(hash));
    return hash;
}

START_TEST(test_json_buffer_round_trips_scalar_types)
{
    json_object *parent;
    bool booleanValue = false;
    uint8_t uint8Value = 0;
    uint16_t uint16Value = 0;
    uint32_t uint32Value = 0;
    uint64_t uint64Value = 0;
    char *stringValue;

    parent = json_buffer_test_parent_object();

    ck_assert(JsonBuffer_Put_bool(parent, "flag", true));
    ck_assert(JsonBuffer_Put_uint8_t(parent, "u8", 255U));
    ck_assert(JsonBuffer_Put_uint16_t(parent, "u16", 65535U));
    ck_assert(JsonBuffer_Put_uint32_t(parent, "u32", 305419896U));
    ck_assert(JsonBuffer_Put_uint64_t(parent, "u64", 9876543210ULL));
    ck_assert(JsonBuffer_Put_String(parent, "text", "payload"));

    ck_assert(JsonBuffer_Get_bool(parent, "flag", &booleanValue));
    ck_assert(booleanValue);
    ck_assert(JsonBuffer_Get_uint8_t(parent, "u8", &uint8Value));
    ck_assert_uint_eq(uint8Value, 255U);
    ck_assert(JsonBuffer_Get_uint16_t(parent, "u16", &uint16Value));
    ck_assert_uint_eq(uint16Value, 65535U);
    ck_assert(JsonBuffer_Get_uint32_t(parent, "u32", &uint32Value));
    ck_assert_uint_eq(uint32Value, 305419896U);
    ck_assert(JsonBuffer_Get_uint64_t(parent, "u64", &uint64Value));
    ck_assert_uint_eq(uint64Value, 9876543210ULL);

    stringValue = JsonBuffer_Get_String(parent, "text");
    ck_assert_ptr_ne(stringValue, NULL);
    ck_assert_str_eq(stringValue, "payload");

    free(stringValue);
    json_object_put(parent);
}
END_TEST

START_TEST(test_json_buffer_get_bool_accepts_string_values)
{
    json_object *parent;
    bool booleanValue = true;

    parent = json_buffer_test_parent_object();
    json_object_object_add(parent, "flag", json_object_new_string("false"));

    ck_assert(JsonBuffer_Get_bool(parent, "flag", &booleanValue));
    ck_assert(!booleanValue);

    json_object_put(parent);
}
END_TEST

START_TEST(test_json_buffer_get_uint64_accepts_decimal_strings)
{
    json_object *parent;
    uint64_t value = 0;

    parent = json_buffer_test_parent_object();
    json_object_object_add(parent, "count", json_object_new_string("4242424242"));

    ck_assert(JsonBuffer_Get_uint64_t(parent, "count", &value));
    ck_assert_uint_eq(value, 4242424242ULL);

    json_object_put(parent);
}
END_TEST

START_TEST(test_json_buffer_round_trips_uuid)
{
    json_object *parent;
    uuid_t expected;
    uuid_t actual;

    ck_assert_int_eq(uuid_parse("00112233-4455-6677-8899-aabbccddeeff", expected), 0);
    parent = json_buffer_test_parent_object();

    ck_assert(JsonBuffer_Put_UUID(parent, "uuid", expected));
    json_buffer_assert_field_matches_schema(parent, "uuid", "uuid.schema.json");
    ck_assert(JsonBuffer_Get_UUID(parent, "uuid", actual));
    ck_assert_int_eq(uuid_compare(expected, actual), 0);

    json_object_put(parent);
}
END_TEST

START_TEST(test_json_buffer_round_trips_byte_arrays)
{
    json_object *parent;
    static const uint8_t bytes[] = { 0x01, 0x02, 0x03, 0xff, 0x0a };
    uint8_t *decoded = NULL;
    uint32_t decodedSize = 0;

    parent = json_buffer_test_parent_object();

    ck_assert(JsonBuffer_Put_ByteArray(parent, "bytes", sizeof(bytes), bytes));
    ck_assert(JsonBuffer_Get_ByteArray(parent, "bytes", &decodedSize, &decoded));
    ck_assert_uint_eq(decodedSize, sizeof(bytes));
    ck_assert_ptr_ne(decoded, NULL);
    ck_assert_mem_eq(decoded, bytes, sizeof(bytes));

    free(decoded);
    json_object_put(parent);
}
END_TEST

START_TEST(test_json_buffer_round_trips_empty_byte_arrays)
{
    json_object *parent;
    static const uint8_t empty[] = { 0x00 };
    uint8_t *decoded = NULL;
    uint32_t decodedSize = 123U;

    parent = json_buffer_test_parent_object();

    ck_assert(JsonBuffer_Put_ByteArray(parent, "bytes", 0U, empty));
    ck_assert(JsonBuffer_Get_ByteArray(parent, "bytes", &decodedSize, &decoded));
    ck_assert_uint_eq(decodedSize, 0U);
    ck_assert_ptr_ne(decoded, NULL);

    free(decoded);
    json_object_put(parent);
}
END_TEST

START_TEST(test_json_buffer_round_trips_hashes)
{
    json_object *parent;
    static const uint8_t payload[] = { 'a', 'b', 'c' };
    struct Hash *expected;
    struct Hash *actual = NULL;

    expected = create_final_hash(HASH_TYPE_SHA256, payload, sizeof(payload));
    parent = json_buffer_test_parent_object();

    ck_assert(JsonBuffer_Put_Hash(parent, "hash", expected));
    json_buffer_assert_field_matches_schema(parent, "hash", "hash.schema.json");
    ck_assert(JsonBuffer_Get_Hash(parent, "hash", &actual));
    ck_assert_ptr_ne(actual, NULL);
    ck_assert(Hash_IsEqual(expected, actual));

    Hash_Destroy(actual);
    Hash_Destroy(expected);
    json_object_put(parent);
}
END_TEST

START_TEST(test_json_buffer_round_trips_opaque_json_object_strings)
{
    json_object *parent;
    char *json_string;

    parent = json_buffer_test_parent_object();

    ck_assert(JsonBuffer_Put_JsonString(parent, "opaque",
                                        "{\"message\":\"payload\",\"count\":2}"));
    json_string = JsonBuffer_Get_JsonString(parent, "opaque");
    assert_json_string_matches(json_string, "{\"message\":\"payload\",\"count\":2}");

    free(json_string);
    json_object_put(parent);
}
END_TEST

START_TEST(test_json_buffer_round_trips_opaque_json_array_strings)
{
    json_object *parent;
    char *json_string;

    parent = json_buffer_test_parent_object();

    ck_assert(JsonBuffer_Put_JsonString(parent, "opaque",
                                        "[{\"message\":\"payload\"},2,true]"));
    json_string = JsonBuffer_Get_JsonString(parent, "opaque");
    assert_json_string_matches(json_string, "[{\"message\":\"payload\"},2,true]");

    free(json_string);
    json_object_put(parent);
}
END_TEST

START_TEST(test_json_buffer_rejects_invalid_opaque_json_strings)
{
    json_object *parent;

    parent = json_buffer_test_parent_object();

    ck_assert(!JsonBuffer_Put_JsonString(parent, "opaque", "{invalid"));
    ck_assert(!JsonBuffer_Put_JsonString(parent, "opaque", "\"string\""));

    json_object_put(parent);
}
END_TEST

START_TEST(test_json_buffer_rejects_non_container_opaque_json_fields)
{
    json_object *parent;

    parent = json_buffer_test_parent_object();
    ck_assert(JsonBuffer_Put_String(parent, "opaque", "payload"));
    ck_assert_ptr_eq(JsonBuffer_Get_JsonString(parent, "opaque"), NULL);

    json_object_put(parent);
}
END_TEST

START_TEST(test_json_buffer_rejects_type_mismatches)
{
    json_object *parent;
    uint8_t value = 0;
    uint8_t *bytes = NULL;
    uint32_t size = 0;

    parent = json_buffer_test_parent_object();
    json_object_object_add(parent, "u8", json_object_new_string("not-an-int"));
    json_object_object_add(parent, "bytes", json_object_new_int(7));

    ck_assert(!JsonBuffer_Get_uint8_t(parent, "u8", &value));
    ck_assert(!JsonBuffer_Get_ByteArray(parent, "bytes", &size, &bytes));

    json_object_put(parent);
}
END_TEST

static Suite *
json_buffer_primitives_suite(void)
{
    Suite *suite;
    TCase *testcase;

    suite = suite_create("json_buffer_primitives");
    testcase = tcase_create("core");

    tcase_add_test(testcase, test_json_buffer_round_trips_scalar_types);
    tcase_add_test(testcase, test_json_buffer_get_bool_accepts_string_values);
    tcase_add_test(testcase, test_json_buffer_get_uint64_accepts_decimal_strings);
    tcase_add_test(testcase, test_json_buffer_round_trips_uuid);
    tcase_add_test(testcase, test_json_buffer_round_trips_byte_arrays);
    tcase_add_test(testcase, test_json_buffer_round_trips_empty_byte_arrays);
    tcase_add_test(testcase, test_json_buffer_round_trips_hashes);
    tcase_add_test(testcase, test_json_buffer_round_trips_opaque_json_object_strings);
    tcase_add_test(testcase, test_json_buffer_round_trips_opaque_json_array_strings);
    tcase_add_test(testcase, test_json_buffer_rejects_invalid_opaque_json_strings);
    tcase_add_test(testcase, test_json_buffer_rejects_non_container_opaque_json_fields);
    tcase_add_test(testcase, test_json_buffer_rejects_type_mismatches);

    suite_add_tcase(suite, testcase);
    return suite;
}

int
main(void)
{
    Suite *suite;
    SRunner *runner;
    int failed;

    suite = json_buffer_primitives_suite();
    runner = srunner_create(suite);
    srunner_set_fork_status(runner, CK_NOFORK);
    srunner_run_all(runner, CK_NORMAL);
    failed = srunner_ntests_failed(runner);
    srunner_free(runner);

    return (failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
