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

#include <stdlib.h>
#include <string.h>

static struct Hash *
create_final_hash(uint32_t type, uint8_t *data, uint32_t length)
{
    struct Hash *hash;

    hash = Hash_Create_Type(type);
    ck_assert_ptr_ne(hash, NULL);
    ck_assert(Hash_Update(hash, data, length));
    ck_assert(Hash_Finalize(hash));
    return hash;
}

START_TEST(test_hash_sha256_known_vector)
{
    static const char expected[] =
        "ba7816bf8f01cfea414140de5dae2223"
        "b00361a396177a9cb410ff61f20015ad";
    uint8_t payload[] = { 'a', 'b', 'c' };
    struct Hash *hash;
    char *text;

    hash = create_final_hash(HASH_TYPE_SHA256, payload, sizeof(payload));
    ck_assert_uint_eq(Hash_DigestLength(hash), 32U);
    ck_assert_uint_eq(Hash_StringLength(hash), sizeof(expected));

    text = Hash_ToText(hash);
    ck_assert_ptr_ne(text, NULL);
    ck_assert_str_eq(text, expected);

    free(text);
    Hash_Destroy(hash);
}
END_TEST

START_TEST(test_hash_create_from_string_matches_finalized_hash)
{
    static const char expected[] =
        "ba7816bf8f01cfea414140de5dae2223"
        "b00361a396177a9cb410ff61f20015ad";
    uint8_t payload[] = { 'a', 'b', 'c' };
    struct Hash *computed;
    struct Hash *parsed;

    computed = create_final_hash(HASH_TYPE_SHA256, payload, sizeof(payload));
    parsed = Hash_Create_From_String(HASH_TYPE_SHA256, expected);

    ck_assert_ptr_ne(parsed, NULL);
    ck_assert(Hash_IsEqual(computed, parsed));

    Hash_Destroy(parsed);
    Hash_Destroy(computed);
}
END_TEST

START_TEST(test_hash_clone_copies_finalized_hash)
{
    uint8_t payload[] = { 'c', 'l', 'o', 'n', 'e' };
    struct Hash *hash;
    struct Hash *clone;

    hash = create_final_hash(HASH_TYPE_SHA1, payload, sizeof(payload));
    clone = Hash_Clone(hash);

    ck_assert_ptr_ne(clone, NULL);
    ck_assert_ptr_ne(clone, hash);
    ck_assert_ptr_ne(clone->pData, hash->pData);
    ck_assert(Hash_IsEqual(hash, clone));

    Hash_Destroy(clone);
    Hash_Destroy(hash);
}
END_TEST

START_TEST(test_hash_clone_rejects_non_finalized_hash)
{
    struct Hash *hash;

    hash = Hash_Create_Type(HASH_TYPE_SHA256);
    ck_assert_ptr_ne(hash, NULL);
    ck_assert_ptr_eq(Hash_Clone(hash), NULL);

    Hash_Destroy(hash);
}
END_TEST

static Suite *
hash_suite(void)
{
    Suite *suite;
    TCase *testcase;

    suite = suite_create("hash");
    testcase = tcase_create("core");

    tcase_add_test(testcase, test_hash_sha256_known_vector);
    tcase_add_test(testcase, test_hash_create_from_string_matches_finalized_hash);
    tcase_add_test(testcase, test_hash_clone_copies_finalized_hash);
    tcase_add_test(testcase, test_hash_clone_rejects_non_finalized_hash);

    suite_add_tcase(suite, testcase);
    return suite;
}

int
main(void)
{
    Suite *suite;
    SRunner *runner;
    int failed;

    suite = hash_suite();
    runner = srunner_create(suite);
    srunner_set_fork_status(runner, CK_NOFORK);
    srunner_run_all(runner, CK_NORMAL);
    failed = srunner_ntests_failed(runner);
    srunner_free(runner);

    return (failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
