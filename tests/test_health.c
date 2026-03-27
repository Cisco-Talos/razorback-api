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

#include "health_internal.h"

#include <razorback/api.h>
#include <razorback/health.h>
#include <razorback/list.h>
#include <razorback/lock.h>

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define HEALTH_TEST_CONNECT_RETRY_MS 10U
#define HEALTH_TEST_CONNECT_RETRIES 50U
#define HEALTH_TEST_RESPONSE_SIZE 4096U

static pthread_mutex_t g_contextMutex = PTHREAD_MUTEX_INITIALIZER;
static struct RazorbackContext *g_contexts[8];
static size_t g_contextCount = 0U;
static uint16_t g_nextHealthPort = 48080U;

static void health_test_sleep_ms(unsigned int milliseconds);
static void health_test_reset_contexts(void);
static void health_test_set_contexts(struct RazorbackContext **contexts, size_t count);
static void health_test_init_context(struct RazorbackContext *context,
                                     bool registered,
                                     bool emergencyShutdownRequested);
static void health_test_destroy_context(struct RazorbackContext *context);
static uint16_t health_test_start_listener(bool requireContextsForReady);
static int health_test_connect(uint16_t port);
static void health_test_send_all(int fd, const char *request);
static void health_test_request(uint16_t port, const char *request,
                                char *response, size_t responseSize);
static bool health_test_custom_check(void *userData);
static void health_test_setup(void);
static void health_test_teardown(void);

bool
Razorback_ForEach_Context(int (*function)(struct RazorbackContext *, void *), void *userData)
{
    struct RazorbackContext *snapshot[8];
    size_t count;
    size_t i;

    pthread_mutex_lock(&g_contextMutex);
    count = g_contextCount;
    memcpy(snapshot, g_contexts, sizeof(struct RazorbackContext *) * count);
    pthread_mutex_unlock(&g_contextMutex);

    for (i = 0U; i < count; i++) {
        int result = function(snapshot[i], userData);

        if (result == LIST_EACH_OK || result == LIST_EACH_REMOVE)
            continue;
        if (result == LIST_EACH_END)
            return true;
        if (result == LIST_EACH_ERROR)
            return false;
    }

    return true;
}

void
rzb_perror(uint64_t component, const char *message)
{
    (void)component;
    (void)message;
}

static void
health_test_sleep_ms(unsigned int milliseconds)
{
    struct timespec req;
    struct timespec rem;

    req.tv_sec = milliseconds / 1000U;
    req.tv_nsec = (long)(milliseconds % 1000U) * 1000000L;

    while (nanosleep(&req, &rem) == -1 && errno == EINTR)
        req = rem;
}

static void
health_test_reset_contexts(void)
{
    pthread_mutex_lock(&g_contextMutex);
    memset(g_contexts, 0, sizeof(g_contexts));
    g_contextCount = 0U;
    pthread_mutex_unlock(&g_contextMutex);
}

static void
health_test_set_contexts(struct RazorbackContext **contexts, size_t count)
{
    ck_assert_msg(count <= (sizeof(g_contexts) / sizeof(g_contexts[0])),
                  "Too many test contexts: %zu", count);

    pthread_mutex_lock(&g_contextMutex);
    memset(g_contexts, 0, sizeof(g_contexts));
    if (count > 0U)
        memcpy(g_contexts, contexts, sizeof(struct RazorbackContext *) * count);
    g_contextCount = count;
    pthread_mutex_unlock(&g_contextMutex);
}

static void
health_test_init_context(struct RazorbackContext *context,
                         bool registered,
                         bool emergencyShutdownRequested)
{
    memset(context, 0, sizeof(*context));
    context->regOk = registered;
    context->inspector.emergencyShutdownRequested = emergencyShutdownRequested;
    context->inspector.emergencyLock = Mutex_Create(MUTEX_MODE_NORMAL);
    ck_assert_ptr_ne(context->inspector.emergencyLock, NULL);
}

static void
health_test_destroy_context(struct RazorbackContext *context)
{
    if (context->inspector.emergencyLock != NULL) {
        Mutex_Destroy(context->inspector.emergencyLock);
        context->inspector.emergencyLock = NULL;
    }
}

static uint16_t
health_test_start_listener(bool requireContextsForReady)
{
    RazorbackHealthServerConfig_t config;
    unsigned int attempts;

    config.bindAddress = "127.0.0.1";
    config.requireContextsForReady = requireContextsForReady;

    for (attempts = 0U; attempts < 128U; attempts++) {
        config.port = g_nextHealthPort++;
        if (g_nextHealthPort >= 49000U)
            g_nextHealthPort = 48080U;

        if (Razorback_Health_Start(&config))
            return config.port;
    }

    ck_abort_msg("Failed to start health listener on a test port");
}

