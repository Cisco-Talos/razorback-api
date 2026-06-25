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

/** @file runtime_next.h
 * Dispatcher-next cluster-join runtime helpers.
 */
#ifndef RAZORBACK_RUNTIME_NEXT_H
#define RAZORBACK_RUNTIME_NEXT_H

#include <razorback/health.h>
#include <razorback/types.h>
#include <razorback/visibility.h>

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct RzbNextRuntime RzbNextRuntime_t;

enum RzbNextRuntimeState
{
    RZB_NEXT_RUNTIME_STARTING = 0,
    RZB_NEXT_RUNTIME_WAITING_FOR_DISPATCHER = 1,
    RZB_NEXT_RUNTIME_REGISTERING = 2,
    RZB_NEXT_RUNTIME_READY = 3,
    RZB_NEXT_RUNTIME_PAUSED = 4,
    RZB_NEXT_RUNTIME_DRAINING = 5,
    RZB_NEXT_RUNTIME_STOPPED = 6,
    RZB_NEXT_RUNTIME_FAILED = 7
};

enum RzbNextRuntimeTransitionKind
{
    RZB_NEXT_RUNTIME_TRANSITION_NONE = 0,
    RZB_NEXT_RUNTIME_TRANSITION_REGISTERED = 1,
    RZB_NEXT_RUNTIME_TRANSITION_REJECTED = 2
};

enum RzbNextRuntimeDirectedEffect
{
    RZB_NEXT_RUNTIME_DIRECTED_IGNORED_STALE = 0,
    RZB_NEXT_RUNTIME_DIRECTED_PAUSE = 1,
    RZB_NEXT_RUNTIME_DIRECTED_RESUME = 2,
    RZB_NEXT_RUNTIME_DIRECTED_SHUTDOWN = 3,
    RZB_NEXT_RUNTIME_DIRECTED_REREGISTER = 4,
    RZB_NEXT_RUNTIME_DIRECTED_CACHE_INVALIDATE = 5
};

enum RzbNextRuntimeRetryAction
{
    RZB_NEXT_RUNTIME_RETRY_START_NOW = 0,
    RZB_NEXT_RUNTIME_RETRY_ALREADY_IN_FLIGHT = 1,
    RZB_NEXT_RUNTIME_RETRY_AFTER = 2,
    RZB_NEXT_RUNTIME_RETRY_STOP = 3
};

struct RzbNextRuntimeHealth
{
    bool startupz;
    bool healthz;
    bool readyz;
};

struct RzbNextRuntimeTransition
{
    enum RzbNextRuntimeTransitionKind kind;
    bool ready;
    bool retryable;
    uint64_t retryAfter;
    char generation[37];
    char reasonCode[128];
};

struct RzbNextRuntimeDirectedResult
{
    enum RzbNextRuntimeDirectedEffect effect;
    bool staleGeneration;
    char invalidationId[37];
};

typedef bool (*RzbNextRuntimeCallbackFn)(
    RzbNextRuntime_t *runtime,
    void *userData
);
typedef bool (*RzbNextRuntimeTransitionCallbackFn)(
    RzbNextRuntime_t *runtime,
    const struct RzbNextRuntimeTransition *transition,
    void *userData
);
typedef bool (*RzbNextRuntimeCacheInvalidateCallbackFn)(
    RzbNextRuntime_t *runtime,
    const char *invalidationId,
    void *userData
);
typedef bool (*RzbNextRuntimeErrorCallbackFn)(
    RzbNextRuntime_t *runtime,
    const char *reasonCode,
    bool retryable,
    void *userData
);

struct RzbNextRuntimeCallbacks
{
    RzbNextRuntimeTransitionCallbackFn onRegistered;
    RzbNextRuntimeCallbackFn onReady;
    RzbNextRuntimeCallbackFn onPause;
    RzbNextRuntimeCallbackFn onResume;
    RzbNextRuntimeCacheInvalidateCallbackFn onCacheInvalidate;
    RzbNextRuntimeCallbackFn onShutdown;
    RzbNextRuntimeErrorCallbackFn onError;
    void *userData;
};

struct RzbNextRuntimeLivenessPlan
{
    char *message;
    uint64_t interval;
    uint64_t freshnessWindow;
    uint64_t clockSkewTolerance;
    uint64_t messageExpiration;
};

struct RzbNextRuntimeRetryPolicy
{
    uint64_t initialBackoff;
    uint64_t maxBackoff;
    uint8_t jitterPercent;
};

struct RzbNextRuntimeRetryState
{
    struct RzbNextRuntimeRetryPolicy policy;
    bool inFlight;
    bool dirtyRerun;
    uint32_t consecutiveFailureCount;
};

struct RzbNextRuntimeRetryDecision
{
    enum RzbNextRuntimeRetryAction action;
    uint64_t delay;
};

