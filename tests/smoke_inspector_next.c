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

#define INSPECTOR_NUGGET_UUID "10000000-0000-4000-8000-000000000002"
#define REQUEST_ID "10000000-0000-4000-8000-000000000102"
#define GENERATION "10000000-0000-4000-8000-000000000202"
#define INVALIDATION_ID "10000000-0000-4000-8000-000000000302"
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
    "\"nugget_uuid\":\"" INSPECTOR_NUGGET_UUID "\","
    "\"registration_generation\":\"" GENERATION "\","
    "\"effective_runtime_policy\":\"running\","
    "\"liveness_interval\":10,"
    "\"liveness_freshness_window\":30,"
    "\"liveness_clock_skew_tolerance\":5,"
    "\"created_at\":\"" CREATED_AT "\""
    "}";

static void
directed(char *buffer, size_t bufferSize, const char *command,
         const char *invalidationId)
{
    snprintf(buffer, bufferSize,
             "{"
             "\"schema_name\":\"razorback.cnc.directed_command\","
             "\"schema_version\":1,"
             "\"command_id\":\"10000000-0000-4000-8000-000000000303\","
             "\"target_nugget_uuid\":\"" INSPECTOR_NUGGET_UUID "\","
             "\"registration_generation\":\"" GENERATION "\","
             "\"command\":\"%s\","
             "\"reason_code\":\"%s\","
             "%s"
             "\"created_at\":\"" CREATED_AT "\""
             "}",
             command,
             strcmp(command, "cache_invalidate") == 0
                 ? "catalog_invalidation"
                 : "operator_requested",
             invalidationId == NULL ? "" :
                 "\"invalidation_id\":\"" INVALIDATION_ID "\",");
}

int
main(void)
{
    RzbNextRuntime_t *runtime;
    struct RzbNextRuntimeTransition transition;
    struct RzbNextRuntimeDirectedResult result;
    struct RzbNextRuntimeHealth health;
    char command[1024];
    char *bye;

    if (RzbSmoke_LiveEnabled())
        return RzbSmoke_RunLive(RZB_SMOKE_ROLE_INSPECTOR) ? 0 : 1;

    runtime = RzbNextRuntime_CreateGenerated(INSPECTOR_NUGGET_UUID);
    assert(runtime != NULL);

    RzbNextRuntime_Initialize(runtime);
    assert(RzbNextRuntime_ObserveDispatcherHello(runtime, DISPATCHER_HELLO));
    assert(RzbNextRuntime_BeginRegistration(runtime, REQUEST_ID));
    assert(RzbNextRuntime_RegistrationAccepted(runtime, REGISTRATION_ACCEPTED,
                                               &transition));
    assert(RzbNextRuntime_State(runtime) == RZB_NEXT_RUNTIME_READY);

    assert(RzbNextRuntime_DependencyUnavailable(runtime));
    health = RzbNextRuntime_Health(runtime);
    assert(health.healthz);
    assert(!health.readyz);
    assert(RzbNextRuntime_DependencyRecovered(runtime));
    assert(RzbNextRuntime_Health(runtime).readyz);

    directed(command, sizeof(command), "cache_invalidate", INVALIDATION_ID);
    assert(RzbNextRuntime_ApplyDirectedCommand(runtime, command, &result));
    assert(result.effect == RZB_NEXT_RUNTIME_DIRECTED_CACHE_INVALIDATE);
    assert(strcmp(result.invalidationId, INVALIDATION_ID) == 0);
    assert(RzbNextRuntime_State(runtime) == RZB_NEXT_RUNTIME_READY);

    directed(command, sizeof(command), "terminate", NULL);
    assert(RzbNextRuntime_ApplyDirectedCommand(runtime, command, &result));
    assert(result.effect == RZB_NEXT_RUNTIME_DIRECTED_SHUTDOWN);
    assert(RzbNextRuntime_State(runtime) == RZB_NEXT_RUNTIME_DRAINING);
    bye = RzbNextRuntime_BuildBye(runtime, "terminate_command", CREATED_AT);
    assert(bye != NULL);
    free(bye);

    RzbNextRuntime_Destroy(runtime);
    puts("dispatcher-next C inspector smoke client passed");
    return 0;
}
