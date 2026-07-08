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

#include <razorback/runtime_next.h>
#include <razorback/messages_next.h>
#include <razorback/telemetry.h>

#include <json-c/json.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NUGGET_UUID "22222222-2222-4222-8222-222222222222"
#define PROCESS_UUID "33333333-3333-4333-8333-333333333333"
#define REQUEST_ID "11111111-1111-4111-8111-111111111111"

static char *read_fixture(const char *directory, const char *name);
static char *json_copy(json_object *object);
static char *mutate_string_field(const char *jsonMessage, const char *field,
                                 const char *value);
static char *mutate_directed_command(const char *jsonMessage,
                                     const char *command);
static char *mutate_rejected_terminal(const char *jsonMessage);
static void assert_json_string_field(const char *jsonMessage, const char *field,
                                     const char *expected);
static void assert_valid_uuid(const char *value);

struct CallbackState
{
    uint32_t registered;
    uint32_t ready;
    uint32_t pause;
    uint32_t resume;
    uint32_t cacheInvalidate;
    uint32_t shutdown;
    uint32_t error;
    bool failPause;
    char lastGeneration[37];
    char lastInvalidationId[37];
    char lastError[128];
};

static bool callback_registered(
    RzbNextRuntime_t *runtime,
    const struct RzbNextRuntimeTransition *transition,
    void *userData
);
static bool callback_ready(RzbNextRuntime_t *runtime, void *userData);
static bool callback_pause(RzbNextRuntime_t *runtime, void *userData);
static bool callback_resume(RzbNextRuntime_t *runtime, void *userData);
static bool callback_cache_invalidate(
    RzbNextRuntime_t *runtime,
    const char *invalidationId,
    void *userData
);
static bool callback_shutdown(RzbNextRuntime_t *runtime, void *userData);
static bool callback_error(
    RzbNextRuntime_t *runtime,
    const char *reasonCode,
    bool retryable,
    void *userData
);

static char *
read_fixture(const char *directory, const char *name)
{
    char path[4096];
    FILE *file;
    long size;
    char *contents;

    snprintf(path, sizeof(path), "%s/%s/%s", RAZORBACK_SCHEMA_FIXTURE_ROOT,
             directory, name);
    file = fopen(path, "rb");
    ck_assert_msg(file != NULL, "failed to open fixture %s", path);
    ck_assert_int_eq(fseek(file, 0, SEEK_END), 0);
    size = ftell(file);
    ck_assert_int_ge(size, 0);
    ck_assert_int_eq(fseek(file, 0, SEEK_SET), 0);

    contents = calloc((size_t)size + 1U, sizeof(char));
    ck_assert_ptr_ne(contents, NULL);
    ck_assert_uint_eq(fread(contents, 1, (size_t)size, file), (size_t)size);
    fclose(file);
    return contents;
}

static char *
json_copy(json_object *object)
{
    const char *jsonText;
    char *copy;
    size_t length;

    jsonText = json_object_to_json_string_ext(object, JSON_C_TO_STRING_PLAIN);
    length = strlen(jsonText) + 1U;
    copy = malloc(length);
    ck_assert_ptr_ne(copy, NULL);
    memcpy(copy, jsonText, length);
    json_object_put(object);
    return copy;
}

static char *
mutate_string_field(const char *jsonMessage, const char *field, const char *value)
{
    json_object *object = json_tokener_parse(jsonMessage);

    ck_assert_ptr_ne(object, NULL);
    json_object_object_del(object, field);
    json_object_object_add(object, field, json_object_new_string(value));
    return json_copy(object);
}

static char *
mutate_directed_command(const char *jsonMessage, const char *command)
{
    json_object *object = json_tokener_parse(jsonMessage);

    ck_assert_ptr_ne(object, NULL);
    json_object_object_del(object, "command");
    json_object_object_del(object, "reason_code");
    json_object_object_del(object, "invalidation_id");
    json_object_object_add(object, "command", json_object_new_string(command));
    json_object_object_add(object, "reason_code",
                           json_object_new_string("operator_requested"));
    return json_copy(object);
}

