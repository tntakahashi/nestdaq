/** @file
 *  @brief Initializes dynamic OpenTelemetry providers and exposes the C ABI.
 */

#include "nestdaq/telemetry/OpenTelemetryRuntime.h"

#include <exception>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <opentelemetry/logs/provider.h>
#include <opentelemetry/metrics/provider.h>
#include <opentelemetry/sdk/logs/logger_provider_factory.h>
#include <opentelemetry/sdk/metrics/meter_provider_factory.h>
#include <opentelemetry/sdk/metrics/view/view_registry_factory.h>
#include <opentelemetry/sdk/trace/tracer_provider_factory.h>
#include <opentelemetry/trace/provider.h>

#include <fairlogger/Logger.h>

#include "nestdaq/telemetry/FairLoggerOpenTelemetrySink.h"

#if __has_include("nestdaq/version.h")
#  include "nestdaq/version.h"
#else
static constexpr std::string_view NESTDAQ_VERSION {"unknown"};
#endif

namespace nestdaq {

using namespace otel_detail;

auto OpenTelemetryInitializer::ForceFlush(uint64_t timeout_ms) -> int
{
    try {
        std::shared_ptr<opentelemetry::sdk::logs::LoggerProvider> loggerProvider;
        std::shared_ptr<opentelemetry::sdk::metrics::MeterProvider> meterProvider;
        std::shared_ptr<opentelemetry::sdk::trace::TracerProvider> tracerProvider;
        {
            auto &state = State();
            std::scoped_lock lock{state.mutex};
            loggerProvider = state.loggerProvider;
            meterProvider = state.meterProvider;
            tracerProvider = state.tracerProvider;
        }
        auto ok = true;
        if (loggerProvider) {
            ok = loggerProvider->ForceFlush(TimeoutFromMs(timeout_ms)) && ok;
        }
        if (meterProvider) {
            ok = meterProvider->ForceFlush(TimeoutFromMs(timeout_ms)) && ok;
        }
        if (tracerProvider) {
            ok = tracerProvider->ForceFlush(TimeoutFromMs(timeout_ms)) && ok;
        }
        if (!ok) {
            return SetLastError("OpenTelemetry force flush failed");
        }
        ClearLastError();
        return NESTDAQ_OTEL_OK;
    } catch (const std::exception &ex) {
        return SetLastError(ex.what());
    } catch (...) {
        return SetLastError("unknown OpenTelemetry force flush error");
    }
}

auto OpenTelemetryInitializer::FlushFrameworkMetricsIfDirty(uint64_t timeout_ms) -> int
{
    return otel_detail::FlushFrameworkMetricsIfDirty(timeout_ms);
}

auto OpenTelemetryInitializer::Initialize(const nestdaq_otel_config *config) -> int
{
    auto localConfig = DefaultConfig();
    if (config != nullptr) {
        if (config->size != sizeof(nestdaq_otel_config)) {
            return SetLastError("nestdaq_otel_config has an unsupported size");
        }
        localConfig = *config;
    }
    if (!ValidateSeverity(localConfig.min_severity)) {
        return SetLastError("min_severity must be a valid fair::Severity numeric value");
    }

    auto logProtocols = std::vector<Protocol>{};
    auto metricProtocols = std::vector<Protocol>{};
    auto traceProtocols = std::vector<Protocol>{};
    if ((SignalEnabled(localConfig.logs) && !ParseProtocols(localConfig.logs.protocol, logProtocols)) ||
        (SignalEnabled(localConfig.metrics) && !ParseProtocols(localConfig.metrics.protocol, metricProtocols)) ||
        (SignalEnabled(localConfig.traces) && !ParseProtocols(localConfig.traces.protocol, traceProtocols))) {
        return SetLastError("unsupported OpenTelemetry protocol; expected comma-separated console, otlp-http, or otlp-grpc");
    }

    try {
        auto resource = MakeResource(localConfig);
        std::shared_ptr<opentelemetry::sdk::logs::LoggerProvider> loggerProvider;
        std::shared_ptr<opentelemetry::sdk::metrics::MeterProvider> meterProvider;
        std::shared_ptr<opentelemetry::sdk::trace::TracerProvider> tracerProvider;

        if (!logProtocols.empty()) {
            auto processors = std::vector<std::unique_ptr<opentelemetry::sdk::logs::LogRecordProcessor>>{};
            for (const auto protocol : logProtocols) {
                processors.emplace_back(CreateLogProcessor(CreateLogExporter(localConfig, protocol), protocol));
            }
            loggerProvider = std::shared_ptr<opentelemetry::sdk::logs::LoggerProvider>{
                opentelemetry::sdk::logs::LoggerProviderFactory::Create(std::move(processors), resource)};
        }

        if (!metricProtocols.empty()) {
            auto views = opentelemetry::sdk::metrics::ViewRegistryFactory::Create();
            meterProvider = std::shared_ptr<opentelemetry::sdk::metrics::MeterProvider>{
                opentelemetry::sdk::metrics::MeterProviderFactory::Create(std::move(views), resource)};
            for (const auto protocol : metricProtocols) {
                meterProvider->AddMetricReader(CreateMetricReader(CreateMetricExporter(localConfig, protocol), localConfig));
            }
        }

        if (!traceProtocols.empty()) {
            auto processors = std::vector<std::unique_ptr<opentelemetry::sdk::trace::SpanProcessor>>{};
            for (const auto protocol : traceProtocols) {
                processors.emplace_back(CreateSpanProcessor(CreateSpanExporter(localConfig, protocol), protocol));
            }
            tracerProvider = std::shared_ptr<opentelemetry::sdk::trace::TracerProvider>{
                opentelemetry::sdk::trace::TracerProviderFactory::Create(std::move(processors), resource)};
        }

        Shutdown(localConfig.timeout_ms);
        {
            auto &state = State();
            std::scoped_lock lock{state.mutex};
            state.loggerProvider = loggerProvider;
            state.meterProvider = meterProvider;
            state.tracerProvider = tracerProvider;
            state.meter = {};
            state.frameworkMeter = {};
            state.tracer = {};
            if (meterProvider) {
                state.meter = meterProvider->GetMeter("nestdaq", std::string{NESTDAQ_VERSION});
            }
            if (!metricProtocols.empty()) {
                StoreFrameworkMetricConfig(state, localConfig, metricProtocols, resource);
                ConfigureFrameworkMetricsProvider(state);
            }
            if (tracerProvider) {
                state.tracer = tracerProvider->GetTracer("nestdaq", std::string{NESTDAQ_VERSION});
            }
            state.lastError.clear();
        }
        if (!metricProtocols.empty()) {
            StartProcessMetricsThread(localConfig.metric_export_interval_ms);
        }

        if (loggerProvider) {
            opentelemetry::logs::Provider::SetLoggerProvider(
                opentelemetry::nostd::shared_ptr<opentelemetry::logs::LoggerProvider>{
                    std::shared_ptr<opentelemetry::logs::LoggerProvider>{loggerProvider}});
            FairLoggerOpenTelemetrySink::SetMinSeverity(localConfig.min_severity);
            FairLoggerOpenTelemetrySink::Initialize();
            LOG(info) << NestDAQMetadataLogBody();
            LOG(info) << FairMQMetadataLogBody(localConfig);
        }
        if (meterProvider) {
            opentelemetry::metrics::Provider::SetMeterProvider(
                opentelemetry::nostd::shared_ptr<opentelemetry::metrics::MeterProvider>{
                    std::shared_ptr<opentelemetry::metrics::MeterProvider>{meterProvider}});
        }
        if (tracerProvider) {
            opentelemetry::trace::Provider::SetTracerProvider(
                opentelemetry::nostd::shared_ptr<opentelemetry::trace::TracerProvider>{
                    std::shared_ptr<opentelemetry::trace::TracerProvider>{tracerProvider}});
        }
        return NESTDAQ_OTEL_OK;
    } catch (const std::exception &ex) {
        return SetLastError(ex.what());
    } catch (...) {
        return SetLastError("unknown OpenTelemetry initialization error");
    }
}

auto OpenTelemetryInitializer::LastError() noexcept -> const char *
{
    auto &state = State();
    std::scoped_lock lock{state.mutex};
    return state.lastError.data();
}

auto OpenTelemetryInitializer::SetNestdaqInstanceId(const char *instance_id) -> int
{
    FairLoggerOpenTelemetrySink::SetNestdaqInstanceId(IsEmpty(instance_id) ? "" : instance_id);
    ClearLastError();
    return NESTDAQ_OTEL_OK;
}

auto OpenTelemetryInitializer::SetMinSeverity(int32_t severity) -> int
{
    if (!ValidateSeverity(severity)) {
        return SetLastError("severity must be a valid fair::Severity numeric value");
    }
    FairLoggerOpenTelemetrySink::SetMinSeverity(severity);
    ClearLastError();
    return NESTDAQ_OTEL_OK;
}

auto OpenTelemetryInitializer::Shutdown(uint64_t timeout_ms) -> int
{
    try {
        StopProcessMetricsThread();
        auto &runtimeState = State();
        std::scoped_lock reconfigureLock{runtimeState.frameworkReconfigureMutex};
        std::shared_ptr<opentelemetry::sdk::logs::LoggerProvider> loggerProvider;
        std::shared_ptr<opentelemetry::sdk::metrics::MeterProvider> meterProvider;
        std::shared_ptr<opentelemetry::sdk::metrics::MeterProvider> frameworkMeterProvider;
        std::shared_ptr<opentelemetry::sdk::trace::TracerProvider> tracerProvider;
        {
            auto &state = runtimeState;
            std::scoped_lock lock{state.mutex};
            loggerProvider = std::move(state.loggerProvider);
            meterProvider = std::move(state.meterProvider);
            frameworkMeterProvider = std::move(state.frameworkMeterProvider);
            tracerProvider = std::move(state.tracerProvider);
            state.loggerProvider.reset();
            state.meterProvider.reset();
            state.frameworkMeterProvider.reset();
            state.tracerProvider.reset();
            state.meter = {};
            state.frameworkMeter = {};
            state.tracer = {};
            state.doubleCounters.clear();
            state.doubleHistograms.clear();
            state.doubleGauges.clear();
            state.doubleGaugeMeasurements.clear();
            state.fairmqMessagesPerSecondGauge = {};
            state.fairmqMegabytesPerSecondGauge = {};
            state.processCpuUsageGauge = {};
            state.processMemoryRssGauge = {};
            state.fairmqStateGauge = {};
            state.pendingFairMQThroughputMeasurements.clear();
            state.exportingFairMQThroughputMeasurements.clear();
            state.pendingProcessUsageMeasurements.clear();
            state.exportingProcessUsageMeasurements.clear();
            state.pendingFairMQStateMeasurements.clear();
            state.exportingFairMQStateMeasurements.clear();
            state.processCpuUsageSample = std::nullopt;
            state.pageSize = 0;
            state.spans.clear();
        }
        FairLoggerOpenTelemetrySink::Shutdown();
        if (loggerProvider) {
            loggerProvider->ForceFlush(TimeoutFromMs(timeout_ms));
            loggerProvider->Shutdown(TimeoutFromMs(timeout_ms));
        }
        if (meterProvider) {
            meterProvider->ForceFlush(TimeoutFromMs(timeout_ms));
            meterProvider->Shutdown(TimeoutFromMs(timeout_ms));
        }
        if (frameworkMeterProvider) {
            frameworkMeterProvider->ForceFlush(TimeoutFromMs(timeout_ms));
            frameworkMeterProvider->Shutdown(TimeoutFromMs(timeout_ms));
        }
        if (tracerProvider) {
            tracerProvider->ForceFlush(TimeoutFromMs(timeout_ms));
            tracerProvider->Shutdown(TimeoutFromMs(timeout_ms));
        }
        InstallNoopProviders();
        ClearLastError();
        return NESTDAQ_OTEL_OK;
    } catch (const std::exception &ex) {
        return SetLastError(ex.what());
    } catch (...) {
        return SetLastError("unknown OpenTelemetry shutdown error");
    }
}

} // namespace nestdaq

