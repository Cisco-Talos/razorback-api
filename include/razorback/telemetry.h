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
 * Telemetry span helpers.
 */
#ifndef RAZORBACK_PUBLIC_TELEMETRY_H
#define RAZORBACK_PUBLIC_TELEMETRY_H

#include <razorback/messages.h>
#include <razorback/queue.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum TelemetrySpanKind
{
    TELEMETRY_SPAN_KIND_INTERNAL = 0,
    TELEMETRY_SPAN_KIND_CONSUMER = 1,
    TELEMETRY_SPAN_KIND_PRODUCER = 2,
    TELEMETRY_SPAN_KIND_CLIENT = 3
} TelemetrySpanKind_t;

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

#ifdef __cplusplus
}
#endif

#endif