static char *
mutate_rejected_terminal(const char *jsonMessage)
{
    json_object *object = json_tokener_parse(jsonMessage);

    ck_assert_ptr_ne(object, NULL);
    json_object_object_del(object, "retryable");
    json_object_object_del(object, "retry_after");
    json_object_object_del(object, "reason_code");
    json_object_object_add(object, "retryable", json_object_new_boolean(false));
    json_object_object_add(object, "reason_code",
                           json_object_new_string("unauthorized"));
    return json_copy(object);
}

static void
assert_json_string_field(const char *jsonMessage, const char *field,
                         const char *expected)
{
    json_object *object;
    json_object *value;
    const char *stringValue;

    object = json_tokener_parse(jsonMessage);
    ck_assert_ptr_ne(object, NULL);
    ck_assert(json_object_object_get_ex(object, field, &value));
    ck_assert(json_object_is_type(value, json_type_string));
    stringValue = json_object_get_string(value);
    ck_assert_ptr_ne(stringValue, NULL);
    ck_assert_str_eq(stringValue, expected);
    json_object_put(object);
}

static void
assert_valid_uuid(const char *value)
{
    size_t index;

    ck_assert_ptr_ne(value, NULL);
    ck_assert_uint_eq(strlen(value), 36U);
    for (index = 0U; index < 36U; index++) {
        if (index == 8U || index == 13U || index == 18U || index == 23U)
            ck_assert_int_eq(value[index], '-');
    }
}

static bool
callback_registered(RzbNextRuntime_t *runtime,
                    const struct RzbNextRuntimeTransition *transition,
                    void *userData)
{
    struct CallbackState *state = userData;

    (void)runtime;
    ck_assert_ptr_ne(state, NULL);
    ck_assert_ptr_ne(transition, NULL);
    state->registered++;
    snprintf(state->lastGeneration, sizeof(state->lastGeneration), "%s",
             transition->generation);
    return true;
}

static bool
callback_ready(RzbNextRuntime_t *runtime, void *userData)
{
    struct CallbackState *state = userData;

    ck_assert_ptr_ne(runtime, NULL);
    ck_assert_ptr_ne(state, NULL);
    state->ready++;
    return true;
}

static bool
callback_pause(RzbNextRuntime_t *runtime, void *userData)
{
    struct CallbackState *state = userData;

    ck_assert_ptr_ne(runtime, NULL);
    ck_assert_ptr_ne(state, NULL);
    state->pause++;
    return !state->failPause;
}

static bool
callback_resume(RzbNextRuntime_t *runtime, void *userData)
{
    struct CallbackState *state = userData;

    ck_assert_ptr_ne(runtime, NULL);
    ck_assert_ptr_ne(state, NULL);
    state->resume++;
    return true;
}

static bool
callback_cache_invalidate(RzbNextRuntime_t *runtime,
                          const char *invalidationId,
                          void *userData)
{
    struct CallbackState *state = userData;

    ck_assert_ptr_ne(runtime, NULL);
    ck_assert_ptr_ne(state, NULL);
    ck_assert_ptr_ne(invalidationId, NULL);
    state->cacheInvalidate++;
    snprintf(state->lastInvalidationId, sizeof(state->lastInvalidationId), "%s",
             invalidationId);
    return true;
}

static bool
callback_shutdown(RzbNextRuntime_t *runtime, void *userData)
{
    struct CallbackState *state = userData;

    ck_assert_ptr_ne(runtime, NULL);
    ck_assert_ptr_ne(state, NULL);
    state->shutdown++;
    return true;
}

static bool
callback_error(RzbNextRuntime_t *runtime,
               const char *reasonCode,
               bool retryable,
               void *userData)
{
    struct CallbackState *state = userData;

    (void)retryable;
    ck_assert_ptr_ne(runtime, NULL);
    ck_assert_ptr_ne(state, NULL);
    ck_assert_ptr_ne(reasonCode, NULL);
    state->error++;
    snprintf(state->lastError, sizeof(state->lastError), "%s", reasonCode);
    return true;
}