extern "C" {

    NESTDAQ_OTEL_EXPORT int nestdaq_otel_force_flush(uint64_t timeout_ms)
    {
        return nestdaq::OpenTelemetryInitializer::ForceFlush(timeout_ms);
    }

    NESTDAQ_OTEL_EXPORT void nestdaq_otel_framework_record_fairmq_state(int64_t state_id,
                                                                        const char *state_name)
    {
        nestdaq::OpenTelemetryInitializer::RecordFrameworkFairMQState(state_id, state_name);
    }

    NESTDAQ_OTEL_EXPORT int nestdaq_otel_init(const nestdaq_otel_config *config)
    {
        return nestdaq::OpenTelemetryInitializer::Initialize(config);
    }

    NESTDAQ_OTEL_EXPORT const char *nestdaq_otel_last_error(void)
    {
        return nestdaq::OpenTelemetryInitializer::LastError();
    }

    NESTDAQ_OTEL_EXPORT int nestdaq_otel_metric_add_double_counter(const char *name,
                                                                   double value,
                                                                   const char *unit,
                                                                   const char *description,
                                                                   const nestdaq_otel_attribute *attributes,
                                                                   uint64_t attribute_count)
    {
        return nestdaq::OpenTelemetryInitializer::MetricAddDoubleCounter(
            name, value, unit, description, attributes, attribute_count);
    }

    NESTDAQ_OTEL_EXPORT int nestdaq_otel_metric_record_double_histogram(const char *name,
                                                                        double value,
                                                                        const char *unit,
                                                                        const char *description,
                                                                        const nestdaq_otel_attribute *attributes,
                                                                        uint64_t attribute_count)
    {
        return nestdaq::OpenTelemetryInitializer::MetricRecordDoubleHistogram(
            name, value, unit, description, attributes, attribute_count);
    }

    NESTDAQ_OTEL_EXPORT int nestdaq_otel_metric_record_double_gauge(const char *name,
                                                                    double value,
                                                                    const char *unit,
                                                                    const char *description,
                                                                    const nestdaq_otel_attribute *attributes,
                                                                    uint64_t attribute_count)
    {
        return nestdaq::OpenTelemetryInitializer::MetricRecordDoubleGauge(
            name, value, unit, description, attributes, attribute_count);
    }

    NESTDAQ_OTEL_EXPORT int nestdaq_otel_set_min_severity(int32_t severity)
    {
        return nestdaq::OpenTelemetryInitializer::SetMinSeverity(severity);
    }

    NESTDAQ_OTEL_EXPORT int nestdaq_otel_set_nestdaq_instance_id(const char *instance_id)
    {
        return nestdaq::OpenTelemetryInitializer::SetNestdaqInstanceId(instance_id);
    }

    NESTDAQ_OTEL_EXPORT int nestdaq_otel_shutdown(uint64_t timeout_ms)
    {
        return nestdaq::OpenTelemetryInitializer::Shutdown(timeout_ms);
    }

    NESTDAQ_OTEL_EXPORT int nestdaq_otel_span_end(uint64_t span_handle)
    {
        return nestdaq::OpenTelemetryInitializer::SpanEnd(span_handle);
    }

    NESTDAQ_OTEL_EXPORT int nestdaq_otel_span_set_attribute(uint64_t span_handle,
                                                           const nestdaq_otel_attribute *attribute)
    {
        return nestdaq::OpenTelemetryInitializer::SpanSetAttribute(span_handle, attribute);
    }

    NESTDAQ_OTEL_EXPORT uint64_t nestdaq_otel_span_start(const char *name,
                                                        const nestdaq_otel_attribute *attributes,
                                                        uint64_t attribute_count)
    {
        return nestdaq::OpenTelemetryInitializer::SpanStart(name, attributes, attribute_count);
    }

}
