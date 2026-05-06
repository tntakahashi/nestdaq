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

typedef enum nestdaq_otel_attribute_type {
    NESTDAQ_OTEL_ATTRIBUTE_STRING = 0,
    NESTDAQ_OTEL_ATTRIBUTE_INT64 = 1,
    NESTDAQ_OTEL_ATTRIBUTE_UINT64 = 2,
    NESTDAQ_OTEL_ATTRIBUTE_DOUBLE = 3,
    NESTDAQ_OTEL_ATTRIBUTE_BOOL = 4
} nestdaq_otel_attribute_type;

typedef struct nestdaq_otel_attribute {
    const char *key;
    nestdaq_otel_attribute_type type;
    const char *string_value;
    int64_t int_value;
    uint64_t uint_value;
    double double_value;
    uint32_t bool_value;
} nestdaq_otel_attribute;

typedef struct nestdaq_otel_signal_config {
    const char *protocol;      /* Comma-separated "console", "otlp-http", and/or "otlp-grpc"; empty disables the signal. */
    const char *endpoint_http; /* Optional OTLP HTTP endpoint for this signal. */
    const char *endpoint_grpc; /* Optional OTLP gRPC endpoint for this signal. */
    const char *headers;       /* Optional comma-separated key=value pairs. */
    uint32_t otlp_http_json;   /* Non-zero selects JSON for otlp-http. */
} nestdaq_otel_signal_config;

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
    int32_t min_severity;            /* fair::Severity numeric value, 0..15. */
    uint32_t timeout_ms;             /* Optional exporter force-flush/shutdown timeout. */
    uint32_t metric_export_interval_ms;
} nestdaq_otel_config;

NESTDAQ_OTEL_EXPORT int nestdaq_otel_force_flush(uint64_t timeout_ms);
NESTDAQ_OTEL_EXPORT int nestdaq_otel_init(const nestdaq_otel_config *config);
NESTDAQ_OTEL_EXPORT const char *nestdaq_otel_last_error(void);
NESTDAQ_OTEL_EXPORT int nestdaq_otel_metric_add_double_counter(const char *name,
                                                               double value,
                                                               const char *unit,
                                                               const char *description,
                                                               const nestdaq_otel_attribute *attributes,
                                                               uint64_t attribute_count);
NESTDAQ_OTEL_EXPORT int nestdaq_otel_metric_record_double_histogram(const char *name,
                                                                    double value,
                                                                    const char *unit,
                                                                    const char *description,
                                                                    const nestdaq_otel_attribute *attributes,
                                                                    uint64_t attribute_count);
NESTDAQ_OTEL_EXPORT int nestdaq_otel_set_min_severity(int32_t severity);
NESTDAQ_OTEL_EXPORT int nestdaq_otel_shutdown(uint64_t timeout_ms);
NESTDAQ_OTEL_EXPORT int nestdaq_otel_span_end(uint64_t span_handle);
NESTDAQ_OTEL_EXPORT int nestdaq_otel_span_set_attribute(uint64_t span_handle,
                                                        const nestdaq_otel_attribute *attribute);
NESTDAQ_OTEL_EXPORT uint64_t nestdaq_otel_span_start(const char *name,
                                                     const nestdaq_otel_attribute *attributes,
                                                     uint64_t attribute_count);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
namespace nestdaq {

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
