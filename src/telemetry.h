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

#ifndef RAZORBACK_TELEMETRY_INTERNAL_H
#define RAZORBACK_TELEMETRY_INTERNAL_H

#include <razorback/telemetry.h>

#ifdef __cplusplus
extern "C" {
#endif

struct TelemetryHeader
{
    char *name;
    char *value;
};

struct TelemetryInjectedHeaders
{
    size_t count;
    struct TelemetryHeader *entries;
};

struct TelemetryContextCarrier
{
    size_t count;
    struct TelemetryHeader *entries;
};

void Telemetry_AddBlockAttributes(TelemetrySpan_t *span,
                                  const struct Block *block);

bool Telemetry_Initialize(void);
void Telemetry_Shutdown(void);

TelemetrySpan_t *
Telemetry_StartQueueSendSpan(const struct Queue *queue,
                             const struct Message *message,
                             const char *destination,
                             struct TelemetryInjectedHeaders *headers);

TelemetrySpan_t *
Telemetry_StartQueueReceiveSpan(const struct Queue *queue,
                                const struct Message *message);

bool Telemetry_InjectCurrentContext(struct TelemetryInjectedHeaders *headers);

bool Telemetry_IsLogEnabled(void);
void Telemetry_LogMessage(unsigned level, uint64_t component, const char *message);

void Telemetry_FreeInjectedHeaders(struct TelemetryInjectedHeaders *headers);

void Telemetry_RecordDispatcherWait(double durationSeconds, const char *outcome);
void Telemetry_RecordDispatcherSelection(const char *path);
void Telemetry_RecordOutboundMessage(uint32_t messageType, const char *outcome);
void Telemetry_RecordOutboundPublishRetry(uint32_t messageType);
void Telemetry_RecordOutboundReconnect(void);
void Telemetry_AddInspectionInFlight(int64_t delta);
void Telemetry_RecordInspectionDuration(double durationSeconds, const char *reason);
void Telemetry_RecordInspectionResult(const char *reason);
void Telemetry_RecordInspectionError(const char *phase);
void Telemetry_RecordShutdownRequeuedInspection(void);
void Telemetry_RecordBlockSubmitDecision(const char *decision);
void Telemetry_RecordCacheResponse(const char *result);
void Telemetry_RecordCacheLookupWait(double durationSeconds);
void Telemetry_RecordSubmitDuration(double durationSeconds,
                                    const char *reason,
                                    const char *outcome);
void Telemetry_RecordTransferFetchDuration(double durationSeconds,
                                           const char *outcome,
                                           const char *protocol);
void Telemetry_RecordTransferStoreDuration(double durationSeconds,
                                           const char *outcome,
                                           const char *protocol);
void Telemetry_RecordTransferFailure(const char *operation,
                                     const char *protocol);

#ifdef __cplusplus
}
#endif

#endif
