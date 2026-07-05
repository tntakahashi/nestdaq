/** @file
 *  @brief Implements user metrics and one-shot framework metrics export.
 */

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

auto metricEndpointGrpc(const nestdaq_otel_config &config) -> const char *
{
    return isEmpty(config.metrics.endpoint_grpc) ? kDefaultGrpcEndpoint.data() : config.metrics.endpoint_grpc;
}

auto metricEndpointHttp(const nestdaq_otel_config &config) -> const char *
{
    return isEmpty(config.metrics.endpoint_http) ? kDefaultMetricHttpEndpoint.data() : config.metrics.endpoint_http;
}

auto observeFairMQThroughput(opentelemetry::metrics::ObserverResult observer, bool observeMegabytes) noexcept -> void;
auto observeFairMQState(opentelemetry::metrics::ObserverResult observer, void * /* state */) noexcept -> void;
auto observeProcessCpuTime(opentelemetry::metrics::ObserverResult observer, void * /* state */) noexcept -> void;
auto observeProcessCpuUtilization(opentelemetry::metrics::ObserverResult observer, void * /* state */) noexcept
-> void;
auto observeProcessMemoryUsage(opentelemetry::metrics::ObserverResult observer, void * /* state */) noexcept -> void;
auto readAvailableCpuCount() noexcept -> double;
auto readProcessCpuUsage() noexcept -> std::optional<ProcessCpuUsageSample>;
auto readProcessMemoryUsageBytes(long page_size) -> std::optional<double>;
auto timevalToSeconds(const timeval &value) noexcept -> double;

auto observeFairMQMegabytesPerSecond(opentelemetry::metrics::ObserverResult observer, void * /* state */) noexcept
-> void
{
    observeFairMQThroughput(observer, true);
}

auto observeFairMQMessagesPerSecond(opentelemetry::metrics::ObserverResult observer, void * /* state */) noexcept
-> void
{
    observeFairMQThroughput(observer, false);
}

