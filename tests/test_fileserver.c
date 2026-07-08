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

#include <razorback/fileserver.h>
#include <razorback/hash.h>

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <uuid/uuid.h>

#define TEST_SERVER_MAX_REQUESTS 4U
#define TEST_SERVER_REQUEST_SIZE 32768U

struct TestResponse
{
    unsigned int status;
    const char *reason;
    const uint8_t *body;
    size_t bodyLength;
    const char *contentType;
};

struct TestServer
{
    int listenFd;
    uint16_t port;
    pthread_t thread;
    const struct TestResponse *responses;
    size_t responseCount;
    size_t requestCount;
    bool failed;
    char requests[TEST_SERVER_MAX_REQUESTS][TEST_SERVER_REQUEST_SIZE];
};

static struct BlockId *
create_sha256_block_id(const char *hashText, uint64_t length)
{
    struct BlockId *blockId;

    blockId = calloc(1, sizeof(*blockId));
    ck_assert_ptr_ne(blockId, NULL);
    blockId->pHash = Hash_Create_From_String(HASH_TYPE_SHA256, hashText);
    ck_assert_ptr_ne(blockId->pHash, NULL);
    blockId->iLength = length;
    uuid_clear(blockId->uuidDataType);
    return blockId;
}

static char *
hash_text_for_payload(const uint8_t *payload, size_t length)
{
    struct Hash *hash;
    char *hashText;

    hash = Hash_Create_Type(HASH_TYPE_SHA256);
    ck_assert_ptr_ne(hash, NULL);
    ck_assert(Hash_Update(hash, (uint8_t *)payload, (uint32_t)length));
    ck_assert(Hash_Finalize(hash));
    hashText = Hash_ToText(hash);
    ck_assert_ptr_ne(hashText, NULL);
    Hash_Destroy(hash);
    return hashText;
}

static struct BlockId *
create_sha256_block_id_for_payload(const uint8_t *payload, size_t payloadLength,
                                   uint64_t blockLength)
{
    char *hashText = hash_text_for_payload(payload, payloadLength);
    struct BlockId *blockId = create_sha256_block_id(hashText, blockLength);

    free(hashText);
    return blockId;
}

static void
destroy_block_id(struct BlockId *blockId)
{
    if (blockId == NULL)
        return;
    Hash_Destroy(blockId->pHash);
    free(blockId);
}

static size_t
parse_content_length(const char *request)
{
    const char *line = request;

    while (line != NULL && *line != '\0') {
        const char *next = strstr(line, "\r\n");
        size_t lineLength = next == NULL ? strlen(line) : (size_t)(next - line);

        if (lineLength >= 15U &&
            strncasecmp(line, "Content-Length:", 15U) == 0) {
            return (size_t)strtoull(line + 15U, NULL, 10);
        }
        if (next == NULL)
            break;
        line = next + 2;
    }
    return 0;
}

static void
test_server_read_request(int clientFd, char *buffer, size_t bufferSize)
{
    size_t used = 0;
    size_t headerLength = 0;
    size_t contentLength = 0;

    while (used + 1U < bufferSize) {
        ssize_t count = recv(clientFd, buffer + used, bufferSize - used - 1U, 0);
        char *headerEnd;

        if (count <= 0)
            break;
        used += (size_t)count;
        buffer[used] = '\0';
        headerEnd = strstr(buffer, "\r\n\r\n");
        if (headerEnd == NULL)
            continue;
        if (headerLength == 0) {
            headerLength = (size_t)(headerEnd - buffer) + 4U;
            contentLength = parse_content_length(buffer);
        }
        if (used >= headerLength + contentLength)
            break;
    }
    buffer[used] = '\0';
}

static bool
test_server_write_all(int fd, const void *data, size_t length)
{
    const uint8_t *bytes = data;
    size_t written = 0;

    while (written < length) {
        ssize_t count = send(fd, bytes + written, length - written, 0);
        if (count <= 0)
            return false;
        written += (size_t)count;
    }
    return true;
}

static void
test_server_send_response(int clientFd, const struct TestResponse *response,
                          bool *failed)
{
    char header[512];
    int length;
    const char *contentType = response->contentType == NULL ?
        "text/plain" : response->contentType;

    length = snprintf(header, sizeof(header),
                      "HTTP/1.1 %u %s\r\n"
                      "Content-Length: %zu\r\n"
                      "Connection: close\r\n"
                      "Content-Type: %s\r\n"
                      "\r\n",
                      response->status, response->reason,
                      response->bodyLength, contentType);
    if (length < 0 || (size_t)length >= sizeof(header) ||
        !test_server_write_all(clientFd, header, (size_t)length)) {
        *failed = true;
        return;
    }
    if (response->bodyLength > 0 &&
        !test_server_write_all(clientFd, response->body,
                               response->bodyLength)) {
        *failed = true;
    }
}

