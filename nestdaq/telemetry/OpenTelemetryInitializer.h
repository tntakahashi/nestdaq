#pragma once

#include <stdint.h>

#if defined(_WIN32)
#  define NESTDAQ_OTEL_EXPORT __declspec(dllexport)
#else
#  define NESTDAQ_OTEL_EXPORT __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
static constexpr int NESTDAQ_OTEL_OK = 0;
static constexpr int NESTDAQ_OTEL_ERROR = -1;
#else
enum {
    NESTDAQ_OTEL_OK = 0,
    NESTDAQ_OTEL_ERROR = -1
};
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Value kind used by @ref nestdaq_otel_attribute.
 *
 * The C ABI exposes a small union-like attribute representation so that NestDAQ
 * executables do not include or link OpenTelemetry C++ headers.
 */
typedef enum nestdaq_otel_attribute_type {
    NESTDAQ_OTEL_ATTRIBUTE_STRING = 0,
    NESTDAQ_OTEL_ATTRIBUTE_INT64 = 1,
    NESTDAQ_OTEL_ATTRIBUTE_UINT64 = 2,
    NESTDAQ_OTEL_ATTRIBUTE_DOUBLE = 3,
    NESTDAQ_OTEL_ATTRIBUTE_BOOL = 4
} nestdaq_otel_attribute_type;

/**
 * @brief Key/value attribute passed to metrics and spans.
 *
 * @p key must be non-null and non-empty. The field selected by @p type is used
 * as the value; all other value fields are ignored. Invalid attributes are
 * ignored when building attribute sets for telemetry records.
 */
typedef struct nestdaq_otel_attribute {
    const char *key;
    nestdaq_otel_attribute_type type;
    const char *string_value;
    int64_t int_value;
    uint64_t uint_value;
    double double_value;
    uint32_t bool_value;
} nestdaq_otel_attribute;

/**
 * @brief Exporter configuration for one OpenTelemetry signal.
 *
 * @p protocol is a comma-separated list of `console`, `otlp-http`, and
 * `otlp-grpc`. An empty protocol disables the signal. The HTTP and gRPC
 * endpoints are signal-specific; @p headers uses comma-separated `key=value`
 * pairs. @p otlp_http_json selects JSON OTLP/HTTP when non-zero and binary
 * protobuf otherwise.
 */
typedef struct nestdaq_otel_signal_config {
    const char *protocol;      /* Comma-separated "console", "otlp-http", and/or "otlp-grpc"; empty disables the signal. */
    const char *endpoint_http; /* Optional OTLP HTTP endpoint for this signal. */
    const char *endpoint_grpc; /* Optional OTLP gRPC endpoint for this signal. */
    const char *headers;       /* Optional comma-separated key=value pairs. */
    uint32_t otlp_http_json;   /* Non-zero selects JSON for otlp-http. */
} nestdaq_otel_signal_config;

/**
 * @brief Process-wide telemetry configuration consumed by `libnestdaq_otel.so`.
 *
 * Callers must set @p size to `sizeof(nestdaq_otel_config)`. String pointers
 * are borrowed for the duration of @ref nestdaq_otel_init only. Logs, metrics,
 * and traces are configured independently but share OpenTelemetry resource
 * attributes. @p min_severity is the FairLogger severity threshold numeric
 * value. A zero timeout means the OpenTelemetry SDK default/no-limit timeout.
 */
typedef struct nestdaq_otel_config {
    uint32_t size;
    nestdaq_otel_signal_config logs;
    nestdaq_otel_signal_config metrics;
    nestdaq_otel_signal_config traces;
    const char *service_name;        /* OpenTelemetry service.name. */
    const char *service_namespace;   /* OpenTelemetry service.namespace. */
    const char *service_instance_id; /* OpenTelemetry service.instance.id. */
    const char *fairmq_id;           /* FairMQ device id. */
    const char *fairmq_device;       /* FairMQ device/class name. */
    const char *fairmq_session;      /* FairMQ session id/name. */
    const char *fairmq_transport;    /* FairMQ transport. */
    const char *fairmq_git_version;  /* FAIRMQ_GIT_VERSION. */
    const char *fairmq_build_type;   /* FAIRMQ_BUILD_TYPE. */
    const char *fairmq_repo_url;     /* FAIRMQ_REPO_URL. */
    const char *fairmq_license;      /* FAIRMQ_LICENSE. */
    const char *fairmq_copyright;    /* FAIRMQ_COPYRIGHT. */
    int32_t min_severity;            /* fair::Severity numeric value. */
    uint32_t timeout_ms;             /* Optional exporter force-flush/shutdown timeout. */
    uint32_t metric_export_interval_ms;
} nestdaq_otel_config;

/**
 * @brief Force-flush all initialized telemetry providers.
 * @return `NESTDAQ_OTEL_OK` on success, otherwise `NESTDAQ_OTEL_ERROR`.
 */
NESTDAQ_OTEL_EXPORT int nestdaq_otel_force_flush(uint64_t timeout_ms);
NESTDAQ_OTEL_EXPORT void nestdaq_otel_framework_record_fairmq_state(int64_t state_id,
                                                                    const char *state_name);
/**
 * @brief Initialize process-wide OpenTelemetry providers and the FairLogger sink.
 *
 * Reinitialization shuts down the previous providers before installing the new
 * providers. Passing null uses defaults.
 *
 * @return `NESTDAQ_OTEL_OK` on success, otherwise `NESTDAQ_OTEL_ERROR`.
 */
NESTDAQ_OTEL_EXPORT int nestdaq_otel_init(const nestdaq_otel_config *config);
/**
 * @brief Return the last error message from the telemetry plugin.
 *
 * The returned pointer is owned by the plugin and remains valid until the next
 * telemetry API call that changes the last-error storage.
 */
