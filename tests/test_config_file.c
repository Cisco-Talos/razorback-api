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

#include <razorback/config_file.h>
#include <razorback/uuids.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <uuid/uuid.h>

static bool parseOption(const char *string, conf_int_t *val);

static RZBConfCallBack parseCallback = {
    &parseOption
};

static char **strings;
static conf_int_t stringsCount = 0;
static conf_int_t *ints;
static conf_int_t intsCount = 0;
static conf_int_t *parsedStrings;
static conf_int_t parsedStringsCount = 0;
static uuid_t *uuids;
static conf_int_t uuidsCount = 0;
static bool *bools;
static conf_int_t boolsCount = 0;

static struct ConfArray stringArray = {
    RZB_CONF_KEY_TYPE_STRING,
    (void **)&strings,
    &stringsCount,
    NULL
};

static struct ConfArray intArray = {
    RZB_CONF_KEY_TYPE_INT,
    (void **)&ints,
    &intsCount,
    NULL
};

static struct ConfArray parsedStringArray = {
    RZB_CONF_KEY_TYPE_PARSED_STRING,
    (void **)&parsedStrings,
    &parsedStringsCount,
    parseOption
};

static struct ConfArray uuidArray = {
    RZB_CONF_KEY_TYPE_UUID,
    (void **)&uuids,
    &uuidsCount,
    NULL
};

static struct ConfArray boolArray = {
    RZB_CONF_KEY_TYPE_BOOL,
    (void **)&bools,
    &boolsCount,
    NULL
};

struct item {
    conf_int_t i;
    bool b;
    char *string;
    conf_int_t parsed;
    uuid_t uuid;
} __attribute__((__packed__));

static struct item *items = NULL;
static conf_int_t itemCount = 0;

static RZBConfKey_t listItems[] =
{
    {"Int", RZB_CONF_KEY_TYPE_INT, NULL, NULL},
    {"Bool", RZB_CONF_KEY_TYPE_BOOL, NULL, NULL},
    {"String", RZB_CONF_KEY_TYPE_STRING, NULL, NULL},
    {"ParsedString", RZB_CONF_KEY_TYPE_PARSED_STRING, NULL, &parseCallback},
    {"UUID", RZB_CONF_KEY_TYPE_UUID, NULL, NULL},
    {NULL, RZB_CONF_KEY_TYPE_END, NULL, NULL}
};

static struct ConfList list = {
    (void **)&items,
    &itemCount,
    listItems
};

static conf_int_t testInt = 0;
static bool testBool = false;
static bool testBool2 = false;
static char *testString = NULL;
static conf_int_t testParsedString1 = 0;
static conf_int_t testParsedString2 = 0;
static uuid_t testUuid;

static RZBConfKey_t global_config[] = {
    {"Int", RZB_CONF_KEY_TYPE_INT, &testInt, NULL},
    {"Bool", RZB_CONF_KEY_TYPE_BOOL, &testBool, NULL},
    {"Bool2", RZB_CONF_KEY_TYPE_BOOL, &testBool2, NULL},
    {"String", RZB_CONF_KEY_TYPE_STRING, &testString, NULL},
    {"UUID", RZB_CONF_KEY_TYPE_UUID, &testUuid, NULL},
    {"ParsedString", RZB_CONF_KEY_TYPE_PARSED_STRING, &testParsedString1, &parseCallback},
    {"ParsedString2", RZB_CONF_KEY_TYPE_PARSED_STRING, &testParsedString2, &parseCallback},
    {"StringArray", RZB_CONF_KEY_TYPE_ARRAY, NULL, &stringArray},
    {"IntArray", RZB_CONF_KEY_TYPE_ARRAY, NULL, &intArray},
    {"ParsedArray", RZB_CONF_KEY_TYPE_ARRAY, NULL, &parsedStringArray},
    {"UuidArray", RZB_CONF_KEY_TYPE_ARRAY, NULL, &uuidArray},
    {"BoolArray", RZB_CONF_KEY_TYPE_ARRAY, NULL, &boolArray},
    {"List", RZB_CONF_KEY_TYPE_LIST, NULL, &list},
    {NULL, RZB_CONF_KEY_TYPE_END, NULL, NULL}
};

static bool
parseOption(const char *string, conf_int_t *val)
{
    if (!strncasecmp(string, "Option1", 7)) {
        *val = 1;
        return true;
    }
    if (!strncasecmp(string, "Option2", 7)) {
        *val = 2;
        return true;
    }
    return false;
}

