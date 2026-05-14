#include "nestdaq/telemetry/OpenTelemetryRuntime.h"

#include <chrono>
#include <cstdint>
#include <exception>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <opentelemetry/common/key_value_iterable_view.h>
#include <opentelemetry/context/context.h>
#include <opentelemetry/exporters/ostream/metric_exporter_factory.h>
#include <opentelemetry/exporters/otlp/otlp_grpc_metric_exporter_factory.h>
#include <opentelemetry/exporters/otlp/otlp_http_metric_exporter_factory.h>
#include <opentelemetry/nostd/variant.h>
#include <opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader_factory.h>
#include <opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader_options.h>
#include <opentelemetry/sdk/metrics/meter_provider_factory.h>
#include <opentelemetry/sdk/metrics/view/view_registry_factory.h>

#include <sys/resource.h>
#include <unistd.h>

#include "nestdaq/telemetry/FairMQThroughputLogParser.h"

#if __has_include("nestdaq/version.h")
#  include "nestdaq/version.h"
#else
static constexpr std::string_view NESTDAQ_VERSION {"unknown"};
#endif

namespace nestdaq::otel_detail {
namespace {

constexpr auto kFrameworkMetricReaderIntervalMs = uint32_t{24U * 60U * 60U * 1000U};

auto MetricEndpointGrpc(const nestdaq_otel_config &config) -> const char *
{
    return IsEmpty(config.metrics.endpoint_grpc) ? kDefaultGrpcEndpoint.data() : config.metrics.endpoint_grpc;
}

auto MetricEndpointHttp(const nestdaq_otel_config &config) -> const char *
{
    return IsEmpty(config.metrics.endpoint_http) ? kDefaultMetricHttpEndpoint.data() : config.metrics.endpoint_http;
}

auto ObserveFairMQThroughput(opentelemetry::metrics::ObserverResult observer, bool observeMegabytes) noexcept -> void;
auto ObserveFairMQState(opentelemetry::metrics::ObserverResult observer, void * /* state */) noexcept -> void;
auto ReadProcessCpuUsage() noexcept -> std::optional<ProcessCpuUsageSample>;
auto ReadProcessMemoryRssMiB(long pageSize) -> std::optional<double>;
auto TimevalToSeconds(const timeval &value) noexcept -> double;

auto ObserveFairMQMegabytesPerSecond(opentelemetry::metrics::ObserverResult observer, void * /* state */) noexcept
-> void
{
    ObserveFairMQThroughput(observer, true);
}

auto ObserveFairMQMessagesPerSecond(opentelemetry::metrics::ObserverResult observer, void * /* state */) noexcept
-> void
{
    ObserveFairMQThroughput(observer, false);
}

auto ObserveFairMQThroughput(opentelemetry::metrics::ObserverResult observer, bool observeMegabytes) noexcept -> void
{
    using DoubleObserver = opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObserverResultT<double>>;
    if (!opentelemetry::nostd::holds_alternative<DoubleObserver>(observer)) {
        return;
    }

    const auto result = opentelemetry::nostd::get<DoubleObserver>(observer);
    if (!result) {
        return;
    }

    auto measurements = std::vector<FairMQThroughputMeasurement>{};
    {
        auto &state = State();
        std::lock_guard lock{state.mutex};
        measurements = state.exportingFairMQThroughputMeasurements;
    }

    for (const auto &measurement : measurements) {
        auto attributes =
            std::vector<std::pair<opentelemetry::nostd::string_view, opentelemetry::common::AttributeValue>>{};
        attributes.reserve(3);
        attributes.emplace_back("fairmq.channel.name",
                                opentelemetry::nostd::string_view{measurement.channelName});
        attributes.emplace_back("network.io.direction",
                                opentelemetry::nostd::string_view{measurement.direction});
        if (measurement.subChannelIndex) {
            attributes.emplace_back("fairmq.channel.index",
                                    static_cast<int64_t>(*measurement.subChannelIndex));
        }
        result->Observe(observeMegabytes ? measurement.megabytesPerSecond : measurement.messagesPerSecond,
                        attributes);
    }
}

auto ObserveProcessCpuUsage(opentelemetry::metrics::ObserverResult observer, void * /* state */) noexcept -> void
{
    using DoubleObserver = opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObserverResultT<double>>;
    if (!opentelemetry::nostd::holds_alternative<DoubleObserver>(observer)) {
        return;
    }

    const auto result = opentelemetry::nostd::get<DoubleObserver>(observer);
    if (!result) {
        return;
    }

    auto measurements = std::vector<ProcessUsageMeasurement>{};
    {
        auto &state = State();
        std::lock_guard lock{state.mutex};
        measurements = state.exportingProcessUsageMeasurements;
    }

    for (const auto &measurement : measurements) {
        result->Observe(measurement.cpuUsagePercent);
    }
}

auto ObserveProcessMemoryRss(opentelemetry::metrics::ObserverResult observer, void * /* state */) noexcept -> void
{
    using DoubleObserver = opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObserverResultT<double>>;
    if (!opentelemetry::nostd::holds_alternative<DoubleObserver>(observer)) {
        return;
    }

    const auto result = opentelemetry::nostd::get<DoubleObserver>(observer);
    if (!result) {
        return;
    }

    auto measurements = std::vector<ProcessUsageMeasurement>{};
    {
        auto &state = State();
        std::lock_guard lock{state.mutex};
        measurements = state.exportingProcessUsageMeasurements;
    }

    for (const auto &measurement : measurements) {
        result->Observe(measurement.memoryRssMiB);
    }
}

auto ObserveUserDoubleGauge(opentelemetry::metrics::ObserverResult observer, void *state) noexcept -> void
{
    using DoubleObserver = opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObserverResultT<double>>;
    if (!opentelemetry::nostd::holds_alternative<DoubleObserver>(observer) || state == nullptr) {
        return;
    }

    const auto result = opentelemetry::nostd::get<DoubleObserver>(observer);
    if (!result) {
        return;
    }

    const auto *metric = static_cast<const MetricKey *>(state);
    auto measurements = std::vector<GaugeMeasurement>{};
    {
        auto &runtime = State();
        std::lock_guard lock{runtime.mutex};
        for (const auto &[sample, value] : runtime.doubleGaugeMeasurements) {
            if (sample.metric == *metric) {
                auto measurement = GaugeMeasurement{};
                measurement.attributes = sample.attributes;
                measurement.value = value;
                measurements.emplace_back(std::move(measurement));
            }
        }
    }

    for (const auto &measurement : measurements) {
        auto attributes =
            std::vector<std::pair<opentelemetry::nostd::string_view, opentelemetry::common::AttributeValue>>{};
        attributes.reserve(measurement.attributes.size());
        for (const auto &attribute : measurement.attributes) {
            auto key = opentelemetry::nostd::string_view{attribute.key};
            switch (attribute.type) {
            case NESTDAQ_OTEL_ATTRIBUTE_STRING:
                attributes.emplace_back(key, opentelemetry::nostd::string_view{attribute.stringValue});
                break;
            case NESTDAQ_OTEL_ATTRIBUTE_INT64:
                attributes.emplace_back(key, attribute.intValue);
                break;
            case NESTDAQ_OTEL_ATTRIBUTE_UINT64:
                attributes.emplace_back(key, attribute.uintValue);
                break;
            case NESTDAQ_OTEL_ATTRIBUTE_DOUBLE:
                attributes.emplace_back(key, attribute.doubleValue);
                break;
            case NESTDAQ_OTEL_ATTRIBUTE_BOOL:
                attributes.emplace_back(key, attribute.boolValue);
                break;
            }
        }
        result->Observe(measurement.value, attributes);
    }
}

auto ObserveFairMQState(opentelemetry::metrics::ObserverResult observer, void * /* state */) noexcept -> void
{
    using DoubleObserver = opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObserverResultT<double>>;
    if (!opentelemetry::nostd::holds_alternative<DoubleObserver>(observer)) {
        return;
    }

    const auto result = opentelemetry::nostd::get<DoubleObserver>(observer);
    if (!result) {
        return;
    }

    auto measurements = std::vector<FairMQStateMeasurement>{};
    {
        auto &state = State();
        std::lock_guard lock{state.mutex};
        measurements = state.exportingFairMQStateMeasurements;
    }

    for (const auto &measurement : measurements) {
        auto attributes =
            std::vector<std::pair<opentelemetry::nostd::string_view, opentelemetry::common::AttributeValue>>{};
        attributes.emplace_back("fairmq.state.name", opentelemetry::nostd::string_view{measurement.stateName});
        result->Observe(static_cast<double>(measurement.stateId), attributes);
    }
}

auto ReadProcessCpuUsage() noexcept -> std::optional<ProcessCpuUsageSample>
{
    auto usage = rusage{};
    if (getrusage(RUSAGE_SELF, &usage) != 0) {
        return std::nullopt;
    }
    return ProcessCpuUsageSample{
        .timestamp = std::chrono::steady_clock::now(),
        .cpuSeconds = TimevalToSeconds(usage.ru_utime) + TimevalToSeconds(usage.ru_stime),
    };
}

auto ReadProcessMemoryRssMiB(long pageSize) -> std::optional<double>
{
    if (pageSize <= 0) {
        return std::nullopt;
    }

    auto statm = std::ifstream{"/proc/self/statm"};
    auto totalPages = uint64_t{0};
    auto residentPages = uint64_t{0};
    if (!(statm >> totalPages >> residentPages)) {
        return std::nullopt;
    }

    const auto bytes = static_cast<double>(residentPages) * static_cast<double>(pageSize);
    return bytes / (1024.0 * 1024.0);
}

auto TimevalToSeconds(const timeval &value) noexcept -> double
{
    return static_cast<double>(value.tv_sec) + (static_cast<double>(value.tv_usec) / 1'000'000.0);
}

} // namespace

auto ConfigureFairMQThroughputMetrics(RuntimeState &state) -> void
{
    if (!state.frameworkMeter) {
        return;
    }

    state.fairmqMessagesPerSecondGauge = state.frameworkMeter->CreateDoubleObservableGauge(
        "fairmq.channel.messages_per_second",
        "FairMQ channel message rate parsed from Device throughput logs",
        "{message}/s");
    if (state.fairmqMessagesPerSecondGauge) {
        state.fairmqMessagesPerSecondGauge->AddCallback(ObserveFairMQMessagesPerSecond, nullptr);
    }

    state.fairmqMegabytesPerSecondGauge = state.frameworkMeter->CreateDoubleObservableGauge(
        "fairmq.channel.megabytes_per_second",
        "FairMQ channel throughput parsed from Device throughput logs",
        "MB/s");
    if (state.fairmqMegabytesPerSecondGauge) {
        state.fairmqMegabytesPerSecondGauge->AddCallback(ObserveFairMQMegabytesPerSecond, nullptr);
    }
}

auto ConfigureProcessMetrics(RuntimeState &state) -> void
{
    if (!state.frameworkMeter) {
        return;
    }

    state.pageSize = sysconf(_SC_PAGESIZE);
    state.processCpuUsageSample = ReadProcessCpuUsage();

    state.processCpuUsageGauge = state.frameworkMeter->CreateDoubleObservableGauge(
        "process.cpu.usage_percent",
        "Process CPU usage in top/htop style percent",
        "%");
    if (state.processCpuUsageGauge) {
        state.processCpuUsageGauge->AddCallback(ObserveProcessCpuUsage, nullptr);
    }

    state.processMemoryRssGauge = state.frameworkMeter->CreateDoubleObservableGauge(
        "process.memory.rss_mib",
        "Process resident memory usage",
        "MiBy");
    if (state.processMemoryRssGauge) {
        state.processMemoryRssGauge->AddCallback(ObserveProcessMemoryRss, nullptr);
    }
}

auto ConfigureFairMQStateMetrics(RuntimeState &state) -> void
{
    if (!state.frameworkMeter) {
        return;
    }

    state.fairmqStateGauge = state.frameworkMeter->CreateDoubleObservableGauge(
        "fairmq.state.id",
        "FairMQ device state numeric id",
        "1");
    if (state.fairmqStateGauge) {
        state.fairmqStateGauge->AddCallback(ObserveFairMQState, nullptr);
    }
}

auto ConfigureFrameworkMetricsProvider(RuntimeState &state) -> void
{
    if (!state.frameworkMetricResource || state.frameworkMetricProtocols.empty()) {
        return;
    }

    auto views = opentelemetry::sdk::metrics::ViewRegistryFactory::Create();
    state.frameworkMeterProvider = std::shared_ptr<opentelemetry::sdk::metrics::MeterProvider>{
        opentelemetry::sdk::metrics::MeterProviderFactory::Create(std::move(views), *state.frameworkMetricResource)};

    auto config = state.frameworkMetricConfig.ToConfig();
    config.metric_export_interval_ms = kFrameworkMetricReaderIntervalMs;
    for (const auto protocol : state.frameworkMetricProtocols) {
        state.frameworkMeterProvider->AddMetricReader(CreateMetricReader(CreateMetricExporter(config, protocol), config));
    }

    state.frameworkMeter = state.frameworkMeterProvider->GetMeter("nestdaq.framework", std::string{NESTDAQ_VERSION});
    ConfigureProcessMetrics(state);
    ConfigureFairMQThroughputMetrics(state);
    ConfigureFairMQStateMetrics(state);
}

auto CreateMetricExporter(const nestdaq_otel_config &config, Protocol protocol)
-> std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter>
{
    switch (protocol) {
    case Protocol::Console:
        return opentelemetry::exporter::metrics::OStreamMetricExporterFactory::Create();
    case Protocol::OtlpHttp: {
        auto options = opentelemetry::exporter::otlp::OtlpHttpMetricExporterOptions{};
        options.url = MetricEndpointHttp(config);
        options.http_headers = ParseHeaders(config.metrics.headers);
        options.content_type = config.metrics.otlp_http_json == 0
                               ? opentelemetry::exporter::otlp::HttpRequestContentType::kBinary
                               : opentelemetry::exporter::otlp::HttpRequestContentType::kJson;
        options.timeout = TimeoutFromMs(config.timeout_ms);
        return opentelemetry::exporter::otlp::OtlpHttpMetricExporterFactory::Create(options);
    }
    case Protocol::OtlpGrpc: {
        auto options = opentelemetry::exporter::otlp::OtlpGrpcMetricExporterOptions{};
        options.endpoint = MetricEndpointGrpc(config);
        options.metadata = ParseHeaders(config.metrics.headers);
        options.timeout = TimeoutFromMs(config.timeout_ms);
        return opentelemetry::exporter::otlp::OtlpGrpcMetricExporterFactory::Create(options);
    }
    }
    return nullptr;
}

auto CreateMetricReader(std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter> exporter,
                        const nestdaq_otel_config &config)
-> std::unique_ptr<opentelemetry::sdk::metrics::MetricReader>
{
    auto options = opentelemetry::sdk::metrics::PeriodicExportingMetricReaderOptions{};
    options.export_interval_millis = std::chrono::milliseconds{
        config.metric_export_interval_ms == 0 ? kDefaultMetricExportIntervalMs : config.metric_export_interval_ms};
    options.export_timeout_millis = std::chrono::milliseconds{config.timeout_ms};
    return opentelemetry::sdk::metrics::PeriodicExportingMetricReaderFactory::Create(std::move(exporter), options);
}

auto StartProcessMetricsThread(uint32_t intervalMs) -> void
{
    StopProcessMetricsThread();
    auto &state = State();
    {
        std::lock_guard lock{state.mutex};
        state.stopProcessMetricsThread = false;
        state.processMetricsInterval = std::chrono::milliseconds{
            intervalMs == 0 ? kDefaultMetricExportIntervalMs : intervalMs};
    }

    state.processMetricsThread = std::thread{[] {
        while (true) {
            auto interval = std::chrono::milliseconds{kDefaultMetricExportIntervalMs};
            {
                auto &runtime = State();
                std::lock_guard lock{runtime.mutex};
                if (runtime.stopProcessMetricsThread) {
                    return;
                }
                interval = runtime.processMetricsInterval;
            }

            std::this_thread::sleep_for(interval);

            auto previousCpu = std::optional<ProcessCpuUsageSample>{};
            auto pageSize = 0L;
            {
                auto &runtime = State();
                std::lock_guard lock{runtime.mutex};
                if (runtime.stopProcessMetricsThread) {
                    return;
                }
                previousCpu = runtime.processCpuUsageSample;
                pageSize = runtime.pageSize;
            }

            const auto currentCpu = ReadProcessCpuUsage();
            const auto currentRss = ReadProcessMemoryRssMiB(pageSize);
            if (!currentCpu || !currentRss) {
                continue;
            }

            auto cpuUsage = 0.0;
            if (previousCpu) {
                const auto elapsedSeconds =
                    std::chrono::duration<double>{currentCpu->timestamp - previousCpu->timestamp}.count();
                if (elapsedSeconds > 0.0) {
                    cpuUsage = ((currentCpu->cpuSeconds - previousCpu->cpuSeconds) / elapsedSeconds) * 100.0;
                }
            }
            {
                auto &runtime = State();
                std::lock_guard lock{runtime.mutex};
                runtime.processCpuUsageSample = currentCpu;
            }
            nestdaq::OpenTelemetryInitializer::RecordFrameworkProcessUsage(cpuUsage, *currentRss);
        }
    }};
}

auto StopProcessMetricsThread() -> void
{
    auto &state = State();
    {
        std::lock_guard lock{state.mutex};
        state.stopProcessMetricsThread = true;
    }
    if (state.processMetricsThread.joinable()) {
        state.processMetricsThread.join();
    }
}

} // namespace nestdaq::otel_detail

