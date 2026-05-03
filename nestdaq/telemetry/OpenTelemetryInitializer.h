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

typedef struct nestdaq_otel_config_v1 {
    uint32_t size;
    const char *protocol;            /* Comma-separated "console", "otlp-http", and/or "otlp-grpc". */
    const char *endpoint;            /* Optional compatibility endpoint for both OTLP exporters. */
    const char *headers;             /* Optional comma-separated key=value pairs. */
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
    uint32_t otlp_http_json;         /* Non-zero selects JSON for otlp-http. */
    const char *endpoint_http;       /* Optional OTLP HTTP logs endpoint. */
    const char *endpoint_grpc;       /* Optional OTLP gRPC logs endpoint. */
} nestdaq_otel_config_v1;

NESTDAQ_OTEL_EXPORT int nestdaq_otel_init_v1(const nestdaq_otel_config_v1 *config);
NESTDAQ_OTEL_EXPORT int nestdaq_otel_set_min_severity(int32_t severity);
NESTDAQ_OTEL_EXPORT int nestdaq_otel_force_flush(uint64_t timeout_ms);
NESTDAQ_OTEL_EXPORT int nestdaq_otel_shutdown(uint64_t timeout_ms);
NESTDAQ_OTEL_EXPORT const char *nestdaq_otel_last_error(void);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
namespace nestdaq {

class OpenTelemetryInitializer {
public:
    OpenTelemetryInitializer() = delete;

    static auto Initialize(const nestdaq_otel_config_v1 *config) -> int;
    static auto SetMinSeverity(int32_t severity) -> int;
    static auto ForceFlush(uint64_t timeout_ms) -> int;
    static auto Shutdown(uint64_t timeout_ms) -> int;
    static auto LastError() noexcept -> const char *;
};

} // namespace nestdaq
#endif
