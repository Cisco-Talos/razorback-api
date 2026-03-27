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

#include "api_internal.h"
#include "health_internal.h"

#include <razorback/api.h>
#include <razorback/debug.h>
#include <razorback/health.h>
#include <razorback/list.h>
#include <razorback/lock.h>
#include <razorback/log.h>
#include <razorback/socket.h>
#include <razorback/thread.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HEALTH_DEFAULT_BIND_ADDRESS "127.0.0.1"
#define HEALTH_HTTP_OK "HTTP/1.1 200 OK"
#define HEALTH_HTTP_BAD_REQUEST "HTTP/1.1 400 Bad Request"
#define HEALTH_HTTP_METHOD_NOT_ALLOWED "HTTP/1.1 405 Method Not Allowed"
#define HEALTH_HTTP_NOT_FOUND "HTTP/1.1 404 Not Found"
#define HEALTH_HTTP_UNHEALTHY "HTTP/1.1 503 Service Unavailable"

struct RazorbackHealthCheck
{
    RazorbackHealthCheckId_t id;
    RazorbackHealthCheckKind_t kind;
    char *name;
    RazorbackHealthCheckFn callback;
    void *userData;
};

struct RazorbackHealthState
{
    Mutex_t *mutex;
    List_t *checks;
    Thread_t *thread;
    struct Socket *listener;
    char *bindAddress;
    uint16_t port;
    bool requireContextsForReady;
    atomic_bool startupComplete;
    RazorbackHealthCheckId_t nextCheckId;
};

struct RazorbackHealthReadinessState
{
    size_t contextCount;
    bool allRegistered;
    bool anyInspectionEmergency;
};

struct RazorbackHealthCheckSnapshotState
{
    RazorbackHealthCheckKind_t kind;
    List_t *snapshot;
    bool ok;
};

struct RazorbackHealthEvaluateState
{
    bool healthy;
};

struct RazorbackHealthHttpRequest
{
    bool headOnly;
    char path[256];
};

struct RazorbackHealthRemoveState
{
    RazorbackHealthCheckId_t id;
    bool removed;
};

static struct RazorbackHealthState sg_health = {
    .mutex = NULL,
    .checks = NULL,
    .thread = NULL,
    .listener = NULL,
    .bindAddress = NULL,
    .port = 0,
    .requireContextsForReady = false,
    .startupComplete = false,
    .nextCheckId = 1U
};

static int Health_Check_Cmp(void *a, void *b);
static int Health_Check_KeyCmp(void *a, const void *b);
static void Health_Check_Destroy(void *item);
static void *Health_Check_Clone(void *item);
static void Health_Thread(Thread_t *thread);
static bool Health_IsInitialized(void);
static bool Health_SendResponse(const struct Socket *socket,
                                const char *statusLine,
                                const char *contentType,
                                const char *body,
                                bool includeBody);
static bool Health_ParseRequestLine(const char *line,
                                    struct RazorbackHealthHttpRequest *request);
static bool Health_EvaluateInternal(RazorbackHealthCheckKind_t kind);
static bool Health_EvaluateBuiltIn(RazorbackHealthCheckKind_t kind);
static bool Health_EvaluateCustomChecks(RazorbackHealthCheckKind_t kind);
static int Health_CheckReadyContext(struct RazorbackContext *context, void *userData);
static int Health_SnapshotCheck(void *item, void *userData);
static int Health_RunCheck(void *item, void *userData);
static int Health_RemoveCheck(void *item, void *userData);
static bool Health_HandleClient(const struct Socket *socket);
static bool Health_SendProbeResponse(const struct Socket *socket,
                                     const struct RazorbackHealthHttpRequest *request,
                                     RazorbackHealthCheckKind_t kind);
static bool Health_SendAggregateResponse(const struct Socket *socket,
                                         const struct RazorbackHealthHttpRequest *request);

static int
Health_Check_Cmp(void *a, void *b)
{
    const struct RazorbackHealthCheck *left = a;
    const struct RazorbackHealthCheck *right = b;

    if (left->id < right->id)
        return -1;
    if (left->id > right->id)
        return 1;
    return 0;
}

static int
Health_Check_KeyCmp(void *a, const void *b)
{
    const struct RazorbackHealthCheck *check = a;
    const RazorbackHealthCheckId_t *id = b;

    if (check->id < *id)
        return -1;
    if (check->id > *id)
        return 1;
    return 0;
}

