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

#include <razorback/runtime_next.h>
#include <razorback/messages_next.h>

#include <json.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <uuid/uuid.h>

struct RzbNextRuntime
{
    char *nuggetUuid;
    char *processUuid;
    char *inFlightRequestId;
    char *acceptedRequestId;
    char *registrationGeneration;
    char *runtimePolicy;
    uint64_t livenessInterval;
    uint64_t livenessFreshnessWindow;
    uint64_t livenessClockSkewTolerance;
    uint32_t inFlightWork;
    struct RzbNextRuntimeCallbacks callbacks;
    uint32_t callbackFailureCount;
    char lastCallbackFailure[64];
    bool dependencyPaused;
    enum RzbNextRuntimeState state;
};

static bool sg_runtimeActive = false;

static bool RzbNextRuntime_IsUuid(const char *value);
static char *RzbNextRuntime_Strdup(const char *value);
static void RzbNextRuntime_Free(char **value);
static void RzbNextRuntime_CopyFixed(char *dest, size_t destSize,
                                     const char *source);
static json_object *RzbNextRuntime_ParseValidated(const char *jsonMessage,
                                                  const char *schemaName);
static const char *RzbNextRuntime_GetString(json_object *object,
                                            const char *field);
static uint64_t RzbNextRuntime_GetOptionalUint64(json_object *object,
                                                const char *field);
static bool RzbNextRuntime_RequireNugget(const RzbNextRuntime_t *runtime,
                                         const char *actual);
static bool RzbNextRuntime_RequireRequest(const RzbNextRuntime_t *runtime,
                                          const char *actual);
static const char *RzbNextRuntime_Availability(
    const RzbNextRuntime_t *runtime
);
static char *RzbNextRuntime_RenderJson(json_object *object);
static void RzbNextRuntime_RecordCallbackFailure(
    RzbNextRuntime_t *runtime,
    const char *callback
);
static void RzbNextRuntime_InvokeCallback(
    RzbNextRuntime_t *runtime,
    const char *callbackName,
    RzbNextRuntimeCallbackFn callback
);
static void RzbNextRuntime_InvokeRegisteredCallback(
    RzbNextRuntime_t *runtime,
    const struct RzbNextRuntimeTransition *transition
);
static void RzbNextRuntime_InvokeCacheInvalidateCallback(
    RzbNextRuntime_t *runtime,
    const char *invalidationId
);
static uint64_t RzbNextRuntime_DelayForFailure(
    struct RzbNextRuntimeRetryPolicy policy,
    uint32_t consecutiveFailureCount,
    uint64_t retryAfter,
    uint8_t jitterPercentile
);

static bool
RzbNextRuntime_IsUuid(const char *value)
{
    size_t index;

    if (value == NULL || strlen(value) != 36)
        return false;
    for (index = 0; index < 36; index++) {
        if (index == 8 || index == 13 || index == 18 || index == 23) {
            if (value[index] != '-')
                return false;
        } else if (!((value[index] >= '0' && value[index] <= '9') ||
                     (value[index] >= 'a' && value[index] <= 'f'))) {
            return false;
        }
    }
    return true;
}

static char *
RzbNextRuntime_Strdup(const char *value)
{
    char *copy;
    size_t length;

    if (value == NULL)
        return NULL;
    length = strlen(value) + 1U;
    copy = malloc(length);
    if (copy == NULL)
        return NULL;
    memcpy(copy, value, length);
    return copy;
}

static void
RzbNextRuntime_Free(char **value)
{
    if (value == NULL || *value == NULL)
        return;
    free(*value);
    *value = NULL;
}

static void
RzbNextRuntime_CopyFixed(char *dest, size_t destSize, const char *source)
{
    if (dest == NULL || destSize == 0U)
        return;
    dest[0] = '\0';
    if (source == NULL)
        return;
    snprintf(dest, destSize, "%s", source);
}

static json_object *
RzbNextRuntime_ParseValidated(const char *jsonMessage, const char *schemaName)
{
    json_object *object;
    const char *actualSchemaName;

    if (jsonMessage == NULL || schemaName == NULL)
        return NULL;
    if (!RzbNextMessage_Validate(jsonMessage))
        return NULL;
    object = json_tokener_parse(jsonMessage);
    if (object == NULL)
        return NULL;
    if (!json_object_is_type(object, json_type_object)) {
        json_object_put(object);
        return NULL;
    }
    actualSchemaName = RzbNextRuntime_GetString(object, "schema_name");
    if (actualSchemaName == NULL || strcmp(actualSchemaName, schemaName) != 0) {
        json_object_put(object);
        return NULL;
    }
    return object;
}

