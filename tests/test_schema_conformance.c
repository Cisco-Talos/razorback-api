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

#include <stdio.h>
#include <stdlib.h>

START_TEST(test_shared_schema_fixture_conformance_passes)
{
    char command[4096];
    int status;

    snprintf(command, sizeof(command),
             "cd '%s' && python3 tools/schema_conformance.py",
             RAZORBACK_REPO_ROOT);
    status = system(command);
    ck_assert_int_eq(status, 0);
}
END_TEST

static Suite *
schema_conformance_suite(void)
{
    Suite *suite;
    TCase *testcase;

    suite = suite_create("schema_conformance");
    testcase = tcase_create("core");
    tcase_add_test(testcase, test_shared_schema_fixture_conformance_passes);
    suite_add_tcase(suite, testcase);
    return suite;
}

int
main(void)
{
    Suite *suite;
    SRunner *runner;
    int failed;

    suite = schema_conformance_suite();
    runner = srunner_create(suite);
    srunner_run_all(runner, CK_NORMAL);
    failed = srunner_ntests_failed(runner);
    srunner_free(runner);
    return failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
