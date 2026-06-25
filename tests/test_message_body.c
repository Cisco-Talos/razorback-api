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

#include <razorback/message_body.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *
read_fixture(const char *name)
{
    char path[4096];
    FILE *file;
    long size;
    char *contents;

    snprintf(path, sizeof(path), "%s/%s", RAZORBACK_SCHEMA_FIXTURE_DIR, name);
    file = fopen(path, "rb");
    ck_assert_ptr_ne(file, NULL);
    ck_assert_int_eq(fseek(file, 0, SEEK_END), 0);
    size = ftell(file);
    ck_assert_int_ge(size, 0);
    ck_assert_int_eq(fseek(file, 0, SEEK_SET), 0);

    contents = calloc((size_t)size + 1, sizeof(char));
    ck_assert_ptr_ne(contents, NULL);
    ck_assert_uint_eq(fread(contents, 1, (size_t)size, file), (size_t)size);
    fclose(file);
    return contents;
}

static struct ClaimCheckReference *
claim_check_template(void)
{
    return ClaimCheckReference_Create(
        "http://object-store.local/claim-check/payload?signature=test",
        "2026-06-17T21:15:00.000Z",
        "claim-check",
        "messages/2026/06/17/payload.zlib",
        "application/json",
        "razorback.messages.analysis_result",
        1
    );
}

START_TEST(test_message_body_default_policy_matches_messaging_threshold)
{
    struct MessageBodyPolicy policy;

    policy = MessageBodyPolicy_Default();
    ck_assert_uint_eq(policy.maxInlineBytes,
                      MESSAGE_BODY_DEFAULT_MAX_INLINE_BYTES);
    ck_assert_uint_eq(policy.maxInlineBytes, 921600U);
}
END_TEST

START_TEST(test_message_body_keeps_small_payload_inline)
{
    const uint8_t body[] = "small";
    struct MessageBodyPolicy policy = { 32 };
    struct EncodedMessageBody *encoded = NULL;

    ck_assert(MessageBody_Encode(&policy, body, sizeof(body) - 1, NULL,
                                 &encoded));
    ck_assert_ptr_ne(encoded, NULL);
    ck_assert_int_eq(encoded->mode, MESSAGE_BODY_MODE_INLINE);
    ck_assert_str_eq(MessageBodyMode_ToString(encoded->mode), "inline");
    ck_assert_ptr_eq(encoded->contentEncoding, NULL);
    ck_assert_ptr_eq(encoded->claimCheckBody, NULL);
    ck_assert_uint_eq(encoded->transportBodySize, sizeof(body) - 1);
    ck_assert_int_eq(memcmp(encoded->transportBody, body, sizeof(body) - 1), 0);

    EncodedMessageBody_Destroy(encoded);
}
END_TEST

START_TEST(test_message_body_compresses_medium_payload_inline)
{
    uint8_t body[1024];
    struct MessageBodyPolicy policy = { 128 };
    struct EncodedMessageBody *encoded = NULL;
    uint8_t *decoded = NULL;
    size_t decodedSize = 0;

    memset(body, 'a', sizeof(body));
    ck_assert(MessageBody_Encode(&policy, body, sizeof(body), NULL, &encoded));
    ck_assert_ptr_ne(encoded, NULL);
    ck_assert_int_eq(encoded->mode, MESSAGE_BODY_MODE_ZLIB);
    ck_assert_str_eq(MessageBodyMode_ToString(encoded->mode), "zlib");
    ck_assert_ptr_ne(encoded->contentEncoding, NULL);
    ck_assert_str_eq(encoded->contentEncoding, MESSAGE_BODY_CONTENT_ENCODING_ZLIB);
    ck_assert_uint_le(encoded->transportBodySize, policy.maxInlineBytes);

    ck_assert(MessageBody_DecodeInline(encoded->transportBody,
                                       encoded->transportBodySize,
                                       encoded->contentEncoding,
                                       &decoded,
                                       &decodedSize));
    ck_assert_uint_eq(decodedSize, sizeof(body));
    ck_assert_int_eq(memcmp(decoded, body, sizeof(body)), 0);

    free(decoded);
    EncodedMessageBody_Destroy(encoded);
}
END_TEST