static void
reset_config_state(void)
{
    strings = NULL;
    stringsCount = 0;
    ints = NULL;
    intsCount = 0;
    parsedStrings = NULL;
    parsedStringsCount = 0;
    uuids = NULL;
    uuidsCount = 0;
    bools = NULL;
    boolsCount = 0;
    items = NULL;
    itemCount = 0;
    testInt = 0;
    testBool = false;
    testBool2 = false;
    testString = NULL;
    testParsedString1 = 0;
    testParsedString2 = 0;
    uuid_clear(testUuid);
}

static void
assert_uuid_text(const uuid_t uuid, const char *expected)
{
    uuid_t expected_uuid;

    ck_assert_int_eq(uuid_parse(expected, expected_uuid), 0);
    ck_assert_int_eq(uuid_compare(uuid, expected_uuid), 0);
}

struct TempConfigFile {
    char *dir;
    char *path;
    const char *name;
};

static struct TempConfigFile
write_temp_config(const char *contents)
{
    struct TempConfigFile temp = { NULL, NULL, "temp.conf" };
    char dir_template[] = "/tmp/razorback-config-XXXXXX";
    FILE *stream;

    ck_assert_ptr_ne(contents, NULL);
    temp.dir = strdup(mkdtemp(dir_template));
    ck_assert_ptr_ne(temp.dir, NULL);
    ck_assert_int_ne(asprintf(&temp.path, "%s/%s", temp.dir, temp.name), -1);

    stream = fopen(temp.path, "w");
    ck_assert_ptr_ne(stream, NULL);
    ck_assert_int_ge(fputs(contents, stream), 0);
    ck_assert_int_eq(fclose(stream), 0);

    return temp;
}

static void
cleanup_temp_config(struct TempConfigFile *temp)
{
    ck_assert_ptr_ne(temp, NULL);

    if (temp->path != NULL) {
        ck_assert_int_eq(unlink(temp->path), 0);
        free(temp->path);
        temp->path = NULL;
    }
    if (temp->dir != NULL) {
        ck_assert_int_eq(rmdir(temp->dir), 0);
        free(temp->dir);
        temp->dir = NULL;
    }
}

static void
cleanup_config_test_state(void)
{
    rzbConfCleanUp();
    reset_config_state();
}

static void
clear_env_override(const char *name)
{
    ck_assert_ptr_ne(name, NULL);
    ck_assert_int_eq(unsetenv(name), 0);
}

START_TEST(test_get_config_file_joins_directory_and_filename)
{
    char *path;

    path = getConfigFile("/tmp/razorback-tests", "sample.conf");
    ck_assert_ptr_ne(path, NULL);
    ck_assert_str_eq(path, "/tmp/razorback-tests/sample.conf");
    free(path);
}
END_TEST

START_TEST(test_read_my_config_populates_scalars_arrays_and_lists)
{
    reset_config_state();

    ck_assert(readMyConfig(RAZORBACK_TESTS_DIR, "test.conf", global_config));

    ck_assert_int_eq(testInt, 1);
    ck_assert(testBool);
    ck_assert(!testBool2);
    ck_assert_str_eq(testString, "Test");
    assert_uuid_text(testUuid, "4d014d0b-f6d4-4a52-96a5-01ad64f5088b");
    ck_assert_int_eq(testParsedString1, 1);
    ck_assert_int_eq(testParsedString2, 2);

    ck_assert_int_eq(stringsCount, 2);
    ck_assert_str_eq(strings[0], "String1");
    ck_assert_str_eq(strings[1], "String2");

    ck_assert_int_eq(intsCount, 5);
    ck_assert_int_eq(ints[0], 1);
    ck_assert_int_eq(ints[1], 2);
    ck_assert_int_eq(ints[2], 3);
    ck_assert_int_eq(ints[3], 4);
    ck_assert_int_eq(ints[4], 5);

    ck_assert_int_eq(parsedStringsCount, 3);
    ck_assert_int_eq(parsedStrings[0], 1);
    ck_assert_int_eq(parsedStrings[1], 2);
    ck_assert_int_eq(parsedStrings[2], 1);

    ck_assert_int_eq(uuidsCount, 3);
    assert_uuid_text(uuids[0], "dafd01fe-9e70-41d4-99c1-835860d28828");
    assert_uuid_text(uuids[1], "6a7bac43-7950-46f7-9561-27335367981b");
    assert_uuid_text(uuids[2], "5e83627a-553b-485c-85a3-5c21e6df688e");

    ck_assert_int_eq(boolsCount, 3);
    ck_assert(bools[0]);
    ck_assert(!bools[1]);
    ck_assert(bools[2]);

    ck_assert_int_eq(itemCount, 2);
    ck_assert_int_eq(items[0].i, 1);
    ck_assert(items[0].b);
    ck_assert_str_eq(items[0].string, "String");
    ck_assert_int_eq(items[0].parsed, 1);
    assert_uuid_text(items[0].uuid, "b09fda11-ac46-49fd-bb27-85f748700f77");

    ck_assert_int_eq(items[1].i, 2);
    ck_assert(!items[1].b);
    ck_assert_str_eq(items[1].string, "String1");
    ck_assert_int_eq(items[1].parsed, 2);
    assert_uuid_text(items[1].uuid, "3e7a02f4-efc4-46f6-9844-717836ebe05b");

    cleanup_config_test_state();
}
END_TEST