static int
health_test_connect(uint16_t port)
{
    struct sockaddr_in address;
    unsigned int attempt;

    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    ck_assert_int_eq(inet_pton(AF_INET, "127.0.0.1", &address.sin_addr), 1);

    for (attempt = 0U; attempt < HEALTH_TEST_CONNECT_RETRIES; attempt++) {
        int fd = socket(AF_INET, SOCK_STREAM, 0);

        ck_assert_msg(fd >= 0, "socket() failed: %s", strerror(errno));
        if (connect(fd, (const struct sockaddr *)&address, sizeof(address)) == 0)
            return fd;

        close(fd);
        health_test_sleep_ms(HEALTH_TEST_CONNECT_RETRY_MS);
    }

    ck_abort_msg("Failed to connect to health listener on port %u", port);
}

static void
health_test_send_all(int fd, const char *request)
{
    size_t remaining = strlen(request);
    const char *cursor = request;

    while (remaining > 0U) {
        ssize_t sent = send(fd, cursor, remaining, 0);

        ck_assert_msg(sent >= 0, "send() failed: %s", strerror(errno));
        cursor += (size_t)sent;
        remaining -= (size_t)sent;
    }
}

static void
health_test_request(uint16_t port, const char *request,
                    char *response, size_t responseSize)
{
    int fd;
    size_t used = 0U;

    ck_assert_ptr_ne(response, NULL);
    ck_assert_uint_gt(responseSize, 0U);

    fd = health_test_connect(port);
    health_test_send_all(fd, request);

    while (used + 1U < responseSize) {
        ssize_t received = recv(fd, response + used, responseSize - used - 1U, 0);

        if (received < 0) {
            ck_assert_msg(errno == ECONNRESET && used > 0U,
                          "recv() failed: %s", strerror(errno));
            break;
        }
        if (received == 0)
            break;
        used += (size_t)received;
    }

    response[used] = '\0';
    close(fd);

    ck_assert_msg(used > 0U, "Health request returned an empty response");
}

static bool
health_test_custom_check(void *userData)
{
    const bool *healthy = userData;

    return *healthy;
}

static void
health_test_setup(void)
{
    ck_assert(Health_Initialize());
    health_test_reset_contexts();
}

static void
health_test_teardown(void)
{
    Razorback_Health_Stop();
    health_test_reset_contexts();
    Health_Shutdown_Global();
}

START_TEST(test_health_livez_and_startupz)
{
    char response[HEALTH_TEST_RESPONSE_SIZE];
    uint16_t port = health_test_start_listener(false);

    health_test_request(port,
                        "GET /livez HTTP/1.1\r\n",
                        response, sizeof(response));
    ck_assert_ptr_nonnull(strstr(response, "HTTP/1.1 200 OK"));
    ck_assert_ptr_nonnull(strstr(response, "\r\n\r\nok\n"));

    health_test_request(port,
                        "GET /startupz HTTP/1.1\r\n",
                        response, sizeof(response));
    ck_assert_ptr_nonnull(strstr(response, "HTTP/1.1 503 Service Unavailable"));
    ck_assert_ptr_nonnull(strstr(response, "\r\n\r\nunhealthy\n"));

    Razorback_Health_SetStartupComplete(true);
    health_test_request(port,
                        "GET /startupz HTTP/1.1\r\n",
                        response, sizeof(response));
    ck_assert_ptr_nonnull(strstr(response, "HTTP/1.1 200 OK"));
    ck_assert_ptr_nonnull(strstr(response, "\r\n\r\nok\n"));
}
END_TEST

START_TEST(test_health_readyz_requires_context_when_configured)
{
    char response[HEALTH_TEST_RESPONSE_SIZE];
    struct RazorbackContext context;
    struct RazorbackContext *contexts[] = { &context };
    uint16_t port = health_test_start_listener(true);

    Razorback_Health_SetStartupComplete(true);

    health_test_request(port,
                        "GET /readyz HTTP/1.1\r\n",
                        response, sizeof(response));
    ck_assert_ptr_nonnull(strstr(response, "HTTP/1.1 503 Service Unavailable"));

    health_test_init_context(&context, true, false);
    health_test_set_contexts(contexts, 1U);

    health_test_request(port,
                        "GET /readyz HTTP/1.1\r\n",
                        response, sizeof(response));
    ck_assert_ptr_nonnull(strstr(response, "HTTP/1.1 200 OK"));
    ck_assert_ptr_nonnull(strstr(response, "\r\n\r\nok\n"));

    health_test_reset_contexts();
    health_test_destroy_context(&context);
}
END_TEST