START_TEST(test_message_body_uses_claim_check_for_oversized_compressed_payload)
{
    uint8_t body[4096];
    struct MessageBodyPolicy policy = { 32 };
    struct ClaimCheckReference *template = NULL;
    struct ClaimCheckReference *reference = NULL;
    struct EncodedMessageBody *encoded = NULL;
    uint8_t *decoded = NULL;
    size_t decodedSize = 0;
    size_t index;

    for (index = 0; index < sizeof(body); index++)
        body[index] = (uint8_t)((index * 31U + 17U) % 251U);

    template = claim_check_template();
    ck_assert_ptr_ne(template, NULL);
    ck_assert(MessageBody_Encode(&policy, body, sizeof(body), template,
                                 &encoded));
    ck_assert_ptr_ne(encoded, NULL);
    ck_assert_int_eq(encoded->mode, MESSAGE_BODY_MODE_CLAIM_CHECK);
    ck_assert_str_eq(MessageBodyMode_ToString(encoded->mode), "claim_check");
    ck_assert_ptr_eq(encoded->contentEncoding, NULL);
    ck_assert_ptr_ne(encoded->claimCheckBody, NULL);
    ck_assert_uint_eq(encoded->claimCheckBodySize,
                      encoded->claimCheckReference->storedCompressedSize);
    ck_assert_uint_eq(encoded->claimCheckReference->uncompressedSize,
                      sizeof(body));
    ck_assert_str_eq(encoded->claimCheckReference->contentEncoding,
                     MESSAGE_BODY_CONTENT_ENCODING_ZLIB);
    ck_assert_uint_eq(strlen(encoded->claimCheckReference->sha256), 64U);

    reference = ClaimCheckReference_FromJson((char *)encoded->transportBody);
    ck_assert_ptr_ne(reference, NULL);
    ck_assert_str_eq(reference->referenceSchemaName,
                     MESSAGE_BODY_CLAIM_CHECK_REFERENCE_SCHEMA_NAME);
    ck_assert_uint_eq(reference->referenceSchemaVersion,
                      MESSAGE_BODY_CLAIM_CHECK_REFERENCE_SCHEMA_VERSION);
    ck_assert_uint_eq(reference->storedCompressedSize,
                      encoded->claimCheckBodySize);

    ck_assert(MessageBody_DecodeClaimCheck(encoded->claimCheckBody,
                                           encoded->claimCheckBodySize,
                                           reference,
                                           &decoded,
                                           &decodedSize));
    ck_assert_uint_eq(decodedSize, sizeof(body));
    ck_assert_int_eq(memcmp(decoded, body, sizeof(body)), 0);

    free(decoded);
    ClaimCheckReference_Destroy(reference);
    EncodedMessageBody_Destroy(encoded);
    ClaimCheckReference_Destroy(template);
}
END_TEST

START_TEST(test_message_body_rejects_oversized_claim_check_without_template)
{
    uint8_t body[4096];
    struct MessageBodyPolicy policy = { 32 };
    struct EncodedMessageBody *encoded = NULL;
    size_t index;

    for (index = 0; index < sizeof(body); index++)
        body[index] = (uint8_t)((index * 31U + 17U) % 251U);

    ck_assert(!MessageBody_Encode(&policy, body, sizeof(body), NULL, &encoded));
    ck_assert_ptr_eq(encoded, NULL);
}
END_TEST

START_TEST(test_message_body_claim_check_fixture_round_trip)
{
    char *fixture;
    char *serialized;
    struct ClaimCheckReference *reference;

    fixture = read_fixture("claim_check_reference.valid.json");
    reference = ClaimCheckReference_FromJson(fixture);
    ck_assert_ptr_ne(reference, NULL);
    ck_assert_str_eq(reference->referenceSchemaName,
                     MESSAGE_BODY_CLAIM_CHECK_REFERENCE_SCHEMA_NAME);
    ck_assert_uint_eq(reference->referenceSchemaVersion, 1U);
    ck_assert_str_eq(reference->contentEncoding,
                     MESSAGE_BODY_CONTENT_ENCODING_ZLIB);
    ck_assert_uint_eq(reference->uncompressedSize, 1048576U);
    ck_assert_uint_eq(reference->storedCompressedSize, 65536U);

    serialized = ClaimCheckReference_ToJson(reference);
    ck_assert_ptr_ne(serialized, NULL);
    free(serialized);
    ClaimCheckReference_Destroy(reference);
    free(fixture);
}
END_TEST

START_TEST(test_message_body_rejects_invalid_claim_check_fixture)
{
    char *fixture;
    struct ClaimCheckReference *reference;

    fixture = read_fixture("claim_check_reference.invalid.json");
    reference = ClaimCheckReference_FromJson(fixture);
    ck_assert_ptr_eq(reference, NULL);
    free(fixture);
}
END_TEST

static Suite *
message_body_suite(void)
{
    Suite *suite;
    TCase *testcase;

    suite = suite_create("message_body");
    testcase = tcase_create("core");
    tcase_add_test(testcase, test_message_body_default_policy_matches_messaging_threshold);
    tcase_add_test(testcase, test_message_body_keeps_small_payload_inline);
    tcase_add_test(testcase, test_message_body_compresses_medium_payload_inline);
    tcase_add_test(testcase, test_message_body_uses_claim_check_for_oversized_compressed_payload);
    tcase_add_test(testcase, test_message_body_rejects_oversized_claim_check_without_template);
    tcase_add_test(testcase, test_message_body_claim_check_fixture_round_trip);
    tcase_add_test(testcase, test_message_body_rejects_invalid_claim_check_fixture);
    suite_add_tcase(suite, testcase);
    return suite;
}

int
main(void)
{
    Suite *suite;
    SRunner *runner;
    int failed;

    suite = message_body_suite();
    runner = srunner_create(suite);
    srunner_run_all(runner, CK_NORMAL);
    failed = srunner_ntests_failed(runner);
    srunner_free(runner);
    return failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
