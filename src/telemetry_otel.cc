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

#include "telemetry.h"
#include "api_internal.h"
#include "block_pool_private.h"
#include "connected_entity_private.h"
#include "submission_private.h"

#include <razorback/api.h>
#include <razorback/block_pool.h>
#include <razorback/messages.h>
#include <razorback/list.h>
#include <razorback/log.h>
#include <razorback/message_formats.h>
#include <razorback/queue.h>
#include <razorback/hash.h>
#include <razorback/uuids.h>

#include <opentelemetry/common/key_value_iterable.h>
#include <opentelemetry/baggage/propagation/baggage_propagator.h>
#include <opentelemetry/context/propagation/global_propagator.h>
#include <opentelemetry/context/propagation/composite_propagator.h>
#include <opentelemetry/context/propagation/text_map_propagator.h>
#include <opentelemetry/context/runtime_context.h>
#include <opentelemetry/exporters/otlp/otlp_http_exporter_factory.h>
#include <opentelemetry/exporters/otlp/otlp_http_exporter_options.h>
#include <opentelemetry/exporters/otlp/otlp_http_log_record_exporter_factory.h>
#include <opentelemetry/exporters/otlp/otlp_http_log_record_exporter_options.h>
#include <opentelemetry/exporters/otlp/otlp_http_metric_exporter_factory.h>
#include <opentelemetry/exporters/otlp/otlp_http_metric_exporter_options.h>
#include <opentelemetry/logs/log_record.h>
#include <opentelemetry/logs/logger.h>
#include <opentelemetry/logs/provider.h>
#include <opentelemetry/logs/severity.h>
#include <opentelemetry/metrics/async_instruments.h>
#include <opentelemetry/metrics/meter.h>
#include <opentelemetry/metrics/noop.h>
#include <opentelemetry/metrics/observer_result.h>
#include <opentelemetry/metrics/provider.h>
#include <opentelemetry/metrics/sync_instruments.h>
#include <opentelemetry/sdk/logs/batch_log_record_processor_factory.h>
#include <opentelemetry/sdk/logs/batch_log_record_processor_options.h>
#include <opentelemetry/sdk/logs/logger_provider.h>
#include <opentelemetry/sdk/logs/logger_provider_factory.h>
#include <opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader_factory.h>
#include <opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader_options.h>
#include <opentelemetry/sdk/metrics/meter_provider.h>
#include <opentelemetry/sdk/metrics/meter_provider_factory.h>
#include <opentelemetry/sdk/metrics/view/view_registry.h>
#include <opentelemetry/sdk/resource/resource.h>
#include <opentelemetry/sdk/trace/batch_span_processor_factory.h>
#include <opentelemetry/sdk/trace/batch_span_processor_options.h>
#include <opentelemetry/sdk/trace/tracer_provider.h>
#include <opentelemetry/trace/context.h>
#include <opentelemetry/trace/noop.h>
#include <opentelemetry/trace/propagation/http_trace_context.h>
#include <opentelemetry/trace/provider.h>
#include <opentelemetry/trace/span_metadata.h>
#include <opentelemetry/trace/scope.h>
#include <opentelemetry/trace/span.h>
#include <opentelemetry/trace/tracer.h>

#include <cstdlib>
#include <cctype>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <strings.h>
#include <utility>
#include <vector>

namespace common_api      = opentelemetry::common;
namespace context_api      = opentelemetry::context;
namespace logs_api         = opentelemetry::logs;
namespace logs_sdk         = opentelemetry::sdk::logs;
namespace metrics_api      = opentelemetry::metrics;
namespace metrics_sdk      = opentelemetry::sdk::metrics;
namespace propagation_api  = opentelemetry::context::propagation;
namespace otlp_exporter    = opentelemetry::exporter::otlp;
namespace resource_sdk     = opentelemetry::sdk::resource;
namespace trace_api        = opentelemetry::trace;
namespace trace_sdk        = opentelemetry::sdk::trace;

static trace_api::SpanKind
TranslateSpanKind(TelemetrySpanKind_t kind)
{
  switch (kind)
  {
  case TELEMETRY_SPAN_KIND_CONSUMER:
    return trace_api::SpanKind::kConsumer;
  case TELEMETRY_SPAN_KIND_PRODUCER:
    return trace_api::SpanKind::kProducer;
  case TELEMETRY_SPAN_KIND_CLIENT:
    return trace_api::SpanKind::kClient;
  case TELEMETRY_SPAN_KIND_INTERNAL:
  default:
    return trace_api::SpanKind::kInternal;
  }
}

struct TelemetrySpan
{
  opentelemetry::nostd::shared_ptr<trace_api::Span> span;
  opentelemetry::nostd::unique_ptr<context_api::Token> token;
};

enum class TelemetryMetricKind
{
  kNone,
  kUInt64Counter,
  kDoubleCounter,
  kInt64UpDownCounter,
  kDoubleUpDownCounter,
  kUInt64Histogram,
  kDoubleHistogram,
  kInt64ObservableGauge,
  kDoubleObservableGauge
};

struct TelemetryMetric
{
  TelemetryMetricKind kind = TelemetryMetricKind::kNone;
  opentelemetry::nostd::unique_ptr<metrics_api::Counter<uint64_t>> uint64Counter;
  opentelemetry::nostd::unique_ptr<metrics_api::Counter<double>> doubleCounter;
  opentelemetry::nostd::unique_ptr<metrics_api::UpDownCounter<int64_t>> int64UpDownCounter;
  opentelemetry::nostd::unique_ptr<metrics_api::UpDownCounter<double>> doubleUpDownCounter;
  opentelemetry::nostd::unique_ptr<metrics_api::Histogram<uint64_t>> uint64Histogram;
  opentelemetry::nostd::unique_ptr<metrics_api::Histogram<double>> doubleHistogram;
  opentelemetry::nostd::shared_ptr<metrics_api::ObservableInstrument> observableGauge;
  TelemetryObservableCallback_t observableCallback = nullptr;
  void *observableUserData = nullptr;
};

struct TelemetryObservation
{
  opentelemetry::nostd::shared_ptr<metrics_api::ObserverResultT<int64_t>> int64Observer;
  opentelemetry::nostd::shared_ptr<metrics_api::ObserverResultT<double>> doubleObserver;
};

namespace
{
class MetricAttributesIterable final : public common_api::KeyValueIterable
{
public:
  MetricAttributesIterable(const TelemetryMetricAttribute_t *attributes,
                           size_t count) noexcept
      : attributes_(attributes), count_(count)
  {
  }

  bool ForEachKeyValue(
      opentelemetry::nostd::function_ref<bool(opentelemetry::nostd::string_view,
                                              common_api::AttributeValue)> callback)
      const noexcept override
  {
    if (attributes_ == nullptr || count_ == 0)
      return true;

    for (size_t i = 0; i < count_; ++i)
    {
      const TelemetryMetricAttribute_t &attribute = attributes_[i];

      if (attribute.name == nullptr || attribute.name[0] == '\0')
        continue;

      switch (attribute.type)
      {
      case TELEMETRY_METRIC_ATTRIBUTE_STRING:
        if (!callback(attribute.name,
                      opentelemetry::nostd::string_view(
                          attribute.stringValue != nullptr ? attribute.stringValue : "")))
          return false;
        break;
      case TELEMETRY_METRIC_ATTRIBUTE_INT:
        if (!callback(attribute.name, attribute.intValue))
          return false;
        break;
      case TELEMETRY_METRIC_ATTRIBUTE_DOUBLE:
        if (!callback(attribute.name, attribute.doubleValue))
          return false;
        break;
      case TELEMETRY_METRIC_ATTRIBUTE_BOOL:
        if (!callback(attribute.name, attribute.boolValue))
          return false;
        break;
      default:
        break;
      }
    }

    return true;
  }

  size_t size() const noexcept override
  {
    size_t valid_count = 0;

    if (attributes_ == nullptr || count_ == 0)
      return 0;

    for (size_t i = 0; i < count_; ++i)
    {
      if (attributes_[i].name != nullptr && attributes_[i].name[0] != '\0')
        ++valid_count;
    }

    return valid_count;
  }

private:
  const TelemetryMetricAttribute_t *attributes_;
  size_t count_;
};

class InjectedHeadersCarrier final : public propagation_api::TextMapCarrier
{
public:
  opentelemetry::nostd::string_view Get(opentelemetry::nostd::string_view) const noexcept override
  {
    return "";
  }

  void Set(opentelemetry::nostd::string_view key,
           opentelemetry::nostd::string_view value) noexcept override
  {
    headers_.emplace_back(std::string(key.data(), key.size()),
                          std::string(value.data(), value.size()));
  }

  const std::vector<std::pair<std::string, std::string>> &Headers() const noexcept
  {
    return headers_;
  }

private:
  std::vector<std::pair<std::string, std::string>> headers_;
};

class MessageHeadersCarrier final : public propagation_api::TextMapCarrier
{
public:
  explicit MessageHeadersCarrier(List_t *headers) noexcept : headers_(headers) {}

  opentelemetry::nostd::string_view Get(opentelemetry::nostd::string_view key) const noexcept override
  {
    std::string lookup(key.data(), key.size());

    if (headers_ == nullptr)
      return "";

    auto *header = static_cast<struct MessageHeader *>(List_Find(headers_, lookup.c_str()));

    if (header == nullptr || header->sValue == nullptr)
      return "";

    return header->sValue;
  }

  void Set(opentelemetry::nostd::string_view,
           opentelemetry::nostd::string_view) noexcept override
  {
  }

private:
  List_t *headers_;
};

class StoredHeadersCarrier final : public propagation_api::TextMapCarrier
{
public:
  explicit StoredHeadersCarrier(const struct TelemetryContextCarrier *carrier) noexcept
      : carrier_(carrier)
  {
  }

  opentelemetry::nostd::string_view Get(opentelemetry::nostd::string_view key) const noexcept override
  {
    std::string lookup(key.data(), key.size());

    if (carrier_ == nullptr || carrier_->entries == nullptr)
      return "";

    for (size_t i = 0; i < carrier_->count; ++i)
    {
      if (carrier_->entries[i].name == nullptr || carrier_->entries[i].value == nullptr)
        continue;

      if (strcmp(carrier_->entries[i].name, lookup.c_str()) == 0)
        return carrier_->entries[i].value;
    }

    return "";
  }

