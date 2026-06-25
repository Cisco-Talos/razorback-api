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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <razorback/runtime_next.h>

#include "smoke_live_next.h"

#define SOURCE_NUGGET_UUID "10000000-0000-4000-8000-000000000001"
#define REQUEST_ID "10000000-0000-4000-8000-000000000101"
#define GENERATION "10000000-0000-4000-8000-000000000201"
#define CREATED_AT "2026-06-25T00:00:00.000Z"

static const char *DISPATCHER_HELLO =
    "{"
    "\"schema_name\":\"razorback.cnc.dispatcher_hello\","
    "\"schema_version\":1,"
    "\"dispatcher_id\":\"77777777-7777-4777-8777-777777777777\","
    "\"created_at\":\"" CREATED_AT "\","
    "\"started_at\":\"" CREATED_AT "\","
    "\"ready\":true,"
    "\"availability\":\"ready\","
    "\"dependency_reason_codes\":[]"
    "}";

static const char *REGISTRATION_ACCEPTED =
    "{"
    "\"schema_name\":\"razorback.cnc.registration_accepted\","
    "\"schema_version\":1,"
    "\"request_id\":\"" REQUEST_ID "\","
    "\"nugget_uuid\":\"" SOURCE_NUGGET_UUID "\","
    "\"registration_generation\":\"" GENERATION "\","
    "\"effective_runtime_policy\":\"running\","
    "\"liveness_interval\":10,"
    "\"liveness_freshness_window\":30,"
    "\"liveness_clock_skew_tolerance\":5,"
    "\"created_at\":\"" CREATED_AT "\""
    "}";

static void
directed(char *buffer, size_t bufferSize, const char *command)
{
    snprintf(buffer, bufferSize,
             "{"
             "\"schema_name\":\"razorback.cnc.directed_command\","
             "\"schema_version\":1,"
             "\"command_id\":\"10000000-0000-4000-8000-000000000301\","
             "\"target_nugget_uuid\":\"" SOURCE_NUGGET_UUID "\","
             "\"registration_generation\":\"" GENERATION "\","
             "\"command\":\"%s\","
             "\"reason_code\":\"operator_requested\","
             "\"created_at\":\"" CREATED_AT "\""
             "}",
             command);
}

int
main(void)
{
    RzbNextRuntime_t *runtime;
    struct RzbNextRuntimeTransition transition;
    struct RzbNextRuntimeDirectedResult result;
    struct RzbNextRuntimeLivenessPlan liveness;
    char command[1024];
    char *bye;

    if (RzbSmoke_LiveEnabled())
        return RzbSmoke_RunLive(RZB_SMOKE_ROLE_SOURCE) ? 0 : 1;

    runtime = RzbNextRuntime_CreateGenerated(SOURCE_NUGGET_UUID);
    assert(runtime != NULL);
    assert(strcmp(RzbNextRuntime_ProcessUuid(runtime), SOURCE_NUGGET_UUID) != 0);

    RzbNextRuntime_Initialize(runtime);
    assert(RzbNextRuntime_State(runtime) == RZB_NEXT_RUNTIME_WAITING_FOR_DISPATCHER);
    assert(RzbNextRuntime_ObserveDispatcherHello(runtime, DISPATCHER_HELLO));
    assert(RzbNextRuntime_State(runtime) == RZB_NEXT_RUNTIME_REGISTERING);
    assert(RzbNextRuntime_BeginRegistration(runtime, REQUEST_ID));
    assert(RzbNextRuntime_RegistrationAccepted(runtime, REGISTRATION_ACCEPTED,
                                               &transition));
    assert(transition.kind == RZB_NEXT_RUNTIME_TRANSITION_REGISTERED);
    assert(transition.ready);
    assert(RzbNextRuntime_State(runtime) == RZB_NEXT_RUNTIME_READY);

    assert(RzbNextRuntime_LivenessPlan(runtime, CREATED_AT, &liveness));
    assert(liveness.interval == 10);
    assert(liveness.messageExpiration == 30);
    RzbNextRuntime_LivenessPlanClear(&liveness);

    directed(command, sizeof(command), "pause");
    assert(RzbNextRuntime_ApplyDirectedCommand(runtime, command, &result));
    assert(result.effect == RZB_NEXT_RUNTIME_DIRECTED_PAUSE);
    assert(RzbNextRuntime_State(runtime) == RZB_NEXT_RUNTIME_PAUSED);

    directed(command, sizeof(command), "go");
    assert(RzbNextRuntime_ApplyDirectedCommand(runtime, command, &result));
    assert(result.effect == RZB_NEXT_RUNTIME_DIRECTED_RESUME);
    assert(RzbNextRuntime_State(runtime) == RZB_NEXT_RUNTIME_READY);

    bye = RzbNextRuntime_BuildBye(runtime, "shutdown", CREATED_AT);
    assert(bye != NULL);
    free(bye);

    RzbNextRuntime_Destroy(runtime);
    puts("dispatcher-next C source smoke client passed");
    return 0;
}
