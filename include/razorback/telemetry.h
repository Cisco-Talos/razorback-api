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

/** @file telemetry.h
 * Telemetry tracing and metrics helpers.
 */
#ifndef RAZORBACK_PUBLIC_TELEMETRY_H
#define RAZORBACK_PUBLIC_TELEMETRY_H

#include <razorback/messages.h>
#include <razorback/queue.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RAZORBACK_RUNTIME_DEPENDENCY_STATE_METRIC "razorback.runtime.dependency.state"
#define RAZORBACK_RUNTIME_WORKFLOW_STATE_METRIC "razorback.runtime.workflow.state"
#define RAZORBACK_RUNTIME_READINESS_STATE_METRIC "razorback.runtime.readiness.state"
#define RAZORBACK_RUNTIME_STARTUP_DURATION_METRIC "razorback.runtime.startup.duration"
#define RAZORBACK_RUNTIME_SHUTDOWN_DRAIN_DURATION_METRIC "razorback.runtime.shutdown.drain.duration"
#define RAZORBACK_RUNTIME_TELEMETRY_FLUSH_OUTCOME_METRIC "razorback.runtime.telemetry.flush.outcome"

typedef enum TelemetrySpanKind
{
    TELEMETRY_SPAN_KIND_INTERNAL = 0,
    TELEMETRY_SPAN_KIND_CONSUMER = 1,
    TELEMETRY_SPAN_KIND_PRODUCER = 2,
    TELEMETRY_SPAN_KIND_CLIENT = 3
} TelemetrySpanKind_t;

typedef enum TelemetryMetricAttributeType
{
    TELEMETRY_METRIC_ATTRIBUTE_STRING = 0,
    TELEMETRY_METRIC_ATTRIBUTE_INT = 1,
    TELEMETRY_METRIC_ATTRIBUTE_DOUBLE = 2,
    TELEMETRY_METRIC_ATTRIBUTE_BOOL = 3
} TelemetryMetricAttributeType_t;

typedef struct TelemetryMetricAttribute
{
    const char *name;
    TelemetryMetricAttributeType_t type;
    const char *stringValue;
    int64_t intValue;
    double doubleValue;
    bool boolValue;
} TelemetryMetricAttribute_t;

typedef void (*TelemetryObservableCallback_t)(
    TelemetryObservation_t *observation,
    void *userData
);

typedef struct TelemetryHeader
{
    char *name;
    char *value;
} TelemetryHeader_t;

typedef struct TelemetryInjectedHeaders
{
    size_t count;
    TelemetryHeader_t *entries;
} TelemetryInjectedHeaders_t;

/**
 * Capture the current tracing context into a reusable carrier.
 * @param context Pointer to the carrier slot to update.
 * @return true on success, false on error.
 */
SO_PUBLIC extern bool Telemetry_UpdateContext(TelemetryContextCarrier_t **context);

/**
 * Start a span using a stored context carrier when present.
 * @param spanName Span name.
 * @param context Pointer to the carrier slot to read and refresh.
 * @return A span handle on success, or NULL when tracing is unavailable.
 */
SO_PUBLIC extern TelemetrySpan_t * Telemetry_StartSpan(
    const char *spanName,
    TelemetryContextCarrier_t **context
);

/**
 * Start a span using a stored context carrier or the current runtime context,
 * with an explicit span kind.
 * @param spanName Span name.
 * @param context Pointer to the carrier slot to read and optionally refresh.
 * @param kind Span kind to emit.
 * @return A span handle on success, or NULL when tracing is unavailable.
 */
SO_PUBLIC extern TelemetrySpan_t * Telemetry_StartSpanWithKind(
    const char *spanName,
    TelemetryContextCarrier_t **context,
    TelemetrySpanKind_t kind
);

/**
 * Start a processing span from a message context.
 * @param queue Queue associated with the message.
 * @param message Message to extract context from.
 * @return A span handle on success, or NULL when tracing is unavailable.
 */
SO_PUBLIC extern TelemetrySpan_t * Telemetry_StartMessageProcessSpan(
    const struct Queue *queue,
    const struct Message *message
);

/**
 * Release a stored tracing context carrier.
 * @param context Pointer to the carrier slot to clear.
 */
SO_PUBLIC extern void Telemetry_ClearContext(TelemetryContextCarrier_t **context);

/**
 * Inject the current tracing context into a reusable header list.
 * @param headers Header container to populate.
 * @return true on success, false on error.
 */
SO_PUBLIC extern bool Telemetry_InjectCurrentContext(
    TelemetryInjectedHeaders_t *headers
);

/**
 * Release a header list populated by Telemetry_InjectCurrentContext().
 * @param headers Header container to clear.
 */
SO_PUBLIC extern void Telemetry_FreeInjectedHeaders(
    TelemetryInjectedHeaders_t *headers
);