static void
Health_Check_Destroy(void *item)
{
    struct RazorbackHealthCheck *check = item;

    if (check == NULL)
        return;

    free(check->name);
    free(check);
}

static void *
Health_Check_Clone(void *item)
{
    struct RazorbackHealthCheck *source = item;
    struct RazorbackHealthCheck *clone;

    if ((clone = calloc(1, sizeof(*clone))) == NULL)
        return NULL;

    clone->id = source->id;
    clone->kind = source->kind;
    clone->callback = source->callback;
    clone->userData = source->userData;
    if (source->name != NULL && (clone->name = strdup(source->name)) == NULL) {
        free(clone);
        return NULL;
    }

    return clone;
}

bool
Health_Initialize(void)
{
    if (sg_health.mutex != NULL)
        return true;

    if ((sg_health.mutex = Mutex_Create(MUTEX_MODE_NORMAL)) == NULL)
        return false;

    sg_health.checks = List_Create(LIST_MODE_GENERIC,
                                   Health_Check_Cmp,
                                   Health_Check_KeyCmp,
                                   Health_Check_Destroy,
                                   Health_Check_Clone,
                                   NULL,
                                   NULL);
    if (sg_health.checks == NULL) {
        Mutex_Destroy(sg_health.mutex);
        sg_health.mutex = NULL;
        return false;
    }

    atomic_init(&sg_health.startupComplete, false);
    sg_health.nextCheckId = 1U;
    return true;
}

void
Health_Shutdown_Global(void)
{
    Razorback_Health_Stop();

    if (sg_health.mutex == NULL)
        return;

    Mutex_Lock(sg_health.mutex);
    if (sg_health.checks != NULL) {
        List_Destroy(sg_health.checks);
        sg_health.checks = NULL;
    }
    free(sg_health.bindAddress);
    sg_health.bindAddress = NULL;
    sg_health.port = 0;
    sg_health.requireContextsForReady = false;
    Mutex_Unlock(sg_health.mutex);

    Mutex_Destroy(sg_health.mutex);
    sg_health.mutex = NULL;
}

static bool
Health_IsInitialized(void)
{
    if (sg_health.mutex != NULL)
        return true;

    rzb_log(LOG_ERR, LOG_C_CORE,
            "%s: Health subsystem is not initialized. Call RZB_Init_API() first",
            __func__);
    return false;
}

SO_PUBLIC bool
Razorback_Health_Start(const RazorbackHealthServerConfig_t *config)
{
    struct Socket *listener;
    char *bindAddress;

    ASSERT(config != NULL);
    if (config == NULL)
        return false;

    if (!Health_IsInitialized())
        return false;

    if (config->port == 0U) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Health listener port must be non-zero", __func__);
        return false;
    }

    if ((bindAddress = strdup((config->bindAddress != NULL) ?
                              config->bindAddress : HEALTH_DEFAULT_BIND_ADDRESS)) == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to allocate bind address", __func__);
        return false;
    }

    if ((listener = Socket_Listen(bindAddress, config->port)) == NULL) {
        free(bindAddress);
        return false;
    }

    Mutex_Lock(sg_health.mutex);
    if (sg_health.thread != NULL) {
        Mutex_Unlock(sg_health.mutex);
        Socket_Close(listener);
        free(bindAddress);
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Health listener is already running", __func__);
        return false;
    }

    sg_health.listener = listener;
    sg_health.bindAddress = bindAddress;
    sg_health.port = config->port;
    sg_health.requireContextsForReady = config->requireContextsForReady;
    sg_health.thread = Thread_Launch(Health_Thread, NULL, "Health Listener", NULL);
    if (sg_health.thread == NULL) {
        Socket_Close(sg_health.listener);
        sg_health.listener = NULL;
        free(sg_health.bindAddress);
        sg_health.bindAddress = NULL;
        sg_health.port = 0;
        sg_health.requireContextsForReady = false;
        Mutex_Unlock(sg_health.mutex);
        return false;
    }
    Mutex_Unlock(sg_health.mutex);

    return true;
}