static void *
test_server_thread(void *userData)
{
    struct TestServer *server = userData;
    size_t index;

    for (index = 0; index < server->responseCount; index++) {
        struct sockaddr_in address;
        socklen_t addressLength = sizeof(address);
        int clientFd;

        clientFd = accept(server->listenFd, (struct sockaddr *)&address,
                          &addressLength);
        if (clientFd < 0) {
            server->failed = true;
            break;
        }
        test_server_read_request(clientFd, server->requests[index],
                                 sizeof(server->requests[index]));
        test_server_send_response(clientFd, &server->responses[index],
                                  &server->failed);
        close(clientFd);
        server->requestCount++;
    }
    close(server->listenFd);
    return NULL;
}

static void
test_server_start(struct TestServer *server,
                  const struct TestResponse *responses,
                  size_t responseCount)
{
    struct sockaddr_in address;
    socklen_t addressLength = sizeof(address);
    struct timeval timeout = { 5, 0 };
    int enabled = 1;

    ck_assert(responseCount <= TEST_SERVER_MAX_REQUESTS);
    memset(server, 0, sizeof(*server));
    server->responses = responses;
    server->responseCount = responseCount;
    server->listenFd = socket(AF_INET, SOCK_STREAM, 0);
    ck_assert_msg(server->listenFd >= 0, "socket failed: %s",
                  strerror(errno));
    ck_assert_int_eq(setsockopt(server->listenFd, SOL_SOCKET, SO_REUSEADDR,
                                &enabled, sizeof(enabled)), 0);
    ck_assert_int_eq(setsockopt(server->listenFd, SOL_SOCKET, SO_RCVTIMEO,
                                &timeout, sizeof(timeout)), 0);
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(0);
    ck_assert_int_eq(bind(server->listenFd, (struct sockaddr *)&address,
                          sizeof(address)), 0);
    ck_assert_int_eq(listen(server->listenFd, 4), 0);
    ck_assert_int_eq(getsockname(server->listenFd, (struct sockaddr *)&address,
                                 &addressLength), 0);
    server->port = ntohs(address.sin_port);
    ck_assert_int_eq(pthread_create(&server->thread, NULL, test_server_thread,
                                    server), 0);
}

static void
test_server_join(struct TestServer *server)
{
    ck_assert_int_eq(pthread_join(server->thread, NULL), 0);
    ck_assert(!server->failed);
}

static char *
test_server_url(const struct TestServer *server)
{
    char *url = NULL;

    ck_assert_int_ne(asprintf(&url, "http://127.0.0.1:%u", server->port), -1);
    ck_assert_ptr_ne(url, NULL);
    return url;
}

static char *
expected_block_path(const struct BlockId *blockId)
{
    char *hashText = Hash_ToText(blockId->pHash);
    char *path = NULL;

    ck_assert_ptr_ne(hashText, NULL);
    ck_assert_int_ne(asprintf(&path, "/%c/%c/%c/%c/%s.%ju",
                              hashText[0], hashText[1], hashText[2],
                              hashText[3], hashText,
                              (uintmax_t)blockId->iLength), -1);
    ck_assert_ptr_ne(path, NULL);
    free(hashText);
    return path;
}

static void
assert_request_starts_with_path(const char *request, const char *method,
                                const struct BlockId *blockId)
{
    char *path = expected_block_path(blockId);
    char *expected = NULL;

    ck_assert_int_ne(asprintf(&expected, "%s %s HTTP/1.1", method, path), -1);
    ck_assert_msg(strncmp(request, expected, strlen(expected)) == 0,
                  "request %s did not start with %s", request, expected);
    free(expected);
    free(path);
}

START_TEST(test_build_url_uses_canonical_hash_path)
{
    const char *hashText =
        "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
    struct BlockId *blockId = create_sha256_block_id(hashText, 12345U);
    RzbNextFileserverClient_t *client;
    char *url = NULL;

    client = RzbNextFileserverClient_Create("https://files.example.test/root/",
                                            0, 0);
    ck_assert_ptr_ne(client, NULL);
    ck_assert_int_eq(RzbNextFileserver_BuildUrl(client, blockId, &url),
                     RZB_NEXT_FILESERVER_OK);
    ck_assert_str_eq(url,
                     "https://files.example.test/root/0/1/2/3/"
                     "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef.12345");

    free(url);
    RzbNextFileserverClient_Destroy(client);
    destroy_block_id(blockId);
}
END_TEST