auto observeFairMQThroughput(opentelemetry::metrics::ObserverResult observer, bool observeMegabytes) noexcept -> void
{
    using DoubleObserver = opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObserverResultT<double>>;
    if (!opentelemetry::nostd::holds_alternative<DoubleObserver>(observer)) {
        return;
    }

    const auto result = opentelemetry::nostd::get<DoubleObserver>(observer);
    if (!result) {
        return;
    }

    auto measurements = std::vector<FairMQThroughputMeasurement> {};
    {
        auto &state = runtimeState();
        std::scoped_lock lock{state.mutex};
        measurements = state.exportingFairMQThroughputMeasurements;
    }

    for (const auto &measurement : measurements) {
        auto attributes =
            std::vector<std::pair<opentelemetry::nostd::string_view, opentelemetry::common::AttributeValue>> {};
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

auto observeProcessCpuTime(opentelemetry::metrics::ObserverResult observer, void * /* state */) noexcept -> void
{
    using DoubleObserver = opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObserverResultT<double>>;
    if (!opentelemetry::nostd::holds_alternative<DoubleObserver>(observer)) {
        return;
    }

    const auto result = opentelemetry::nostd::get<DoubleObserver>(observer);
    if (!result) {
        return;
    }

    auto measurements = std::vector<ProcessUsageMeasurement> {};
    {
        auto &state = runtimeState();
        std::scoped_lock lock{state.mutex};
        measurements = state.exportingProcessUsageMeasurements;
    }

    for (const auto &measurement : measurements) {
        auto userAttributes =
            std::vector<std::pair<opentelemetry::nostd::string_view, opentelemetry::common::AttributeValue>> {};
        userAttributes.emplace_back("cpu.mode", opentelemetry::nostd::string_view{"user"});
        result->Observe(measurement.cpuUserSeconds, userAttributes);

        auto systemAttributes =
            std::vector<std::pair<opentelemetry::nostd::string_view, opentelemetry::common::AttributeValue>> {};
        systemAttributes.emplace_back("cpu.mode", opentelemetry::nostd::string_view{"system"});
        result->Observe(measurement.cpuSystemSeconds, systemAttributes);
    }
}

auto observeProcessCpuUtilization(opentelemetry::metrics::ObserverResult observer, void * /* state */) noexcept
-> void
{
    using DoubleObserver = opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObserverResultT<double>>;
    if (!opentelemetry::nostd::holds_alternative<DoubleObserver>(observer)) {
        return;
    }

    const auto result = opentelemetry::nostd::get<DoubleObserver>(observer);
    if (!result) {
        return;
    }

    auto measurements = std::vector<ProcessUsageMeasurement> {};
    {
        auto &state = runtimeState();
        std::scoped_lock lock{state.mutex};
        measurements = state.exportingProcessUsageMeasurements;
    }

    for (const auto &measurement : measurements) {
        if (measurement.cpuUtilization) {
            result->Observe(*measurement.cpuUtilization);
        }
    }
}

auto observeProcessMemoryUsage(opentelemetry::metrics::ObserverResult observer, void * /* state */) noexcept -> void
{
    using DoubleObserver = opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObserverResultT<double>>;
    if (!opentelemetry::nostd::holds_alternative<DoubleObserver>(observer)) {
        return;
    }

    const auto result = opentelemetry::nostd::get<DoubleObserver>(observer);
    if (!result) {
        return;
    }

    auto measurements = std::vector<ProcessUsageMeasurement> {};
    {
        auto &state = runtimeState();
        std::scoped_lock lock{state.mutex};
        measurements = state.exportingProcessUsageMeasurements;
    }

    for (const auto &measurement : measurements) {
        result->Observe(measurement.memoryUsageBytes);
    }
}

auto observeUserDoubleGauge(opentelemetry::metrics::ObserverResult observer, void *state) noexcept -> void
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
    auto measurements = std::vector<GaugeMeasurement> {};
    {
        auto &runtime = runtimeState();
        std::scoped_lock lock{runtime.mutex};
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
            std::vector<std::pair<opentelemetry::nostd::string_view, opentelemetry::common::AttributeValue>> {};
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

auto observeFairMQState(opentelemetry::metrics::ObserverResult observer, void * /* state */) noexcept -> void
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
        auto &state = runtimeState();
        std::scoped_lock lock{state.mutex};
        measurements = state.exportingFairMQStateMeasurements;
    }

    for (const auto &measurement : measurements) {
        auto attributes =
            std::vector<std::pair<opentelemetry::nostd::string_view, opentelemetry::common::AttributeValue>> {};
        attributes.emplace_back("fairmq.state.name", opentelemetry::nostd::string_view{measurement.stateName});
        result->Observe(static_cast<double>(measurement.stateId), attributes);
    }
}

auto readAvailableCpuCount() noexcept -> double
{
    const auto cpu_count = sysconf(_SC_NPROCESSORS_ONLN);
    return cpu_count > 0 ? static_cast<double>(cpu_count) : 0.0;
}

auto readProcessCpuUsage() noexcept -> std::optional<ProcessCpuUsageSample>
{
    auto usage = rusage{};
    if (getrusage(RUSAGE_SELF, &usage) != 0) {
        return std::nullopt;
    }
    return ProcessCpuUsageSample{
        .timestamp = std::chrono::steady_clock::now(),
        .userSeconds = timevalToSeconds(usage.ru_utime),
        .systemSeconds = timevalToSeconds(usage.ru_stime),
    };
}

auto readProcessMemoryUsageBytes(long page_size) -> std::optional<double>
{
    if (page_size <= 0) {
        return std::nullopt;
    }

    auto statm = std::ifstream{"/proc/self/statm"};
    auto totalPages = uint64_t{0};
    auto residentPages = uint64_t{0};
    if (!(statm >> totalPages >> residentPages)) {
        return std::nullopt;
    }

    return static_cast<double>(residentPages) * static_cast<double>(page_size);
}

auto timevalToSeconds(const timeval &value) noexcept -> double
{
    return static_cast<double>(value.tv_sec) + (static_cast<double>(value.tv_usec) / 1'000'000.0);
}

} // namespace

auto configureFairMQThroughputMetrics(RuntimeState &state) -> void
{
    if (!state.frameworkMeter) {
        return;
    }

    state.fairmqMessagesPerSecondGauge = state.frameworkMeter->CreateDoubleObservableGauge(
            "fairmq.channel.messages_per_second",
            "FairMQ channel message rate parsed from Device throughput logs",
            "{message}/s");
    if (state.fairmqMessagesPerSecondGauge) {
        state.fairmqMessagesPerSecondGauge->AddCallback(observeFairMQMessagesPerSecond, nullptr);
    }

    state.fairmqMegabytesPerSecondGauge = state.frameworkMeter->CreateDoubleObservableGauge(
            "fairmq.channel.megabytes_per_second",
            "FairMQ channel throughput parsed from Device throughput logs",
            "MB/s");
    if (state.fairmqMegabytesPerSecondGauge) {
        state.fairmqMegabytesPerSecondGauge->AddCallback(observeFairMQMegabytesPerSecond, nullptr);
    }
}