START_TEST(test_runtime_create_enforces_single_context_and_generates_process_uuid)
{
    RzbNextRuntime_t *runtime;
    RzbNextRuntime_t *second;
    const char *generatedProcessUuid;

    runtime = RzbNextRuntime_Create(NUGGET_UUID, PROCESS_UUID);
    ck_assert_ptr_ne(runtime, NULL);
    ck_assert_str_eq(RzbNextRuntime_ProcessUuid(runtime), PROCESS_UUID);
    second = RzbNextRuntime_Create(
        "99999999-9999-4999-8999-999999999999",
        "88888888-8888-4888-8888-888888888888");
    ck_assert_ptr_eq(second, NULL);
    RzbNextRuntime_Destroy(runtime);

    runtime = RzbNextRuntime_CreateGenerated(NUGGET_UUID);
    ck_assert_ptr_ne(runtime, NULL);
    generatedProcessUuid = RzbNextRuntime_ProcessUuid(runtime);
    assert_valid_uuid(generatedProcessUuid);
    ck_assert_str_ne(generatedProcessUuid, PROCESS_UUID);
    RzbNextRuntime_Destroy(runtime);
}
END_TEST

START_TEST(test_runtime_common_metric_names_are_stable)
{
    ck_assert_str_eq(RAZORBACK_RUNTIME_DEPENDENCY_STATE_METRIC,
                     "razorback.runtime.dependency.state");
    ck_assert_str_eq(RAZORBACK_RUNTIME_WORKFLOW_STATE_METRIC,
                     "razorback.runtime.workflow.state");
    ck_assert_str_eq(RAZORBACK_RUNTIME_READINESS_STATE_METRIC,
                     "razorback.runtime.readiness.state");
    ck_assert_str_eq(RAZORBACK_RUNTIME_STARTUP_DURATION_METRIC,
                     "razorback.runtime.startup.duration");
    ck_assert_str_eq(RAZORBACK_RUNTIME_SHUTDOWN_DRAIN_DURATION_METRIC,
                     "razorback.runtime.shutdown.drain.duration");
    ck_assert_str_eq(RAZORBACK_RUNTIME_TELEMETRY_FLUSH_OUTCOME_METRIC,
                     "razorback.runtime.telemetry.flush.outcome");
}
END_TEST

START_TEST(test_runtime_health_and_dispatcher_hello_gate)
{
    RzbNextRuntime_t *runtime;
    struct RzbNextRuntimeHealth health;
    char *hello = read_fixture("messages", "cnc_dispatcher_hello.valid.json");
    char *notReady = mutate_string_field(hello, "availability", "starting");

    runtime = RzbNextRuntime_Create(NUGGET_UUID, PROCESS_UUID);
    ck_assert_ptr_ne(runtime, NULL);
    health = RzbNextRuntime_Health(runtime);
    ck_assert(health.healthz);
    ck_assert(!health.readyz);
    ck_assert(RzbNextRuntime_HealthLiveCheck(runtime));
    ck_assert(!RzbNextRuntime_HealthReadyCheck(runtime));

    RzbNextRuntime_Initialize(runtime);
    ck_assert_int_eq(RzbNextRuntime_State(runtime),
                     RZB_NEXT_RUNTIME_WAITING_FOR_DISPATCHER);
    ck_assert(!RzbNextRuntime_ObserveDispatcherHello(runtime, notReady));
    ck_assert(RzbNextRuntime_ObserveDispatcherHello(runtime, hello));
    ck_assert_int_eq(RzbNextRuntime_State(runtime), RZB_NEXT_RUNTIME_REGISTERING);

    RzbNextRuntime_Destroy(runtime);
    free(notReady);
    free(hello);
}
END_TEST