SO_PUBLIC void
Razorback_Health_Stop(void)
{
    Thread_t *thread = NULL;

    if (!Health_IsInitialized())
        return;

    Mutex_Lock(sg_health.mutex);
    if (sg_health.thread != NULL) {
        thread = sg_health.thread;
        sg_health.thread = NULL;
    }
    Mutex_Unlock(sg_health.mutex);

    if (thread != NULL) {
        Thread_StopAndJoin(thread);
        Thread_Destroy(thread);
    }

    Mutex_Lock(sg_health.mutex);
    if (sg_health.listener != NULL) {
        Socket_Close(sg_health.listener);
        sg_health.listener = NULL;
    }
    free(sg_health.bindAddress);
    sg_health.bindAddress = NULL;
    sg_health.port = 0;
    sg_health.requireContextsForReady = false;
    Mutex_Unlock(sg_health.mutex);
}

SO_PUBLIC bool
Razorback_Health_IsRunning(void)
{
    bool running;

    if (!Health_IsInitialized())
        return false;

    Mutex_Lock(sg_health.mutex);
    running = (sg_health.thread != NULL);
    Mutex_Unlock(sg_health.mutex);

    return running;
}

SO_PUBLIC void
Razorback_Health_SetStartupComplete(bool complete)
{
    if (!Health_IsInitialized())
        return;

    atomic_store(&sg_health.startupComplete, complete);
}

SO_PUBLIC bool
Razorback_Health_Evaluate(RazorbackHealthCheckKind_t kind)
{
    if (!Health_IsInitialized())
        return false;

    return Health_EvaluateInternal(kind);
}

SO_PUBLIC RazorbackHealthCheckId_t
Razorback_Health_RegisterCheck(RazorbackHealthCheckKind_t kind,
                               const char *name,
                               RazorbackHealthCheckFn callback,
                               void *userData)
{
    struct RazorbackHealthCheck *check;
    RazorbackHealthCheckId_t id;

    ASSERT(callback != NULL);
    if (callback == NULL)
        return 0U;

    if (!Health_IsInitialized())
        return 0U;

    if ((check = calloc(1, sizeof(*check))) == NULL)
        return 0U;

    if (name != NULL && (check->name = strdup(name)) == NULL) {
        free(check);
        return 0U;
    }

    check->kind = kind;
    check->callback = callback;
    check->userData = userData;

    Mutex_Lock(sg_health.mutex);
    id = sg_health.nextCheckId++;
    check->id = id;
    if (!List_Push(sg_health.checks, check)) {
        Mutex_Unlock(sg_health.mutex);
        Health_Check_Destroy(check);
        return 0U;
    }
    Mutex_Unlock(sg_health.mutex);

    return id;
}

static int
Health_RemoveCheck(void *item, void *userData)
{
    struct RazorbackHealthCheck *check = item;
    struct RazorbackHealthRemoveState *state = userData;

    if (check->id != state->id)
        return LIST_EACH_OK;

    state->removed = true;
    return LIST_EACH_REMOVE;
}

SO_PUBLIC bool
Razorback_Health_UnregisterCheck(RazorbackHealthCheckId_t id)
{
    struct RazorbackHealthRemoveState state = { id, false };

    if (!Health_IsInitialized())
        return false;

    if (id == 0U)
        return false;

    Mutex_Lock(sg_health.mutex);
    List_ForEach(sg_health.checks, Health_RemoveCheck, &state);
    Mutex_Unlock(sg_health.mutex);

    return state.removed;
}

static bool
Health_EvaluateInternal(RazorbackHealthCheckKind_t kind)
{
    if (!Health_EvaluateBuiltIn(kind))
        return false;

    return Health_EvaluateCustomChecks(kind);
}

static bool
Health_EvaluateBuiltIn(RazorbackHealthCheckKind_t kind)
{
    struct RazorbackHealthReadinessState readyState = { 0U, true, false };
    bool requireContextsForReady;

    switch (kind) {
    case RAZORBACK_HEALTH_LIVE:
        return true;

    case RAZORBACK_HEALTH_STARTUP:
        return atomic_load(&sg_health.startupComplete);

    case RAZORBACK_HEALTH_READY:
        if (!atomic_load(&sg_health.startupComplete))
            return false;

        Mutex_Lock(sg_health.mutex);
        requireContextsForReady = sg_health.requireContextsForReady;
        Mutex_Unlock(sg_health.mutex);

        if (!Razorback_ForEach_Context(Health_CheckReadyContext, &readyState))
            return false;

        if (requireContextsForReady && readyState.contextCount == 0U)
            return false;

        return readyState.allRegistered && !readyState.anyInspectionEmergency;

    default:
        return false;
    }
}

