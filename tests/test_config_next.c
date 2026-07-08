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

#include <razorback/config_next.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static char *
write_temp_yaml(const char *contents)
{
    char template[] = "/tmp/rzb-config-next-XXXXXX";
    int fd;
    FILE *file;
    char *path;

    fd = mkstemp(template);
    ck_assert_int_ge(fd, 0);
    file = fdopen(fd, "wb");
    ck_assert_ptr_ne(file, NULL);
    ck_assert_int_ge(fputs(contents, file), 0);
    ck_assert_int_eq(fclose(file), 0);
    path = strdup(template);
    ck_assert_ptr_ne(path, NULL);
    return path;
}

static void
remove_temp_yaml(char *path)
{
    if (path == NULL)
        return;
    unlink(path);
    free(path);
}

START_TEST(test_config_next_applies_base_local_and_env_precedence)
{
    int maxInlineBytes = 0;
    char *host = NULL;
    bool tls = true;
    char *baseFile;
    char *localFile;
    struct RzbNextConfigKey keys[] = {
        { "message.max_inline_bytes", RZB_NEXT_CONFIG_INT, &maxInlineBytes },
        { "rabbitmq.host", RZB_NEXT_CONFIG_STRING, &host },
        { "rabbitmq.tls", RZB_NEXT_CONFIG_BOOL, &tls },
        { NULL, 0, NULL }
    };

    baseFile = write_temp_yaml(
        "message:\n"
        "  max_inline_bytes: 1024\n"
        "rabbitmq:\n"
        "  host: base-rabbitmq\n"
        "  tls: false\n"
    );
    localFile = write_temp_yaml(
        "rabbitmq:\n"
        "  host: local-rabbitmq\n"
    );
    ck_assert_int_eq(setenv("RZB_TESTCFG_MESSAGE__MAX_INLINE_BYTES",
                            "2048", 1), 0);
    ck_assert(RzbNextConfig_Load(baseFile, localFile, "RZB_TESTCFG", keys));
    ck_assert_int_eq(maxInlineBytes, 2048);
    ck_assert_str_eq(host, "local-rabbitmq");
    ck_assert(!tls);

    unsetenv("RZB_TESTCFG_MESSAGE__MAX_INLINE_BYTES");
    free(host);
    remove_temp_yaml(baseFile);
    remove_temp_yaml(localFile);
}
END_TEST

START_TEST(test_config_next_rejects_unknown_yaml_keys)
{
    int maxInlineBytes = 0;
    char *baseFile;
    struct RzbNextConfigKey keys[] = {
        { "message.max_inline_bytes", RZB_NEXT_CONFIG_INT, &maxInlineBytes },
        { NULL, 0, NULL }
    };

    baseFile = write_temp_yaml(
        "message:\n"
        "  max_inline_bytes: 1024\n"
        "  unknown: true\n"
    );
    ck_assert(!RzbNextConfig_Load(baseFile, NULL, "RZB_TESTCFG", keys));
    remove_temp_yaml(baseFile);
}
END_TEST

START_TEST(test_config_next_rejects_unknown_owned_env_overrides)
{
    int maxInlineBytes = 0;
    char *baseFile;
    struct RzbNextConfigKey keys[] = {
        { "message.max_inline_bytes", RZB_NEXT_CONFIG_INT, &maxInlineBytes },
        { NULL, 0, NULL }
    };

    baseFile = write_temp_yaml(
        "message:\n"
        "  max_inline_bytes: 1024\n"
    );
    ck_assert_int_eq(setenv("RZB_TESTCFG_MESSAGE__UNKNOWN", "true", 1), 0);
    ck_assert(!RzbNextConfig_Load(baseFile, NULL, "RZB_TESTCFG", keys));
    unsetenv("RZB_TESTCFG_MESSAGE__UNKNOWN");
    remove_temp_yaml(baseFile);
}
END_TEST

START_TEST(test_config_next_ignores_unowned_env_overrides)
{
    int maxInlineBytes = 0;
    char *baseFile;
    struct RzbNextConfigKey keys[] = {
        { "message.max_inline_bytes", RZB_NEXT_CONFIG_INT, &maxInlineBytes },
        { NULL, 0, NULL }
    };

    baseFile = write_temp_yaml(
        "message:\n"
        "  max_inline_bytes: 1024\n"
    );
    ck_assert_int_eq(setenv("RZB_TESTCFG_OTHER__UNKNOWN", "true", 1), 0);
    ck_assert(RzbNextConfig_Load(baseFile, NULL, "RZB_TESTCFG", keys));
    ck_assert_int_eq(maxInlineBytes, 1024);
    unsetenv("RZB_TESTCFG_OTHER__UNKNOWN");
    remove_temp_yaml(baseFile);
}
END_TEST

static Suite *
config_next_suite(void)
{
    Suite *suite;
    TCase *testcase;

    suite = suite_create("config_next");
    testcase = tcase_create("core");
    tcase_add_test(testcase,
                   test_config_next_applies_base_local_and_env_precedence);
    tcase_add_test(testcase, test_config_next_rejects_unknown_yaml_keys);
    tcase_add_test(testcase,
                   test_config_next_rejects_unknown_owned_env_overrides);
    tcase_add_test(testcase, test_config_next_ignores_unowned_env_overrides);
    suite_add_tcase(suite, testcase);
    return suite;
}

int
main(void)
{
    Suite *suite;
    SRunner *runner;
    int failed;

    suite = config_next_suite();
    runner = srunner_create(suite);
    srunner_run_all(runner, CK_NORMAL);
    failed = srunner_ntests_failed(runner);
    srunner_free(runner);
    return failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