START_TEST(test_build_url_uses_compose_default_base_url)
{
    const char *hashText =
        "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
    struct BlockId *blockId = create_sha256_block_id(hashText, 12345U);
    RzbNextFileserverClient_t *client;
    char *url = NULL;

    unsetenv("RZB_FILESERVER__URL");
    unsetenv("RZB_FILESERVER_URL");

    client = RzbNextFileserverClient_Create(NULL, 0, 0);
    ck_assert_ptr_ne(client, NULL);
    ck_assert_int_eq(RzbNextFileserver_BuildUrl(client, blockId, &url),
                     RZB_NEXT_FILESERVER_OK);
    ck_assert_str_eq(url,
                     "http://file-server:8080/0/1/2/3/"
                     "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef.12345");

    free(url);
    RzbNextFileserverClient_Destroy(client);
    destroy_block_id(blockId);
}
END_TEST

START_TEST(test_build_url_rejects_non_sha256_identity)
{
    struct BlockId *blockId = create_sha256_block_id(
        "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef",
        12345U
    );
    RzbNextFileserverClient_t *client;
    char *url = NULL;

    blockId->pHash->iType = HASH_TYPE_SHA1;
    client = RzbNextFileserverClient_Create("https://files.example.test", 0, 0);
    ck_assert_ptr_ne(client, NULL);
    ck_assert_int_eq(RzbNextFileserver_BuildUrl(client, blockId, &url),
                     RZB_NEXT_FILESERVER_LOCAL_ERROR);
    ck_assert_ptr_eq(url, NULL);

    RzbNextFileserverClient_Destroy(client);
    destroy_block_id(blockId);
}
END_TEST

START_TEST(test_store_bytes_posts_multipart_to_canonical_path)
{
    const uint8_t payload[] = "razorback c fileserver payload";
    const struct TestResponse responses[] = {
        { 201U, "Created", NULL, 0, NULL }
    };
    struct BlockId *blockId = create_sha256_block_id_for_payload(
        payload, sizeof(payload) - 1U, sizeof(payload) - 1U
    );
    struct TestServer server;
    RzbNextFileserverClient_t *client;
    char *url;

    test_server_start(&server, responses, 1U);
    url = test_server_url(&server);
    client = RzbNextFileserverClient_Create(url, 1, 1);
    ck_assert_ptr_ne(client, NULL);
    ck_assert_int_eq(RzbNextFileserver_StoreBytes(client, blockId, payload,
                                                  sizeof(payload) - 1U),
                     RZB_NEXT_FILESERVER_OK);
    test_server_join(&server);
    ck_assert_int_eq(server.requestCount, 1);
    assert_request_starts_with_path(server.requests[0], "POST", blockId);
    ck_assert_ptr_ne(strstr(server.requests[0], "name=\"file\""), NULL);

    RzbNextFileserverClient_Destroy(client);
    free(url);
    destroy_block_id(blockId);
}
END_TEST

START_TEST(test_store_bytes_treats_duplicate_upload_as_success)
{
    const uint8_t payload[] = "duplicate block payload";
    const uint8_t body[] = "already exists";
    const struct TestResponse responses[] = {
        { 400U, "Bad Request", body, sizeof(body) - 1U, NULL }
    };
    struct BlockId *blockId = create_sha256_block_id_for_payload(
        payload, sizeof(payload) - 1U, sizeof(payload) - 1U
    );
    struct TestServer server;
    RzbNextFileserverClient_t *client;
    char *url;

    test_server_start(&server, responses, 1U);
    url = test_server_url(&server);
    client = RzbNextFileserverClient_Create(url, 1, 1);
    ck_assert_ptr_ne(client, NULL);
    ck_assert_int_eq(RzbNextFileserver_StoreBytes(client, blockId, payload,
                                                  sizeof(payload) - 1U),
                     RZB_NEXT_FILESERVER_OK);
    test_server_join(&server);
    ck_assert_int_eq(server.requestCount, 1);

    RzbNextFileserverClient_Destroy(client);
    free(url);
    destroy_block_id(blockId);
}
END_TEST

