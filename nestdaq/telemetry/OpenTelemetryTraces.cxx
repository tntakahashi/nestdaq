/** @file
 *  @brief Builds trace exporters/processors and manages plugin-owned span handles.
 */

#include "nestdaq/telemetry/OpenTelemetryRuntime.h"

#include <memory>
#include <utility>

#include <opentelemetry/exporters/ostream/span_exporter_factory.h>
#include <opentelemetry/exporters/otlp/otlp_grpc_exporter_factory.h>
#include <opentelemetry/exporters/otlp/otlp_http_exporter_factory.h>
#include <opentelemetry/sdk/trace/batch_span_processor_factory.h>
#include <opentelemetry/sdk/trace/batch_span_processor_options.h>
#include <opentelemetry/sdk/trace/simple_processor_factory.h>

namespace nestdaq::otel_detail {
namespace {

auto TraceEndpointGrpc(const nestdaq_otel_config &config) -> const char *
{
    return IsEmpty(config.traces.endpoint_grpc) ? kDefaultGrpcEndpoint.data() : config.traces.endpoint_grpc;
}

auto TraceEndpointHttp(const nestdaq_otel_config &config) -> const char *
{
    return IsEmpty(config.traces.endpoint_http) ? kDefaultTraceHttpEndpoint.data() : config.traces.endpoint_http;
}

} // namespace

auto CreateSpanExporter(const nestdaq_otel_config &config, Protocol protocol)
-> std::unique_ptr<opentelemetry::sdk::trace::SpanExporter>
{
    switch (protocol) {
    case Protocol::Console:
        return opentelemetry::exporter::trace::OStreamSpanExporterFactory::Create();
    case Protocol::OtlpHttp: {
        auto options = opentelemetry::exporter::otlp::OtlpHttpExporterOptions{};
        options.url = TraceEndpointHttp(config);
        options.http_headers = ParseHeaders(config.traces.headers);
        options.content_type = config.traces.otlp_http_json == 0
                               ? opentelemetry::exporter::otlp::HttpRequestContentType::kBinary
                               : opentelemetry::exporter::otlp::HttpRequestContentType::kJson;
        options.timeout = TimeoutFromMs(config.timeout_ms);
        return opentelemetry::exporter::otlp::OtlpHttpExporterFactory::Create(options);
    }
    case Protocol::OtlpGrpc: {
        auto options = opentelemetry::exporter::otlp::OtlpGrpcExporterOptions{};
        options.endpoint = TraceEndpointGrpc(config);
        options.metadata = ParseHeaders(config.traces.headers);
        options.timeout = TimeoutFromMs(config.timeout_ms);
        return opentelemetry::exporter::otlp::OtlpGrpcExporterFactory::Create(options);
    }
    }
    return nullptr;
}

auto CreateSpanProcessor(std::unique_ptr<opentelemetry::sdk::trace::SpanExporter> exporter,
                         Protocol protocol) -> std::unique_ptr<opentelemetry::sdk::trace::SpanProcessor>
{
    if (protocol == Protocol::Console) {
        return opentelemetry::sdk::trace::SimpleSpanProcessorFactory::Create(std::move(exporter));
    }
    auto options = opentelemetry::sdk::trace::BatchSpanProcessorOptions{};
    return opentelemetry::sdk::trace::BatchSpanProcessorFactory::Create(std::move(exporter), options);
}

} // namespace nestdaq::otel_detail

namespace nestdaq {

auto OpenTelemetryInitializer::SpanEnd(uint64_t span_handle) -> int
{
    opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> span;
    {
        auto &state = otel_detail::State();
        std::scoped_lock lock{state.mutex};
        auto it = state.spans.find(span_handle);
        if (it == state.spans.end()) {
            return otel_detail::SetLastError("OpenTelemetry span handle is not active");
        }
        span = it->second;
        state.spans.erase(it);
        state.lastError.clear();
    }
    span->End();
    return NESTDAQ_OTEL_OK;
}

auto OpenTelemetryInitializer::SpanSetAttribute(uint64_t span_handle, const nestdaq_otel_attribute *attribute) -> int
{
    if (!otel_detail::ValidateAttribute(attribute)) {
        return otel_detail::SetLastError("OpenTelemetry span attribute is invalid");
    }
    opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> span;
    {
        auto &state = otel_detail::State();
        std::scoped_lock lock{state.mutex};
        auto it = state.spans.find(span_handle);
        if (it == state.spans.end()) {
            return otel_detail::SetLastError("OpenTelemetry span handle is not active");
        }
        span = it->second;
        state.lastError.clear();
    }
    auto storage = otel_detail::AttributeStorage{};
    storage.keys.reserve(1);
    storage.values.reserve(1);
    otel_detail::AppendAttribute(storage, *attribute);
    if (!storage.values.empty()) {
        span->SetAttribute(storage.values.front().first, storage.values.front().second);
    }
    return NESTDAQ_OTEL_OK;
}

auto OpenTelemetryInitializer::SpanStart(const char *name,
        const nestdaq_otel_attribute *attributes,
        uint64_t attribute_count) -> uint64_t
{
    if (otel_detail::IsEmpty(name)) {
        otel_detail::SetLastError("OpenTelemetry span name is empty");
        return 0;
    }
    auto attrs = otel_detail::BuildAttributes(attributes, attribute_count);
    auto &state = otel_detail::State();
    std::scoped_lock lock{state.mutex};
    if (!state.tracer) {
        state.lastError.clear();
        return 0;
    }
    auto span = state.tracer->StartSpan(name, attrs.values);
    const auto handle = state.nextSpanHandle.fetch_add(1, std::memory_order_relaxed);
    state.spans.emplace(handle, std::move(span));
    state.lastError.clear();
    return handle;
}

} // namespace nestdaq