NESTDAQ_OTEL_EXPORT const char *nestdaq_otel_last_error(void);
/**
 * @brief Add a value to a double counter instrument.
 *
 * If metrics are disabled this is a successful no-op. @p name must be non-empty
 * when metrics are enabled. Instrument identity is `(name, unit, description)`.
 */
NESTDAQ_OTEL_EXPORT int nestdaq_otel_metric_add_double_counter(const char *name,
                                                               double value,
                                                               const char *unit,
                                                               const char *description,
                                                               const nestdaq_otel_attribute *attributes,
                                                               uint64_t attribute_count);
/**
 * @brief Record a value in a double histogram instrument.
 *
 * If metrics are disabled this is a successful no-op. @p name must be non-empty
 * when metrics are enabled. Instrument identity is `(name, unit, description)`.
 */
NESTDAQ_OTEL_EXPORT int nestdaq_otel_metric_record_double_histogram(const char *name,
                                                                    double value,
                                                                    const char *unit,
                                                                    const char *description,
                                                                    const nestdaq_otel_attribute *attributes,
                                                                    uint64_t attribute_count);
/**
 * @brief Record the latest value for a double observable gauge instrument.
 *
 * If metrics are disabled this is a successful no-op. @p name must be non-empty
 * when metrics are enabled. Instrument identity is `(name, unit, description)`.
 */
NESTDAQ_OTEL_EXPORT int nestdaq_otel_metric_record_double_gauge(const char *name,
                                                                double value,
                                                                const char *unit,
                                                                const char *description,
                                                                const nestdaq_otel_attribute *attributes,
                                                                uint64_t attribute_count);
/**
 * @brief Update the FairLogger severity threshold exported to OpenTelemetry logs.
 */
NESTDAQ_OTEL_EXPORT int nestdaq_otel_set_min_severity(int32_t severity);
/**
 * @brief Update the NestDAQ FairMQ device instance id attached to log records.
 */
NESTDAQ_OTEL_EXPORT int nestdaq_otel_set_nestdaq_instance_id(const char *instance_id);
/**
 * @brief Flush, shut down, and uninstall all telemetry providers.
 */
NESTDAQ_OTEL_EXPORT int nestdaq_otel_shutdown(uint64_t timeout_ms);
/**
 * @brief End and release a span handle returned by @ref nestdaq_otel_span_start.
 */
NESTDAQ_OTEL_EXPORT int nestdaq_otel_span_end(uint64_t span_handle);
/**
 * @brief Set an attribute on an active span handle.
 */
NESTDAQ_OTEL_EXPORT int nestdaq_otel_span_set_attribute(uint64_t span_handle,
                                                        const nestdaq_otel_attribute *attribute);
/**
 * @brief Start a span and return an opaque handle.
 *
 * Returns 0 when traces are disabled or span creation fails. Non-zero handles
 * must be ended exactly once with @ref nestdaq_otel_span_end.
 */
NESTDAQ_OTEL_EXPORT uint64_t nestdaq_otel_span_start(const char *name,
                                                     const nestdaq_otel_attribute *attributes,
                                                     uint64_t attribute_count);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
namespace nestdaq {
namespace telemetry {
struct FairMQThroughputSample;
}

/**
 * @brief C++ implementation facade for the `libnestdaq_otel.so` C ABI.
 *
 * NestDAQ executables normally use `nestdaq::telemetry::TelemetryLibrary`
 * instead of this class so they can remain OpenTelemetry-unlinked. This class is
 * compiled into the telemetry plugin itself.
 */
class OpenTelemetryInitializer {
public:
    OpenTelemetryInitializer() = delete;

    static auto ForceFlush(uint64_t timeout_ms) -> int;
    static auto Initialize(const nestdaq_otel_config *config) -> int;
    static auto LastError() noexcept -> const char *;
    static auto MetricAddDoubleCounter(const char *name,
                                       double value,
                                       const char *unit,
                                       const char *description,
                                       const nestdaq_otel_attribute *attributes,
                                       uint64_t attribute_count) -> int;
    static auto MetricRecordDoubleHistogram(const char *name,
                                            double value,
                                            const char *unit,
                                            const char *description,
                                            const nestdaq_otel_attribute *attributes,
                                            uint64_t attribute_count) -> int;
    static auto MetricRecordDoubleGauge(const char *name,
                                        double value,
                                        const char *unit,
                                        const char *description,
                                        const nestdaq_otel_attribute *attributes,
                                        uint64_t attribute_count) -> int;
    static auto FlushFrameworkMetricsIfDirty(uint64_t timeout_ms) -> int;
    static auto RecordFrameworkFairMQState(int64_t state_id, const char *state_name) noexcept -> void;
    static auto RecordFrameworkFairMQThroughput(const telemetry::FairMQThroughputSample &sample) noexcept -> void;
    static auto RecordFrameworkProcessUsage(double cpu_usage_percent, double memory_rss_mib) noexcept -> void;
    static auto RecordFairMQThroughput(const telemetry::FairMQThroughputSample &sample) noexcept -> void;
    static auto SetNestdaqInstanceId(const char *instance_id) -> int;
    static auto SetMinSeverity(int32_t severity) -> int;
    static auto Shutdown(uint64_t timeout_ms) -> int;
    static auto SpanEnd(uint64_t span_handle) -> int;
    static auto SpanSetAttribute(uint64_t span_handle, const nestdaq_otel_attribute *attribute) -> int;
    static auto SpanStart(const char *name,
                          const nestdaq_otel_attribute *attributes,
                          uint64_t attribute_count) -> uint64_t;
};

} // namespace nestdaq
#endif