START_TEST(test_health_readyz_fails_for_unregistered_or_emergency_context)
{
    char response[HEALTH_TEST_RESPONSE_SIZE];
    struct RazorbackContext context;
    struct RazorbackContext *contexts[] = { &context };
    uint16_t port = health_test_start_listener(false);

    Razorback_Health_SetStartupComplete(true);

    health_test_init_context(&context, false, false);
    health_test_set_contexts(contexts, 1U);
    health_test_request(port,
                        "GET /readyz HTTP/1.1\r\n",
                        response, sizeof(response));
    ck_assert_ptr_nonnull(strstr(response, "HTTP/1.1 503 Service Unavailable"));

    health_test_destroy_context(&context);
    health_test_init_context(&context, true, true);
    health_test_set_contexts(contexts, 1U);
    health_test_request(port,
                        "GET /readyz HTTP/1.1\r\n",
                        response, sizeof(response));
    ck_assert_ptr_nonnull(strstr(response, "HTTP/1.1 503 Service Unavailable"));

    health_test_reset_contexts();
    health_test_destroy_context(&context);
}
END_TEST

START_TEST(test_health_custom_ready_check_affects_probe)
{
    char response[HEALTH_TEST_RESPONSE_SIZE];
    bool customHealthy = false;
    RazorbackHealthCheckId_t checkId;
    uint16_t port = health_test_start_listener(false);

    Razorback_Health_SetStartupComplete(true);

    checkId = Razorback_Health_RegisterCheck(RAZORBACK_HEALTH_READY,
                                             "test-ready-check",
                                             health_test_custom_check,
                                             &customHealthy);
    ck_assert_uint_ne(checkId, 0U);

    health_test_request(port,
                        "GET /readyz HTTP/1.1\r\n",
                        response, sizeof(response));
    ck_assert_ptr_nonnull(strstr(response, "HTTP/1.1 503 Service Unavailable"));

    customHealthy = true;
    health_test_request(port,
                        "GET /readyz HTTP/1.1\r\n",
                        response, sizeof(response));
    ck_assert_ptr_nonnull(strstr(response, "HTTP/1.1 200 OK"));

    ck_assert(Razorback_Health_UnregisterCheck(checkId));
}
END_TEST

START_TEST(test_healthz_reports_aggregate_json)
{
    char response[HEALTH_TEST_RESPONSE_SIZE];
    uint16_t port = health_test_start_listener(false);

    Razorback_Health_SetStartupComplete(true);

    health_test_request(port,
                        "GET /healthz HTTP/1.1\r\n",
                        response, sizeof(response));
    ck_assert_ptr_nonnull(strstr(response, "HTTP/1.1 200 OK"));
    ck_assert_ptr_nonnull(strstr(response, "{\"live\":true,\"ready\":true,\"startup\":true}\n"));

    Razorback_Health_SetStartupComplete(false);
    health_test_request(port,
                        "GET /healthz HTTP/1.1\r\n",
                        response, sizeof(response));
    ck_assert_ptr_nonnull(strstr(response, "HTTP/1.1 503 Service Unavailable"));
    ck_assert_ptr_nonnull(strstr(response, "{\"live\":true,\"ready\":false,\"startup\":false}\n"));
}
END_TEST

static Suite *
health_suite(void)
{
    Suite *suite;
    TCase *testcase;

    suite = suite_create("health");
    testcase = tcase_create("core");

    tcase_add_checked_fixture(testcase, health_test_setup, health_test_teardown);
    tcase_add_test(testcase, test_health_livez_and_startupz);
    tcase_add_test(testcase, test_health_readyz_requires_context_when_configured);
    tcase_add_test(testcase, test_health_readyz_fails_for_unregistered_or_emergency_context);
    tcase_add_test(testcase, test_health_custom_ready_check_affects_probe);
    tcase_add_test(testcase, test_healthz_reports_aggregate_json);
    tcase_set_timeout(testcase, 20);

    suite_add_tcase(suite, testcase);
    return suite;
}

int
main(void)
{
    Suite *suite;
    SRunner *runner;
    int failed;

    suite = health_suite();
    runner = srunner_create(suite);
    srunner_run_all(runner, CK_NORMAL);
    failed = srunner_ntests_failed(runner);
    srunner_free(runner);
    return (failed == 0) ? 0 : 1;
}