namespace nestdaq {

auto OpenTelemetryInitializer::MetricAddDoubleCounter(const char *name,
                                                      double value,
                                                      const char *unit,
                                                      const char *description,
                                                      const nestdaq_otel_attribute *attributes,
                                                      uint64_t attribute_count) -> int
{
    using namespace otel_detail;
    if (IsEmpty(name)) {
        return SetLastError("metric counter name is empty");
    }
    auto attrs = BuildAttributes(attributes, attribute_count);
    auto &state = State();
    std::lock_guard lock{state.mutex};
    if (!state.meter) {
        state.lastError.clear();
        return NESTDAQ_OTEL_OK;
    }
    auto key = MetricKey{.kind = MetricKind::DoubleCounter,
                         .name = name,
                         .unit = IsEmpty(unit) ? "" : unit,
                         .description = IsEmpty(description) ? "" : description};
    auto &counter = state.doubleCounters[key];
    if (!counter) {
        counter = state.meter->CreateDoubleCounter(key.name, key.description, key.unit);
    }
    auto view = opentelemetry::common::KeyValueIterableView<decltype(attrs.values)>{attrs.values};
    counter->Add(value, view);
    state.lastError.clear();
    return NESTDAQ_OTEL_OK;
}

auto OpenTelemetryInitializer::MetricRecordDoubleHistogram(const char *name,
                                                           double value,
                                                           const char *unit,
                                                           const char *description,
                                                           const nestdaq_otel_attribute *attributes,
                                                           uint64_t attribute_count) -> int
{
    using namespace otel_detail;
    if (IsEmpty(name)) {
        return SetLastError("metric histogram name is empty");
    }
    auto attrs = BuildAttributes(attributes, attribute_count);
    auto &state = State();
    std::lock_guard lock{state.mutex};
    if (!state.meter) {
        state.lastError.clear();
        return NESTDAQ_OTEL_OK;
    }
    auto key = MetricKey{.kind = MetricKind::DoubleHistogram,
                         .name = name,
                         .unit = IsEmpty(unit) ? "" : unit,
                         .description = IsEmpty(description) ? "" : description};
    auto &histogram = state.doubleHistograms[key];
    if (!histogram) {
        histogram = state.meter->CreateDoubleHistogram(key.name, key.description, key.unit);
    }
    auto view = opentelemetry::common::KeyValueIterableView<decltype(attrs.values)>{attrs.values};
    histogram->Record(value, view, opentelemetry::context::Context{});
    state.lastError.clear();
    return NESTDAQ_OTEL_OK;
}

auto OpenTelemetryInitializer::MetricRecordDoubleGauge(const char *name,
                                                       double value,
                                                       const char *unit,
                                                       const char *description,
                                                       const nestdaq_otel_attribute *attributes,
                                                       uint64_t attribute_count) -> int
{
    using namespace otel_detail;
    if (IsEmpty(name)) {
        return SetLastError("metric gauge name is empty");
    }
    auto gaugeAttributes = BuildGaugeAttributes(attributes, attribute_count);
    auto key = MetricKey{.kind = MetricKind::DoubleGauge,
                         .name = name,
                         .unit = IsEmpty(unit) ? "" : unit,
                         .description = IsEmpty(description) ? "" : description};

    opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObservableInstrument> newGauge;
    MetricKey *callbackKey = nullptr;
    {
        auto &state = State();
        std::lock_guard lock{state.mutex};
        if (!state.meter) {
            state.lastError.clear();
            return NESTDAQ_OTEL_OK;
        }

        auto [gauge, _] = state.doubleGauges.try_emplace(key);
        if (!gauge->second.instrument) {
            gauge->second.callbackKey = key;
            gauge->second.instrument = state.meter->CreateDoubleObservableGauge(key.name, key.description, key.unit);
            if (gauge->second.instrument) {
                newGauge = gauge->second.instrument;
                callbackKey = &gauge->second.callbackKey;
            }
        }

        state.doubleGaugeMeasurements[GaugeSampleKey{.metric = std::move(key),
                                                     .attributes = std::move(gaugeAttributes)}] = value;
        state.lastError.clear();
    }
    if (newGauge) {
        newGauge->AddCallback(ObserveUserDoubleGauge, callbackKey);
    }
    return NESTDAQ_OTEL_OK;
}

auto OpenTelemetryInitializer::RecordFairMQThroughput(const telemetry::FairMQThroughputSample &sample) noexcept
-> void
{
    RecordFrameworkFairMQThroughput(sample);
}

auto OpenTelemetryInitializer::RecordFrameworkFairMQThroughput(const telemetry::FairMQThroughputSample &sample) noexcept
-> void
{
    try {
        {
            auto &state = otel_detail::State();
            std::lock_guard reconfigureLock{state.frameworkReconfigureMutex};
            std::lock_guard lock{state.mutex};
            if (!state.frameworkMeterProvider) {
                return;
            }
            state.pendingFairMQThroughputMeasurements.emplace_back(otel_detail::FairMQThroughputMeasurement{
                .channelName = sample.channelName,
                .subChannelName = sample.subChannelName,
                .direction = "in",
                .subChannelIndex = sample.subChannelIndex,
                .messagesPerSecond = sample.messagesPerSecondIn,
                .megabytesPerSecond = sample.megabytesPerSecondIn,
            });
            state.pendingFairMQThroughputMeasurements.emplace_back(otel_detail::FairMQThroughputMeasurement{
                .channelName = sample.channelName,
                .subChannelName = sample.subChannelName,
                .direction = "out",
                .subChannelIndex = sample.subChannelIndex,
                .messagesPerSecond = sample.messagesPerSecondOut,
                .megabytesPerSecond = sample.megabytesPerSecondOut,
            });
        }
        static_cast<void>(otel_detail::FlushFrameworkMetricsIfDirty(otel_detail::kDefaultMetricExportIntervalMs));
    } catch (...) {
    }
}

auto OpenTelemetryInitializer::RecordFrameworkProcessUsage(double cpu_usage_percent, double memory_rss_mib) noexcept
-> void
{
    try {
        {
            auto &state = otel_detail::State();
            std::lock_guard reconfigureLock{state.frameworkReconfigureMutex};
            std::lock_guard lock{state.mutex};
            if (!state.frameworkMeterProvider) {
                return;
            }
            state.pendingProcessUsageMeasurements.emplace_back(otel_detail::ProcessUsageMeasurement{
                .cpuUsagePercent = cpu_usage_percent,
                .memoryRssMiB = memory_rss_mib,
            });
        }
        static_cast<void>(otel_detail::FlushFrameworkMetricsIfDirty(otel_detail::kDefaultMetricExportIntervalMs));
    } catch (...) {
    }
}

auto OpenTelemetryInitializer::RecordFrameworkFairMQState(int64_t state_id, const char *state_name) noexcept -> void
{
    try {
        {
            auto &state = otel_detail::State();
            std::lock_guard reconfigureLock{state.frameworkReconfigureMutex};
            std::lock_guard lock{state.mutex};
            if (!state.frameworkMeterProvider) {
                return;
            }
            state.pendingFairMQStateMeasurements.emplace_back(otel_detail::FairMQStateMeasurement{
                .stateId = state_id,
                .stateName = otel_detail::IsEmpty(state_name) ? "" : state_name,
            });
        }
        static_cast<void>(otel_detail::FlushFrameworkMetricsIfDirty(otel_detail::kDefaultMetricExportIntervalMs));
    } catch (...) {
    }
}

} // namespace nestdaq