START_TEST(test_registration_acceptance_liveness_pause_go_and_bye)
{
    RzbNextRuntime_t *runtime;
    struct RzbNextRuntimeTransition transition;
    struct RzbNextRuntimeDirectedResult result;
    struct RzbNextRuntimeLivenessPlan plan;
    char *accepted = read_fixture("messages", "cnc_registration_accepted.valid.json");
    char *command = read_fixture("messages", "cnc_directed_command.valid.json");
    char *pause = mutate_directed_command(command, "pause");
    char *go = mutate_directed_command(command, "go");
    char *bye;

    runtime = RzbNextRuntime_Create(NUGGET_UUID, PROCESS_UUID);
    ck_assert_ptr_ne(runtime, NULL);
    ck_assert(RzbNextRuntime_BeginRegistration(runtime, REQUEST_ID));
    ck_assert(RzbNextRuntime_RegistrationAccepted(runtime, accepted, &transition));
    ck_assert_int_eq(transition.kind, RZB_NEXT_RUNTIME_TRANSITION_REGISTERED);
    ck_assert(transition.ready);
    ck_assert_str_eq(transition.generation,
                     "44444444-4444-4444-8444-444444444444");
    ck_assert_int_eq(RzbNextRuntime_State(runtime), RZB_NEXT_RUNTIME_READY);
    ck_assert_str_eq(RzbNextRuntime_RegistrationGeneration(runtime),
                     "44444444-4444-4444-8444-444444444444");
    ck_assert(RzbNextRuntime_LivenessPlan(runtime,
                                          "2026-06-17T21:00:10.000Z",
                                          &plan));
    ck_assert_uint_eq(plan.interval, 10U);
    ck_assert_uint_eq(plan.freshnessWindow, 30U);
    ck_assert_uint_eq(plan.messageExpiration, 30U);
    ck_assert(RzbNextMessage_Validate(plan.message));
    assert_json_string_field(plan.message, "runtime_policy", "running");
    assert_json_string_field(plan.message, "availability", "ready");
    RzbNextRuntime_LivenessPlanClear(&plan);

    ck_assert(RzbNextRuntime_ApplyDirectedCommand(runtime, pause, &result));
    ck_assert_int_eq(result.effect, RZB_NEXT_RUNTIME_DIRECTED_PAUSE);
    ck_assert_int_eq(RzbNextRuntime_State(runtime), RZB_NEXT_RUNTIME_PAUSED);
    ck_assert(RzbNextRuntime_LivenessPlan(runtime,
                                          "2026-06-17T21:00:11.000Z",
                                          &plan));
    assert_json_string_field(plan.message, "runtime_policy", "paused");
    assert_json_string_field(plan.message, "availability", "ready");
    RzbNextRuntime_LivenessPlanClear(&plan);

    ck_assert(RzbNextRuntime_ApplyDirectedCommand(runtime, go, &result));
    ck_assert_int_eq(result.effect, RZB_NEXT_RUNTIME_DIRECTED_RESUME);
    ck_assert_int_eq(RzbNextRuntime_State(runtime), RZB_NEXT_RUNTIME_READY);

    bye = RzbNextRuntime_BuildBye(runtime, "shutdown",
                                  "2026-06-17T21:00:20.000Z");
    ck_assert_ptr_ne(bye, NULL);
    ck_assert(RzbNextMessage_Validate(bye));
    assert_json_string_field(bye, "reason", "shutdown");

    RzbNext_FreeString(bye);
    RzbNextRuntime_Destroy(runtime);
    free(go);
    free(pause);
    free(command);
    free(accepted);
}
END_TEST

