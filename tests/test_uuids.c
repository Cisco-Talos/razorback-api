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

#include <razorback/list.h>
#include <razorback/uuids.h>

#include "init.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <uuid/uuid.h>

struct UUIDNodeCapture {
    struct UUIDListNode *node;
};

static void
ensure_uuid_tables_initialized(void)
{
    static bool initialized = false;

    if (!initialized) {
        initUuids();
        initialized = true;
    }
}

static int
capture_uuid_node(void *item, void *user_data)
{
    struct UUIDNodeCapture *capture = user_data;

    ck_assert_ptr_eq(capture->node, NULL);
    capture->node = item;
    return LIST_EACH_END;
}

START_TEST(test_uuid_add_list_entry_accepts_optional_description)
{
    List_t *list;
    struct UUIDNodeCapture capture = { NULL };
    uuid_t uuid;

    list = UUID_Create_List();
    ck_assert_ptr_ne(list, NULL);
    ck_assert_int_eq(uuid_parse("00112233-4455-6677-8899-aabbccddeeff", uuid), 0);

    ck_assert(UUID_Add_List_Entry(list, uuid, "alpha", NULL));
    ck_assert_uint_eq(List_Length(list), 1U);
    ck_assert(List_ForEach(list, capture_uuid_node, &capture));
    ck_assert_ptr_ne(capture.node, NULL);
    ck_assert_int_eq(uuid_compare(capture.node->uuid, uuid), 0);
    ck_assert_str_eq(capture.node->sName, "alpha");
    ck_assert_ptr_eq(capture.node->sDescription, NULL);

    List_Destroy(list);
}
END_TEST

START_TEST(test_uuid_list_clone_duplicates_node_storage)
{
    List_t *list;
    List_t *clone;
    struct UUIDNodeCapture original = { NULL };
    struct UUIDNodeCapture copied = { NULL };
    uuid_t uuid;

    list = UUID_Create_List();
    ck_assert_ptr_ne(list, NULL);
    ck_assert_int_eq(uuid_parse("12345678-1234-5678-9abc-def012345678", uuid), 0);
    ck_assert(UUID_Add_List_Entry(list, uuid, "beta", "second"));

    clone = List_Clone(list);
    ck_assert_ptr_ne(clone, NULL);
    ck_assert(List_ForEach(list, capture_uuid_node, &original));
    ck_assert(List_ForEach(clone, capture_uuid_node, &copied));
    ck_assert_ptr_ne(original.node, NULL);
    ck_assert_ptr_ne(copied.node, NULL);
    ck_assert_ptr_ne(original.node, copied.node);
    ck_assert_ptr_ne(original.node->sName, copied.node->sName);
    ck_assert_ptr_ne(original.node->sDescription, copied.node->sDescription);
    ck_assert_str_eq(copied.node->sName, "beta");
    ck_assert_str_eq(copied.node->sDescription, "second");
    ck_assert_int_eq(uuid_compare(copied.node->uuid, uuid), 0);

    List_Destroy(clone);
    List_Destroy(list);
}
END_TEST

START_TEST(test_uuid_lookup_helpers_handle_optional_description)
{
    List_t *list;
    uuid_t uuid;
    char *name;
    char *uuid_text;

    ensure_uuid_tables_initialized();

    list = UUID_Get_List(UUID_TYPE_NUGGET);
    ck_assert_ptr_ne(list, NULL);
    ck_assert_int_eq(uuid_parse("aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee", uuid), 0);
    ck_assert(UUID_Add_List_Entry(list, uuid, "custom-nugget", NULL));

    ck_assert(UUID_Get_UUID("custom-nugget", UUID_TYPE_NUGGET, uuid));
    name = UUID_Get_NameByUUID(uuid, UUID_TYPE_NUGGET);
    ck_assert_ptr_ne(name, NULL);
    ck_assert_str_eq(name, "custom-nugget");
    ck_assert_ptr_eq(UUID_Get_Description("custom-nugget", UUID_TYPE_NUGGET), NULL);
    ck_assert_ptr_eq(UUID_Get_DescriptionByUUID(uuid, UUID_TYPE_NUGGET), NULL);

    uuid_text = UUID_Get_UUIDAsString("custom-nugget", UUID_TYPE_NUGGET);
    ck_assert_ptr_ne(uuid_text, NULL);
    ck_assert_str_eq(uuid_text, "aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee");

    free(uuid_text);
    free(name);
}
END_TEST