static const char *
RzbNextRuntime_GetString(json_object *object, const char *field)
{
    json_object *value;

    if (object == NULL || field == NULL)
        return NULL;
    if (!json_object_object_get_ex(object, field, &value) ||
        !json_object_is_type(value, json_type_string)) {
        return NULL;
    }
    return json_object_get_string(value);
}

static uint64_t
RzbNextRuntime_GetOptionalUint64(json_object *object, const char *field)
{
    json_object *value;

    if (object == NULL || field == NULL)
        return 0U;
    if (!json_object_object_get_ex(object, field, &value) ||
        !json_object_is_type(value, json_type_int)) {
        return 0U;
    }
    return (uint64_t)json_object_get_int64(value);
}

static bool
RzbNextRuntime_RequireNugget(const RzbNextRuntime_t *runtime, const char *actual)
{
    return runtime != NULL && actual != NULL &&
           strcmp(runtime->nuggetUuid, actual) == 0;
}

static bool
RzbNextRuntime_RequireRequest(const RzbNextRuntime_t *runtime, const char *actual)
{
    return runtime != NULL && actual != NULL &&
           runtime->inFlightRequestId != NULL &&
           strcmp(runtime->inFlightRequestId, actual) == 0;
}

static const char *
RzbNextRuntime_Availability(const RzbNextRuntime_t *runtime)
{
    if (runtime == NULL)
        return "failed";
    switch (runtime->state) {
    case RZB_NEXT_RUNTIME_READY:
    case RZB_NEXT_RUNTIME_PAUSED:
        if (runtime->dependencyPaused)
            return "dependency_paused";
        return "ready";
    case RZB_NEXT_RUNTIME_DRAINING:
        return "draining";
    case RZB_NEXT_RUNTIME_STOPPED:
    case RZB_NEXT_RUNTIME_FAILED:
        return "failed";
    default:
        return "registration_gated";
    }
}

static char *
RzbNextRuntime_RenderJson(json_object *object)
{
    const char *jsonText;
    char *copy;

    if (object == NULL)
        return NULL;
    jsonText = json_object_to_json_string_ext(object, JSON_C_TO_STRING_PLAIN);
    copy = RzbNextRuntime_Strdup(jsonText);
    json_object_put(object);
    return copy;
}

static void
RzbNextRuntime_RecordCallbackFailure(RzbNextRuntime_t *runtime,
                                     const char *callback)
{
    if (runtime == NULL || callback == NULL)
        return;
    runtime->callbackFailureCount++;
    RzbNextRuntime_CopyFixed(runtime->lastCallbackFailure,
                             sizeof(runtime->lastCallbackFailure), callback);
    if (runtime->callbacks.onError != NULL && strcmp(callback, "onError") != 0) {
        if (!runtime->callbacks.onError(runtime, "callback_failed", false,
                                        runtime->callbacks.userData)) {
            runtime->callbackFailureCount++;
            RzbNextRuntime_CopyFixed(runtime->lastCallbackFailure,
                                     sizeof(runtime->lastCallbackFailure),
                                     "onError");
        }
    }
}

static void
RzbNextRuntime_InvokeCallback(RzbNextRuntime_t *runtime,
                              const char *callbackName,
                              RzbNextRuntimeCallbackFn callback)
{
    if (runtime == NULL || callbackName == NULL || callback == NULL)
        return;
    if (!callback(runtime, runtime->callbacks.userData))
        RzbNextRuntime_RecordCallbackFailure(runtime, callbackName);
}

static void
RzbNextRuntime_InvokeRegisteredCallback(
    RzbNextRuntime_t *runtime,
    const struct RzbNextRuntimeTransition *transition
)
{
    if (runtime == NULL || transition == NULL ||
        runtime->callbacks.onRegistered == NULL) {
        return;
    }
    if (!runtime->callbacks.onRegistered(runtime, transition,
                                         runtime->callbacks.userData)) {
        RzbNextRuntime_RecordCallbackFailure(runtime, "onRegistered");
    }
}

static void
RzbNextRuntime_InvokeCacheInvalidateCallback(RzbNextRuntime_t *runtime,
                                             const char *invalidationId)
{
    if (runtime == NULL || invalidationId == NULL ||
        runtime->callbacks.onCacheInvalidate == NULL) {
        return;
    }
    if (!runtime->callbacks.onCacheInvalidate(runtime, invalidationId,
                                              runtime->callbacks.userData)) {
        RzbNextRuntime_RecordCallbackFailure(runtime, "onCacheInvalidate");
    }
}