START_TEST(test_read_my_config_rejects_invalid_bool_value)
{
    struct TempConfigFile temp;
    bool value = false;
    RZBConfKey_t config[] = {
        {"Bool", RZB_CONF_KEY_TYPE_BOOL, &value, NULL},
        {NULL, RZB_CONF_KEY_TYPE_END, NULL, NULL}
    };

    reset_config_state();
    temp = write_temp_config("Bool=\"maybe\";\n");

    ck_assert(!readMyConfig(temp.dir, temp.name, config));

    cleanup_config_test_state();
    cleanup_temp_config(&temp);
}
END_TEST

START_TEST(test_read_my_config_rejects_invalid_uuid_value)
{
    struct TempConfigFile temp;
    uuid_t value;
    RZBConfKey_t config[] = {
        {"UUID", RZB_CONF_KEY_TYPE_UUID, &value, NULL},
        {NULL, RZB_CONF_KEY_TYPE_END, NULL, NULL}
    };

    reset_config_state();
    uuid_clear(value);
    temp = write_temp_config("UUID=\"not-a-uuid\";\n");

    ck_assert(!readMyConfig(temp.dir, temp.name, config));

    cleanup_config_test_state();
    cleanup_temp_config(&temp);
}
END_TEST

START_TEST(test_read_my_config_rejects_invalid_parsed_string_value)
{
    struct TempConfigFile temp;
    conf_int_t parsed = 0;
    RZBConfKey_t config[] = {
        {"ParsedString", RZB_CONF_KEY_TYPE_PARSED_STRING, &parsed, &parseCallback},
        {NULL, RZB_CONF_KEY_TYPE_END, NULL, NULL}
    };

    reset_config_state();
    temp = write_temp_config("ParsedString=\"InvalidOption\";\n");

    ck_assert(!readMyConfig(temp.dir, temp.name, config));

    cleanup_config_test_state();
    cleanup_temp_config(&temp);
}
END_TEST

START_TEST(test_read_my_config_rejects_non_array_for_array_binding)
{
    struct TempConfigFile temp;
    char **local_strings = NULL;
    conf_int_t local_count = 0;
    struct ConfArray local_array = {
        RZB_CONF_KEY_TYPE_STRING,
        (void **)&local_strings,
        &local_count,
        NULL
    };
    RZBConfKey_t config[] = {
        {"StringArray", RZB_CONF_KEY_TYPE_ARRAY, NULL, &local_array},
        {NULL, RZB_CONF_KEY_TYPE_END, NULL, NULL}
    };

    reset_config_state();
    temp = write_temp_config("StringArray=\"not-an-array\";\n");

    ck_assert(!readMyConfig(temp.dir, temp.name, config));

    cleanup_config_test_state();
    cleanup_temp_config(&temp);
}
END_TEST