/**
 * Add a string attribute to a span.
 * @param span Span handle.
 * @param name Attribute name.
 * @param value Attribute value.
 */
SO_PUBLIC extern void Telemetry_AddStringAttribute(
    TelemetrySpan_t *span,
    const char *name,
    const char *value
);

/**
 * Add an integer attribute to a span.
 * @param span Span handle.
 * @param name Attribute name.
 * @param value Attribute value.
 */
SO_PUBLIC extern void Telemetry_AddIntAttribute(
    TelemetrySpan_t *span,
    const char *name,
    int64_t value
);

/**
 * Add a boolean attribute to a span.
 * @param span Span handle.
 * @param name Attribute name.
 * @param value Attribute value.
 */
SO_PUBLIC extern void Telemetry_AddBoolAttribute(
    TelemetrySpan_t *span,
    const char *name,
    bool value
);

/**
 * End a span and set its status.
 * @param span Span handle.
 * @param success Whether the operation succeeded.
 * @param description Optional error description.
 */
SO_PUBLIC extern void Telemetry_EndSpan(
    TelemetrySpan_t *span,
    bool success,
    const char *description
);

/**
 * Get a monotonic timestamp expressed in seconds.
 * @return Current monotonic time in seconds.
 */
SO_PUBLIC extern double Telemetry_GetMonotonicTimeSeconds(void);

/**
 * Create a uint64 counter instrument.
 * @param name Metric name.
 * @param description Metric description.
 * @param unit Metric unit.
 * @return A metric handle on success, or NULL when metrics are unavailable.
 */
SO_PUBLIC extern TelemetryMetric_t * Telemetry_CreateUInt64Counter(
    const char *name,
    const char *description,
    const char *unit
);

/**
 * Create a double counter instrument.
 * @param name Metric name.
 * @param description Metric description.
 * @param unit Metric unit.
 * @return A metric handle on success, or NULL when metrics are unavailable.
 */
SO_PUBLIC extern TelemetryMetric_t * Telemetry_CreateDoubleCounter(
    const char *name,
    const char *description,
    const char *unit
);

/**
 * Create an int64 up-down counter instrument.
 * @param name Metric name.
 * @param description Metric description.
 * @param unit Metric unit.
 * @return A metric handle on success, or NULL when metrics are unavailable.
 */
SO_PUBLIC extern TelemetryMetric_t * Telemetry_CreateInt64UpDownCounter(
    const char *name,
    const char *description,
    const char *unit
);

/**
 * Create a double up-down counter instrument.
 * @param name Metric name.
 * @param description Metric description.
 * @param unit Metric unit.
 * @return A metric handle on success, or NULL when metrics are unavailable.
 */
SO_PUBLIC extern TelemetryMetric_t * Telemetry_CreateDoubleUpDownCounter(
    const char *name,
    const char *description,
    const char *unit
);

/**
 * Create a uint64 histogram instrument.
 * @param name Metric name.
 * @param description Metric description.
 * @param unit Metric unit.
 * @return A metric handle on success, or NULL when metrics are unavailable.
 */
SO_PUBLIC extern TelemetryMetric_t * Telemetry_CreateUInt64Histogram(
    const char *name,
    const char *description,
    const char *unit
);

/**
 * Create a double histogram instrument.
 * @param name Metric name.
 * @param description Metric description.
 * @param unit Metric unit.
 * @return A metric handle on success, or NULL when metrics are unavailable.
 */
SO_PUBLIC extern TelemetryMetric_t * Telemetry_CreateDoubleHistogram(
    const char *name,
    const char *description,
    const char *unit
);

/**
 * Create an int64 observable gauge instrument.
 * @param name Metric name.
 * @param description Metric description.
 * @param unit Metric unit.
 * @param callback Callback invoked during metric collection.
 * @param userData Opaque user pointer passed to the callback.
 * @return A metric handle on success, or NULL when metrics are unavailable.
 */
SO_PUBLIC extern TelemetryMetric_t * Telemetry_CreateInt64ObservableGauge(
    const char *name,
    const char *description,
    const char *unit,
    TelemetryObservableCallback_t callback,
    void *userData
);

/**
 * Create a double observable gauge instrument.
 * @param name Metric name.
 * @param description Metric description.
 * @param unit Metric unit.
 * @param callback Callback invoked during metric collection.
 * @param userData Opaque user pointer passed to the callback.
 * @return A metric handle on success, or NULL when metrics are unavailable.
 */
SO_PUBLIC extern TelemetryMetric_t * Telemetry_CreateDoubleObservableGauge(
    const char *name,
    const char *description,
    const char *unit,
    TelemetryObservableCallback_t callback,
    void *userData
);

/**
 * Destroy a metric instrument handle.
 * @param metric Metric handle to release.
 */
SO_PUBLIC extern void Telemetry_DestroyMetric(TelemetryMetric_t *metric);