START_TEST(test_runtime_callbacks_follow_registration_and_directed_effects)
{
    RzbNextRuntime_t *runtime;
    struct CallbackState state;
    struct RzbNextRuntimeCallbacks callbacks;
    struct RzbNextRuntimeTransition transition;
    struct RzbNextRuntimeDirectedResult result;
    char *accepted = read_fixture("messages", "cnc_registration_accepted.valid.json");
    char *command = read_fixture("messages", "cnc_directed_command.valid.json");
    char *pause = mutate_directed_command(command, "pause");
    char *go = mutate_directed_command(command, "go");
    char *terminate = mutate_directed_command(command, "terminate");

    memset(&state, 0, sizeof(state));
    memset(&callbacks, 0, sizeof(callbacks));
    callbacks.onRegistered = callback_registered;
    callbacks.onReady = callback_ready;
    callbacks.onPause = callback_pause;
    callbacks.onResume = callback_resume;
    callbacks.onCacheInvalidate = callback_cache_invalidate;
    callbacks.onShutdown = callback_shutdown;
    callbacks.onError = callback_error;
    callbacks.userData = &state;

    runtime = RzbNextRuntime_Create(NUGGET_UUID, PROCESS_UUID);
    ck_assert_ptr_ne(runtime, NULL);
    ck_assert(RzbNextRuntime_SetCallbacks(runtime, &callbacks));
    ck_assert(RzbNextRuntime_BeginRegistration(runtime, REQUEST_ID));
    ck_assert(RzbNextRuntime_RegistrationAccepted(runtime, accepted, &transition));
    ck_assert_uint_eq(state.registered, 1U);
    ck_assert_uint_eq(state.ready, 1U);
    ck_assert_str_eq(state.lastGeneration, transition.generation);

    ck_assert(RzbNextRuntime_ApplyDirectedCommand(runtime, pause, &result));
    ck_assert_int_eq(result.effect, RZB_NEXT_RUNTIME_DIRECTED_PAUSE);
    ck_assert_uint_eq(state.pause, 1U);

    ck_assert(RzbNextRuntime_ApplyDirectedCommand(runtime, go, &result));
    ck_assert_int_eq(result.effect, RZB_NEXT_RUNTIME_DIRECTED_RESUME);
    ck_assert_uint_eq(state.resume, 1U);

    ck_assert(RzbNextRuntime_ApplyDirectedCommand(runtime, command, &result));
    ck_assert_int_eq(result.effect, RZB_NEXT_RUNTIME_DIRECTED_CACHE_INVALIDATE);
    ck_assert_str_eq(result.invalidationId,
                     "66666666-6666-4666-8666-666666666666");
    ck_assert_uint_eq(state.cacheInvalidate, 1U);
    ck_assert_str_eq(state.lastInvalidationId,
                     "66666666-6666-4666-8666-666666666666");

    ck_assert(RzbNextRuntime_ApplyDirectedCommand(runtime, terminate, &result));
    ck_assert_int_eq(result.effect, RZB_NEXT_RUNTIME_DIRECTED_SHUTDOWN);
    ck_assert_int_eq(RzbNextRuntime_State(runtime), RZB_NEXT_RUNTIME_DRAINING);
    ck_assert_uint_eq(state.shutdown, 1U);
    ck_assert_uint_eq(RzbNextRuntime_CallbackFailureCount(runtime), 0U);
    ck_assert_ptr_eq(RzbNextRuntime_LastCallbackFailure(runtime), NULL);

    RzbNextRuntime_Destroy(runtime);
    free(terminate);
    free(go);
    free(pause);
    free(command);
    free(accepted);
}
END_TEST

START_TEST(test_runtime_callback_failure_is_diagnostic_only)
{
    RzbNextRuntime_t *runtime;
    struct CallbackState state;
    struct RzbNextRuntimeCallbacks callbacks;
    struct RzbNextRuntimeDirectedResult result;
    char *accepted = read_fixture("messages", "cnc_registration_accepted.valid.json");
    char *command = read_fixture("messages", "cnc_directed_command.valid.json");
    char *pause = mutate_directed_command(command, "pause");

    memset(&state, 0, sizeof(state));
    state.failPause = true;
    memset(&callbacks, 0, sizeof(callbacks));
    callbacks.onPause = callback_pause;
    callbacks.onError = callback_error;
    callbacks.userData = &state;

    runtime = RzbNextRuntime_Create(NUGGET_UUID, PROCESS_UUID);
    ck_assert_ptr_ne(runtime, NULL);
    ck_assert(RzbNextRuntime_SetCallbacks(runtime, &callbacks));
    ck_assert(RzbNextRuntime_BeginRegistration(runtime, REQUEST_ID));
    ck_assert(RzbNextRuntime_RegistrationAccepted(runtime, accepted, NULL));

    ck_assert(RzbNextRuntime_ApplyDirectedCommand(runtime, pause, &result));
    ck_assert_int_eq(result.effect, RZB_NEXT_RUNTIME_DIRECTED_PAUSE);
    ck_assert_int_eq(RzbNextRuntime_State(runtime), RZB_NEXT_RUNTIME_PAUSED);
    ck_assert_uint_eq(state.pause, 1U);
    ck_assert_uint_eq(state.error, 1U);
    ck_assert_str_eq(state.lastError, "callback_failed");
    ck_assert_uint_eq(RzbNextRuntime_CallbackFailureCount(runtime), 1U);
    ck_assert_str_eq(RzbNextRuntime_LastCallbackFailure(runtime), "onPause");

    RzbNextRuntime_Destroy(runtime);
    free(pause);
    free(command);
    free(accepted);
}
END_TEST