static int
Health_CheckReadyContext(struct RazorbackContext *context, void *userData)
{
    struct RazorbackHealthReadinessState *state = userData;

    state->contextCount++;
    if (!context->regOk) {
        state->allRegistered = false;
        return LIST_EACH_END;
    }

    if (context->inspector.emergencyLock != NULL) {
        Mutex_Lock(context->inspector.emergencyLock);
        if (context->inspector.emergencyShutdownRequested)
            state->anyInspectionEmergency = true;
        Mutex_Unlock(context->inspector.emergencyLock);
        if (state->anyInspectionEmergency)
            return LIST_EACH_END;
    }

    return LIST_EACH_OK;
}

static int
Health_SnapshotCheck(void *item, void *userData)
{
    struct RazorbackHealthCheck *check = item;
    struct RazorbackHealthCheckSnapshotState *state = userData;
    struct RazorbackHealthCheck *clone;

    if (check->kind != state->kind)
        return LIST_EACH_OK;

    if ((clone = Health_Check_Clone(check)) == NULL) {
        state->ok = false;
        return LIST_EACH_ERROR;
    }

    if (!List_Push(state->snapshot, clone)) {
        Health_Check_Destroy(clone);
        state->ok = false;
        return LIST_EACH_ERROR;
    }

    return LIST_EACH_OK;
}

static int
Health_RunCheck(void *item, void *userData)
{
    struct RazorbackHealthCheck *check = item;
    struct RazorbackHealthEvaluateState *state = userData;

    if (!check->callback(check->userData)) {
        rzb_log(LOG_DEBUG, LOG_C_CORE,
                "%s: Health check '%s' failed",
                __func__,
                (check->name != NULL) ? check->name : "(unnamed)");
        state->healthy = false;
        return LIST_EACH_END;
    }

    return LIST_EACH_OK;
}

static bool
Health_EvaluateCustomChecks(RazorbackHealthCheckKind_t kind)
{
    List_t *snapshot;
    struct RazorbackHealthCheckSnapshotState snapshotState;
    struct RazorbackHealthEvaluateState evaluateState = { true };

    if ((snapshot = List_Create(LIST_MODE_GENERIC,
                                Health_Check_Cmp,
                                Health_Check_KeyCmp,
                                Health_Check_Destroy,
                                Health_Check_Clone,
                                NULL,
                                NULL)) == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to create health snapshot list", __func__);
        return false;
    }

    snapshotState.kind = kind;
    snapshotState.snapshot = snapshot;
    snapshotState.ok = true;

    Mutex_Lock(sg_health.mutex);
    if (!List_ForEach(sg_health.checks, Health_SnapshotCheck, &snapshotState))
        snapshotState.ok = false;
    Mutex_Unlock(sg_health.mutex);

    if (snapshotState.ok)
        List_ForEach(snapshot, Health_RunCheck, &evaluateState);

    List_Destroy(snapshot);
    return snapshotState.ok && evaluateState.healthy;
}

static bool
Health_SendResponse(const struct Socket *socket,
                    const char *statusLine,
                    const char *contentType,
                    const char *body,
                    bool includeBody)
{
    char header[512];
    int headerLength;
    size_t bodyLength = 0U;

    ASSERT(socket != NULL);
    if (socket == NULL)
        return false;

    ASSERT(statusLine != NULL);
    if (statusLine == NULL)
        return false;

    ASSERT(contentType != NULL);
    if (contentType == NULL)
        return false;

    if (body != NULL)
        bodyLength = strlen(body);

    headerLength = snprintf(header, sizeof(header),
                            "%s\r\n"
                            "Content-Type: %s\r\n"
                            "Content-Length: %zu\r\n"
                            "Connection: close\r\n"
                            "\r\n",
                            statusLine, contentType, bodyLength);
    if (headerLength < 0 || (size_t)headerLength >= sizeof(header))
        return false;

    if (Socket_Tx(socket, (size_t)headerLength, (const uint8_t *)header) != headerLength)
        return false;

    if (includeBody && body != NULL && bodyLength > 0U) {
        if (Socket_Tx(socket, bodyLength, (const uint8_t *)body) != (ssize_t)bodyLength)
            return false;
    }

    return true;
}

static bool
Health_ParseRequestLine(const char *line, struct RazorbackHealthHttpRequest *request)
{
    char method[16];
    char version[16];

    if (sscanf(line, "%15s %255s %15s", method, request->path, version) != 3)
        return false;

    if (strcmp(method, "GET") == 0) {
        request->headOnly = false;
        return true;
    }
    if (strcmp(method, "HEAD") == 0) {
        request->headOnly = true;
        return true;
    }

    request->path[0] = '\0';
    return false;
}