auto configureProcessMetrics(RuntimeState &state) -> void
{
    if (!state.frameworkMeter) {
        return;
    }

    state.pageSize = sysconf(_SC_PAGESIZE);
    state.availableCpuCount = readAvailableCpuCount();
    state.processCpuUsageSample = readProcessCpuUsage();

    state.processCpuTimeCounter = state.frameworkMeter->CreateDoubleObservableCounter(
                                      "process.cpu.time",
                                      "Total CPU seconds broken down by mode",
                                      "s");
    if (state.processCpuTimeCounter) {
        state.processCpuTimeCounter->AddCallback(observeProcessCpuTime, nullptr);
    }

    if (state.availableCpuCount > 0.0) {
        state.processCpuUtilizationGauge = state.frameworkMeter->CreateDoubleObservableGauge(
                                               "process.cpu.utilization",
                                               "Process CPU utilization normalized by available CPU count",
                                               "1");
        if (state.processCpuUtilizationGauge) {
            state.processCpuUtilizationGauge->AddCallback(observeProcessCpuUtilization, nullptr);
        }
    }

    state.processMemoryUsageCounter = state.frameworkMeter->CreateDoubleObservableUpDownCounter(
                                          "process.memory.usage",
                                          "Physical memory in use by the process",
                                          "By");
    if (state.processMemoryUsageCounter) {
        state.processMemoryUsageCounter->AddCallback(observeProcessMemoryUsage, nullptr);
    }
}

auto configureFairMQStateMetrics(RuntimeState &state) -> void
{
    if (!state.frameworkMeter) {
        return;
    }

    state.fairmqStateGauge = state.frameworkMeter->CreateDoubleObservableGauge(
                                 "fairmq.state.id",
                                 "FairMQ device state numeric id",
                                 "1");
    if (state.fairmqStateGauge) {
        state.fairmqStateGauge->AddCallback(observeFairMQState, nullptr);
    }
}

auto configureFrameworkMetricsProvider(RuntimeState &state) -> void
{
    if (!state.frameworkMetricResource || state.frameworkMetricProtocols.empty()) {
        return;
    }

    auto views = opentelemetry::sdk::metrics::ViewRegistryFactory::Create();
    state.frameworkMeterProvider = std::shared_ptr<opentelemetry::sdk::metrics::MeterProvider> {
        opentelemetry::sdk::metrics::MeterProviderFactory::Create(std::move(views), *state.frameworkMetricResource)
    };

    auto config = state.frameworkMetricConfig.ToConfig();
    config.metric_export_interval_ms = kFrameworkMetricReaderIntervalMs;
    for (const auto protocol : state.frameworkMetricProtocols) {
        state.frameworkMeterProvider->AddMetricReader(createMetricReader(createMetricExporter(config, protocol), config));
    }

    state.frameworkMeter = state.frameworkMeterProvider->GetMeter("nestdaq.framework", std::string{NESTDAQ_VERSION});
    configureProcessMetrics(state);
    configureFairMQThroughputMetrics(state);
    configureFairMQStateMetrics(state);
}

auto createMetricExporter(const nestdaq_otel_config &config, Protocol protocol)
-> std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter>
{
    switch (protocol) {
    case Protocol::Console:
        return opentelemetry::exporter::metrics::OStreamMetricExporterFactory::Create();
    case Protocol::OtlpHttp: {
        auto options = opentelemetry::exporter::otlp::OtlpHttpMetricExporterOptions{};
        options.url = metricEndpointHttp(config);
        options.http_headers = parseHeaders(config.metrics.headers);
        options.content_type = config.metrics.otlp_http_json == 0
                               ? opentelemetry::exporter::otlp::HttpRequestContentType::kBinary
                               : opentelemetry::exporter::otlp::HttpRequestContentType::kJson;
        options.timeout = timeoutFromMs(config.timeout_ms);
        return opentelemetry::exporter::otlp::OtlpHttpMetricExporterFactory::Create(options);
    }
    case Protocol::OtlpGrpc: {
        auto options = opentelemetry::exporter::otlp::OtlpGrpcMetricExporterOptions{};
        options.endpoint = metricEndpointGrpc(config);
        options.metadata = parseHeaders(config.metrics.headers);
        options.timeout = timeoutFromMs(config.timeout_ms);
        return opentelemetry::exporter::otlp::OtlpGrpcMetricExporterFactory::Create(options);
    }
    }
    return nullptr;
}