START_TEST(test_operational_readiness_gate_pauses_resumes_and_drains)
{
    RzbNextRuntime_t *runtime;
    struct RzbNextRuntimeHealth health;
    char *accepted = read_fixture("messages", "cnc_registration_accepted.valid.json");

    runtime = RzbNextRuntime_Create(NUGGET_UUID, PROCESS_UUID);
    ck_assert_ptr_ne(runtime, NULL);
    ck_assert(RzbNextRuntime_BeginRegistration(runtime, REQUEST_ID));
    ck_assert(RzbNextRuntime_RegistrationAccepted(runtime, accepted, NULL));
    ck_assert(RzbNextRuntime_AcceptsWork(runtime));
    ck_assert(RzbNextRuntime_StartWhenReady(runtime));
    ck_assert(RzbNextRuntime_StartWhenReady(runtime));
    ck_assert_uint_eq(RzbNextRuntime_InFlightWork(runtime), 2U);

    RzbNextRuntime_PauseNewWork(runtime);
    ck_assert_int_eq(RzbNextRuntime_State(runtime), RZB_NEXT_RUNTIME_PAUSED);
    ck_assert(!RzbNextRuntime_AcceptsWork(runtime));
    ck_assert(!RzbNextRuntime_StartWhenReady(runtime));
    health = RzbNextRuntime_Health(runtime);
    ck_assert(health.healthz);
    ck_assert(!health.readyz);

    ck_assert(RzbNextRuntime_ResumeWhenReady(runtime));
    ck_assert_int_eq(RzbNextRuntime_State(runtime), RZB_NEXT_RUNTIME_READY);
    ck_assert(RzbNextRuntime_AcceptsWork(runtime));

    RzbNextRuntime_PauseNewWork(runtime);
    RzbNextRuntime_CompleteWork(runtime);
    ck_assert_uint_eq(RzbNextRuntime_InFlightWork(runtime), 1U);
    ck_assert(!RzbNextRuntime_Drain(runtime));
    ck_assert_int_eq(RzbNextRuntime_State(runtime), RZB_NEXT_RUNTIME_DRAINING);
    ck_assert(!RzbNextRuntime_AcceptsWork(runtime));
    ck_assert(!RzbNextRuntime_ResumeWhenReady(runtime));
    RzbNextRuntime_CompleteWork(runtime);
    ck_assert_uint_eq(RzbNextRuntime_InFlightWork(runtime), 0U);
    ck_assert(RzbNextRuntime_Drain(runtime));

    RzbNextRuntime_Destroy(runtime);
    free(accepted);
}
END_TEST

START_TEST(test_dependency_outage_pauses_readiness_until_recovered)
{
    RzbNextRuntime_t *runtime;
    struct RzbNextRuntimeHealth health;
    struct RzbNextRuntimeLivenessPlan plan;
    struct RzbNextRuntimeDirectedResult result;
    char *accepted = read_fixture("messages", "cnc_registration_accepted.valid.json");
    char *command = read_fixture("messages", "cnc_directed_command.valid.json");
    char *go = mutate_directed_command(command, "go");

    runtime = RzbNextRuntime_Create(NUGGET_UUID, PROCESS_UUID);
    ck_assert_ptr_ne(runtime, NULL);
    ck_assert(RzbNextRuntime_BeginRegistration(runtime, REQUEST_ID));
    ck_assert(RzbNextRuntime_RegistrationAccepted(runtime, accepted, NULL));
    ck_assert(RzbNextRuntime_AcceptsWork(runtime));

    ck_assert(RzbNextRuntime_DependencyUnavailable(runtime));

    ck_assert_int_eq(RzbNextRuntime_State(runtime), RZB_NEXT_RUNTIME_READY);
    ck_assert(!RzbNextRuntime_AcceptsWork(runtime));
    ck_assert(!RzbNextRuntime_StartWhenReady(runtime));
    health = RzbNextRuntime_Health(runtime);
    ck_assert(health.healthz);
    ck_assert(!health.readyz);
    ck_assert(RzbNextRuntime_LivenessPlan(runtime,
                                          "2026-06-17T21:00:13.000Z",
                                          &plan));
    assert_json_string_field(plan.message, "runtime_policy", "running");
    assert_json_string_field(plan.message, "availability",
                             "dependency_paused");
    RzbNextRuntime_LivenessPlanClear(&plan);

    ck_assert(RzbNextRuntime_ApplyDirectedCommand(runtime, go, &result));
    ck_assert_int_eq(result.effect, RZB_NEXT_RUNTIME_DIRECTED_RESUME);
    ck_assert_int_eq(RzbNextRuntime_State(runtime), RZB_NEXT_RUNTIME_READY);
    ck_assert(!RzbNextRuntime_AcceptsWork(runtime));

    ck_assert(RzbNextRuntime_DependencyRecovered(runtime));

    ck_assert(RzbNextRuntime_AcceptsWork(runtime));
    health = RzbNextRuntime_Health(runtime);
    ck_assert(health.readyz);
    ck_assert(RzbNextRuntime_LivenessPlan(runtime,
                                          "2026-06-17T21:00:14.000Z",
                                          &plan));
    assert_json_string_field(plan.message, "availability", "ready");
    RzbNextRuntime_LivenessPlanClear(&plan);

    RzbNextRuntime_Destroy(runtime);
    free(go);
    free(command);
    free(accepted);
}
END_TEST