const char *
RzbNextRuntime_StateString(enum RzbNextRuntimeState state)
{
    switch (state) {
    case RZB_NEXT_RUNTIME_STARTING:
        return "starting";
    case RZB_NEXT_RUNTIME_WAITING_FOR_DISPATCHER:
        return "waiting_for_dispatcher";
    case RZB_NEXT_RUNTIME_REGISTERING:
        return "registering";
    case RZB_NEXT_RUNTIME_READY:
        return "ready";
    case RZB_NEXT_RUNTIME_PAUSED:
        return "paused";
    case RZB_NEXT_RUNTIME_DRAINING:
        return "draining";
    case RZB_NEXT_RUNTIME_STOPPED:
        return "stopped";
    case RZB_NEXT_RUNTIME_FAILED:
        return "failed";
    default:
        return "unknown";
    }
}

RzbNextRuntime_t *
RzbNextRuntime_Create(const char *nuggetUuid, const char *processUuid)
{
    RzbNextRuntime_t *runtime;

    if (sg_runtimeActive || !RzbNextRuntime_IsUuid(nuggetUuid) ||
        !RzbNextRuntime_IsUuid(processUuid)) {
        return NULL;
    }
    runtime = calloc(1U, sizeof(*runtime));
    if (runtime == NULL)
        return NULL;
    runtime->nuggetUuid = RzbNextRuntime_Strdup(nuggetUuid);
    runtime->processUuid = RzbNextRuntime_Strdup(processUuid);
    runtime->runtimePolicy = RzbNextRuntime_Strdup("running");
    runtime->state = RZB_NEXT_RUNTIME_STARTING;
    if (runtime->nuggetUuid == NULL || runtime->processUuid == NULL ||
        runtime->runtimePolicy == NULL) {
        RzbNextRuntime_Destroy(runtime);
        return NULL;
    }
    sg_runtimeActive = true;
    return runtime;
}

RzbNextRuntime_t *
RzbNextRuntime_CreateGenerated(const char *nuggetUuid)
{
    uuid_t generated;
    char processUuid[37];

    uuid_generate_random(generated);
    uuid_unparse_lower(generated, processUuid);
    return RzbNextRuntime_Create(nuggetUuid, processUuid);
}

void
RzbNextRuntime_Destroy(RzbNextRuntime_t *runtime)
{
    if (runtime == NULL)
        return;
    RzbNextRuntime_Free(&runtime->nuggetUuid);
    RzbNextRuntime_Free(&runtime->processUuid);
    RzbNextRuntime_Free(&runtime->inFlightRequestId);
    RzbNextRuntime_Free(&runtime->acceptedRequestId);
    RzbNextRuntime_Free(&runtime->registrationGeneration);
    RzbNextRuntime_Free(&runtime->runtimePolicy);
    free(runtime);
    sg_runtimeActive = false;
}

bool
RzbNextRuntime_SetCallbacks(RzbNextRuntime_t *runtime,
                            const struct RzbNextRuntimeCallbacks *callbacks)
{
    if (runtime == NULL)
        return false;
    if (callbacks == NULL) {
        memset(&runtime->callbacks, 0, sizeof(runtime->callbacks));
    } else {
        runtime->callbacks = *callbacks;
    }
    return true;
}

uint32_t
RzbNextRuntime_CallbackFailureCount(const RzbNextRuntime_t *runtime)
{
    return runtime == NULL ? 0U : runtime->callbackFailureCount;
}

const char *
RzbNextRuntime_LastCallbackFailure(const RzbNextRuntime_t *runtime)
{
    if (runtime == NULL || runtime->callbackFailureCount == 0U)
        return NULL;
    return runtime->lastCallbackFailure;
}

enum RzbNextRuntimeState
RzbNextRuntime_State(const RzbNextRuntime_t *runtime)
{
    if (runtime == NULL)
        return RZB_NEXT_RUNTIME_FAILED;
    return runtime->state;
}

const char *
RzbNextRuntime_ProcessUuid(const RzbNextRuntime_t *runtime)
{
    if (runtime == NULL)
        return NULL;
    return runtime->processUuid;
}

const char *
RzbNextRuntime_RegistrationGeneration(const RzbNextRuntime_t *runtime)
{
    if (runtime == NULL)
        return NULL;
    return runtime->registrationGeneration;
}

