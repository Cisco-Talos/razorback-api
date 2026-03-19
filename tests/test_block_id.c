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

#include <stdlib.h>
#include <string.h>
#include <uuid/uuid.h>

static struct BlockId *
create_block_id(const char *uuidText, uint64_t length, uint32_t hashType,
                const char *hashText)
{
    struct BlockId *blockId;

    blockId = BlockId_Create_Shallow();
    ck_assert_ptr_ne(blockId, NULL);
    ck_assert_int_eq(uuid_parse(uuidText, blockId->uuidDataType), 0);

    blockId->iLength = length;
    blockId->pHash = Hash_Create_From_String(hashType, hashText);
    ck_assert_ptr_ne(blockId->pHash, NULL);

    return blockId;
}

START_TEST(test_block_id_to_text_formats_expected_value)
{
    static const char uuidText[] = "00112233-4455-6677-8899-aabbccddeeff";
    static const char hashText[] =
        "ba7816bf8f01cfea414140de5dae2223"
        "b00361a396177a9cb410ff61f20015ad";
    static const char expected[] =
        "00112233-4455-6677-8899-aabbccddeeff-000004d2-"
        "ba7816bf8f01cfea414140de5dae2223"
        "b00361a396177a9cb410ff61f20015ad";
    struct BlockId *blockId;
    char *text;

    blockId = create_block_id(uuidText, 0x4d2U, HASH_TYPE_SHA256, hashText);
    text = BlockId_ToText(blockId);

    ck_assert_ptr_ne(text, NULL);
    ck_assert_str_eq(text, expected);
    ck_assert_uint_eq(BlockId_StringLength(blockId), strlen(expected) + 1U);

    free(text);
    BlockId_Destroy(blockId);
}
END_TEST

START_TEST(test_block_id_clone_preserves_value_and_owns_hash)
{
    static const char uuidText[] = "12345678-1234-5678-9abc-def012345678";
    static const char hashText[] = "a94a8fe5ccb19ba61c4c0873d391e987982fbbd3";
    struct BlockId *original;
    struct BlockId *clone;

    original = create_block_id(uuidText, 42U, HASH_TYPE_SHA1, hashText);
    clone = BlockId_Clone(original);

    ck_assert_ptr_ne(clone, NULL);
    ck_assert_ptr_ne(clone, original);
    ck_assert_ptr_ne(clone->pHash, original->pHash);
    ck_assert_ptr_ne(clone->pHash->pData, original->pHash->pData);
    ck_assert(BlockId_IsEqual(original, clone));

    BlockId_Destroy(clone);
    BlockId_Destroy(original);
}
END_TEST

START_TEST(test_block_id_is_equal_detects_different_values)
{
    static const char uuidText[] = "aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee";
    static const char hashA[] =
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
    static const char hashB[] =
        "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
        "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
    struct BlockId *base;
    struct BlockId *differentLength;
    struct BlockId *differentHash;

    base = create_block_id(uuidText, 7U, HASH_TYPE_SHA256, hashA);
    differentLength = create_block_id(uuidText, 8U, HASH_TYPE_SHA256, hashA);
    differentHash = create_block_id(uuidText, 7U, HASH_TYPE_SHA256, hashB);

    ck_assert(!BlockId_IsEqual(base, differentLength));
    ck_assert(!BlockId_IsEqual(base, differentHash));

    BlockId_Destroy(differentHash);
    BlockId_Destroy(differentLength);
    BlockId_Destroy(base);
}
END_TEST

static Suite *
block_id_suite(void)
{
    Suite *suite;
    TCase *testcase;

    suite = suite_create("block_id");
    testcase = tcase_create("core");

    tcase_add_test(testcase, test_block_id_to_text_formats_expected_value);
    tcase_add_test(testcase, test_block_id_clone_preserves_value_and_owns_hash);
    tcase_add_test(testcase, test_block_id_is_equal_detects_different_values);

    suite_add_tcase(suite, testcase);
    return suite;
}

int
main(void)
{
    Suite *suite;
    SRunner *runner;
    int failed;

    suite = block_id_suite();
    runner = srunner_create(suite);
    srunner_set_fork_status(runner, CK_NOFORK);
    srunner_run_all(runner, CK_NORMAL);
    failed = srunner_ntests_failed(runner);
    srunner_free(runner);

    return (failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