START_TEST(test_stale_generation_triggers_reregister_and_pre_registration_ignored)
{
    RzbNextRuntime_t *runtime;
    struct RzbNextRuntimeDirectedResult result;
    char *accepted = read_fixture("messages", "cnc_registration_accepted.valid.json");
    char *command = read_fixture("messages", "cnc_directed_command.valid.json");
    char *stale = mutate_string_field(command, "registration_generation",
                                      "77777777-7777-4777-8777-777777777777");

    runtime = RzbNextRuntime_Create(NUGGET_UUID, PROCESS_UUID);
    ck_assert_ptr_ne(runtime, NULL);
    ck_assert(RzbNextRuntime_ApplyDirectedCommand(runtime, command, &result));
    ck_assert_int_eq(result.effect, RZB_NEXT_RUNTIME_DIRECTED_IGNORED_STALE);

    ck_assert(RzbNextRuntime_BeginRegistration(runtime, REQUEST_ID));
    ck_assert(RzbNextRuntime_RegistrationAccepted(runtime, accepted, NULL));
    ck_assert(RzbNextRuntime_ApplyDirectedCommand(runtime, stale, &result));
    ck_assert_int_eq(result.effect, RZB_NEXT_RUNTIME_DIRECTED_REREGISTER);
    ck_assert(result.staleGeneration);
    ck_assert_int_eq(RzbNextRuntime_State(runtime), RZB_NEXT_RUNTIME_REGISTERING);
    ck_assert_ptr_eq(RzbNextRuntime_RegistrationGeneration(runtime), NULL);

    RzbNextRuntime_Destroy(runtime);
    free(stale);
    free(command);
    free(accepted);
}
END_TEST

START_TEST(test_registration_rejected_retryable_and_terminal)
{
    RzbNextRuntime_t *runtime;
    struct RzbNextRuntimeTransition transition;
    char *rejected = read_fixture("messages", "cnc_registration_rejected.valid.json");
    char *terminal = mutate_rejected_terminal(rejected);

    runtime = RzbNextRuntime_Create(NUGGET_UUID, PROCESS_UUID);
    ck_assert_ptr_ne(runtime, NULL);
    ck_assert(RzbNextRuntime_BeginRegistration(runtime, REQUEST_ID));
    ck_assert(RzbNextRuntime_RegistrationRejected(runtime, rejected, &transition));
    ck_assert_int_eq(transition.kind, RZB_NEXT_RUNTIME_TRANSITION_REJECTED);
    ck_assert(transition.retryable);
    ck_assert_uint_eq(transition.retryAfter, 15U);
    ck_assert_str_eq(transition.reasonCode, "dependency_unavailable");
    ck_assert_int_eq(RzbNextRuntime_State(runtime), RZB_NEXT_RUNTIME_REGISTERING);

    ck_assert(RzbNextRuntime_BeginRegistration(runtime, REQUEST_ID));
    ck_assert(RzbNextRuntime_RegistrationRejected(runtime, terminal, &transition));
    ck_assert(!transition.retryable);
    ck_assert_str_eq(transition.reasonCode, "unauthorized");
    ck_assert_int_eq(RzbNextRuntime_State(runtime), RZB_NEXT_RUNTIME_FAILED);
    ck_assert(!RzbNextRuntime_HealthLiveCheck(runtime));

    RzbNextRuntime_Destroy(runtime);
    free(terminal);
    free(rejected);
}
END_TEST