struct RzbNextRuntimeHealth
RzbNextRuntime_Health(const RzbNextRuntime_t *runtime)
{
    struct RzbNextRuntimeHealth health = { true, false, false };

    if (runtime == NULL)
        return health;
    switch (runtime->state) {
    case RZB_NEXT_RUNTIME_STARTING:
    case RZB_NEXT_RUNTIME_WAITING_FOR_DISPATCHER:
    case RZB_NEXT_RUNTIME_REGISTERING:
    case RZB_NEXT_RUNTIME_READY:
    case RZB_NEXT_RUNTIME_PAUSED:
    case RZB_NEXT_RUNTIME_DRAINING:
        health.healthz = true;
        break;
    default:
        health.healthz = false;
        break;
    }
    health.readyz = runtime->state == RZB_NEXT_RUNTIME_READY &&
                    !runtime->dependencyPaused;
    return health;
}

bool
RzbNextRuntime_HealthStartupCheck(void *userData)
{
    return RzbNextRuntime_Health(userData).startupz;
}

bool
RzbNextRuntime_HealthLiveCheck(void *userData)
{
    return RzbNextRuntime_Health(userData).healthz;
}

bool
RzbNextRuntime_HealthReadyCheck(void *userData)
{
    return RzbNextRuntime_Health(userData).readyz;
}

void
RzbNextRuntime_Initialize(RzbNextRuntime_t *runtime)
{
    if (runtime != NULL && runtime->state == RZB_NEXT_RUNTIME_STARTING)
        runtime->state = RZB_NEXT_RUNTIME_WAITING_FOR_DISPATCHER;
}

bool
RzbNextRuntime_ObserveDispatcherHello(RzbNextRuntime_t *runtime,
                                      const char *jsonMessage)
{
    if (runtime == NULL || !RzbNextCnc_IsReadyDispatcherHello(jsonMessage))
        return false;
    if (runtime->state == RZB_NEXT_RUNTIME_STARTING ||
        runtime->state == RZB_NEXT_RUNTIME_WAITING_FOR_DISPATCHER) {
        runtime->state = RZB_NEXT_RUNTIME_REGISTERING;
    }
    return true;
}

bool
RzbNextRuntime_BeginRegistration(RzbNextRuntime_t *runtime, const char *requestId)
{
    char *copy;

    if (runtime == NULL || !RzbNextRuntime_IsUuid(requestId))
        return false;
    copy = RzbNextRuntime_Strdup(requestId);
    if (copy == NULL)
        return false;
    RzbNextRuntime_Free(&runtime->inFlightRequestId);
    runtime->inFlightRequestId = copy;
    runtime->state = RZB_NEXT_RUNTIME_REGISTERING;
    return true;
}

bool
RzbNextRuntime_RegistrationAccepted(RzbNextRuntime_t *runtime,
                                    const char *jsonMessage,
                                    struct RzbNextRuntimeTransition *transition)
{
    json_object *object;
    const char *requestId;
    const char *nuggetUuid;
    const char *generation;
    const char *policy;
    uint64_t interval = 0U;
    uint64_t freshness = 0U;
    uint64_t skew = 0U;
    bool success;
    struct RzbNextRuntimeTransition appliedTransition;

    if (transition != NULL)
        memset(transition, 0, sizeof(*transition));
    memset(&appliedTransition, 0, sizeof(appliedTransition));
    if (runtime == NULL)
        return false;
    object = RzbNextRuntime_ParseValidated(
        jsonMessage, RZB_NEXT_SCHEMA_CNC_REGISTRATION_ACCEPTED);
    if (object == NULL)
        return false;
    requestId = RzbNextRuntime_GetString(object, "request_id");
    nuggetUuid = RzbNextRuntime_GetString(object, "nugget_uuid");
    generation = RzbNextRuntime_GetString(object, "registration_generation");
    policy = RzbNextRuntime_GetString(object, "effective_runtime_policy");
    if (!RzbNextRuntime_RequireNugget(runtime, nuggetUuid) ||
        !RzbNextRuntime_RequireRequest(runtime, requestId) ||
        !RzbNextCnc_RegistrationAcceptedTiming(jsonMessage, &interval,
                                               &freshness, &skew)) {
        json_object_put(object);
        return false;
    }