auto createMetricReader(std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter> exporter,
                        const nestdaq_otel_config &config)
-> std::unique_ptr<opentelemetry::sdk::metrics::MetricReader>
{
    auto options = opentelemetry::sdk::metrics::PeriodicExportingMetricReaderOptions{};
    options.export_interval_millis = std::chrono::milliseconds{
        config.metric_export_interval_ms == 0 ? kDefaultMetricExportIntervalMs : config.metric_export_interval_ms};
    options.export_timeout_millis = std::chrono::milliseconds{config.timeout_ms};
    return opentelemetry::sdk::metrics::PeriodicExportingMetricReaderFactory::Create(std::move(exporter), options);
}

auto startProcessMetricsThread(uint32_t intervalMs) -> void
{
    stopProcessMetricsThread();
    auto &state = runtimeState();
    {
        std::scoped_lock lock{state.mutex};
        state.stopProcessMetricsThread = false;
        state.processMetricsInterval = std::chrono::milliseconds{
            intervalMs == 0 ? kDefaultMetricExportIntervalMs : intervalMs};
    }

    // CPU utilization needs two process CPU samples. CPU time and memory usage
    // are exported as one-shot process metrics on each successful tick.
    state.processMetricsThread = std::thread{[] {
            while (true) {
                auto interval = std::chrono::milliseconds{kDefaultMetricExportIntervalMs};
                {
                    auto &runtime = runtimeState();
                    std::scoped_lock lock{runtime.mutex};
                    if (runtime.stopProcessMetricsThread) {
                        return;
                    }
                    interval = runtime.processMetricsInterval;
                }

                std::this_thread::sleep_for(interval);

                auto previousCpu = std::optional<ProcessCpuUsageSample> {};
                auto page_size = 0L;
                auto availableCpuCount = 0.0;
                {
                    auto &runtime = runtimeState();
                    std::scoped_lock lock{runtime.mutex};
                    if (runtime.stopProcessMetricsThread) {
                        return;
                    }
                    previousCpu = runtime.processCpuUsageSample;
                    page_size = runtime.pageSize;
                    availableCpuCount = runtime.availableCpuCount;
                }

                const auto current_cpu = readProcessCpuUsage();
                const auto current_memory_usage = readProcessMemoryUsageBytes(page_size);
                if (!current_cpu || !current_memory_usage) {
                    continue;
                }

                auto cpuUtilization = std::optional<double> {};
                if (previousCpu && availableCpuCount > 0.0) {
                    const auto elapsedSeconds =
                        std::chrono::duration<double> {current_cpu->timestamp - previousCpu->timestamp}.count();
                    if (elapsedSeconds > 0.0) {
                        const auto cpuSeconds =
                            (current_cpu->userSeconds + current_cpu->systemSeconds) -
                            (previousCpu->userSeconds + previousCpu->systemSeconds);
                        cpuUtilization = cpuSeconds / elapsedSeconds / availableCpuCount;
                    }
                }
                {
                    auto &runtime = runtimeState();
                    std::scoped_lock lock{runtime.mutex};
                    runtime.processCpuUsageSample = current_cpu;
                }
                nestdaq::OpenTelemetryInitializer::RecordFrameworkProcessUsage(current_cpu->userSeconds,
                        current_cpu->systemSeconds,
                        cpuUtilization,
                        *current_memory_usage);
            }
        }};
}

