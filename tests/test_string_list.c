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
#include <razorback/string_list.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

START_TEST(test_string_list_add_duplicates_input_storage)
{
    List_t *list;
    char source[] = "mutable";
    char *stored;

    list = StringList_Create();
    ck_assert_ptr_ne(list, NULL);
    ck_assert(StringList_Add(list, source));

    source[0] = 'X';
    stored = List_Find(list, "mutable");
    ck_assert_ptr_ne(stored, NULL);
    ck_assert_str_eq(stored, "mutable");

    List_Destroy(list);
}
END_TEST

START_TEST(test_string_list_size_includes_strings_and_length_prefix)
{
    List_t *list;
    uint32_t expected;

    list = StringList_Create();
    ck_assert_ptr_ne(list, NULL);

    ck_assert(StringList_Add(list, "alpha"));
    ck_assert(StringList_Add(list, "beta"));

    expected = sizeof(uint32_t) + (uint32_t)strlen("alpha") + 1U +
        (uint32_t)strlen("beta") + 1U;
    ck_assert_uint_eq(StringList_Size(list), expected);

    List_Destroy(list);
}
END_TEST

START_TEST(test_string_list_clone_returns_independent_strings)
{
    List_t *list;
    List_t *clone;
    char *original;
    char *copied;

    list = StringList_Create();
    ck_assert_ptr_ne(list, NULL);
    ck_assert(StringList_Add(list, "alpha"));

    clone = List_Clone(list);
    ck_assert_ptr_ne(clone, NULL);

    original = List_Find(list, "alpha");
    copied = List_Find(clone, "alpha");

    ck_assert_ptr_ne(original, NULL);
    ck_assert_ptr_ne(copied, NULL);
    ck_assert_ptr_ne(original, copied);
    ck_assert_str_eq(copied, "alpha");

    original[0] = 'A';
    ck_assert_str_eq(copied, "alpha");

    List_Destroy(clone);
    List_Destroy(list);
}
END_TEST

START_TEST(test_string_list_remove_uses_string_compare)
{
    List_t *list;
    char key[] = "alpha";

    list = StringList_Create();
    ck_assert_ptr_ne(list, NULL);
    ck_assert(StringList_Add(list, "alpha"));
    ck_assert(StringList_Add(list, "beta"));

    ck_assert(List_Remove(list, key));
    ck_assert_ptr_eq(List_Find(list, "alpha"), NULL);
    ck_assert_ptr_ne(List_Find(list, "beta"), NULL);
    ck_assert_uint_eq(List_Length(list), 1U);

    List_Destroy(list);
}
END_TEST

static Suite *
string_list_suite(void)
{
    Suite *suite;
    TCase *testcase;

    suite = suite_create("string_list");
    testcase = tcase_create("core");

    tcase_add_test(testcase, test_string_list_add_duplicates_input_storage);
    tcase_add_test(testcase, test_string_list_size_includes_strings_and_length_prefix);
    tcase_add_test(testcase, test_string_list_clone_returns_independent_strings);
    tcase_add_test(testcase, test_string_list_remove_uses_string_compare);

    suite_add_tcase(suite, testcase);
    return suite;
}

int
main(void)
{
    Suite *suite;
    SRunner *runner;
    int failed;

    suite = string_list_suite();
    runner = srunner_create(suite);
    srunner_set_fork_status(runner, CK_NOFORK);
    srunner_run_all(runner, CK_NORMAL);
    failed = srunner_ntests_failed(runner);
    srunner_free(runner);

    return (failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