    RzbNextRuntime_Free(&runtime->acceptedRequestId);
    RzbNextRuntime_Free(&runtime->registrationGeneration);
    RzbNextRuntime_Free(&runtime->runtimePolicy);
    runtime->acceptedRequestId = RzbNextRuntime_Strdup(requestId);
    runtime->registrationGeneration = RzbNextRuntime_Strdup(generation);
    runtime->runtimePolicy = RzbNextRuntime_Strdup(policy);
    RzbNextRuntime_Free(&runtime->inFlightRequestId);
    runtime->livenessInterval = interval;
    runtime->livenessFreshnessWindow = freshness;
    runtime->livenessClockSkewTolerance = skew;
    runtime->state = strcmp(policy, "running") == 0
                     ? RZB_NEXT_RUNTIME_READY
                     : RZB_NEXT_RUNTIME_PAUSED;
    appliedTransition.kind = RZB_NEXT_RUNTIME_TRANSITION_REGISTERED;
    appliedTransition.ready = runtime->state == RZB_NEXT_RUNTIME_READY;
    RzbNextRuntime_CopyFixed(appliedTransition.generation,
                             sizeof(appliedTransition.generation), generation);
    if (transition != NULL)
        *transition = appliedTransition;
    success = runtime->acceptedRequestId != NULL &&
              runtime->registrationGeneration != NULL &&
              runtime->runtimePolicy != NULL;
    if (success) {
        RzbNextRuntime_InvokeRegisteredCallback(runtime, &appliedTransition);
        if (appliedTransition.ready) {
            RzbNextRuntime_InvokeCallback(runtime, "onReady",
                                          runtime->callbacks.onReady);
        }
    }
    json_object_put(object);
    return success;
}

bool
RzbNextRuntime_RegistrationRejected(RzbNextRuntime_t *runtime,
                                    const char *jsonMessage,
                                    struct RzbNextRuntimeTransition *transition)
{
    json_object *object;
    const char *requestId;
    const char *nuggetUuid;
    const char *reasonCode;
    json_object *retryableObject = NULL;
    bool retryable;

    if (transition != NULL)
        memset(transition, 0, sizeof(*transition));
    if (runtime == NULL)
        return false;
    object = RzbNextRuntime_ParseValidated(
        jsonMessage, RZB_NEXT_SCHEMA_CNC_REGISTRATION_REJECTED);
    if (object == NULL)
        return false;
    requestId = RzbNextRuntime_GetString(object, "request_id");
    nuggetUuid = RzbNextRuntime_GetString(object, "nugget_uuid");
    reasonCode = RzbNextRuntime_GetString(object, "reason_code");
    if (!RzbNextRuntime_RequireNugget(runtime, nuggetUuid) ||
        !RzbNextRuntime_RequireRequest(runtime, requestId) ||
        !json_object_object_get_ex(object, "retryable", &retryableObject)) {
        json_object_put(object);
        return false;
    }
    retryable = json_object_get_boolean(retryableObject);
    RzbNextRuntime_Free(&runtime->inFlightRequestId);
    if (retryable) {
        runtime->state = RZB_NEXT_RUNTIME_REGISTERING;
    } else {
        RzbNextRuntime_Free(&runtime->acceptedRequestId);
        RzbNextRuntime_Free(&runtime->registrationGeneration);
        runtime->state = RZB_NEXT_RUNTIME_FAILED;
    }
    if (transition != NULL) {
        transition->kind = RZB_NEXT_RUNTIME_TRANSITION_REJECTED;
        transition->retryable = retryable;
        transition->retryAfter =
            RzbNextRuntime_GetOptionalUint64(object, "retry_after");
        RzbNextRuntime_CopyFixed(transition->reasonCode,
                                 sizeof(transition->reasonCode), reasonCode);
    }
    if (!retryable && runtime->callbacks.onError != NULL &&
        !runtime->callbacks.onError(runtime, reasonCode, false,
                                    runtime->callbacks.userData)) {
        RzbNextRuntime_RecordCallbackFailure(runtime, "onError");
    }
    json_object_put(object);
    return true;
}

bool
RzbNextRuntime_ApplyDirectedCommand(RzbNextRuntime_t *runtime,
                                    const char *jsonMessage,
                                    struct RzbNextRuntimeDirectedResult *result)
{
    json_object *object;
    const char *target;
    const char *generation;
    const char *command;
    const char *invalidationId;
    bool hadAcceptedGeneration;

    if (result != NULL)
        memset(result, 0, sizeof(*result));
    if (runtime == NULL)
        return false;
    object = RzbNextRuntime_ParseValidated(
        jsonMessage, RZB_NEXT_SCHEMA_CNC_DIRECTED_COMMAND);
    if (object == NULL)
        return false;
    target = RzbNextRuntime_GetString(object, "target_nugget_uuid");
    generation = RzbNextRuntime_GetString(object, "registration_generation");
    command = RzbNextRuntime_GetString(object, "command");
    if (!RzbNextRuntime_RequireNugget(runtime, target)) {
        json_object_put(object);
        return false;
    }
    hadAcceptedGeneration = runtime->registrationGeneration != NULL;
    if (!hadAcceptedGeneration ||
        strcmp(runtime->registrationGeneration, generation) != 0) {
        RzbNextRuntime_Free(&runtime->acceptedRequestId);
        RzbNextRuntime_Free(&runtime->registrationGeneration);
        runtime->state = RZB_NEXT_RUNTIME_REGISTERING;
        if (result != NULL) {
            result->effect = hadAcceptedGeneration
                             ? RZB_NEXT_RUNTIME_DIRECTED_REREGISTER
                             : RZB_NEXT_RUNTIME_DIRECTED_IGNORED_STALE;
            result->staleGeneration = true;
        }
        json_object_put(object);
        return true;
    }