static bool
Health_SendProbeResponse(const struct Socket *socket,
                         const struct RazorbackHealthHttpRequest *request,
                         RazorbackHealthCheckKind_t kind)
{
    bool healthy = Health_EvaluateInternal(kind);

    return Health_SendResponse(socket,
                               healthy ? HEALTH_HTTP_OK : HEALTH_HTTP_UNHEALTHY,
                               "text/plain; charset=utf-8",
                               healthy ? "ok\n" : "unhealthy\n",
                               !request->headOnly);
}

static bool
Health_SendAggregateResponse(const struct Socket *socket,
                             const struct RazorbackHealthHttpRequest *request)
{
    char body[128];
    bool live = Health_EvaluateInternal(RAZORBACK_HEALTH_LIVE);
    bool ready = Health_EvaluateInternal(RAZORBACK_HEALTH_READY);
    bool startup = Health_EvaluateInternal(RAZORBACK_HEALTH_STARTUP);
    bool healthy = live && ready && startup;
    int length;

    length = snprintf(body, sizeof(body),
                      "{\"live\":%s,\"ready\":%s,\"startup\":%s}\n",
                      live ? "true" : "false",
                      ready ? "true" : "false",
                      startup ? "true" : "false");
    if (length < 0 || (size_t)length >= sizeof(body))
        return false;

    return Health_SendResponse(socket,
                               healthy ? HEALTH_HTTP_OK : HEALTH_HTTP_UNHEALTHY,
                               "application/json",
                               body,
                               !request->headOnly);
}

static bool
Health_HandleClient(const struct Socket *socket)
{
    uint8_t *line = NULL;
    ssize_t length;
    struct RazorbackHealthHttpRequest request;
    bool parsed;

    memset(&request, 0, sizeof(request));

    length = Socket_Rx_Until(socket, &line, '\n');
    if (length <= 0) {
        free(line);
        return false;
    }

    if (length > 0 && line[length - 1] == '\n')
        line[length - 1] = '\0';
    if (length > 1 && line[length - 2] == '\r')
        line[length - 2] = '\0';

    parsed = Health_ParseRequestLine((const char *)line, &request);
    free(line);

    if (!parsed) {
        if (request.path[0] == '\0') {
            return Health_SendResponse(socket,
                                       HEALTH_HTTP_METHOD_NOT_ALLOWED,
                                       "text/plain; charset=utf-8",
                                       "method not allowed\n",
                                       true);
        }

        return Health_SendResponse(socket,
                                   HEALTH_HTTP_BAD_REQUEST,
                                   "text/plain; charset=utf-8",
                                   "bad request\n",
                                   true);
    }

    if (strcmp(request.path, "/livez") == 0)
        return Health_SendProbeResponse(socket, &request, RAZORBACK_HEALTH_LIVE);
    if (strcmp(request.path, "/readyz") == 0)
        return Health_SendProbeResponse(socket, &request, RAZORBACK_HEALTH_READY);
    if (strcmp(request.path, "/startupz") == 0)
        return Health_SendProbeResponse(socket, &request, RAZORBACK_HEALTH_STARTUP);
    if (strcmp(request.path, "/healthz") == 0)
        return Health_SendAggregateResponse(socket, &request);

    return Health_SendResponse(socket,
                               HEALTH_HTTP_NOT_FOUND,
                               "text/plain; charset=utf-8",
                               "not found\n",
                               !request.headOnly);
}

static void
Health_Thread(Thread_t *thread)
{
    struct Socket *listener;

    ASSERT(thread != NULL);
    if (thread == NULL)
        return;

    Mutex_Lock(sg_health.mutex);
    listener = sg_health.listener;
    Mutex_Unlock(sg_health.mutex);

    if (listener == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Health listener socket is NULL", __func__);
        return;
    }

    while (!Thread_IsStopped(thread)) {
        struct Socket *client = NULL;
        int acceptResult = Socket_Accept(&client, listener);

        if (acceptResult < 0) {
            if (!Thread_IsStopped(thread))
                Thread_Sleep(50U);
            continue;
        }
        if (acceptResult == 0)
            continue;

        (void)Health_HandleClient(client);
        Socket_Close(client);
    }
}