START_TEST(test_store_bytes_retries_retryable_http_failure)
{
    const uint8_t payload[] = "retryable upload payload";
    const struct TestResponse responses[] = {
        { 500U, "Internal Server Error", NULL, 0, NULL },
        { 204U, "No Content", NULL, 0, NULL }
    };
    struct BlockId *blockId = create_sha256_block_id_for_payload(
        payload, sizeof(payload) - 1U, sizeof(payload) - 1U
    );
    struct TestServer server;
    RzbNextFileserverClient_t *client;
    char *url;

    test_server_start(&server, responses, 2U);
    url = test_server_url(&server);
    client = RzbNextFileserverClient_Create(url, 1, 1);
    ck_assert_ptr_ne(client, NULL);
    ck_assert_int_eq(RzbNextFileserver_StoreBytes(client, blockId, payload,
                                                  sizeof(payload) - 1U),
                     RZB_NEXT_FILESERVER_OK);
    test_server_join(&server);
    ck_assert_int_eq(server.requestCount, 2);

    RzbNextFileserverClient_Destroy(client);
    free(url);
    destroy_block_id(blockId);
}
END_TEST

START_TEST(test_store_bytes_rejects_payload_mismatch_before_http)
{
    const uint8_t payload[] = "payload mismatch";
    struct BlockId *blockId = create_sha256_block_id_for_payload(
        payload, sizeof(payload) - 1U, sizeof(payload)
    );
    RzbNextFileserverClient_t *client;

    client = RzbNextFileserverClient_Create("http://127.0.0.1:1", 1, 1);
    ck_assert_ptr_ne(client, NULL);
    ck_assert_int_eq(RzbNextFileserver_StoreBytes(client, blockId, payload,
                                                  sizeof(payload) - 1U),
                     RZB_NEXT_FILESERVER_PAYLOAD_MISMATCH);

    RzbNextFileserverClient_Destroy(client);
    destroy_block_id(blockId);
}
END_TEST

START_TEST(test_fetch_to_file_streams_and_verifies_payload)
{
    const uint8_t payload[] = "fetch success payload";
    const struct TestResponse responses[] = {
        { 200U, "OK", payload, sizeof(payload) - 1U,
          "application/octet-stream" }
    };
    struct BlockId *blockId = create_sha256_block_id_for_payload(
        payload, sizeof(payload) - 1U, sizeof(payload) - 1U
    );
    struct TestServer server;
    RzbNextFileserverClient_t *client;
    char *url;
    char *fileName = NULL;
    FILE *file;
    uint8_t buffer[sizeof(payload)];
    size_t readLength;

    test_server_start(&server, responses, 1U);
    url = test_server_url(&server);
    client = RzbNextFileserverClient_Create(url, 1, 1);
    ck_assert_ptr_ne(client, NULL);
    ck_assert_int_eq(RzbNextFileserver_FetchToFile(client, blockId, &fileName),
                     RZB_NEXT_FILESERVER_OK);
    test_server_join(&server);
    ck_assert_int_eq(server.requestCount, 1);
    assert_request_starts_with_path(server.requests[0], "GET", blockId);
    ck_assert_ptr_ne(fileName, NULL);
    file = fopen(fileName, "rb");
    ck_assert_ptr_ne(file, NULL);
    readLength = fread(buffer, 1, sizeof(buffer), file);
    ck_assert_int_eq(fclose(file), 0);
    ck_assert_int_eq(readLength, sizeof(payload) - 1U);
    ck_assert_int_eq(memcmp(buffer, payload, sizeof(payload) - 1U), 0);

    unlink(fileName);
    free(fileName);
    RzbNextFileserverClient_Destroy(client);
    free(url);
    destroy_block_id(blockId);
}
END_TEST

START_TEST(test_fetch_to_file_reports_not_found)
{
    const uint8_t payload[] = "not found payload";
    const uint8_t body[] = "missing";
    const struct TestResponse responses[] = {
        { 404U, "Not Found", body, sizeof(body) - 1U, NULL }
    };
    struct BlockId *blockId = create_sha256_block_id_for_payload(
        payload, sizeof(payload) - 1U, sizeof(payload) - 1U
    );
    struct TestServer server;
    RzbNextFileserverClient_t *client;
    char *url;
    char *fileName = NULL;

    test_server_start(&server, responses, 1U);
    url = test_server_url(&server);
    client = RzbNextFileserverClient_Create(url, 1, 1);
    ck_assert_ptr_ne(client, NULL);
    ck_assert_int_eq(RzbNextFileserver_FetchToFile(client, blockId, &fileName),
                     RZB_NEXT_FILESERVER_NOT_FOUND);
    test_server_join(&server);
    ck_assert_int_eq(server.requestCount, 1);
    ck_assert_ptr_eq(fileName, NULL);

    RzbNextFileserverClient_Destroy(client);
    free(url);
    destroy_block_id(blockId);
}
END_TEST