START_TEST(test_uuid_builtin_lookup_returns_known_nugget_type)
{
    uuid_t uuid;
    char *name;
    char *description;

    ensure_uuid_tables_initialized();

    ck_assert(UUID_Get_UUID(NUGGET_TYPE_DISPATCHER, UUID_TYPE_NUGGET_TYPE, uuid));
    name = UUID_Get_NameByUUID(uuid, UUID_TYPE_NUGGET_TYPE);
    description = UUID_Get_DescriptionByUUID(uuid, UUID_TYPE_NUGGET_TYPE);

    ck_assert_ptr_ne(name, NULL);
    ck_assert_ptr_ne(description, NULL);
    ck_assert_str_eq(name, NUGGET_TYPE_DISPATCHER);
    ck_assert_str_eq(description, "Message Dispatcher Nugget");

    free(description);
    free(name);
}
END_TEST

START_TEST(test_uuid_binary_size_counts_uuid_bytes_and_name_lengths)
{
    List_t *list;
    uuid_t first;
    uuid_t second;
    size_t expected;

    list = UUID_Create_List();
    ck_assert_ptr_ne(list, NULL);
    ck_assert_int_eq(uuid_parse("00112233-4455-6677-8899-aabbccddeeff", first), 0);
    ck_assert_int_eq(uuid_parse("12345678-1234-5678-9abc-def012345678", second), 0);

    ck_assert(UUID_Add_List_Entry(list, first, "alpha", "first"));
    ck_assert(UUID_Add_List_Entry(list, second, "beta", NULL));

    expected = (2U * 16U) + strlen("alpha") + 1U + strlen("beta") + 1U;
    ck_assert_uint_eq(UUIDList_BinarySize(list), expected);

    List_Destroy(list);
}
END_TEST

START_TEST(test_uuid_lookup_helpers_fail_for_unknown_entries)
{
    uuid_t uuid;

    ensure_uuid_tables_initialized();

    ck_assert(!UUID_Get_UUID("does-not-exist", UUID_TYPE_NUGGET_TYPE, uuid));
    ck_assert_ptr_eq(UUID_Get_NameByUUID(uuid, UUID_TYPE_NUGGET_TYPE), NULL);
    ck_assert_ptr_eq(UUID_Get_Description("does-not-exist", UUID_TYPE_NUGGET_TYPE), NULL);
    ck_assert_ptr_eq(UUID_Get_DescriptionByUUID(uuid, UUID_TYPE_NUGGET_TYPE), NULL);
    ck_assert_ptr_eq(UUID_Get_UUIDAsString("does-not-exist", UUID_TYPE_NUGGET_TYPE), NULL);
}
END_TEST

START_TEST(test_uuid_matches_uuid_reports_expected_result)
{
    uuid_t uuid;
    uuid_t other_uuid;
    bool matches;

    ensure_uuid_tables_initialized();

    ck_assert(UUID_Get_UUID(NUGGET_TYPE_DISPATCHER, UUID_TYPE_NUGGET_TYPE, uuid));
    ck_assert_int_eq(uuid_parse("00112233-4455-6677-8899-aabbccddeeff", other_uuid), 0);
    ck_assert(UUID_Is_Named_UUID(NUGGET_TYPE_DISPATCHER, UUID_TYPE_NUGGET_TYPE,
                                 uuid, &matches));
    ck_assert(matches);
    ck_assert(UUID_Is_Named_UUID(NUGGET_TYPE_DISPATCHER, UUID_TYPE_NUGGET_TYPE,
                                 other_uuid, &matches));
    ck_assert(!matches);
    ck_assert(!UUID_Is_Named_UUID("does-not-exist", UUID_TYPE_NUGGET_TYPE,
                                  uuid, &matches));
}
END_TEST

static Suite *
uuid_suite(void)
{
    Suite *suite;
    TCase *testcase;

    suite = suite_create("uuids");
    testcase = tcase_create("core");

    tcase_add_test(testcase, test_uuid_add_list_entry_accepts_optional_description);
    tcase_add_test(testcase, test_uuid_list_clone_duplicates_node_storage);
    tcase_add_test(testcase, test_uuid_lookup_helpers_handle_optional_description);
    tcase_add_test(testcase, test_uuid_builtin_lookup_returns_known_nugget_type);
    tcase_add_test(testcase, test_uuid_binary_size_counts_uuid_bytes_and_name_lengths);
    tcase_add_test(testcase, test_uuid_lookup_helpers_fail_for_unknown_entries);
    tcase_add_test(testcase, test_uuid_matches_uuid_reports_expected_result);

    suite_add_tcase(suite, testcase);
    return suite;
}

int
main(void)
{
    Suite *suite;
    SRunner *runner;
    int failed;

    suite = uuid_suite();
    runner = srunner_create(suite);
    srunner_set_fork_status(runner, CK_NOFORK);
    srunner_run_all(runner, CK_NORMAL);
    failed = srunner_ntests_failed(runner);
    srunner_free(runner);

    return (failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