SO_PUBLIC extern const char * RzbNextRuntime_StateString(
    enum RzbNextRuntimeState state
);
SO_PUBLIC extern RzbNextRuntime_t * RzbNextRuntime_Create(
    const char *nuggetUuid,
    const char *processUuid
);
SO_PUBLIC extern RzbNextRuntime_t * RzbNextRuntime_CreateGenerated(
    const char *nuggetUuid
);
SO_PUBLIC extern void RzbNextRuntime_Destroy(RzbNextRuntime_t *runtime);
SO_PUBLIC extern bool RzbNextRuntime_SetCallbacks(
    RzbNextRuntime_t *runtime,
    const struct RzbNextRuntimeCallbacks *callbacks
);
SO_PUBLIC extern uint32_t RzbNextRuntime_CallbackFailureCount(
    const RzbNextRuntime_t *runtime
);
SO_PUBLIC extern const char * RzbNextRuntime_LastCallbackFailure(
    const RzbNextRuntime_t *runtime
);
SO_PUBLIC extern enum RzbNextRuntimeState RzbNextRuntime_State(
    const RzbNextRuntime_t *runtime
);
SO_PUBLIC extern const char * RzbNextRuntime_ProcessUuid(
    const RzbNextRuntime_t *runtime
);
SO_PUBLIC extern const char * RzbNextRuntime_RegistrationGeneration(
    const RzbNextRuntime_t *runtime
);
SO_PUBLIC extern struct RzbNextRuntimeHealth RzbNextRuntime_Health(
    const RzbNextRuntime_t *runtime
);
SO_PUBLIC extern bool RzbNextRuntime_HealthStartupCheck(void *userData);
SO_PUBLIC extern bool RzbNextRuntime_HealthLiveCheck(void *userData);
SO_PUBLIC extern bool RzbNextRuntime_HealthReadyCheck(void *userData);
SO_PUBLIC extern void RzbNextRuntime_Initialize(RzbNextRuntime_t *runtime);
SO_PUBLIC extern bool RzbNextRuntime_ObserveDispatcherHello(
    RzbNextRuntime_t *runtime,
    const char *jsonMessage
);
SO_PUBLIC extern bool RzbNextRuntime_BeginRegistration(
    RzbNextRuntime_t *runtime,
    const char *requestId
);
SO_PUBLIC extern bool RzbNextRuntime_RegistrationAccepted(
    RzbNextRuntime_t *runtime,
    const char *jsonMessage,
    struct RzbNextRuntimeTransition *transition
);
SO_PUBLIC extern bool RzbNextRuntime_RegistrationRejected(
    RzbNextRuntime_t *runtime,
    const char *jsonMessage,
    struct RzbNextRuntimeTransition *transition
);
SO_PUBLIC extern bool RzbNextRuntime_ApplyDirectedCommand(
    RzbNextRuntime_t *runtime,
    const char *jsonMessage,
    struct RzbNextRuntimeDirectedResult *result
);
SO_PUBLIC extern bool RzbNextRuntime_LivenessPlan(
    const RzbNextRuntime_t *runtime,
    const char *createdAt,
    struct RzbNextRuntimeLivenessPlan *plan
);
SO_PUBLIC extern char * RzbNextRuntime_BuildBye(
    const RzbNextRuntime_t *runtime,
    const char *reason,
    const char *createdAt
);
SO_PUBLIC extern void RzbNextRuntime_LivenessPlanClear(
    struct RzbNextRuntimeLivenessPlan *plan
);
SO_PUBLIC extern bool RzbNextRuntime_AcceptsWork(
    const RzbNextRuntime_t *runtime
);
SO_PUBLIC extern bool RzbNextRuntime_StartWhenReady(
    RzbNextRuntime_t *runtime
);
SO_PUBLIC extern void RzbNextRuntime_CompleteWork(RzbNextRuntime_t *runtime);
SO_PUBLIC extern uint32_t RzbNextRuntime_InFlightWork(
    const RzbNextRuntime_t *runtime
);
SO_PUBLIC extern void RzbNextRuntime_PauseNewWork(RzbNextRuntime_t *runtime);
SO_PUBLIC extern bool RzbNextRuntime_ResumeWhenReady(RzbNextRuntime_t *runtime);
SO_PUBLIC extern bool RzbNextRuntime_Drain(RzbNextRuntime_t *runtime);
SO_PUBLIC extern void RzbNextRuntime_BeginDraining(RzbNextRuntime_t *runtime);
SO_PUBLIC extern bool RzbNextRuntime_DependencyUnavailable(
    RzbNextRuntime_t *runtime
);
SO_PUBLIC extern bool RzbNextRuntime_DependencyRecovered(
    RzbNextRuntime_t *runtime
);
SO_PUBLIC extern void RzbNextRuntime_MarkStopped(RzbNextRuntime_t *runtime);
SO_PUBLIC extern void RzbNextRuntime_MarkFailed(RzbNextRuntime_t *runtime);
SO_PUBLIC extern void RzbNextRuntimeRetry_Init(
    struct RzbNextRuntimeRetryState *state,
    struct RzbNextRuntimeRetryPolicy policy
);
SO_PUBLIC extern struct RzbNextRuntimeRetryDecision
RzbNextRuntimeRetry_RequestRegistration(struct RzbNextRuntimeRetryState *state);
SO_PUBLIC extern void RzbNextRuntimeRetry_Accepted(
    struct RzbNextRuntimeRetryState *state
);
SO_PUBLIC extern struct RzbNextRuntimeRetryDecision
RzbNextRuntimeRetry_RetryableFailure(
    struct RzbNextRuntimeRetryState *state,
    uint64_t retryAfter,
    uint8_t jitterPercentile
);
SO_PUBLIC extern struct RzbNextRuntimeRetryDecision
RzbNextRuntimeRetry_TerminalFailure(struct RzbNextRuntimeRetryState *state);

#ifdef __cplusplus
}
#endif
#endif /* RAZORBACK_RUNTIME_NEXT_H */