auto stopProcessMetricsThread() -> void
{
    auto &state = runtimeState();
    {
        std::scoped_lock lock{state.mutex};
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
    if (isEmpty(name)) {
        return setLastError("metric counter name is empty");
    }
    auto attrs = buildAttributes(attributes, attribute_count);
    auto &state = runtimeState();
    std::scoped_lock lock{state.mutex};
    if (!state.meter) {
        state.lastError.clear();
        return NESTDAQ_OTEL_OK;
    }
    auto key = MetricKey{.kind = MetricKind::DoubleCounter,
                         .name = name,
                         .unit = isEmpty(unit) ? "" : unit,
                         .description = isEmpty(description) ? "" : description};
    auto &counter = state.doubleCounters[key];
    if (!counter) {
        counter = state.meter->CreateDoubleCounter(key.name, key.description, key.unit);
    }
    auto view = opentelemetry::common::KeyValueIterableView<decltype(attrs.values)> {attrs.values};
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
    if (isEmpty(name)) {
        return setLastError("metric histogram name is empty");
    }
    auto attrs = buildAttributes(attributes, attribute_count);
    auto &state = runtimeState();
    std::scoped_lock lock{state.mutex};
    if (!state.meter) {
        state.lastError.clear();
        return NESTDAQ_OTEL_OK;
    }
    auto key = MetricKey{.kind = MetricKind::DoubleHistogram,
                         .name = name,
                         .unit = isEmpty(unit) ? "" : unit,
                         .description = isEmpty(description) ? "" : description};
    auto &histogram = state.doubleHistograms[key];
    if (!histogram) {
        histogram = state.meter->CreateDoubleHistogram(key.name, key.description, key.unit);
    }
    auto view = opentelemetry::common::KeyValueIterableView<decltype(attrs.values)> {attrs.values};
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
    if (isEmpty(name)) {
        return setLastError("metric gauge name is empty");
    }
    auto gauge_attributes = buildGaugeAttributes(attributes, attribute_count);
    auto key = MetricKey{.kind = MetricKind::DoubleGauge,
                         .name = name,
                         .unit = isEmpty(unit) ? "" : unit,
                         .description = isEmpty(description) ? "" : description};

    opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObservableInstrument> newGauge;
    MetricKey *callbackKey = nullptr;
    {
        auto &state = runtimeState();
        std::scoped_lock lock{state.mutex};
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
                                      .attributes = std::move(gauge_attributes)}] = value;
        state.lastError.clear();
    }
    if (newGauge) {
        newGauge->AddCallback(observeUserDoubleGauge, callbackKey);
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
            auto &state = otel_detail::runtimeState();
            std::scoped_lock reconfigureLock{state.frameworkReconfigureMutex};
            std::scoped_lock lock{state.mutex};
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
        static_cast<void>(otel_detail::flushFrameworkMetricsIfDirty(otel_detail::kDefaultMetricExportIntervalMs));
    } catch (...) {
    }
}

auto OpenTelemetryInitializer::RecordFrameworkProcessUsage(double cpu_user_seconds,
        double cpu_system_seconds,
        std::optional<double> cpu_utilization,
        double memory_usage_bytes) noexcept -> void
{
    try {
        {
            auto &state = otel_detail::runtimeState();
            std::scoped_lock reconfigureLock{state.frameworkReconfigureMutex};
            std::scoped_lock lock{state.mutex};
            if (!state.frameworkMeterProvider) {
                return;
            }
            state.pendingProcessUsageMeasurements.emplace_back(otel_detail::ProcessUsageMeasurement{
                .cpuUserSeconds = cpu_user_seconds,
                .cpuSystemSeconds = cpu_system_seconds,
                .cpuUtilization = cpu_utilization,
                .memoryUsageBytes = memory_usage_bytes,
            });
        }
        static_cast<void>(otel_detail::flushFrameworkMetricsIfDirty(otel_detail::kDefaultMetricExportIntervalMs));
    } catch (...) {
    }
}

auto OpenTelemetryInitializer::RecordFrameworkFairMQState(int64_t state_id, const char *state_name) noexcept -> void
{
    try {
        {
            auto &state = otel_detail::runtimeState();
            std::scoped_lock reconfigureLock{state.frameworkReconfigureMutex};
            std::scoped_lock lock{state.mutex};
            if (!state.frameworkMeterProvider) {
                return;
            }
            state.pendingFairMQStateMeasurements.emplace_back(otel_detail::FairMQStateMeasurement{
                .stateId = state_id,
                .stateName = otel_detail::isEmpty(state_name) ? "" : state_name,
            });
        }
        static_cast<void>(otel_detail::flushFrameworkMetricsIfDirty(otel_detail::kDefaultMetricExportIntervalMs));
    } catch (...) {
    }
}

} // namespace nestdaq