/**
 * Add a value to a uint64 counter.
 * @param metric Counter handle created by Telemetry_CreateUInt64Counter.
 * @param value Value to add.
 * @param attributes Optional attribute array.
 * @param attributeCount Number of attributes in the array.
 */
SO_PUBLIC extern void Telemetry_CounterAddUInt64(
    TelemetryMetric_t *metric,
    uint64_t value,
    const TelemetryMetricAttribute_t *attributes,
    size_t attributeCount
);

/**
 * Add a value to a double counter.
 * @param metric Counter handle created by Telemetry_CreateDoubleCounter.
 * @param value Value to add.
 * @param attributes Optional attribute array.
 * @param attributeCount Number of attributes in the array.
 */
SO_PUBLIC extern void Telemetry_CounterAddDouble(
    TelemetryMetric_t *metric,
    double value,
    const TelemetryMetricAttribute_t *attributes,
    size_t attributeCount
);

/**
 * Add a value to an int64 up-down counter.
 * @param metric Counter handle created by Telemetry_CreateInt64UpDownCounter.
 * @param value Value to add.
 * @param attributes Optional attribute array.
 * @param attributeCount Number of attributes in the array.
 */
SO_PUBLIC extern void Telemetry_UpDownCounterAddInt64(
    TelemetryMetric_t *metric,
    int64_t value,
    const TelemetryMetricAttribute_t *attributes,
    size_t attributeCount
);

/**
 * Add a value to a double up-down counter.
 * @param metric Counter handle created by Telemetry_CreateDoubleUpDownCounter.
 * @param value Value to add.
 * @param attributes Optional attribute array.
 * @param attributeCount Number of attributes in the array.
 */
SO_PUBLIC extern void Telemetry_UpDownCounterAddDouble(
    TelemetryMetric_t *metric,
    double value,
    const TelemetryMetricAttribute_t *attributes,
    size_t attributeCount
);

/**
 * Record a value to a uint64 histogram.
 * @param metric Histogram handle created by Telemetry_CreateUInt64Histogram.
 * @param value Value to record.
 * @param attributes Optional attribute array.
 * @param attributeCount Number of attributes in the array.
 */
SO_PUBLIC extern void Telemetry_HistogramRecordUInt64(
    TelemetryMetric_t *metric,
    uint64_t value,
    const TelemetryMetricAttribute_t *attributes,
    size_t attributeCount
);

/**
 * Record a value to a double histogram.
 * @param metric Histogram handle created by Telemetry_CreateDoubleHistogram.
 * @param value Value to record.
 * @param attributes Optional attribute array.
 * @param attributeCount Number of attributes in the array.
 */
SO_PUBLIC extern void Telemetry_HistogramRecordDouble(
    TelemetryMetric_t *metric,
    double value,
    const TelemetryMetricAttribute_t *attributes,
    size_t attributeCount
);

/**
 * Observe an int64 value inside an observable gauge callback.
 * @param observation Observation handle passed to the callback.
 * @param value Value to observe.
 * @param attributes Optional attribute array.
 * @param attributeCount Number of attributes in the array.
 */
SO_PUBLIC extern void Telemetry_ObservableObserveInt64(
    TelemetryObservation_t *observation,
    int64_t value,
    const TelemetryMetricAttribute_t *attributes,
    size_t attributeCount
);

/**
 * Observe a double value inside an observable gauge callback.
 * @param observation Observation handle passed to the callback.
 * @param value Value to observe.
 * @param attributes Optional attribute array.
 * @param attributeCount Number of attributes in the array.
 */
SO_PUBLIC extern void Telemetry_ObservableObserveDouble(
    TelemetryObservation_t *observation,
    double value,
    const TelemetryMetricAttribute_t *attributes,
    size_t attributeCount
);

SO_PUBLIC extern void Telemetry_RecordRuntimeDependencyState(
    const char *dependency,
    const char *status,
    const char *reasonCode
);

SO_PUBLIC extern void Telemetry_RecordRuntimeWorkflowState(
    const char *workflow,
    const char *state,
    const char *reasonCode
);

SO_PUBLIC extern void Telemetry_RecordRuntimeReadinessState(
    const char *state,
    const char *outcome,
    const char *reasonCode
);

SO_PUBLIC extern void Telemetry_RecordRuntimeStartupDuration(
    double durationSeconds,
    const char *outcome,
    const char *reasonCode
);

SO_PUBLIC extern void Telemetry_RecordRuntimeShutdownDrainDuration(
    double durationSeconds,
    const char *service,
    const char *outcome,
    const char *reasonCode
);

SO_PUBLIC extern void Telemetry_RecordRuntimeTelemetryFlushOutcome(
    const char *service,
    const char *outcome,
    const char *reasonCode
);

#ifdef __cplusplus
}
#endif

#endif