    if (strcmp(command, "pause") == 0) {
        RzbNextRuntime_PauseNewWork(runtime);
        if (result != NULL)
            result->effect = RZB_NEXT_RUNTIME_DIRECTED_PAUSE;
        RzbNextRuntime_InvokeCallback(runtime, "onPause",
                                      runtime->callbacks.onPause);
    } else if (strcmp(command, "go") == 0) {
        if (!RzbNextRuntime_ResumeWhenReady(runtime)) {
            json_object_put(object);
            return false;
        }
        if (result != NULL)
            result->effect = RZB_NEXT_RUNTIME_DIRECTED_RESUME;
        RzbNextRuntime_InvokeCallback(runtime, "onResume",
                                      runtime->callbacks.onResume);
    } else if (strcmp(command, "terminate") == 0) {
        RzbNextRuntime_BeginDraining(runtime);
        if (result != NULL)
            result->effect = RZB_NEXT_RUNTIME_DIRECTED_SHUTDOWN;
        RzbNextRuntime_InvokeCallback(runtime, "onShutdown",
                                      runtime->callbacks.onShutdown);
    } else if (strcmp(command, "re_register") == 0) {
        RzbNextRuntime_Free(&runtime->acceptedRequestId);
        RzbNextRuntime_Free(&runtime->registrationGeneration);
        runtime->state = RZB_NEXT_RUNTIME_REGISTERING;
        if (result != NULL)
            result->effect = RZB_NEXT_RUNTIME_DIRECTED_REREGISTER;
    } else if (strcmp(command, "cache_invalidate") == 0) {
        if (result != NULL) {
            result->effect = RZB_NEXT_RUNTIME_DIRECTED_CACHE_INVALIDATE;
            invalidationId = RzbNextRuntime_GetString(object, "invalidation_id");
            RzbNextRuntime_CopyFixed(result->invalidationId,
                                     sizeof(result->invalidationId),
                                     invalidationId);
        }
        RzbNextRuntime_InvokeCacheInvalidateCallback(
            runtime,
            RzbNextRuntime_GetString(object, "invalidation_id")
        );
    } else {
        json_object_put(object);
        return false;
    }
    json_object_put(object);
    return true;
}

bool
RzbNextRuntime_LivenessPlan(const RzbNextRuntime_t *runtime,
                            const char *createdAt,
                            struct RzbNextRuntimeLivenessPlan *plan)
{
    json_object *object;

    if (plan != NULL)
        memset(plan, 0, sizeof(*plan));
    if (runtime == NULL || plan == NULL ||
        runtime->registrationGeneration == NULL || runtime->runtimePolicy == NULL ||
        createdAt == NULL) {
        return false;
    }
    object = json_object_new_object();
    if (object == NULL)
        return false;
    json_object_object_add(object, "schema_name",
                           json_object_new_string(RZB_NEXT_SCHEMA_CNC_LIVENESS));
    json_object_object_add(object, "schema_version",
                           json_object_new_int(RZB_NEXT_SCHEMA_VERSION));
    json_object_object_add(object, "nugget_uuid",
                           json_object_new_string(runtime->nuggetUuid));
    json_object_object_add(object, "registration_generation",
                           json_object_new_string(runtime->registrationGeneration));
    json_object_object_add(object, "runtime_policy",
                           json_object_new_string(runtime->runtimePolicy));
    json_object_object_add(object, "availability",
                           json_object_new_string(RzbNextRuntime_Availability(runtime)));
    json_object_object_add(object, "created_at", json_object_new_string(createdAt));
    plan->message = RzbNextRuntime_RenderJson(object);
    if (plan->message == NULL || !RzbNextMessage_Validate(plan->message)) {
        RzbNextRuntime_LivenessPlanClear(plan);
        return false;
    }
    plan->interval = runtime->livenessInterval;
    plan->freshnessWindow = runtime->livenessFreshnessWindow;
    plan->clockSkewTolerance = runtime->livenessClockSkewTolerance;
    plan->messageExpiration = runtime->livenessFreshnessWindow;
    return true;
}