  void Set(opentelemetry::nostd::string_view,
           opentelemetry::nostd::string_view) noexcept override
  {
  }

private:
  const struct TelemetryContextCarrier *carrier_;
};

struct TelemetryState
{
  bool initialized      = false;
  bool tracing_enabled  = false;
  bool logging_enabled  = false;
  bool metrics_enabled  = false;
  std::shared_ptr<trace_sdk::TracerProvider> sdk_provider;
  opentelemetry::nostd::shared_ptr<trace_api::TracerProvider> provider;
  opentelemetry::nostd::shared_ptr<trace_api::Tracer> tracer;
  std::shared_ptr<logs_sdk::LoggerProvider> sdk_logger_provider;
  opentelemetry::nostd::shared_ptr<logs_api::LoggerProvider> logger_provider;
  opentelemetry::nostd::shared_ptr<logs_api::Logger> logger;
  std::shared_ptr<metrics_sdk::MeterProvider> sdk_meter_provider;
  opentelemetry::nostd::shared_ptr<metrics_api::MeterProvider> meter_provider;
  opentelemetry::nostd::shared_ptr<metrics_api::Meter> meter;
};

struct RazorbackStandardMetrics
{
  bool initialized = false;
  TelemetryMetric_t *dispatcherWait = nullptr;
  TelemetryMetric_t *dispatcherSelection = nullptr;
  TelemetryMetric_t *outboundMessages = nullptr;
  TelemetryMetric_t *outboundPublishRetry = nullptr;
  TelemetryMetric_t *outboundReconnect = nullptr;
  TelemetryMetric_t *inspectionInFlight = nullptr;
  TelemetryMetric_t *inspectionDuration = nullptr;
  TelemetryMetric_t *inspectionResults = nullptr;
  TelemetryMetric_t *inspectionErrors = nullptr;
  TelemetryMetric_t *shutdownRequeuedInspections = nullptr;
  TelemetryMetric_t *blockSubmitDecisions = nullptr;
  TelemetryMetric_t *cacheResponses = nullptr;
  TelemetryMetric_t *cacheLookupWait = nullptr;
  TelemetryMetric_t *submitDuration = nullptr;
  TelemetryMetric_t *transferFetchDuration = nullptr;
  TelemetryMetric_t *transferStoreDuration = nullptr;
  TelemetryMetric_t *transferFailures = nullptr;
  TelemetryMetric_t *inspectionWorkQueueDepth = nullptr;
  TelemetryMetric_t *inspectionResultQueueDepth = nullptr;
  TelemetryMetric_t *submitQueueDepth = nullptr;
  TelemetryMetric_t *blockPoolSize = nullptr;
  TelemetryMetric_t *dispatcherAvailable = nullptr;
  TelemetryMetric_t *dispatcherUsable = nullptr;
  TelemetryMetric_t *runtimeDependencyState = nullptr;
  TelemetryMetric_t *runtimeWorkflowState = nullptr;
  TelemetryMetric_t *runtimeReadinessState = nullptr;
  TelemetryMetric_t *runtimeStartupDuration = nullptr;
  TelemetryMetric_t *runtimeShutdownDrainDuration = nullptr;
  TelemetryMetric_t *runtimeTelemetryFlushOutcome = nullptr;
};

TelemetryState &
GetTelemetryState() noexcept
{
  static TelemetryState state;

  return state;
}

RazorbackStandardMetrics &
GetRazorbackStandardMetrics() noexcept
{
  static RazorbackStandardMetrics metrics;

  return metrics;
}

double
GetMonotonicTimeSecondsInternal() noexcept
{
  return std::chrono::duration<double>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

const char *
MetricLabelOrUnknown(const char *value) noexcept
{
  return (value != nullptr && value[0] != '\0') ? value : "unknown";
}

const char *
MetricLabelOrNone(const char *value) noexcept
{
  return (value != nullptr && value[0] != '\0') ? value : "none";
}

const char *
MetricBoundedLabelOrNone(const char *value) noexcept
{
  if (value == nullptr || value[0] == '\0')
    return "none";

  return strlen(value) > 128 ? "other" : value;
}

static void
LowercaseString(char *value) noexcept
{
  if (value == nullptr)
    return;

  for (; *value != '\0'; ++value)
    *value = static_cast<char>(std::tolower(static_cast<unsigned char>(*value)));
}

static const char *
MetricBoolLabel(bool value) noexcept
{
  return value ? "true" : "false";
}

static const char *
MetricStateFromContext(const struct RazorbackContext *context,
                       const char *stateOverride) noexcept
{
  if (stateOverride != nullptr && stateOverride[0] != '\0')
    return stateOverride;

  if (context == nullptr)
    return "none";

  if (atomic_load(&context->paused))
    return "paused";

  if (context->regOk)
    return "running";

  return "unregistered";
}

static size_t
AppendContextMetricAttributes(TelemetryMetricAttribute_t *attributes,
                              size_t attributeCount,
                              const struct RazorbackContext *context,
                              bool includeState,
                              const char *stateOverride,
                              char **nuggetTypeNameStorage)
{
  if (nuggetTypeNameStorage != nullptr)
    *nuggetTypeNameStorage = nullptr;

  if (context != nullptr)
  {
    const char *nuggetTypeName = nullptr;

    if (nuggetTypeNameStorage != nullptr)
    {
      *nuggetTypeNameStorage =
          UUID_Get_NameByUUID(const_cast<unsigned char *>(context->uuidNuggetType),
                              UUID_TYPE_NUGGET_TYPE);
      if (*nuggetTypeNameStorage != nullptr)
        LowercaseString(*nuggetTypeNameStorage);
      nuggetTypeName = *nuggetTypeNameStorage;
    }

    attributes[attributeCount++] = {"rzb.nugget.name", TELEMETRY_METRIC_ATTRIBUTE_STRING,
                                    MetricLabelOrUnknown(context->sNuggetName), 0, 0.0, false};
    attributes[attributeCount++] = {"rzb.nugget.type", TELEMETRY_METRIC_ATTRIBUTE_STRING,
                                    MetricLabelOrUnknown(nuggetTypeName), 0, 0.0, false};
    attributes[attributeCount++] = {"rzb.locality", TELEMETRY_METRIC_ATTRIBUTE_INT,
                                    nullptr, context->locality, 0.0, false};
    attributes[attributeCount++] = {"rzb.dev_mode", TELEMETRY_METRIC_ATTRIBUTE_STRING,
                                    MetricBoolLabel((context->iFlags & CONTEXT_FLAG_STAND_ALONE) ==
                                                    CONTEXT_FLAG_STAND_ALONE),
                                    0, 0.0, false};
  }

  if (includeState)
  {
    attributes[attributeCount++] = {"state", TELEMETRY_METRIC_ATTRIBUTE_STRING,
                                    MetricStateFromContext(context, stateOverride), 0, 0.0,
                                    false};
  }

  return attributeCount;
}

static void
ObserveInt64ForContext(TelemetryObservation_t *observation,
                       int64_t value,
                       const struct RazorbackContext *context,
                       bool includeState,
                       const char *stateOverride)
{
  TelemetryMetricAttribute_t attributes[5];
  char *nuggetTypeName = nullptr;
  size_t attributeCount = 0;

  attributeCount = AppendContextMetricAttributes(attributes, attributeCount, context,
                                                 includeState, stateOverride,
                                                 &nuggetTypeName);

  Telemetry_ObservableObserveInt64(observation, value, attributes, attributeCount);
  free(nuggetTypeName);
}

struct PerContextObservationState
{
  TelemetryObservation_t *observation;
  bool observed;
};

int
ObserveInspectionWorkQueueDepthForContext(struct RazorbackContext *context, void *userData)
{
  auto *state = static_cast<PerContextObservationState *>(userData);
  int64_t depth = 0;

  if (context == nullptr || state == nullptr || state->observation == nullptr)
    return LIST_EACH_OK;

  if (context->inspector.pendingMessages != nullptr)
    depth = static_cast<int64_t>(List_Length(context->inspector.pendingMessages));
  ObserveInt64ForContext(state->observation, depth, context, true, nullptr);
  state->observed = true;
  return LIST_EACH_OK;
}

int
ObserveInspectionResultQueueDepthForContext(struct RazorbackContext *context, void *userData)
{
  auto *state = static_cast<PerContextObservationState *>(userData);
  int64_t depth = 0;

  if (context == nullptr || state == nullptr || state->observation == nullptr)
    return LIST_EACH_OK;

  if (context->inspector.completedMessages != nullptr)
    depth = static_cast<int64_t>(List_Length(context->inspector.completedMessages));
  ObserveInt64ForContext(state->observation, depth, context, true, nullptr);
  state->observed = true;
  return LIST_EACH_OK;
}

int
ObserveSubmitQueueDepthForContext(struct RazorbackContext *context, void *userData)
{
  auto *state = static_cast<PerContextObservationState *>(userData);

  if (context == nullptr || state == nullptr || state->observation == nullptr)
    return LIST_EACH_OK;

  ObserveInt64ForContext(
      state->observation,
      static_cast<int64_t>(Submission_GetContextSubmitQueueDepth(context)),
      context,
      true,
      nullptr);
  state->observed = true;
  return LIST_EACH_OK;
}

int
ObserveBlockPoolSizeForContext(struct RazorbackContext *context, void *userData)
{
  auto *state = static_cast<PerContextObservationState *>(userData);

  if (context == nullptr || state == nullptr || state->observation == nullptr)
    return LIST_EACH_OK;

  ObserveInt64ForContext(
      state->observation,
      static_cast<int64_t>(BlockPool_GetContextItemCount(context)),
      context,
      true,
      nullptr);
  state->observed = true;
  return LIST_EACH_OK;
}

void
ObserveInspectionWorkQueueDepth(TelemetryObservation_t *observation, void *userData)
{
  PerContextObservationState state = {observation, false};

  (void)userData;
  (void)Razorback_ForEach_Context(ObserveInspectionWorkQueueDepthForContext, &state);
  if (!state.observed && observation != nullptr)
    ObserveInt64ForContext(observation, 0, nullptr, true, nullptr);
}

void
ObserveInspectionResultQueueDepth(TelemetryObservation_t *observation, void *userData)
{
  PerContextObservationState state = {observation, false};

  (void)userData;
  (void)Razorback_ForEach_Context(ObserveInspectionResultQueueDepthForContext, &state);
  if (!state.observed && observation != nullptr)
    ObserveInt64ForContext(observation, 0, nullptr, true, nullptr);
}

void
ObserveSubmitQueueDepth(TelemetryObservation_t *observation, void *userData)
{
  PerContextObservationState state = {observation, false};

  (void)userData;
  (void)Razorback_ForEach_Context(ObserveSubmitQueueDepthForContext, &state);
  if (!state.observed && observation != nullptr)
    ObserveInt64ForContext(observation, 0, nullptr, true, nullptr);
}

void
ObserveBlockPoolSize(TelemetryObservation_t *observation, void *userData)
{
  PerContextObservationState state = {observation, false};

  (void)userData;
  (void)Razorback_ForEach_Context(ObserveBlockPoolSizeForContext, &state);
  if (!state.observed && observation != nullptr)
    ObserveInt64ForContext(observation, 0, nullptr, true, nullptr);
}

void
ObserveDispatcherAvailable(TelemetryObservation_t *observation, void *userData)
{
  (void)userData;
  ObserveInt64ForContext(observation,
                         static_cast<int64_t>(ConnectedEntityList_CountDispatchers()),
                         nullptr,
                         true,
                         nullptr);
}

void
ObserveDispatcherUsable(TelemetryObservation_t *observation, void *userData)
{
  (void)userData;
  ObserveInt64ForContext(observation,
                         static_cast<int64_t>(ConnectedEntityList_CountUsableDispatchers()),
                         nullptr,
                         true,
                         nullptr);
}

void
DestroyRazorbackStandardMetrics() noexcept
{
  RazorbackStandardMetrics &metrics = GetRazorbackStandardMetrics();
  TelemetryMetric_t **all_metrics[] = {
      &metrics.dispatcherWait,
      &metrics.dispatcherSelection,
      &metrics.outboundMessages,
      &metrics.outboundPublishRetry,
      &metrics.outboundReconnect,
      &metrics.inspectionInFlight,
      &metrics.inspectionDuration,
      &metrics.inspectionResults,
      &metrics.inspectionErrors,
      &metrics.shutdownRequeuedInspections,
      &metrics.blockSubmitDecisions,
      &metrics.cacheResponses,
      &metrics.cacheLookupWait,
      &metrics.submitDuration,
      &metrics.transferFetchDuration,
      &metrics.transferStoreDuration,
      &metrics.transferFailures,
      &metrics.inspectionWorkQueueDepth,
      &metrics.inspectionResultQueueDepth,
      &metrics.submitQueueDepth,
      &metrics.blockPoolSize,
      &metrics.dispatcherAvailable,
      &metrics.dispatcherUsable,
      &metrics.runtimeDependencyState,
      &metrics.runtimeWorkflowState,
      &metrics.runtimeReadinessState,
      &metrics.runtimeStartupDuration,
      &metrics.runtimeShutdownDrainDuration,
      &metrics.runtimeTelemetryFlushOutcome,
  };

  for (auto *metric_ptr : all_metrics)
  {
    Telemetry_DestroyMetric(*metric_ptr);
    *metric_ptr = nullptr;
  }

  metrics.initialized = false;
}

void
InitializeRazorbackStandardMetrics() noexcept
{
  RazorbackStandardMetrics &metrics = GetRazorbackStandardMetrics();

  if (metrics.initialized)
    return;

  metrics.initialized = true;
  metrics.dispatcherWait = Telemetry_CreateDoubleHistogram(
      "rzb.dispatcher.wait.seconds",
      "Time spent waiting for an available dispatcher.",
      "s");
  metrics.dispatcherSelection = Telemetry_CreateUInt64Counter(
      "rzb.dispatcher.selection.total",
      "Dispatcher selection decisions.",
      "");
  metrics.outboundMessages = Telemetry_CreateUInt64Counter(
      "rzb.outbound.messages.total",
      "Outbound message serialization and publish outcomes.",
      "");
  metrics.outboundPublishRetry = Telemetry_CreateUInt64Counter(
      "rzb.outbound.publish.retry.total",
      "Outbound message publish retries after broker failures.",
      "");
  metrics.outboundReconnect = Telemetry_CreateUInt64Counter(
      "rzb.outbound.reconnect.total",
      "Outbound sender reconnect attempts.",
      "");
  metrics.inspectionInFlight = Telemetry_CreateInt64UpDownCounter(
      "rzb.inspection.inflight",
      "Inspection messages currently being processed.",
      "");
  metrics.inspectionDuration = Telemetry_CreateDoubleHistogram(
      "rzb.inspection.duration.seconds",
      "End-to-end inspection processing duration.",
      "s");
  metrics.inspectionResults = Telemetry_CreateUInt64Counter(
      "rzb.inspection.results.total",
      "Final inspection result outcomes.",
      "");
  metrics.inspectionErrors = Telemetry_CreateUInt64Counter(
      "rzb.inspection.errors.total",
      "Inspection errors grouped by stage.",
      "");
  metrics.shutdownRequeuedInspections = Telemetry_CreateUInt64Counter(
      "rzb.shutdown.requeued.inspections.total",
      "Inspection messages explicitly requeued during shutdown.",
      "");
  metrics.blockSubmitDecisions = Telemetry_CreateUInt64Counter(
      "rzb.block.submit.decisions.total",
      "Block submission decision outcomes.",
      "");
  metrics.cacheResponses = Telemetry_CreateUInt64Counter(
      "rzb.cache.responses.total",
      "Global cache response outcomes.",
      "");
  metrics.cacheLookupWait = Telemetry_CreateDoubleHistogram(
      "rzb.cache.lookup.wait.seconds",
      "Time from cache request submission to cache response handling.",
      "s");
  metrics.submitDuration = Telemetry_CreateDoubleHistogram(
      "rzb.submit.duration.seconds",
      "Submit-thread processing duration per block.",
      "s");
  metrics.transferFetchDuration = Telemetry_CreateDoubleHistogram(
      "rzb.transfer.fetch.duration.seconds",
      "Transfer fetch duration.",
      "s");
  metrics.transferStoreDuration = Telemetry_CreateDoubleHistogram(
      "rzb.transfer.store.duration.seconds",
      "Transfer store duration.",
      "s");
  metrics.transferFailures = Telemetry_CreateUInt64Counter(
      "rzb.transfer.failures.total",
      "Transfer failures grouped by operation and protocol.",
      "");
  metrics.inspectionWorkQueueDepth = Telemetry_CreateInt64ObservableGauge(
      "rzb.inspection.work_queue.depth",
      "Current inspection work queue depth.",
      "",
      ObserveInspectionWorkQueueDepth,
      nullptr);
  metrics.inspectionResultQueueDepth = Telemetry_CreateInt64ObservableGauge(
      "rzb.inspection.result_queue.depth",
      "Current inspection result queue depth.",
      "",
      ObserveInspectionResultQueueDepth,
      nullptr);
  metrics.submitQueueDepth = Telemetry_CreateInt64ObservableGauge(
      "rzb.submit.queue.depth",
      "Current block submission queue depth.",
      "",
      ObserveSubmitQueueDepth,
      nullptr);
  metrics.blockPoolSize = Telemetry_CreateInt64ObservableGauge(
      "rzb.block_pool.size",
      "Current number of tracked block-pool items.",
      "",
      ObserveBlockPoolSize,
      nullptr);
  metrics.dispatcherAvailable = Telemetry_CreateInt64ObservableGauge(
      "rzb.dispatcher.available",
      "Number of known dispatchers.",
      "",
      ObserveDispatcherAvailable,
      nullptr);
  metrics.dispatcherUsable = Telemetry_CreateInt64ObservableGauge(
      "rzb.dispatcher.usable",
      "Number of usable dispatchers.",
      "",
      ObserveDispatcherUsable,
      nullptr);
  metrics.runtimeDependencyState = Telemetry_CreateUInt64Counter(
      RAZORBACK_RUNTIME_DEPENDENCY_STATE_METRIC,
      "Phase 14 runtime dependency state transitions.",
      "");
  metrics.runtimeWorkflowState = Telemetry_CreateUInt64Counter(
      RAZORBACK_RUNTIME_WORKFLOW_STATE_METRIC,
      "Phase 14 runtime workflow state transitions.",
      "");
  metrics.runtimeReadinessState = Telemetry_CreateUInt64Counter(
      RAZORBACK_RUNTIME_READINESS_STATE_METRIC,
      "Phase 14 runtime readiness transitions.",
      "");
  metrics.runtimeStartupDuration = Telemetry_CreateDoubleHistogram(
      RAZORBACK_RUNTIME_STARTUP_DURATION_METRIC,
      "Phase 14 runtime startup duration.",
      "ms");
  metrics.runtimeShutdownDrainDuration = Telemetry_CreateDoubleHistogram(
      RAZORBACK_RUNTIME_SHUTDOWN_DRAIN_DURATION_METRIC,
      "Phase 14 runtime shutdown drain duration.",
      "ms");
  metrics.runtimeTelemetryFlushOutcome = Telemetry_CreateUInt64Counter(
      RAZORBACK_RUNTIME_TELEMETRY_FLUSH_OUTCOME_METRIC,
      "Phase 14 runtime telemetry flush outcomes.",
      "");
}

bool
IsEnvTrue(const char *name) noexcept
{
  const char *value = std::getenv(name);

  if (value == nullptr)
    return false;

  return (strcasecmp(value, "true") == 0) || (strcmp(value, "1") == 0) ||
         (strcasecmp(value, "yes") == 0);
}

const char *
GetServiceName() noexcept
{
  const char *service_name = std::getenv("OTEL_SERVICE_NAME");

  if (service_name != nullptr && service_name[0] != '\0')
    return service_name;

  return "razorback-api";
}

logs_api::Severity
TranslateLogSeverity(unsigned level) noexcept
{
  switch (level)
  {
  case LOG_EMERG:
    return logs_api::Severity::kFatal4;
  case LOG_ALERT:
    return logs_api::Severity::kFatal3;
  case LOG_CRIT:
    return logs_api::Severity::kFatal2;
  case LOG_ERR:
    return logs_api::Severity::kError;
  case LOG_WARNING:
    return logs_api::Severity::kWarn;
  case LOG_NOTICE:
    return logs_api::Severity::kInfo2;
  case LOG_INFO:
    return logs_api::Severity::kInfo;
  case LOG_DEBUG:
  default:
    return logs_api::Severity::kDebug;
  }
}

const char *
GetLogComponentName(uint64_t component) noexcept
{
  switch (component)
  {
  case LOG_C_CORE:
    return "core";
  case LOG_C_NETWORK:
    return "network";
  case LOG_C_STOMP:
    return "stomp";
  case LOG_C_QUEUE:
    return "queue";
  case LOG_C_TRANSFER:
    return "transfer";
  case LOG_C_CNC:
    return "cnc";
  case LOG_C_CONFIG:
    return "config";
  case LOG_C_MAGIC:
    return "magic";
  case LOG_C_LIST:
    return "list";
  case LOG_C_JSON:
    return "json";
  case LOG_C_DISPATCHER:
    return "dispatcher";
  case LOG_C_NUGGET:
    return "nugget";
  default:
    return nullptr;
  }
}

class ScopedBoolGuard
{
public:
  explicit ScopedBoolGuard(bool &flag) noexcept : flag_(flag) { flag_ = true; }
  ~ScopedBoolGuard() { flag_ = false; }

private:
  bool &flag_;
};

std::string
MakeSpanName(const char *operation, const char *destination)
{
  std::string name;

  name = (operation != nullptr) ? operation : "messaging";
  if (destination != nullptr && destination[0] != '\0')
  {
    name += ' ';
    name += destination;
  }

  return name;
}

const char *
SafeString(const char *value) noexcept
{
  return value != nullptr ? value : "";
}

context_api::Context
ExtractMessageContext(const struct Message *message) noexcept
{
  propagation_api::TextMapPropagator *propagator_ptr = nullptr;
  opentelemetry::nostd::shared_ptr<propagation_api::TextMapPropagator> propagator;
  MessageHeadersCarrier carrier(message != nullptr ? message->headers : nullptr);
  context_api::Context current;

  if (message == nullptr || message->headers == nullptr)
    return current;

  propagator = propagation_api::GlobalTextMapPropagator::GetGlobalPropagator();
  propagator_ptr = propagator.get();
  if (propagator_ptr == nullptr)
    return current;

  return propagator_ptr->Extract(carrier, current);
}

context_api::Context
ExtractStoredContext(const struct TelemetryContextCarrier *carrier) noexcept
{
  propagation_api::TextMapPropagator *propagator_ptr = nullptr;
  opentelemetry::nostd::shared_ptr<propagation_api::TextMapPropagator> propagator;
  StoredHeadersCarrier stored_carrier(carrier);
  context_api::Context current;

  if (carrier == nullptr || carrier->entries == nullptr || carrier->count == 0)
    return current;

  propagator = propagation_api::GlobalTextMapPropagator::GetGlobalPropagator();
  propagator_ptr = propagator.get();
  if (propagator_ptr == nullptr)
    return current;

  return propagator_ptr->Extract(stored_carrier, current);
}

trace_api::SpanContext
GetLinkedSpanContext(const context_api::Context &context) noexcept
{
  return trace_api::GetSpan(context)->GetContext();
}

context_api::Context
MarkContextAsRoot(const context_api::Context &context) noexcept
{
  context_api::Context root_context = context;

  return root_context.SetValue(trace_api::kIsRootSpanKey, true);
}

void
FreeTelemetryHeaders(struct TelemetryHeader *entries, size_t count) noexcept
{
  if (entries == nullptr)
    return;

  for (size_t i = 0; i < count; ++i)
  {
    free(entries[i].name);
    free(entries[i].value);
  }

  free(entries);
}

void
DestroyContextCarrier(struct TelemetryContextCarrier *carrier) noexcept
{
  if (carrier == nullptr)
    return;

  FreeTelemetryHeaders(carrier->entries, carrier->count);
  free(carrier);
}

bool
ReplaceContextCarrier(const context_api::Context &context,
                      struct TelemetryContextCarrier **carrier) noexcept
{
  propagation_api::TextMapPropagator *propagator_ptr = nullptr;
  opentelemetry::nostd::shared_ptr<propagation_api::TextMapPropagator> propagator;
  InjectedHeadersCarrier injected_headers;
  struct TelemetryContextCarrier *next = nullptr;

  if (carrier == nullptr)
    return false;

  propagator = propagation_api::GlobalTextMapPropagator::GetGlobalPropagator();
  propagator_ptr = propagator.get();
  if (propagator_ptr == nullptr)
    return false;

  next = static_cast<struct TelemetryContextCarrier *>(
      calloc(1, sizeof(struct TelemetryContextCarrier)));
  if (next == nullptr)
    return false;

  propagator_ptr->Inject(injected_headers, context);
  next->count = injected_headers.Headers().size();
  if (next->count > 0)
  {
    next->entries = static_cast<struct TelemetryHeader *>(
        calloc(next->count, sizeof(struct TelemetryHeader)));
    if (next->entries == nullptr)
    {
      free(next);
      return false;
    }

    for (size_t i = 0; i < next->count; ++i)
    {
      next->entries[i].name = strdup(injected_headers.Headers()[i].first.c_str());
      next->entries[i].value = strdup(injected_headers.Headers()[i].second.c_str());
      if (next->entries[i].name == nullptr || next->entries[i].value == nullptr)
      {
        FreeTelemetryHeaders(next->entries, next->count);
        free(next);
        return false;
      }
    }
  }

  DestroyContextCarrier(*carrier);
  *carrier = next;
  return true;
}

bool
ReplaceInjectedHeaders(const context_api::Context &context,
                       struct TelemetryInjectedHeaders *headers) noexcept
{
  propagation_api::TextMapPropagator *propagator_ptr = nullptr;
  opentelemetry::nostd::shared_ptr<propagation_api::TextMapPropagator> propagator;
  InjectedHeadersCarrier carrier;

  if (headers == nullptr)
    return false;

  FreeTelemetryHeaders(headers->entries, headers->count);
  headers->entries = nullptr;
  headers->count = 0;

  propagator = propagation_api::GlobalTextMapPropagator::GetGlobalPropagator();
  propagator_ptr = propagator.get();
  if (propagator_ptr == nullptr)
    return false;

  propagator_ptr->Inject(carrier, context);
  headers->count = carrier.Headers().size();
  if (headers->count == 0)
    return true;

  headers->entries = static_cast<struct TelemetryHeader *>(
      calloc(headers->count, sizeof(struct TelemetryHeader)));
  if (headers->entries == nullptr)
  {
    headers->count = 0;
    return false;
  }

  for (size_t i = 0; i < headers->count; ++i)
  {
    headers->entries[i].name = strdup(carrier.Headers()[i].first.c_str());
    headers->entries[i].value = strdup(carrier.Headers()[i].second.c_str());
    if (headers->entries[i].name == nullptr || headers->entries[i].value == nullptr)
    {
      FreeTelemetryHeaders(headers->entries, headers->count);
      headers->entries = nullptr;
      headers->count = 0;
      return false;
    }
  }

  return true;
}

bool
SetMessageHeaderValue(struct Message *message, const char *name, const char *value) noexcept
{
  struct MessageHeader *header = NULL;
  char *newValue = NULL;

  if (message == NULL || message->headers == NULL || name == NULL || value == NULL)
    return false;

  header = static_cast<struct MessageHeader *>(List_Find(message->headers, name));
  if (header == NULL)
    return Message_Add_Header(message, name, value);

  newValue = strdup(value);
  if (newValue == NULL)
    return false;

  free(header->sValue);
  header->sValue = newValue;
  return true;
}

void
InjectContextIntoMessageHeaders(const context_api::Context &context, struct Message *message) noexcept
{
  propagation_api::TextMapPropagator *propagator_ptr = nullptr;
  opentelemetry::nostd::shared_ptr<propagation_api::TextMapPropagator> propagator;
  InjectedHeadersCarrier carrier;

  if (message == nullptr || message->headers == nullptr)
    return;

  propagator = propagation_api::GlobalTextMapPropagator::GetGlobalPropagator();
  propagator_ptr = propagator.get();
  if (propagator_ptr == nullptr)
    return;

  propagator_ptr->Inject(carrier, context);

  for (const auto &header : carrier.Headers())
  {
    if (!SetMessageHeaderValue(message, header.first.c_str(), header.second.c_str()))
    {
      rzb_log(LOG_ERR, LOG_C_CORE, "%s: failed to update propagated header %s",
              __func__, header.first.c_str());
    }
  }
}

void
SetCommonMessageAttributes(const opentelemetry::nostd::shared_ptr<trace_api::Span> &span,
                           const struct Queue *queue,
                           const struct Message *message,
                           const char *destination,
                           const char *operation_type,
                           const char *operation_name) noexcept
{
  if (span == nullptr || !span->IsRecording())
    return;

  span->SetAttribute("messaging.system", "rabbitmq");
  if (destination != nullptr && destination[0] != '\0')
    span->SetAttribute("messaging.destination.name", destination);
  if (operation_type != nullptr)
    span->SetAttribute("messaging.operation.type", operation_type);
  if (operation_name != nullptr)
    span->SetAttribute("messaging.operation.name", operation_name);

  if (queue != nullptr)
  {
    if (queue->sHostname != nullptr)
      span->SetAttribute("server.address", queue->sHostname);
    span->SetAttribute("server.port", static_cast<int64_t>(queue->iPort));
    span->SetAttribute("network.protocol.name", queue->bUseSSL ? "amqps" : "amqp");
    span->SetAttribute("rzb.queue.name", queue->sName != nullptr ? queue->sName : "");
    span->SetAttribute("rzb.queue.topic", queue->bTopic);

    if (queue->bTopic && destination != nullptr && destination[0] != '\0')
      span->SetAttribute("messaging.rabbitmq.destination.routing_key", destination);
  }

  if (message != nullptr)
  {
    span->SetAttribute("rzb.message.type", static_cast<int64_t>(message->type));
    span->SetAttribute("rzb.message.version", static_cast<int64_t>(message->version));
    span->SetAttribute("messaging.message.body.size",
                       static_cast<int64_t>(message->length));
  }
}

extern "C" void
Telemetry_AddBlockAttributes(TelemetrySpan_t *span,
                             const struct Block *block)
{
  char *hash_text = NULL;
  char *datatype_name = NULL;

  if (span == NULL || block == NULL || block->pId == NULL)
    return;

  Telemetry_AddIntAttribute(span, "rzb.block.size",
                            static_cast<int64_t>(block->pId->iLength));

  if (block->pId->pHash != NULL &&
      (block->pId->pHash->iFlags & HASH_FLAG_FINAL) != 0)
  {
    hash_text = Hash_ToText(block->pId->pHash);
    if (hash_text != NULL)
    {
      Telemetry_AddStringAttribute(span, "rzb.block.sha256", hash_text);
      free(hash_text);
    }
  }

  if (uuid_is_null(block->pId->uuidDataType) != 1)
  {
    datatype_name = UUID_Get_NameByUUID(block->pId->uuidDataType,
                                        UUID_TYPE_DATA_TYPE);
    if (datatype_name != NULL)
    {
      Telemetry_AddStringAttribute(span, "rzb.block.datatype", datatype_name);
      free(datatype_name);
    }
  }

  if (block->data.fileName != NULL && block->data.fileName[0] != '\0')
    Telemetry_AddStringAttribute(span, "rzb.file.name", block->data.fileName);
}

enum class MessageContextMode
{
  kIgnore,
  kLink,
  kParent
};

TelemetrySpan_t *
StartSpanWithLink(const char *span_name,
                  trace_api::SpanKind kind,
                  const struct Queue *queue,
                  const struct Message *message,
                  const char *destination,
                  const char *operation_type,
                  const char *operation_name,
                  MessageContextMode message_context_mode,
                  bool update_message_headers,
                  bool inject_headers,
                  struct TelemetryInjectedHeaders *headers) noexcept
{
  TelemetryState &state = GetTelemetryState();
  TelemetrySpan_t *handle = nullptr;
  trace_api::StartSpanOptions options;
  trace_api::SpanContext linked_context = trace_api::SpanContext::GetInvalid();
  context_api::Context extracted_context;
  context_api::Context parent_context;
  context_api::Context scope_context = context_api::RuntimeContext::GetCurrent();
  bool has_extracted_parent = false;

  if (!state.tracing_enabled || state.tracer == nullptr)
  {
    if (headers != nullptr)
    {
      headers->count = 0;
      headers->entries = nullptr;
    }
    return nullptr;
  }

  try
  {
    using EmptyAttributes =
        std::array<std::pair<opentelemetry::nostd::string_view,
                             opentelemetry::common::AttributeValue>,
                   0>;
    using Link = std::pair<trace_api::SpanContext, EmptyAttributes>;

    options.kind = kind;
    handle = new TelemetrySpan;
    extracted_context = ExtractMessageContext(message);
    linked_context = GetLinkedSpanContext(extracted_context);
    has_extracted_parent = linked_context.IsValid();
    if (message_context_mode == MessageContextMode::kParent)
    {
      parent_context = has_extracted_parent ? extracted_context
                                            : MarkContextAsRoot(extracted_context);
      options.parent = parent_context;
    }

    if (message_context_mode == MessageContextMode::kLink && linked_context.IsValid())
    {
      std::array<Link, 1> links = {{{linked_context, {}}}};

      handle->span = state.tracer->StartSpan(MakeSpanName(span_name, destination),
                                                EmptyAttributes{}, links, options);
    }
    else
    {
      handle->span = state.tracer->StartSpan(MakeSpanName(span_name, destination), options);
    }
    if (message_context_mode == MessageContextMode::kParent)
      scope_context = parent_context;
    scope_context = trace_api::SetSpan(scope_context, handle->span);
    handle->token = context_api::RuntimeContext::Attach(scope_context);

    SetCommonMessageAttributes(handle->span, queue, message, destination, operation_type,
                               operation_name);

    if (update_message_headers)
      InjectContextIntoMessageHeaders(context_api::RuntimeContext::GetCurrent(), const_cast<struct Message *>(message));

    if (inject_headers && headers != nullptr)
      (void)ReplaceInjectedHeaders(context_api::RuntimeContext::GetCurrent(), headers);

    return handle;
  }
  catch (...)
  {
    Telemetry_FreeInjectedHeaders(headers);
    delete handle;
    rzb_log(LOG_ERR, LOG_C_CORE, "%s: failed to start telemetry span", __func__);
    return nullptr;
  }
}

template <typename Instrument, typename Value>
void
AddMetricValue(Instrument *instrument,
               Value value,
               const TelemetryMetricAttribute_t *attributes,
               size_t attribute_count) noexcept
{
  MetricAttributesIterable metric_attributes(attributes, attribute_count);

  if (instrument == nullptr)
    return;

  instrument->Add(value, metric_attributes, context_api::RuntimeContext::GetCurrent());
}

template <typename Instrument, typename Value>
void
RecordMetricValue(Instrument *instrument,
                  Value value,
                  const TelemetryMetricAttribute_t *attributes,
                  size_t attribute_count) noexcept
{
  MetricAttributesIterable metric_attributes(attributes, attribute_count);

  if (instrument == nullptr)
    return;

  instrument->Record(value, metric_attributes, context_api::RuntimeContext::GetCurrent());
}

void
TelemetryObservableGaugeCallbackBridge(metrics_api::ObserverResult observer_result,
                                       void *state) noexcept
{
  auto *metric = static_cast<TelemetryMetric_t *>(state);
  TelemetryObservation_t observation;

  if (metric == nullptr || metric->observableCallback == nullptr)
    return;

  try
  {
    switch (metric->kind)
    {
    case TelemetryMetricKind::kInt64ObservableGauge:
      if (!opentelemetry::nostd::holds_alternative<
              opentelemetry::nostd::shared_ptr<metrics_api::ObserverResultT<int64_t>>>(
              observer_result))
        return;
      observation.int64Observer =
          opentelemetry::nostd::get<
              opentelemetry::nostd::shared_ptr<metrics_api::ObserverResultT<int64_t>>>(
              observer_result);
      break;
    case TelemetryMetricKind::kDoubleObservableGauge:
      if (!opentelemetry::nostd::holds_alternative<
              opentelemetry::nostd::shared_ptr<metrics_api::ObserverResultT<double>>>(
              observer_result))
        return;
      observation.doubleObserver =
          opentelemetry::nostd::get<
              opentelemetry::nostd::shared_ptr<metrics_api::ObserverResultT<double>>>(
              observer_result);
      break;
    default:
      return;
    }

    metric->observableCallback(&observation, metric->observableUserData);
  }
  catch (...)
  {
    rzb_log(LOG_ERR, LOG_C_CORE, "%s: observable metric callback failed", __func__);
  }
}
}  // namespace

extern "C" bool
Telemetry_Initialize(void)
{
  TelemetryState &state = GetTelemetryState();

  try
  {
    std::unique_ptr<trace_sdk::SpanExporter> trace_exporter;
    std::unique_ptr<trace_sdk::SpanProcessor> trace_processor;
    std::unique_ptr<logs_sdk::LogRecordExporter> log_exporter;
    std::unique_ptr<logs_sdk::LogRecordProcessor> log_processor;
    std::unique_ptr<metrics_sdk::PushMetricExporter> metric_exporter;
    std::unique_ptr<metrics_sdk::MetricReader> metric_reader;
    std::vector<std::unique_ptr<propagation_api::TextMapPropagator>> propagators;
    resource_sdk::Resource resource;
    resource_sdk::ResourceAttributes resource_attributes;
    otlp_exporter::OtlpHttpExporterOptions trace_exporter_options;
    otlp_exporter::OtlpHttpLogRecordExporterOptions log_exporter_options;
    otlp_exporter::OtlpHttpMetricExporterOptions metric_exporter_options;
    trace_sdk::BatchSpanProcessorOptions trace_batch_options;
    logs_sdk::BatchLogRecordProcessorOptions log_batch_options;
    metrics_sdk::PeriodicExportingMetricReaderOptions metric_reader_options;

    if (state.initialized)
      return true;

    state.tracing_enabled = false;
    state.logging_enabled = false;
    state.metrics_enabled = false;

    if (IsEnvTrue("OTEL_SDK_DISABLED"))
    {
      state.initialized = true;
      return true;
    }

    resource_attributes["service.name"] = std::string(GetServiceName());
    resource_attributes["service.version"] = std::string(PACKAGE_VERSION);
    resource = resource_sdk::Resource::Create(resource_attributes);

    trace_exporter = otlp_exporter::OtlpHttpExporterFactory::Create(trace_exporter_options);
    trace_processor =
        trace_sdk::BatchSpanProcessorFactory::Create(std::move(trace_exporter),
                                                     trace_batch_options);
    state.sdk_provider =
        std::make_shared<trace_sdk::TracerProvider>(std::move(trace_processor), resource);
    state.provider = opentelemetry::nostd::shared_ptr<trace_api::TracerProvider>(
        std::shared_ptr<trace_api::TracerProvider>(state.sdk_provider));
    trace_api::Provider::SetTracerProvider(state.provider);
    state.tracer =
        state.provider->GetTracer("razorback.api.messaging", PACKAGE_VERSION);
    state.tracing_enabled = true;

    log_exporter =
        otlp_exporter::OtlpHttpLogRecordExporterFactory::Create(log_exporter_options);
    log_processor = logs_sdk::BatchLogRecordProcessorFactory::Create(
        std::move(log_exporter), log_batch_options);
    state.sdk_logger_provider = std::shared_ptr<logs_sdk::LoggerProvider>(
        logs_sdk::LoggerProviderFactory::Create(std::move(log_processor), resource).release());
    state.logger_provider = opentelemetry::nostd::shared_ptr<logs_api::LoggerProvider>(
        std::shared_ptr<logs_api::LoggerProvider>(state.sdk_logger_provider));
    logs_api::Provider::SetLoggerProvider(state.logger_provider);
    state.logger =
        state.logger_provider->GetLogger("razorback.api.logging", "", PACKAGE_VERSION);
    state.logging_enabled = true;

    metric_exporter =
        otlp_exporter::OtlpHttpMetricExporterFactory::Create(metric_exporter_options);
    metric_reader = metrics_sdk::PeriodicExportingMetricReaderFactory::Create(
        std::move(metric_exporter), metric_reader_options);
    state.sdk_meter_provider = std::shared_ptr<metrics_sdk::MeterProvider>(
        metrics_sdk::MeterProviderFactory::Create(
            std::unique_ptr<metrics_sdk::ViewRegistry>(new metrics_sdk::ViewRegistry()),
            resource)
            .release());
    state.sdk_meter_provider->AddMetricReader(
        std::shared_ptr<metrics_sdk::MetricReader>(metric_reader.release()));
    state.meter_provider = opentelemetry::nostd::shared_ptr<metrics_api::MeterProvider>(
        std::shared_ptr<metrics_api::MeterProvider>(state.sdk_meter_provider));
    metrics_api::Provider::SetMeterProvider(state.meter_provider);
    state.meter =
        state.meter_provider->GetMeter("razorback.api.metrics", PACKAGE_VERSION);
    state.metrics_enabled = true;
    InitializeRazorbackStandardMetrics();

    propagators.emplace_back(new trace_api::propagation::HttpTraceContext);
    propagators.emplace_back(new opentelemetry::baggage::propagation::BaggagePropagator);
    propagation_api::GlobalTextMapPropagator::SetGlobalPropagator(
        opentelemetry::nostd::shared_ptr<propagation_api::TextMapPropagator>(
            new propagation_api::CompositePropagator(std::move(propagators))));

    state.initialized = true;
    return true;
  }
  catch (...)
  {
    rzb_log(LOG_ERR, LOG_C_CORE, "%s: failed to initialize OpenTelemetry telemetry", __func__);
    DestroyRazorbackStandardMetrics();
    state.tracer = opentelemetry::nostd::shared_ptr<trace_api::Tracer>();
    state.provider = opentelemetry::nostd::shared_ptr<trace_api::TracerProvider>();
    state.sdk_provider.reset();
    state.logger = opentelemetry::nostd::shared_ptr<logs_api::Logger>();
    state.logger_provider = opentelemetry::nostd::shared_ptr<logs_api::LoggerProvider>();
    state.sdk_logger_provider.reset();
    metrics_api::Provider::SetMeterProvider(
        opentelemetry::nostd::shared_ptr<metrics_api::MeterProvider>(
            std::make_shared<metrics_api::NoopMeterProvider>()));
    state.meter = opentelemetry::nostd::shared_ptr<metrics_api::Meter>();
    state.meter_provider = opentelemetry::nostd::shared_ptr<metrics_api::MeterProvider>();
    state.sdk_meter_provider.reset();
    state.tracing_enabled = false;
    state.logging_enabled = false;
    state.metrics_enabled = false;
    state.initialized = false;
    return false;
  }
}

extern "C" void
Telemetry_Shutdown(void)
{
  TelemetryState &state = GetTelemetryState();

  if (!state.initialized)
    return;

  Telemetry_RecordRuntimeTelemetryFlushOutcome(GetServiceName(), "attempted", nullptr);

  DestroyRazorbackStandardMetrics();

  if (state.sdk_provider)
  {
    state.sdk_provider->ForceFlush();
    state.sdk_provider->Shutdown();
  }
  if (state.sdk_logger_provider)
  {
    state.sdk_logger_provider->ForceFlush();
    state.sdk_logger_provider->Shutdown();
  }
  if (state.sdk_meter_provider)
  {
    state.sdk_meter_provider->ForceFlush();
    state.sdk_meter_provider->Shutdown();
  }

  trace_api::Provider::SetTracerProvider(
      opentelemetry::nostd::shared_ptr<trace_api::TracerProvider>(
          std::make_shared<trace_api::NoopTracerProvider>()));
  logs_api::Provider::SetLoggerProvider(
      opentelemetry::nostd::shared_ptr<logs_api::LoggerProvider>(
          std::make_shared<logs_api::NoopLoggerProvider>()));
  metrics_api::Provider::SetMeterProvider(
      opentelemetry::nostd::shared_ptr<metrics_api::MeterProvider>(
          std::make_shared<metrics_api::NoopMeterProvider>()));
  state.tracer = opentelemetry::nostd::shared_ptr<trace_api::Tracer>();
  state.provider = opentelemetry::nostd::shared_ptr<trace_api::TracerProvider>();
  state.sdk_provider.reset();
  state.logger = opentelemetry::nostd::shared_ptr<logs_api::Logger>();
  state.logger_provider = opentelemetry::nostd::shared_ptr<logs_api::LoggerProvider>();
  state.sdk_logger_provider.reset();
  state.meter = opentelemetry::nostd::shared_ptr<metrics_api::Meter>();
  state.meter_provider = opentelemetry::nostd::shared_ptr<metrics_api::MeterProvider>();
  state.sdk_meter_provider.reset();
  state.tracing_enabled = false;
  state.logging_enabled = false;
  state.metrics_enabled = false;
  state.initialized = false;
}

extern "C" double
Telemetry_GetMonotonicTimeSeconds(void)
{
  return GetMonotonicTimeSecondsInternal();
}

extern "C" void
Telemetry_RecordRuntimeDependencyState(const char *dependency,
                                       const char *status,
                                       const char *reasonCode)
{
  RazorbackStandardMetrics &metrics = GetRazorbackStandardMetrics();
  TelemetryMetricAttribute_t attributes[3] = {
      {"dependency", TELEMETRY_METRIC_ATTRIBUTE_STRING,
       MetricBoundedLabelOrNone(dependency), 0, 0.0, false},
      {"status", TELEMETRY_METRIC_ATTRIBUTE_STRING,
       MetricBoundedLabelOrNone(status), 0, 0.0, false},
      {"reason_code", TELEMETRY_METRIC_ATTRIBUTE_STRING,
       MetricBoundedLabelOrNone(reasonCode), 0, 0.0, false},
  };

  Telemetry_CounterAddUInt64(metrics.runtimeDependencyState, 1, attributes, 3);
}

extern "C" void
Telemetry_RecordRuntimeWorkflowState(const char *workflow,
                                     const char *state,
                                     const char *reasonCode)
{
  RazorbackStandardMetrics &metrics = GetRazorbackStandardMetrics();
  TelemetryMetricAttribute_t attributes[3] = {
      {"workflow", TELEMETRY_METRIC_ATTRIBUTE_STRING,
       MetricBoundedLabelOrNone(workflow), 0, 0.0, false},
      {"state", TELEMETRY_METRIC_ATTRIBUTE_STRING,
       MetricBoundedLabelOrNone(state), 0, 0.0, false},
      {"reason_code", TELEMETRY_METRIC_ATTRIBUTE_STRING,
       MetricBoundedLabelOrNone(reasonCode), 0, 0.0, false},
  };

  Telemetry_CounterAddUInt64(metrics.runtimeWorkflowState, 1, attributes, 3);
}

extern "C" void
Telemetry_RecordRuntimeReadinessState(const char *state,
                                      const char *outcome,
                                      const char *reasonCode)
{
  RazorbackStandardMetrics &metrics = GetRazorbackStandardMetrics();
  TelemetryMetricAttribute_t attributes[3] = {
      {"state", TELEMETRY_METRIC_ATTRIBUTE_STRING,
       MetricBoundedLabelOrNone(state), 0, 0.0, false},
      {"outcome", TELEMETRY_METRIC_ATTRIBUTE_STRING,
       MetricBoundedLabelOrNone(outcome), 0, 0.0, false},
      {"reason_code", TELEMETRY_METRIC_ATTRIBUTE_STRING,
       MetricBoundedLabelOrNone(reasonCode), 0, 0.0, false},
  };

  Telemetry_CounterAddUInt64(metrics.runtimeReadinessState, 1, attributes, 3);
}

extern "C" void
Telemetry_RecordRuntimeStartupDuration(double durationSeconds,
                                       const char *outcome,
                                       const char *reasonCode)
{
  RazorbackStandardMetrics &metrics = GetRazorbackStandardMetrics();
  TelemetryMetricAttribute_t attributes[2] = {
      {"outcome", TELEMETRY_METRIC_ATTRIBUTE_STRING,
       MetricBoundedLabelOrNone(outcome), 0, 0.0, false},
      {"reason_code", TELEMETRY_METRIC_ATTRIBUTE_STRING,
       MetricBoundedLabelOrNone(reasonCode), 0, 0.0, false},
  };

  Telemetry_HistogramRecordDouble(metrics.runtimeStartupDuration,
                                  durationSeconds * 1000.0, attributes, 2);
}

extern "C" void
Telemetry_RecordRuntimeShutdownDrainDuration(double durationSeconds,
                                             const char *service,
                                             const char *outcome,
                                             const char *reasonCode)
{
  RazorbackStandardMetrics &metrics = GetRazorbackStandardMetrics();
  TelemetryMetricAttribute_t attributes[3] = {
      {"service", TELEMETRY_METRIC_ATTRIBUTE_STRING,
       MetricBoundedLabelOrNone(service), 0, 0.0, false},
      {"outcome", TELEMETRY_METRIC_ATTRIBUTE_STRING,
       MetricBoundedLabelOrNone(outcome), 0, 0.0, false},
      {"reason_code", TELEMETRY_METRIC_ATTRIBUTE_STRING,
       MetricBoundedLabelOrNone(reasonCode), 0, 0.0, false},
  };

  Telemetry_HistogramRecordDouble(metrics.runtimeShutdownDrainDuration,
                                  durationSeconds * 1000.0, attributes, 3);
}

extern "C" void
Telemetry_RecordRuntimeTelemetryFlushOutcome(const char *service,
                                             const char *outcome,
                                             const char *reasonCode)
{
  RazorbackStandardMetrics &metrics = GetRazorbackStandardMetrics();
  TelemetryMetricAttribute_t attributes[3] = {
      {"service", TELEMETRY_METRIC_ATTRIBUTE_STRING,
       MetricBoundedLabelOrNone(service), 0, 0.0, false},
      {"outcome", TELEMETRY_METRIC_ATTRIBUTE_STRING,
       MetricBoundedLabelOrNone(outcome), 0, 0.0, false},
      {"reason_code", TELEMETRY_METRIC_ATTRIBUTE_STRING,
       MetricBoundedLabelOrNone(reasonCode), 0, 0.0, false},
  };

  Telemetry_CounterAddUInt64(metrics.runtimeTelemetryFlushOutcome, 1, attributes, 3);
}

extern "C" void
Telemetry_RecordDispatcherWait(double durationSeconds,
                               const char *outcome,
                               const char *stage,
                               const struct RazorbackContext *context)
{
  RazorbackStandardMetrics &metrics = GetRazorbackStandardMetrics();
  TelemetryMetricAttribute_t attributes[6];
  char *nuggetTypeName = nullptr;
  size_t attributeCount = 0;

  attributes[attributeCount++] = {"outcome", TELEMETRY_METRIC_ATTRIBUTE_STRING,
                                  MetricLabelOrUnknown(outcome), 0, 0.0, false};
  attributes[attributeCount++] = {"stage", TELEMETRY_METRIC_ATTRIBUTE_STRING,
                                  MetricLabelOrUnknown(stage), 0, 0.0, false};
  attributeCount = AppendContextMetricAttributes(attributes, attributeCount, context, false,
                                                 nullptr, &nuggetTypeName);

  Telemetry_HistogramRecordDouble(metrics.dispatcherWait,
                                  (durationSeconds >= 0.0) ? durationSeconds : 0.0,
                                  attributes,
                                  attributeCount);
  free(nuggetTypeName);
}

extern "C" void
Telemetry_RecordDispatcherSelection(const char *path,
                                    const char *selectedLocality,
                                    const struct RazorbackContext *context)
{
  RazorbackStandardMetrics &metrics = GetRazorbackStandardMetrics();
  TelemetryMetricAttribute_t attributes[6];
  char *nuggetTypeName = nullptr;
  size_t attributeCount = 0;

  attributes[attributeCount++] = {"path", TELEMETRY_METRIC_ATTRIBUTE_STRING,
                                  MetricLabelOrUnknown(path), 0, 0.0, false};
  attributes[attributeCount++] = {"selected_locality", TELEMETRY_METRIC_ATTRIBUTE_STRING,
                                  MetricLabelOrUnknown(selectedLocality), 0, 0.0, false};
  attributeCount = AppendContextMetricAttributes(attributes, attributeCount, context, false,
                                                 nullptr, &nuggetTypeName);
  Telemetry_CounterAddUInt64(metrics.dispatcherSelection, 1, attributes, attributeCount);
  free(nuggetTypeName);
}

extern "C" void
Telemetry_RecordOutboundMessage(uint32_t messageType,
                                const char *outcome,
                                const char *messageFamily,
                                const char *destination,
                                const char *exchangeKind,
                                const struct RazorbackContext *context)
{
  RazorbackStandardMetrics &metrics = GetRazorbackStandardMetrics();
  TelemetryMetricAttribute_t attributes[9];
  char typeBuffer[32];
  char *nuggetTypeName = nullptr;
  size_t attributeCount = 0;

  std::snprintf(typeBuffer, sizeof(typeBuffer), "%u", messageType);
  attributes[attributeCount++] = {"message_type", TELEMETRY_METRIC_ATTRIBUTE_STRING,
                                  typeBuffer, 0, 0.0, false};
  attributes[attributeCount++] = {"outcome", TELEMETRY_METRIC_ATTRIBUTE_STRING,
                                  MetricLabelOrUnknown(outcome), 0, 0.0, false};
  attributes[attributeCount++] = {"message_family", TELEMETRY_METRIC_ATTRIBUTE_STRING,
                                  MetricLabelOrUnknown(messageFamily), 0, 0.0, false};
  attributes[attributeCount++] = {"destination", TELEMETRY_METRIC_ATTRIBUTE_STRING,
                                  MetricLabelOrUnknown(destination), 0, 0.0, false};
  attributes[attributeCount++] = {"exchange_kind", TELEMETRY_METRIC_ATTRIBUTE_STRING,
                                  MetricLabelOrUnknown(exchangeKind), 0, 0.0, false};
  attributeCount = AppendContextMetricAttributes(attributes, attributeCount, context, false,
                                                 nullptr, &nuggetTypeName);
  Telemetry_CounterAddUInt64(metrics.outboundMessages, 1, attributes, attributeCount);
  free(nuggetTypeName);
}

extern "C" void
Telemetry_RecordOutboundPublishRetry(uint32_t messageType,
                                     const char *messageFamily,
                                     const char *destination,
                                     const char *exchangeKind,
                                     const struct RazorbackContext *context)
{
  RazorbackStandardMetrics &metrics = GetRazorbackStandardMetrics();
  TelemetryMetricAttribute_t attributes[8];
  char typeBuffer[32];
  char *nuggetTypeName = nullptr;
  size_t attributeCount = 0;

  std::snprintf(typeBuffer, sizeof(typeBuffer), "%u", messageType);
  attributes[attributeCount++] = {"message_type", TELEMETRY_METRIC_ATTRIBUTE_STRING,
                                  typeBuffer, 0, 0.0, false};
  attributes[attributeCount++] = {"message_family", TELEMETRY_METRIC_ATTRIBUTE_STRING,
                                  MetricLabelOrUnknown(messageFamily), 0, 0.0, false};
  attributes[attributeCount++] = {"destination", TELEMETRY_METRIC_ATTRIBUTE_STRING,
                                  MetricLabelOrUnknown(destination), 0, 0.0, false};
  attributes[attributeCount++] = {"exchange_kind", TELEMETRY_METRIC_ATTRIBUTE_STRING,
                                  MetricLabelOrUnknown(exchangeKind), 0, 0.0, false};
  attributeCount = AppendContextMetricAttributes(attributes, attributeCount, context, false,
                                                 nullptr, &nuggetTypeName);
  Telemetry_CounterAddUInt64(metrics.outboundPublishRetry, 1, attributes, attributeCount);
  free(nuggetTypeName);
}

extern "C" void
Telemetry_RecordOutboundReconnect(const char *cause,
                                  const struct RazorbackContext *context)
{
  RazorbackStandardMetrics &metrics = GetRazorbackStandardMetrics();
  TelemetryMetricAttribute_t attributes[6];
  char *nuggetTypeName = nullptr;
  size_t attributeCount = 0;

  attributes[attributeCount++] = {"cause", TELEMETRY_METRIC_ATTRIBUTE_STRING,
                                  MetricLabelOrUnknown(cause), 0, 0.0, false};
  attributeCount = AppendContextMetricAttributes(attributes, attributeCount, context, false,
                                                 nullptr, &nuggetTypeName);
  Telemetry_CounterAddUInt64(metrics.outboundReconnect, 1, attributes, attributeCount);
  free(nuggetTypeName);
}

extern "C" void
Telemetry_AddInspectionInFlight(int64_t delta,
                                bool needsFile,
                                const struct RazorbackContext *context)
{
  RazorbackStandardMetrics &metrics = GetRazorbackStandardMetrics();
  TelemetryMetricAttribute_t attributes[6];
  char *nuggetTypeName = nullptr;
  size_t attributeCount = 0;

  attributes[attributeCount++] = {"needs_file", TELEMETRY_METRIC_ATTRIBUTE_STRING,
                                  MetricBoolLabel(needsFile), 0, 0.0, false};
  attributeCount = AppendContextMetricAttributes(attributes, attributeCount, context, false,
                                                 nullptr, &nuggetTypeName);
  Telemetry_UpDownCounterAddInt64(metrics.inspectionInFlight, delta, attributes,
                                  attributeCount);
  free(nuggetTypeName);
}

extern "C" void
Telemetry_RecordInspectionDuration(double durationSeconds,
                                   const char *reason,
                                   bool needsFile,
                                   bool hasAlerts,
                                   const struct RazorbackContext *context)
{
  RazorbackStandardMetrics &metrics = GetRazorbackStandardMetrics();
  TelemetryMetricAttribute_t attributes[7];
  char *nuggetTypeName = nullptr;
  size_t attributeCount = 0;

  attributes[attributeCount++] = {"reason", TELEMETRY_METRIC_ATTRIBUTE_STRING,
                                  MetricLabelOrUnknown(reason), 0, 0.0, false};
  attributes[attributeCount++] = {"needs_file", TELEMETRY_METRIC_ATTRIBUTE_STRING,
                                  MetricBoolLabel(needsFile), 0, 0.0, false};
  attributes[attributeCount++] = {"has_alerts", TELEMETRY_METRIC_ATTRIBUTE_STRING,
                                  MetricBoolLabel(hasAlerts), 0, 0.0, false};
  attributeCount = AppendContextMetricAttributes(attributes, attributeCount, context, false,
                                                 nullptr, &nuggetTypeName);

  Telemetry_HistogramRecordDouble(metrics.inspectionDuration,
                                  (durationSeconds >= 0.0) ? durationSeconds : 0.0,
                                  attributes,
                                  attributeCount);
  free(nuggetTypeName);
}

extern "C" void
Telemetry_RecordInspectionResult(const char *reason,
                                 bool hasAlerts,
                                 const struct RazorbackContext *context)
{
  RazorbackStandardMetrics &metrics = GetRazorbackStandardMetrics();
  TelemetryMetricAttribute_t attributes[6];
  char *nuggetTypeName = nullptr;
  size_t attributeCount = 0;

  attributes[attributeCount++] = {"reason", TELEMETRY_METRIC_ATTRIBUTE_STRING,
                                  MetricLabelOrUnknown(reason), 0, 0.0, false};
  attributes[attributeCount++] = {"has_alerts", TELEMETRY_METRIC_ATTRIBUTE_STRING,
                                  MetricBoolLabel(hasAlerts), 0, 0.0, false};
  attributeCount = AppendContextMetricAttributes(attributes, attributeCount, context, false,
                                                 nullptr, &nuggetTypeName);
  Telemetry_CounterAddUInt64(metrics.inspectionResults, 1, attributes, attributeCount);
  free(nuggetTypeName);
}

extern "C" void
Telemetry_RecordInspectionError(const char *stage,
                                const char *errorClass,
                                const struct RazorbackContext *context)
{
  RazorbackStandardMetrics &metrics = GetRazorbackStandardMetrics();
  TelemetryMetricAttribute_t attributes[6];
  char *nuggetTypeName = nullptr;
  size_t attributeCount = 0;

  attributes[attributeCount++] = {"stage", TELEMETRY_METRIC_ATTRIBUTE_STRING,
                                  MetricLabelOrUnknown(stage), 0, 0.0, false};
  attributes[attributeCount++] = {"error_class", TELEMETRY_METRIC_ATTRIBUTE_STRING,
                                  MetricLabelOrUnknown(errorClass), 0, 0.0, false};
  attributeCount = AppendContextMetricAttributes(attributes, attributeCount, context, false,
                                                 nullptr, &nuggetTypeName);
  Telemetry_CounterAddUInt64(metrics.inspectionErrors, 1, attributes, attributeCount);
  free(nuggetTypeName);
}

extern "C" void
Telemetry_RecordShutdownRequeuedInspection(const struct RazorbackContext *context)
{
  RazorbackStandardMetrics &metrics = GetRazorbackStandardMetrics();
  TelemetryMetricAttribute_t attributes[5];
  char *nuggetTypeName = nullptr;
  size_t attributeCount = 0;

  attributeCount = AppendContextMetricAttributes(attributes, attributeCount, context, true,
                                                 "stopping", &nuggetTypeName);
  Telemetry_CounterAddUInt64(metrics.shutdownRequeuedInspections, 1, attributes,
                             attributeCount);
  free(nuggetTypeName);
}

extern "C" void
Telemetry_RecordBlockSubmitDecision(const char *decision,
                                    const char *origin,
                                    const struct RazorbackContext *context)
{
  RazorbackStandardMetrics &metrics = GetRazorbackStandardMetrics();
  TelemetryMetricAttribute_t attributes[6];
  char *nuggetTypeName = nullptr;
  size_t attributeCount = 0;

  attributes[attributeCount++] = {"decision", TELEMETRY_METRIC_ATTRIBUTE_STRING,
                                  MetricLabelOrUnknown(decision), 0, 0.0, false};
  attributes[attributeCount++] = {"origin", TELEMETRY_METRIC_ATTRIBUTE_STRING,
                                  MetricLabelOrUnknown(origin), 0, 0.0, false};
  attributeCount = AppendContextMetricAttributes(attributes, attributeCount, context, false,
                                                 nullptr, &nuggetTypeName);
  Telemetry_CounterAddUInt64(metrics.blockSubmitDecisions, 1, attributes, attributeCount);
  free(nuggetTypeName);
}

extern "C" void
Telemetry_RecordCacheResponse(const char *result,
                              const char *canHaz,
                              const struct RazorbackContext *context)
{
  RazorbackStandardMetrics &metrics = GetRazorbackStandardMetrics();
  TelemetryMetricAttribute_t attributes[6];
  char *nuggetTypeName = nullptr;
  size_t attributeCount = 0;

  attributes[attributeCount++] = {"result", TELEMETRY_METRIC_ATTRIBUTE_STRING,
                                  MetricLabelOrUnknown(result), 0, 0.0, false};
  attributes[attributeCount++] = {"can_haz", TELEMETRY_METRIC_ATTRIBUTE_STRING,
                                  MetricLabelOrUnknown(canHaz), 0, 0.0, false};
  attributeCount = AppendContextMetricAttributes(attributes, attributeCount, context, false,
                                                 nullptr, &nuggetTypeName);
  Telemetry_CounterAddUInt64(metrics.cacheResponses, 1, attributes, attributeCount);
  free(nuggetTypeName);
}

extern "C" void
Telemetry_RecordCacheLookupWait(double durationSeconds,
                                const char *result,
                                const struct RazorbackContext *context)
{
  RazorbackStandardMetrics &metrics = GetRazorbackStandardMetrics();
  TelemetryMetricAttribute_t attributes[5];
  char *nuggetTypeName = nullptr;
  size_t attributeCount = 0;

  attributes[attributeCount++] = {"result", TELEMETRY_METRIC_ATTRIBUTE_STRING,
                                  MetricLabelOrUnknown(result), 0, 0.0, false};
  attributeCount = AppendContextMetricAttributes(attributes, attributeCount, context, false,
                                                 nullptr, &nuggetTypeName);

  Telemetry_HistogramRecordDouble(metrics.cacheLookupWait,
                                  (durationSeconds >= 0.0) ? durationSeconds : 0.0,
                                  attributes,
                                  attributeCount);
  free(nuggetTypeName);
}

extern "C" void
Telemetry_RecordSubmitDuration(double durationSeconds,
                               const char *reason,
                               const char *outcome,
                               const char *origin,
                               bool needsStore,
                               const struct RazorbackContext *context)
{
  RazorbackStandardMetrics &metrics = GetRazorbackStandardMetrics();
  TelemetryMetricAttribute_t attributes[8];
  char *nuggetTypeName = nullptr;
  size_t attributeCount = 0;

  attributes[attributeCount++] = {"reason", TELEMETRY_METRIC_ATTRIBUTE_STRING,
                                  MetricLabelOrUnknown(reason), 0, 0.0, false};
  attributes[attributeCount++] = {"outcome", TELEMETRY_METRIC_ATTRIBUTE_STRING,
                                  MetricLabelOrUnknown(outcome), 0, 0.0, false};
  attributes[attributeCount++] = {"origin", TELEMETRY_METRIC_ATTRIBUTE_STRING,
                                  MetricLabelOrUnknown(origin), 0, 0.0, false};
  attributes[attributeCount++] = {"needs_store", TELEMETRY_METRIC_ATTRIBUTE_STRING,
                                  MetricBoolLabel(needsStore), 0, 0.0, false};
  attributeCount = AppendContextMetricAttributes(attributes, attributeCount, context, false,
                                                 nullptr, &nuggetTypeName);

  Telemetry_HistogramRecordDouble(metrics.submitDuration,
                                  (durationSeconds >= 0.0) ? durationSeconds : 0.0,
                                  attributes,
                                  attributeCount);
  free(nuggetTypeName);
}

extern "C" void
Telemetry_RecordTransferFetchDuration(double durationSeconds,
                                      const char *outcome,
                                      const char *protocol,
                                      const char *dispatcherLocality,
                                      const struct RazorbackContext *context)
{
  RazorbackStandardMetrics &metrics = GetRazorbackStandardMetrics();
  TelemetryMetricAttribute_t attributes[7];
  char *nuggetTypeName = nullptr;
  size_t attributeCount = 0;

  attributes[attributeCount++] = {"outcome", TELEMETRY_METRIC_ATTRIBUTE_STRING,
                                  MetricLabelOrUnknown(outcome), 0, 0.0, false};
  attributes[attributeCount++] = {"protocol", TELEMETRY_METRIC_ATTRIBUTE_STRING,
                                  MetricLabelOrNone(protocol), 0, 0.0, false};
  attributes[attributeCount++] = {"dispatcher_locality",
                                  TELEMETRY_METRIC_ATTRIBUTE_STRING,
                                  MetricLabelOrUnknown(dispatcherLocality), 0, 0.0, false};
  attributeCount = AppendContextMetricAttributes(attributes, attributeCount, context, false,
                                                 nullptr, &nuggetTypeName);

  Telemetry_HistogramRecordDouble(metrics.transferFetchDuration,
                                  (durationSeconds >= 0.0) ? durationSeconds : 0.0,
                                  attributes,
                                  attributeCount);
  free(nuggetTypeName);
}

extern "C" void
Telemetry_RecordTransferStoreDuration(double durationSeconds,
                                      const char *outcome,
                                      const char *protocol,
                                      const char *dispatcherLocality,
                                      const char *streamKind,
                                      const struct RazorbackContext *context)
{
  RazorbackStandardMetrics &metrics = GetRazorbackStandardMetrics();
  TelemetryMetricAttribute_t attributes[8];
  char *nuggetTypeName = nullptr;
  size_t attributeCount = 0;

  attributes[attributeCount++] = {"outcome", TELEMETRY_METRIC_ATTRIBUTE_STRING,
                                  MetricLabelOrUnknown(outcome), 0, 0.0, false};
  attributes[attributeCount++] = {"protocol", TELEMETRY_METRIC_ATTRIBUTE_STRING,
                                  MetricLabelOrNone(protocol), 0, 0.0, false};
  attributes[attributeCount++] = {"dispatcher_locality",
                                  TELEMETRY_METRIC_ATTRIBUTE_STRING,
                                  MetricLabelOrUnknown(dispatcherLocality), 0, 0.0, false};
  attributes[attributeCount++] = {"stream_kind", TELEMETRY_METRIC_ATTRIBUTE_STRING,
                                  MetricLabelOrUnknown(streamKind), 0, 0.0, false};
  attributeCount = AppendContextMetricAttributes(attributes, attributeCount, context, false,
                                                 nullptr, &nuggetTypeName);

  Telemetry_HistogramRecordDouble(metrics.transferStoreDuration,
                                  (durationSeconds >= 0.0) ? durationSeconds : 0.0,
                                  attributes,
                                  attributeCount);
  free(nuggetTypeName);
}

extern "C" void
Telemetry_RecordTransferFailure(const char *operation, const char *protocol,
                                const char *errorClass,
                                const char *dispatcherLocality,
                                const struct RazorbackContext *context)
{
  RazorbackStandardMetrics &metrics = GetRazorbackStandardMetrics();
  TelemetryMetricAttribute_t attributes[8];
  char *nuggetTypeName = nullptr;
  size_t attributeCount = 0;

  attributes[attributeCount++] = {"operation", TELEMETRY_METRIC_ATTRIBUTE_STRING,
                                  MetricLabelOrUnknown(operation), 0, 0.0, false};
  attributes[attributeCount++] = {"protocol", TELEMETRY_METRIC_ATTRIBUTE_STRING,
                                  MetricLabelOrNone(protocol), 0, 0.0, false};
  attributes[attributeCount++] = {"error_class", TELEMETRY_METRIC_ATTRIBUTE_STRING,
                                  MetricLabelOrUnknown(errorClass), 0, 0.0, false};
  attributes[attributeCount++] = {"dispatcher_locality",
                                  TELEMETRY_METRIC_ATTRIBUTE_STRING,
                                  MetricLabelOrUnknown(dispatcherLocality), 0, 0.0, false};
  attributeCount = AppendContextMetricAttributes(attributes, attributeCount, context, false,
                                                 nullptr, &nuggetTypeName);
  Telemetry_CounterAddUInt64(metrics.transferFailures, 1, attributes, attributeCount);
  free(nuggetTypeName);
}

extern "C" TelemetrySpan_t *
Telemetry_StartQueueSendSpan(const struct Queue *queue,
                             const struct Message *message,
                             const char *destination,
                             struct TelemetryInjectedHeaders *headers)
{
  return StartSpanWithLink("send", trace_api::SpanKind::kProducer, queue, message,
                           destination, "send", "send", MessageContextMode::kLink,
                           false, true, headers);
}

extern "C" TelemetrySpan_t *
Telemetry_StartQueueReceiveSpan(const struct Queue *queue,
                                const struct Message *message)
{
  const char *destination = (queue != nullptr) ? queue->sName : nullptr;

  return StartSpanWithLink("receive", trace_api::SpanKind::kClient, queue, message,
                           destination, "receive", "receive", MessageContextMode::kParent,
                           true, false, nullptr);
}

extern "C" bool
Telemetry_InjectCurrentContext(struct TelemetryInjectedHeaders *headers)
{
  TelemetryState &state = GetTelemetryState();

  if (headers == nullptr)
    return false;

  FreeTelemetryHeaders(headers->entries, headers->count);
  headers->entries = nullptr;
  headers->count = 0;

  if (!state.tracing_enabled)
    return true;

  return ReplaceInjectedHeaders(context_api::RuntimeContext::GetCurrent(), headers);
}

extern "C" bool
Telemetry_IsLogEnabled(void)
{
  TelemetryState &state = GetTelemetryState();

  return state.logging_enabled && state.logger != nullptr;
}

extern "C" void
Telemetry_LogMessage(unsigned level, uint64_t component, const char *message)
{
  TelemetryState &state = GetTelemetryState();
  static thread_local bool is_emitting = false;
  context_api::Context current_context;
  trace_api::SpanContext span_context = trace_api::SpanContext::GetInvalid();
  opentelemetry::nostd::unique_ptr<logs_api::LogRecord> log_record;
  std::chrono::system_clock::time_point now;
  const char *component_name = nullptr;

  if (!state.logging_enabled || state.logger == nullptr || message == nullptr || is_emitting)
    return;

  try
  {
    ScopedBoolGuard emitting_guard(is_emitting);

    log_record = state.logger->CreateLogRecord();
    if (!log_record)
      return;

    now = std::chrono::system_clock::now();
    log_record->SetTimestamp(opentelemetry::common::SystemTimestamp(now));
    log_record->SetObservedTimestamp(opentelemetry::common::SystemTimestamp(now));
    log_record->SetSeverity(TranslateLogSeverity(level));
    log_record->SetBody(opentelemetry::nostd::string_view(message));
    log_record->SetAttribute("rzb.log.component.mask", static_cast<int64_t>(component));
    log_record->SetAttribute("rzb.log.syslog.level", static_cast<int64_t>(level));

    component_name = GetLogComponentName(component);
    if (component_name != nullptr)
      log_record->SetAttribute("rzb.log.component.name", component_name);

    current_context = context_api::RuntimeContext::GetCurrent();
    span_context = trace_api::GetSpan(current_context)->GetContext();
    if (span_context.IsValid())
    {
      log_record->SetTraceId(span_context.trace_id());
      log_record->SetSpanId(span_context.span_id());
      log_record->SetTraceFlags(span_context.trace_flags());
    }

    state.logger->EmitLogRecord(std::move(log_record));
  }
  catch (...)
  {
  }
}

extern "C" TelemetrySpan_t *
Telemetry_StartMessageProcessSpan(const struct Queue *queue,
                                  const struct Message *message)
{
  const char *destination = (queue != nullptr) ? queue->sName : nullptr;

  return StartSpanWithLink("process", trace_api::SpanKind::kConsumer,
                           queue, message, destination,
                           "process", "process", MessageContextMode::kParent,
                           false, false, nullptr);
}

extern "C" bool
Telemetry_UpdateContext(TelemetryContextCarrier_t **context)
{
  TelemetryState &state = GetTelemetryState();

  if (context == nullptr)
    return false;

  if (!state.tracing_enabled)
  {
    DestroyContextCarrier(*context);
    *context = nullptr;
    return true;
  }

  try
  {
    return ReplaceContextCarrier(context_api::RuntimeContext::GetCurrent(),
                                 context);
  }
  catch (...)
  {
    rzb_log(LOG_ERR, LOG_C_CORE, "%s: failed to capture tracing context", __func__);
    return false;
  }
}

extern "C" TelemetrySpan_t *
Telemetry_StartSpan(const char *spanName,
                    TelemetryContextCarrier_t **context)
{
  return Telemetry_StartSpanWithKind(spanName, context,
                                     TELEMETRY_SPAN_KIND_INTERNAL);
}

extern "C" TelemetrySpan_t *
Telemetry_StartSpanWithKind(const char *spanName,
                            TelemetryContextCarrier_t **context,
                            TelemetrySpanKind_t kind)
{
  TelemetryState &state = GetTelemetryState();
  TelemetrySpan_t *handle = nullptr;
  trace_api::StartSpanOptions options;
  context_api::Context parent_context;
  context_api::Context scope_context;

  if (!state.tracing_enabled || state.tracer == nullptr)
    return nullptr;

  try
  {
    handle = new TelemetrySpan;
    parent_context = (context != nullptr && *context != nullptr)
                         ? ExtractStoredContext(*context)
                         : context_api::RuntimeContext::GetCurrent();
    scope_context = parent_context;
    options.kind = TranslateSpanKind(kind);
    options.parent = parent_context;
    handle->span =
        state.tracer->StartSpan(spanName != nullptr ? spanName : "process",
                                options);
    scope_context = trace_api::SetSpan(scope_context, handle->span);
    handle->token = context_api::RuntimeContext::Attach(scope_context);
    Telemetry_UpdateContext(context);
    return handle;
  }
  catch (...)
  {
    delete handle;
    rzb_log(LOG_ERR, LOG_C_CORE, "%s: failed to start telemetry span", __func__);
    return nullptr;
  }
}

extern "C" void
Telemetry_ClearContext(TelemetryContextCarrier_t **context)
{
  if (context == nullptr)
    return;

  DestroyContextCarrier(*context);
  *context = nullptr;
}

extern "C" void
Telemetry_AddStringAttribute(TelemetrySpan_t *span,
                             const char *name,
                             const char *value)
{
  if (span == nullptr || span->span == nullptr || name == nullptr || value == nullptr)
    return;

  if (!span->span->IsRecording())
    return;

  span->span->SetAttribute(name, value);
}

extern "C" void
Telemetry_AddIntAttribute(TelemetrySpan_t *span,
                          const char *name,
                          int64_t value)
{
  if (span == nullptr || span->span == nullptr || name == nullptr)
    return;

  if (!span->span->IsRecording())
    return;

  span->span->SetAttribute(name, value);
}

extern "C" void
Telemetry_AddBoolAttribute(TelemetrySpan_t *span,
                           const char *name,
                           bool value)
{
  if (span == nullptr || span->span == nullptr || name == nullptr)
    return;

  if (!span->span->IsRecording())
    return;

  span->span->SetAttribute(name, value);
}

extern "C" void
Telemetry_EndSpan(TelemetrySpan_t *span, bool success, const char *description)
{
  if (span == nullptr)
    return;

  if (span->span != nullptr)
  {
    if (success)
      span->span->SetStatus(trace_api::StatusCode::kOk);
    else
      span->span->SetStatus(trace_api::StatusCode::kError,
                            description != nullptr ? description : "");

    span->token.reset();
    span->span->End();
  }

  delete span;
}

extern "C" TelemetryMetric_t *
Telemetry_CreateUInt64Counter(const char *name,
                              const char *description,
                              const char *unit)
{
  TelemetryState &state = GetTelemetryState();
  TelemetryMetric_t *metric = nullptr;

  if (!state.metrics_enabled || state.meter == nullptr || name == nullptr || name[0] == '\0')
    return nullptr;

  try
  {
    metric = new TelemetryMetric;
    metric->kind = TelemetryMetricKind::kUInt64Counter;
    metric->uint64Counter =
        state.meter->CreateUInt64Counter(SafeString(name), SafeString(description),
                                         SafeString(unit));
    if (metric->uint64Counter == nullptr)
    {
      delete metric;
      return nullptr;
    }
    return metric;
  }
  catch (...)
  {
    delete metric;
    rzb_log(LOG_ERR, LOG_C_CORE, "%s: failed to create telemetry metric", __func__);
    return nullptr;
  }
}

extern "C" TelemetryMetric_t *
Telemetry_CreateDoubleCounter(const char *name,
                              const char *description,
                              const char *unit)
{
  TelemetryState &state = GetTelemetryState();
  TelemetryMetric_t *metric = nullptr;

  if (!state.metrics_enabled || state.meter == nullptr || name == nullptr || name[0] == '\0')
    return nullptr;

  try
  {
    metric = new TelemetryMetric;
    metric->kind = TelemetryMetricKind::kDoubleCounter;
    metric->doubleCounter =
        state.meter->CreateDoubleCounter(SafeString(name), SafeString(description),
                                         SafeString(unit));
    if (metric->doubleCounter == nullptr)
    {
      delete metric;
      return nullptr;
    }
    return metric;
  }
  catch (...)
  {
    delete metric;
    rzb_log(LOG_ERR, LOG_C_CORE, "%s: failed to create telemetry metric", __func__);
    return nullptr;
  }
}

extern "C" TelemetryMetric_t *
Telemetry_CreateInt64UpDownCounter(const char *name,
                                   const char *description,
                                   const char *unit)
{
  TelemetryState &state = GetTelemetryState();
  TelemetryMetric_t *metric = nullptr;

  if (!state.metrics_enabled || state.meter == nullptr || name == nullptr || name[0] == '\0')
    return nullptr;

  try
  {
    metric = new TelemetryMetric;
    metric->kind = TelemetryMetricKind::kInt64UpDownCounter;
    metric->int64UpDownCounter =
        state.meter->CreateInt64UpDownCounter(SafeString(name),
                                              SafeString(description),
                                              SafeString(unit));
    if (metric->int64UpDownCounter == nullptr)
    {
      delete metric;
      return nullptr;
    }
    return metric;
  }
  catch (...)
  {
    delete metric;
    rzb_log(LOG_ERR, LOG_C_CORE, "%s: failed to create telemetry metric", __func__);
    return nullptr;
  }
}

extern "C" TelemetryMetric_t *
Telemetry_CreateDoubleUpDownCounter(const char *name,
                                    const char *description,
                                    const char *unit)
{
  TelemetryState &state = GetTelemetryState();
  TelemetryMetric_t *metric = nullptr;

  if (!state.metrics_enabled || state.meter == nullptr || name == nullptr || name[0] == '\0')
    return nullptr;

  try
  {
    metric = new TelemetryMetric;
    metric->kind = TelemetryMetricKind::kDoubleUpDownCounter;
    metric->doubleUpDownCounter =
        state.meter->CreateDoubleUpDownCounter(SafeString(name),
                                               SafeString(description),
                                               SafeString(unit));
    if (metric->doubleUpDownCounter == nullptr)
    {
      delete metric;
      return nullptr;
    }
    return metric;
  }
  catch (...)
  {
    delete metric;
    rzb_log(LOG_ERR, LOG_C_CORE, "%s: failed to create telemetry metric", __func__);
    return nullptr;
  }
}

extern "C" TelemetryMetric_t *
Telemetry_CreateUInt64Histogram(const char *name,
                                const char *description,
                                const char *unit)
{
  TelemetryState &state = GetTelemetryState();
  TelemetryMetric_t *metric = nullptr;

  if (!state.metrics_enabled || state.meter == nullptr || name == nullptr || name[0] == '\0')
    return nullptr;

  try
  {
    metric = new TelemetryMetric;
    metric->kind = TelemetryMetricKind::kUInt64Histogram;
    metric->uint64Histogram =
        state.meter->CreateUInt64Histogram(SafeString(name), SafeString(description),
                                           SafeString(unit));
    if (metric->uint64Histogram == nullptr)
    {
      delete metric;
      return nullptr;
    }
    return metric;
  }
  catch (...)
  {
    delete metric;
    rzb_log(LOG_ERR, LOG_C_CORE, "%s: failed to create telemetry metric", __func__);
    return nullptr;
  }
}

extern "C" TelemetryMetric_t *
Telemetry_CreateDoubleHistogram(const char *name,
                                const char *description,
                                const char *unit)
{
  TelemetryState &state = GetTelemetryState();
  TelemetryMetric_t *metric = nullptr;

  if (!state.metrics_enabled || state.meter == nullptr || name == nullptr || name[0] == '\0')
    return nullptr;

  try
  {
    metric = new TelemetryMetric;
    metric->kind = TelemetryMetricKind::kDoubleHistogram;
    metric->doubleHistogram =
        state.meter->CreateDoubleHistogram(SafeString(name), SafeString(description),
                                           SafeString(unit));
    if (metric->doubleHistogram == nullptr)
    {
      delete metric;
      return nullptr;
    }
    return metric;
  }
  catch (...)
  {
    delete metric;
    rzb_log(LOG_ERR, LOG_C_CORE, "%s: failed to create telemetry metric", __func__);
    return nullptr;
  }
}

extern "C" TelemetryMetric_t *
Telemetry_CreateInt64ObservableGauge(const char *name,
                                     const char *description,
                                     const char *unit,
                                     TelemetryObservableCallback_t callback,
                                     void *userData)
{
  TelemetryState &state = GetTelemetryState();
  TelemetryMetric_t *metric = nullptr;

  if (!state.metrics_enabled || state.meter == nullptr || name == nullptr ||
      name[0] == '\0' || callback == nullptr)
    return nullptr;

  try
  {
    metric = new TelemetryMetric;
    metric->kind = TelemetryMetricKind::kInt64ObservableGauge;
    metric->observableCallback = callback;
    metric->observableUserData = userData;
    metric->observableGauge = state.meter->CreateInt64ObservableGauge(
        SafeString(name), SafeString(description), SafeString(unit));
    if (metric->observableGauge == nullptr)
    {
      delete metric;
      return nullptr;
    }

    metric->observableGauge->AddCallback(TelemetryObservableGaugeCallbackBridge,
                                         metric);
    return metric;
  }
  catch (...)
  {
    delete metric;
    rzb_log(LOG_ERR, LOG_C_CORE, "%s: failed to create telemetry metric", __func__);
    return nullptr;
  }
}

extern "C" TelemetryMetric_t *
Telemetry_CreateDoubleObservableGauge(const char *name,
                                      const char *description,
                                      const char *unit,
                                      TelemetryObservableCallback_t callback,
                                      void *userData)
{
  TelemetryState &state = GetTelemetryState();
  TelemetryMetric_t *metric = nullptr;

  if (!state.metrics_enabled || state.meter == nullptr || name == nullptr ||
      name[0] == '\0' || callback == nullptr)
    return nullptr;

  try
  {
    metric = new TelemetryMetric;
    metric->kind = TelemetryMetricKind::kDoubleObservableGauge;
    metric->observableCallback = callback;
    metric->observableUserData = userData;
    metric->observableGauge = state.meter->CreateDoubleObservableGauge(
        SafeString(name), SafeString(description), SafeString(unit));
    if (metric->observableGauge == nullptr)
    {
      delete metric;
      return nullptr;
    }

    metric->observableGauge->AddCallback(TelemetryObservableGaugeCallbackBridge,
                                         metric);
    return metric;
  }
  catch (...)
  {
    delete metric;
    rzb_log(LOG_ERR, LOG_C_CORE, "%s: failed to create telemetry metric", __func__);
    return nullptr;
  }
}

extern "C" void
Telemetry_DestroyMetric(TelemetryMetric_t *metric)
{
  if (metric != nullptr && metric->observableGauge != nullptr &&
      metric->observableCallback != nullptr)
  {
    metric->observableGauge->RemoveCallback(TelemetryObservableGaugeCallbackBridge,
                                            metric);
  }

  delete metric;
}

extern "C" void
Telemetry_CounterAddUInt64(TelemetryMetric_t *metric,
                           uint64_t value,
                           const TelemetryMetricAttribute_t *attributes,
                           size_t attributeCount)
{
  if (metric == nullptr || metric->kind != TelemetryMetricKind::kUInt64Counter)
    return;

  AddMetricValue(metric->uint64Counter.get(), value, attributes, attributeCount);
}

extern "C" void
Telemetry_CounterAddDouble(TelemetryMetric_t *metric,
                           double value,
                           const TelemetryMetricAttribute_t *attributes,
                           size_t attributeCount)
{
  if (metric == nullptr || metric->kind != TelemetryMetricKind::kDoubleCounter)
    return;

  AddMetricValue(metric->doubleCounter.get(), value, attributes, attributeCount);
}

extern "C" void
Telemetry_UpDownCounterAddInt64(TelemetryMetric_t *metric,
                                int64_t value,
                                const TelemetryMetricAttribute_t *attributes,
                                size_t attributeCount)
{
  if (metric == nullptr || metric->kind != TelemetryMetricKind::kInt64UpDownCounter)
    return;

  AddMetricValue(metric->int64UpDownCounter.get(), value, attributes, attributeCount);
}

extern "C" void
Telemetry_UpDownCounterAddDouble(TelemetryMetric_t *metric,
                                 double value,
                                 const TelemetryMetricAttribute_t *attributes,
                                 size_t attributeCount)
{
  if (metric == nullptr || metric->kind != TelemetryMetricKind::kDoubleUpDownCounter)
    return;

  AddMetricValue(metric->doubleUpDownCounter.get(), value, attributes, attributeCount);
}

extern "C" void
Telemetry_HistogramRecordUInt64(TelemetryMetric_t *metric,
                                uint64_t value,
                                const TelemetryMetricAttribute_t *attributes,
                                size_t attributeCount)
{
  if (metric == nullptr || metric->kind != TelemetryMetricKind::kUInt64Histogram)
    return;

  RecordMetricValue(metric->uint64Histogram.get(), value, attributes, attributeCount);
}

extern "C" void
Telemetry_HistogramRecordDouble(TelemetryMetric_t *metric,
                                double value,
                                const TelemetryMetricAttribute_t *attributes,
                                size_t attributeCount)
{
  if (metric == nullptr || metric->kind != TelemetryMetricKind::kDoubleHistogram)
    return;

  RecordMetricValue(metric->doubleHistogram.get(), value, attributes, attributeCount);
}

extern "C" void
Telemetry_ObservableObserveInt64(TelemetryObservation_t *observation,
                                 int64_t value,
                                 const TelemetryMetricAttribute_t *attributes,
                                 size_t attributeCount)
{
  MetricAttributesIterable metric_attributes(attributes, attributeCount);

  if (observation == nullptr || observation->int64Observer == nullptr)
    return;

  observation->int64Observer->Observe(value, metric_attributes);
}

extern "C" void
Telemetry_ObservableObserveDouble(TelemetryObservation_t *observation,
                                  double value,
                                  const TelemetryMetricAttribute_t *attributes,
                                  size_t attributeCount)
{
  MetricAttributesIterable metric_attributes(attributes, attributeCount);

  if (observation == nullptr || observation->doubleObserver == nullptr)
    return;

  observation->doubleObserver->Observe(value, metric_attributes);
}

extern "C" void
Telemetry_FreeInjectedHeaders(struct TelemetryInjectedHeaders *headers)
{
  if (headers == nullptr || headers->entries == nullptr)
    return;

  FreeTelemetryHeaders(headers->entries, headers->count);
  headers->entries = nullptr;
  headers->count = 0;
}
