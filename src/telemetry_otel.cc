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

#include <razorback/messages.h>
#include <razorback/list.h>
#include <razorback/log.h>
#include <razorback/message_formats.h>
#include <razorback/queue.h>
#include <razorback/hash.h>
#include <razorback/uuids.h>

#include <opentelemetry/baggage/propagation/baggage_propagator.h>
#include <opentelemetry/context/propagation/global_propagator.h>
#include <opentelemetry/context/propagation/composite_propagator.h>
#include <opentelemetry/context/propagation/text_map_propagator.h>
#include <opentelemetry/context/runtime_context.h>
#include <opentelemetry/exporters/otlp/otlp_http_exporter_factory.h>
#include <opentelemetry/exporters/otlp/otlp_http_exporter_options.h>
#include <opentelemetry/sdk/resource/resource.h>
#include <opentelemetry/sdk/trace/batch_span_processor_factory.h>
#include <opentelemetry/sdk/trace/batch_span_processor_options.h>
#include <opentelemetry/sdk/trace/tracer_provider.h>
#include <opentelemetry/trace/context.h>
#include <opentelemetry/trace/noop.h>
#include <opentelemetry/trace/propagation/http_trace_context.h>
#include <opentelemetry/trace/provider.h>
#include <opentelemetry/trace/scope.h>
#include <opentelemetry/trace/span.h>
#include <opentelemetry/trace/tracer.h>

#include <cstdlib>
#include <array>
#include <cstring>
#include <memory>
#include <string>
#include <strings.h>
#include <utility>
#include <vector>

namespace context_api      = opentelemetry::context;
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

namespace
{
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
  bool initialized = false;
  bool enabled     = false;
  std::shared_ptr<trace_sdk::TracerProvider> sdk_provider;
  opentelemetry::nostd::shared_ptr<trace_api::TracerProvider> provider;
  opentelemetry::nostd::shared_ptr<trace_api::Tracer> tracer;
};

TelemetryState &
GetTelemetryState() noexcept
{
  static TelemetryState state;

  return state;
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

void
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
  context_api::Context scope_context = context_api::RuntimeContext::GetCurrent();

  if (!state.enabled || state.tracer == nullptr)
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
    if (message_context_mode == MessageContextMode::kParent)
      options.parent = extracted_context;

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
      scope_context = extracted_context;
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
}  // namespace

extern "C" bool
Telemetry_Initialize(void)
{
  TelemetryState &state = GetTelemetryState();

  try
  {
    std::unique_ptr<trace_sdk::SpanExporter> exporter;
    std::unique_ptr<trace_sdk::SpanProcessor> processor;
    std::vector<std::unique_ptr<propagation_api::TextMapPropagator>> propagators;
    resource_sdk::Resource resource;
    resource_sdk::ResourceAttributes resource_attributes;
    otlp_exporter::OtlpHttpExporterOptions exporter_options;
    trace_sdk::BatchSpanProcessorOptions batch_options;

    if (state.initialized)
      return true;

    state.enabled = false;

    if (IsEnvTrue("OTEL_SDK_DISABLED"))
    {
      state.initialized = true;
      return true;
    }

    exporter = otlp_exporter::OtlpHttpExporterFactory::Create(exporter_options);
    processor = trace_sdk::BatchSpanProcessorFactory::Create(std::move(exporter), batch_options);

    resource_attributes["service.name"] = std::string(GetServiceName());
    resource_attributes["service.version"] = std::string(PACKAGE_VERSION);
    resource = resource_sdk::Resource::Create(resource_attributes);

    state.sdk_provider = std::make_shared<trace_sdk::TracerProvider>(std::move(processor), resource);
    state.provider = opentelemetry::nostd::shared_ptr<trace_api::TracerProvider>(
        std::shared_ptr<trace_api::TracerProvider>(state.sdk_provider));
    trace_api::Provider::SetTracerProvider(state.provider);
    state.tracer =
        state.provider->GetTracer("razorback.api.messaging", PACKAGE_VERSION);

    propagators.emplace_back(new trace_api::propagation::HttpTraceContext);
    propagators.emplace_back(new opentelemetry::baggage::propagation::BaggagePropagator);
    propagation_api::GlobalTextMapPropagator::SetGlobalPropagator(
        opentelemetry::nostd::shared_ptr<propagation_api::TextMapPropagator>(
            new propagation_api::CompositePropagator(std::move(propagators))));

    state.enabled = true;
    state.initialized = true;
    return true;
  }
  catch (...)
  {
    rzb_log(LOG_ERR, LOG_C_CORE, "%s: failed to initialize OpenTelemetry tracing", __func__);
    state.tracer = opentelemetry::nostd::shared_ptr<trace_api::Tracer>();
    state.provider = opentelemetry::nostd::shared_ptr<trace_api::TracerProvider>();
    state.sdk_provider.reset();
    state.enabled = false;
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

  if (state.sdk_provider)
  {
    state.sdk_provider->ForceFlush();
    state.sdk_provider->Shutdown();
  }

  trace_api::Provider::SetTracerProvider(
      opentelemetry::nostd::shared_ptr<trace_api::TracerProvider>(
          std::make_shared<trace_api::NoopTracerProvider>()));
  state.tracer = opentelemetry::nostd::shared_ptr<trace_api::Tracer>();
  state.provider = opentelemetry::nostd::shared_ptr<trace_api::TracerProvider>();
  state.sdk_provider.reset();
  state.enabled = false;
  state.initialized = false;
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

  if (!state.enabled)
    return true;

  return ReplaceInjectedHeaders(context_api::RuntimeContext::GetCurrent(), headers);
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

  if (!state.enabled)
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

  if (!state.enabled || state.tracer == nullptr)
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

extern "C" void
Telemetry_FreeInjectedHeaders(struct TelemetryInjectedHeaders *headers)
{
  if (headers == nullptr || headers->entries == nullptr)
    return;

  FreeTelemetryHeaders(headers->entries, headers->count);
  headers->entries = nullptr;
  headers->count = 0;
}