char *
RzbNextRuntime_BuildBye(const RzbNextRuntime_t *runtime,
                        const char *reason,
                        const char *createdAt)
{
    json_object *object;
    char *message;

    if (runtime == NULL || runtime->registrationGeneration == NULL ||
        reason == NULL || createdAt == NULL) {
        return NULL;
    }
    object = json_object_new_object();
    if (object == NULL)
        return NULL;
    json_object_object_add(object, "schema_name",
                           json_object_new_string(RZB_NEXT_SCHEMA_CNC_BYE));
    json_object_object_add(object, "schema_version",
                           json_object_new_int(RZB_NEXT_SCHEMA_VERSION));
    json_object_object_add(object, "nugget_uuid",
                           json_object_new_string(runtime->nuggetUuid));
    json_object_object_add(object, "registration_generation",
                           json_object_new_string(runtime->registrationGeneration));
    json_object_object_add(object, "reason", json_object_new_string(reason));
    json_object_object_add(object, "created_at", json_object_new_string(createdAt));
    message = RzbNextRuntime_RenderJson(object);
    if (message == NULL || !RzbNextMessage_Validate(message)) {
        free(message);
        return NULL;
    }
    return message;
}

void
RzbNextRuntime_LivenessPlanClear(struct RzbNextRuntimeLivenessPlan *plan)
{
    if (plan == NULL)
        return;
    free(plan->message);
    memset(plan, 0, sizeof(*plan));
}

bool
RzbNextRuntime_AcceptsWork(const RzbNextRuntime_t *runtime)
{
    return runtime != NULL && runtime->state == RZB_NEXT_RUNTIME_READY &&
           !runtime->dependencyPaused;
}

bool
RzbNextRuntime_StartWhenReady(RzbNextRuntime_t *runtime)
{
    if (!RzbNextRuntime_AcceptsWork(runtime))
        return false;
    runtime->inFlightWork++;
    return true;
}

void
RzbNextRuntime_CompleteWork(RzbNextRuntime_t *runtime)
{
    if (runtime == NULL || runtime->inFlightWork == 0U)
        return;
    runtime->inFlightWork--;
}

uint32_t
RzbNextRuntime_InFlightWork(const RzbNextRuntime_t *runtime)
{
    if (runtime == NULL)
        return 0U;
    return runtime->inFlightWork;
}

void
RzbNextRuntime_PauseNewWork(RzbNextRuntime_t *runtime)
{
    if (runtime == NULL ||
        runtime->state == RZB_NEXT_RUNTIME_DRAINING ||
        runtime->state == RZB_NEXT_RUNTIME_STOPPED ||
        runtime->state == RZB_NEXT_RUNTIME_FAILED) {
        return;
    }
    RzbNextRuntime_Free(&runtime->runtimePolicy);
    runtime->runtimePolicy = RzbNextRuntime_Strdup("paused");
    runtime->state = RZB_NEXT_RUNTIME_PAUSED;
}

bool
RzbNextRuntime_ResumeWhenReady(RzbNextRuntime_t *runtime)
{
    if (runtime == NULL || runtime->registrationGeneration == NULL ||
        runtime->state == RZB_NEXT_RUNTIME_DRAINING ||
        runtime->state == RZB_NEXT_RUNTIME_STOPPED ||
        runtime->state == RZB_NEXT_RUNTIME_FAILED) {
        return false;
    }
    RzbNextRuntime_Free(&runtime->runtimePolicy);
    runtime->runtimePolicy = RzbNextRuntime_Strdup("running");
    if (runtime->runtimePolicy == NULL)
        return false;
    runtime->state = RZB_NEXT_RUNTIME_READY;
    return true;
}

bool
RzbNextRuntime_Drain(RzbNextRuntime_t *runtime)
{
    if (runtime == NULL)
        return false;
    RzbNextRuntime_BeginDraining(runtime);
    return runtime->inFlightWork == 0U;
}

bool
RzbNextRuntime_DependencyUnavailable(RzbNextRuntime_t *runtime)
{
    if (runtime == NULL)
        return false;
    runtime->dependencyPaused = true;
    return true;
}

bool
RzbNextRuntime_DependencyRecovered(RzbNextRuntime_t *runtime)
{
    if (runtime == NULL)
        return false;
    runtime->dependencyPaused = false;
    return true;
}

void
RzbNextRuntime_BeginDraining(RzbNextRuntime_t *runtime)
{
    if (runtime != NULL)
        runtime->state = RZB_NEXT_RUNTIME_DRAINING;
}