START_TEST(test_fetch_to_file_rejects_size_mismatch)
{
    const uint8_t payload[] = "size mismatch payload";
    const struct TestResponse responses[] = {
        { 200U, "OK", payload, sizeof(payload) - 1U,
          "application/octet-stream" }
    };
    struct BlockId *blockId = create_sha256_block_id_for_payload(
        payload, sizeof(payload) - 1U, sizeof(payload)
    );
    struct TestServer server;
    RzbNextFileserverClient_t *client;
    char *url;
    char *fileName = NULL;

    test_server_start(&server, responses, 1U);
    url = test_server_url(&server);
    client = RzbNextFileserverClient_Create(url, 1, 1);
    ck_assert_ptr_ne(client, NULL);
    ck_assert_int_eq(RzbNextFileserver_FetchToFile(client, blockId, &fileName),
                     RZB_NEXT_FILESERVER_PAYLOAD_MISMATCH);
    test_server_join(&server);
    ck_assert_int_eq(server.requestCount, 1);
    ck_assert_ptr_eq(fileName, NULL);

    RzbNextFileserverClient_Destroy(client);
    free(url);
    destroy_block_id(blockId);
}
END_TEST

START_TEST(test_fetch_to_file_rejects_hash_mismatch)
{
    const uint8_t payload[] = "hash mismatch payload";
    const uint8_t other[] = "different block body";
    const struct TestResponse responses[] = {
        { 200U, "OK", payload, sizeof(payload) - 1U,
          "application/octet-stream" }
    };
    struct BlockId *blockId = create_sha256_block_id_for_payload(
        other, sizeof(other) - 1U, sizeof(payload) - 1U
    );
    struct TestServer server;
    RzbNextFileserverClient_t *client;
    char *url;
    char *fileName = NULL;

    test_server_start(&server, responses, 1U);
    url = test_server_url(&server);
    client = RzbNextFileserverClient_Create(url, 1, 1);
    ck_assert_ptr_ne(client, NULL);
    ck_assert_int_eq(RzbNextFileserver_FetchToFile(client, blockId, &fileName),
                     RZB_NEXT_FILESERVER_PAYLOAD_MISMATCH);
    test_server_join(&server);
    ck_assert_int_eq(server.requestCount, 1);
    ck_assert_ptr_eq(fileName, NULL);

    RzbNextFileserverClient_Destroy(client);
    free(url);
    destroy_block_id(blockId);
}
END_TEST

static Suite *
fileserver_suite(void)
{
    Suite *suite;
    TCase *testcase;

    suite = suite_create("fileserver");
    testcase = tcase_create("canonical-url");
    tcase_add_test(testcase, test_build_url_uses_canonical_hash_path);
    tcase_add_test(testcase, test_build_url_uses_compose_default_base_url);
    tcase_add_test(testcase, test_build_url_rejects_non_sha256_identity);
    tcase_add_test(testcase, test_store_bytes_posts_multipart_to_canonical_path);
    tcase_add_test(testcase, test_store_bytes_treats_duplicate_upload_as_success);
    tcase_add_test(testcase, test_store_bytes_retries_retryable_http_failure);
    tcase_add_test(testcase, test_store_bytes_rejects_payload_mismatch_before_http);
    tcase_add_test(testcase, test_fetch_to_file_streams_and_verifies_payload);
    tcase_add_test(testcase, test_fetch_to_file_reports_not_found);
    tcase_add_test(testcase, test_fetch_to_file_rejects_size_mismatch);
    tcase_add_test(testcase, test_fetch_to_file_rejects_hash_mismatch);
    suite_add_tcase(suite, testcase);
    return suite;
}

int
main(void)
{
    Suite *suite = fileserver_suite();
    SRunner *runner = srunner_create(suite);
    int failed;

    srunner_set_fork_status(runner, CK_NOFORK);
    srunner_run_all(runner, CK_NORMAL);
    failed = srunner_ntests_failed(runner);
    srunner_free(runner);
    return failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