START_TEST(test_read_my_config_environment_overrides_nested_scalar_values)
{
    struct TempConfigFile temp;
    conf_int_t value = 0;
    bool enabled = false;
    char *string_value = NULL;
    conf_int_t parsed = 0;
    uuid_t parsed_uuid;
    RZBConfKey_t config[] = {
        {"Parent.Int", RZB_CONF_KEY_TYPE_INT, &value, NULL},
        {"Parent.Bool", RZB_CONF_KEY_TYPE_BOOL, &enabled, NULL},
        {"Parent.String", RZB_CONF_KEY_TYPE_STRING, &string_value, NULL},
        {"Parent.Parsed", RZB_CONF_KEY_TYPE_PARSED_STRING, &parsed, &parseCallback},
        {"Parent.UUID", RZB_CONF_KEY_TYPE_UUID, &parsed_uuid, NULL},
        {NULL, RZB_CONF_KEY_TYPE_END, NULL, NULL}
    };

    reset_config_state();
    uuid_clear(parsed_uuid);
    temp = write_temp_config(
        "Parent: {\n"
        "  Int = 1;\n"
        "  Bool = \"false\";\n"
        "  String = \"FileValue\";\n"
        "  Parsed = \"Option1\";\n"
        "  UUID = \"4d014d0b-f6d4-4a52-96a5-01ad64f5088b\";\n"
        "};\n");

    ck_assert_int_eq(setenv("RZB_Parent__Int", "42", 1), 0);
    ck_assert_int_eq(setenv("RZB_Parent__Bool", "true", 1), 0);
    ck_assert_int_eq(setenv("RZB_Parent__String", "EnvValue", 1), 0);
    ck_assert_int_eq(setenv("RZB_Parent__Parsed", "Option2", 1), 0);
    ck_assert_int_eq(setenv("RZB_Parent__UUID", "3e7a02f4-efc4-46f6-9844-717836ebe05b", 1), 0);

    ck_assert(readMyConfig(temp.dir, temp.name, config));

    ck_assert_int_eq(value, 42);
    ck_assert(enabled);
    ck_assert_str_eq(string_value, "EnvValue");
    ck_assert_int_eq(parsed, 2);
    assert_uuid_text(parsed_uuid, "3e7a02f4-efc4-46f6-9844-717836ebe05b");

    clear_env_override("RZB_Parent__Int");
    clear_env_override("RZB_Parent__Bool");
    clear_env_override("RZB_Parent__String");
    clear_env_override("RZB_Parent__Parsed");
    clear_env_override("RZB_Parent__UUID");
    cleanup_config_test_state();
    cleanup_temp_config(&temp);
}
END_TEST

START_TEST(test_read_my_config_accepts_environment_only_nested_scalar_values)
{
    char dir_template[] = "/tmp/razorback-config-XXXXXX";
    char *dir;
    conf_int_t value = 0;
    RZBConfKey_t config[] = {
        {"Parent.Int", RZB_CONF_KEY_TYPE_INT, &value, NULL},
        {NULL, RZB_CONF_KEY_TYPE_END, NULL, NULL}
    };

    reset_config_state();
    dir = strdup(mkdtemp(dir_template));
    ck_assert_ptr_ne(dir, NULL);

    ck_assert_int_eq(setenv("RZB_Parent__Int", "99", 1), 0);
    ck_assert(readMyConfig(dir, "missing.conf", config));
    ck_assert_int_eq(value, 99);

    clear_env_override("RZB_Parent__Int");
    cleanup_config_test_state();
    ck_assert_int_eq(rmdir(dir), 0);
    free(dir);
}
END_TEST

static Suite *
config_file_suite(void)
{
    Suite *suite;
    TCase *testcase;

    suite = suite_create("config_file");
    testcase = tcase_create("core");

    tcase_add_test(testcase, test_get_config_file_joins_directory_and_filename);
    tcase_add_test(testcase, test_read_my_config_populates_scalars_arrays_and_lists);
    tcase_add_test(testcase, test_read_my_config_rejects_invalid_bool_value);
    tcase_add_test(testcase, test_read_my_config_rejects_invalid_uuid_value);
    tcase_add_test(testcase, test_read_my_config_rejects_invalid_parsed_string_value);
    tcase_add_test(testcase, test_read_my_config_rejects_non_array_for_array_binding);
    tcase_add_test(testcase, test_read_my_config_environment_overrides_nested_scalar_values);
    tcase_add_test(testcase, test_read_my_config_accepts_environment_only_nested_scalar_values);

    suite_add_tcase(suite, testcase);
    return suite;
}

int
main(void)
{
    Suite *suite;
    SRunner *runner;
    int failed;

    suite = config_file_suite();
    runner = srunner_create(suite);
    srunner_set_fork_status(runner, CK_NOFORK);
    srunner_run_all(runner, CK_NORMAL);
    failed = srunner_ntests_failed(runner);
    srunner_free(runner);

    return (failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