void
RzbNextRuntime_MarkStopped(RzbNextRuntime_t *runtime)
{
    if (runtime != NULL)
        runtime->state = RZB_NEXT_RUNTIME_STOPPED;
}

void
RzbNextRuntime_MarkFailed(RzbNextRuntime_t *runtime)
{
    if (runtime != NULL)
        runtime->state = RZB_NEXT_RUNTIME_FAILED;
}

void
RzbNextRuntimeRetry_Init(struct RzbNextRuntimeRetryState *state,
                         struct RzbNextRuntimeRetryPolicy policy)
{
    if (state == NULL)
        return;
    memset(state, 0, sizeof(*state));
    state->policy = policy;
}

struct RzbNextRuntimeRetryDecision
RzbNextRuntimeRetry_RequestRegistration(struct RzbNextRuntimeRetryState *state)
{
    struct RzbNextRuntimeRetryDecision decision = {
        RZB_NEXT_RUNTIME_RETRY_STOP, 0U
    };

    if (state == NULL)
        return decision;
    if (state->inFlight) {
        state->dirtyRerun = true;
        decision.action = RZB_NEXT_RUNTIME_RETRY_ALREADY_IN_FLIGHT;
        return decision;
    }
    state->inFlight = true;
    decision.action = RZB_NEXT_RUNTIME_RETRY_START_NOW;
    return decision;
}

void
RzbNextRuntimeRetry_Accepted(struct RzbNextRuntimeRetryState *state)
{
    if (state == NULL)
        return;
    state->inFlight = false;
    state->dirtyRerun = false;
    state->consecutiveFailureCount = 0U;
}

struct RzbNextRuntimeRetryDecision
RzbNextRuntimeRetry_RetryableFailure(struct RzbNextRuntimeRetryState *state,
                                     uint64_t retryAfter,
                                     uint8_t jitterPercentile)
{
    struct RzbNextRuntimeRetryDecision decision = {
        RZB_NEXT_RUNTIME_RETRY_STOP, 0U
    };

    if (state == NULL)
        return decision;
    state->inFlight = false;
    if (state->dirtyRerun) {
        state->dirtyRerun = false;
        state->consecutiveFailureCount = 0U;
        state->inFlight = true;
        decision.action = RZB_NEXT_RUNTIME_RETRY_START_NOW;
        return decision;
    }
    decision.action = RZB_NEXT_RUNTIME_RETRY_AFTER;
    decision.delay = RzbNextRuntime_DelayForFailure(
        state->policy, state->consecutiveFailureCount, retryAfter,
        jitterPercentile);
    state->consecutiveFailureCount++;
    return decision;
}

struct RzbNextRuntimeRetryDecision
RzbNextRuntimeRetry_TerminalFailure(struct RzbNextRuntimeRetryState *state)
{
    struct RzbNextRuntimeRetryDecision decision = {
        RZB_NEXT_RUNTIME_RETRY_STOP, 0U
    };

    if (state != NULL) {
        state->inFlight = false;
        state->dirtyRerun = false;
    }
    return decision;
}

static uint64_t
RzbNextRuntime_DelayForFailure(struct RzbNextRuntimeRetryPolicy policy,
                               uint32_t consecutiveFailureCount,
                               uint64_t retryAfter,
                               uint8_t jitterPercentile)
{
    uint64_t delay = policy.initialBackoff;
    uint64_t lower;
    uint64_t upper;
    uint64_t span;
    uint8_t jitter;
    uint8_t percentile;
    uint32_t index;

    if (retryAfter > 0U)
        return retryAfter;
    if (policy.maxBackoff == 0U)
        policy.maxBackoff = delay;
    for (index = 0U; index < consecutiveFailureCount; index++) {
        if (delay > policy.maxBackoff / 2U) {
            delay = policy.maxBackoff;
            break;
        }
        delay *= 2U;
        if (delay >= policy.maxBackoff) {
            delay = policy.maxBackoff;
            break;
        }
    }
    jitter = policy.jitterPercent > 100U ? 100U : policy.jitterPercent;
    if (jitter == 0U)
        return delay;
    percentile = jitterPercentile > 100U ? 100U : jitterPercentile;
    lower = (delay * (uint64_t)(100U - jitter)) / 100U;
    upper = (delay * (uint64_t)(100U + jitter)) / 100U;
    if (lower < policy.initialBackoff)
        lower = policy.initialBackoff;
    if (upper > policy.maxBackoff)
        upper = policy.maxBackoff;
    if (upper <= lower)
        return lower;
    span = upper - lower;
    return lower + ((span * percentile) / 100U);
}
