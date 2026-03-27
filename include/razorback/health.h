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

/** @file health.h
 * Process-wide health API.
 */
#ifndef RAZORBACK_HEALTH_H
#define RAZORBACK_HEALTH_H

#include <razorback/types.h>
#include <razorback/visibility.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Process-wide health probe kinds.
 * These map to the built-in HTTP endpoints exposed by Razorback:
 * `/livez`, `/readyz`, and `/startupz`.
 */
typedef enum
{
    RAZORBACK_HEALTH_LIVE = 1,     ///< Liveness probe.
    RAZORBACK_HEALTH_READY = 2,    ///< Readiness probe.
    RAZORBACK_HEALTH_STARTUP = 3   ///< Startup probe.
} RazorbackHealthCheckKind_t;

/** Registered health check identifier. */
typedef uint64_t RazorbackHealthCheckId_t;

/**
 * Callback signature for custom process-wide health checks.
 * Custom checks are ANDed with the built-in health result for the selected
 * probe kind.
 * @param userData User data supplied at registration time.
 * @return true when the check passes, false otherwise.
 */
typedef bool (*RazorbackHealthCheckFn)(void *userData);

/**
 * Configuration for the optional process-wide health HTTP listener.
 * The listener is started explicitly with Razorback_Health_Start() after
 * RZB_Init_API() has completed.
 */
struct RazorbackHealthServerConfig
{
    const char *bindAddress;      ///< Listener bind address. NULL defaults to 127.0.0.1.
    uint16_t port;                ///< Listener TCP port.
    bool requireContextsForReady; ///< When true, readiness fails if there are no active contexts.
};

typedef struct RazorbackHealthServerConfig RazorbackHealthServerConfig_t;

/**
 * Start the process-wide health HTTP listener.
 * The listener exposes `/livez`, `/readyz`, `/startupz`, and `/healthz`.
 * This function must be called only after RZB_Init_API().
 * @param config Listener configuration.
 * @return true on success, false on failure.
 */
SO_PUBLIC extern bool Razorback_Health_Start(const RazorbackHealthServerConfig_t *config);

/**
 * Stop the process-wide health HTTP listener.
 * It is safe to call this when the listener is not running.
 * @return No return value.
 */
SO_PUBLIC extern void Razorback_Health_Stop(void);

/**
 * Check whether the process-wide health HTTP listener is currently running.
 * @return true when the listener thread is active, false otherwise.
 */
SO_PUBLIC extern bool Razorback_Health_IsRunning(void);

/**
 * Set the built-in startup probe completion state.
 * Library consumers should call this once their own startup sequence is
 * complete so `/startupz` and readiness can transition healthy.
 * @param complete true when application startup is complete, false otherwise.
 * @return No return value.
 */
SO_PUBLIC extern void Razorback_Health_SetStartupComplete(bool complete);

/**
 * Evaluate one built-in health probe without using the HTTP listener.
 * This includes both the built-in Razorback health logic and any custom checks
 * registered for the selected probe kind.
 * @param kind Probe kind to evaluate.
 * @return true when healthy, false otherwise.
 */
SO_PUBLIC extern bool Razorback_Health_Evaluate(RazorbackHealthCheckKind_t kind);

/**
 * Register a custom process-wide health check.
 * The callback is evaluated as part of the selected probe kind and must remain
 * valid until it is unregistered.
 * @param kind Probe kind the callback contributes to.
 * @param name Optional check name used for diagnostics. NULL is allowed.
 * @param callback Callback to invoke during evaluation.
 * @param userData User data passed to the callback.
 * @return Non-zero registration id on success, zero on failure.
 */
SO_PUBLIC extern RazorbackHealthCheckId_t Razorback_Health_RegisterCheck(
    RazorbackHealthCheckKind_t kind,
    const char *name,
    RazorbackHealthCheckFn callback,
    void *userData
);

/**
 * Unregister a previously registered custom process-wide health check.
 * @param id Registration id returned by Razorback_Health_RegisterCheck().
 * @return true if the check was removed, false otherwise.
 */
SO_PUBLIC extern bool Razorback_Health_UnregisterCheck(RazorbackHealthCheckId_t id);

#ifdef __cplusplus
}
#endif

#endif /* RAZORBACK_HEALTH_H */