START_TEST(test_retry_state_coalesces_dirty_rerun_and_terminal_stop)
{
    struct RzbNextRuntimeRetryPolicy policy = { 1U, 30U, 20U };
    struct RzbNextRuntimeRetryState state;
    struct RzbNextRuntimeRetryDecision decision;

    RzbNextRuntimeRetry_Init(&state, policy);
    decision = RzbNextRuntimeRetry_RequestRegistration(&state);
    ck_assert_int_eq(decision.action, RZB_NEXT_RUNTIME_RETRY_START_NOW);
    ck_assert(state.inFlight);
    decision = RzbNextRuntimeRetry_RequestRegistration(&state);
    ck_assert_int_eq(decision.action,
                     RZB_NEXT_RUNTIME_RETRY_ALREADY_IN_FLIGHT);
    ck_assert(state.dirtyRerun);
    decision = RzbNextRuntimeRetry_RetryableFailure(&state, 0U, 50U);
    ck_assert_int_eq(decision.action, RZB_NEXT_RUNTIME_RETRY_START_NOW);
    ck_assert(state.inFlight);
    ck_assert(!state.dirtyRerun);

    decision = RzbNextRuntimeRetry_RetryableFailure(&state, 7U, 100U);
    ck_assert_int_eq(decision.action, RZB_NEXT_RUNTIME_RETRY_AFTER);
    ck_assert_uint_eq(decision.delay, 7U);
    decision = RzbNextRuntimeRetry_RequestRegistration(&state);
    ck_assert_int_eq(decision.action, RZB_NEXT_RUNTIME_RETRY_START_NOW);
    RzbNextRuntimeRetry_Accepted(&state);
    ck_assert(!state.inFlight);
    ck_assert_uint_eq(state.consecutiveFailureCount, 0U);

    decision = RzbNextRuntimeRetry_TerminalFailure(&state);
    ck_assert_int_eq(decision.action, RZB_NEXT_RUNTIME_RETRY_STOP);
    ck_assert(!state.inFlight);
}
END_TEST

static Suite *
runtime_next_suite(void)
{
    Suite *suite;
    TCase *testcase;

    suite = suite_create("runtime_next");
    testcase = tcase_create("core");
    tcase_add_test(testcase,
                   test_runtime_create_enforces_single_context_and_generates_process_uuid);
    tcase_add_test(testcase, test_runtime_common_metric_names_are_stable);
    tcase_add_test(testcase, test_runtime_health_and_dispatcher_hello_gate);
    tcase_add_test(testcase,
                   test_registration_acceptance_liveness_pause_go_and_bye);
    tcase_add_test(testcase,
                   test_runtime_callbacks_follow_registration_and_directed_effects);
    tcase_add_test(testcase,
                   test_runtime_callback_failure_is_diagnostic_only);
    tcase_add_test(testcase,
                   test_operational_readiness_gate_pauses_resumes_and_drains);
    tcase_add_test(testcase,
                   test_dependency_outage_pauses_readiness_until_recovered);
    tcase_add_test(testcase,
                   test_stale_generation_triggers_reregister_and_pre_registration_ignored);
    tcase_add_test(testcase, test_registration_rejected_retryable_and_terminal);
    tcase_add_test(testcase,
                   test_retry_state_coalesces_dirty_rerun_and_terminal_stop);
    suite_add_tcase(suite, testcase);
    return suite;
}

int
main(void)
{
    Suite *suite;
    SRunner *runner;
    int failed;

    suite = runtime_next_suite();
    runner = srunner_create(suite);
    srunner_run_all(runner, CK_NORMAL);
    failed = srunner_ntests_failed(runner);
    srunner_free(runner);
    return failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
