#include <fairmq/ProgramOptions.h>

#include <opentelemetry/trace/semantic_conventions.h>

#include <opentelemetry/exporters/ostream/log_record_exporter_factory.h>
#include <opentelemetry/exporters/otlp/otlp_grpc_log_record_exporter_factory.h>
#include <opentelemetry/exporters/otlp/otlp_http_log_record_exporter_factory.h>

#include <opentelemetry/sdk/logs/exporter.h>
#include <opentelemetry/sdk/logs/logger_provider_factory.h>
#include <opentelemetry/sdk/logs/processor.h>
#include <opentelemetry/sdk/logs/provider.h>
#include <opentelemetry/sdk/logs/simple_log_record_processor_factory.h>
#include <openteneletry/sdk/version/version.h>


#include <opentelemetry/exporters/ostream/span_exporter_factory.h>
#include <opentelemetry/sdk/trace/exporter.h>
#include <opentelemetry/sdk/trace/processor.h>
#include <opentelemetry/sdk/trace/simple_processor_factory.h>
#include <opentelemetry/sdk/trace/tracer_provider_factory.h>
#include <opentelemetry/trace/provider.h>

#include "nestdaq/telemetry/OpenTelemetryInitializer.h"


namespace nestdaq {

using opt = OpenTelemetryInitializer::OptionKey;

auto GetGrpcExporterOptions(const fair::mq::ProgramOptions& config) -> opentelemetry::exporter::otlp::OtlpGrpcExporterOptions
{
    opentelemetry::exporter::otlp::OtlpGrpcExporterOptions opts;
    if (auto key = opt::otel_exporter_otlp_grpc_endpoint.data(); config.count(key)>0) {
      opts.endpoint = xconfig[key];
    }
    if (auto key = opt::otel_exporter_otlp_grpc_ssl_enable.data(); config.count(key)>0) {
      opts.use_ssl_credentials = config[key];
    }
    if (auto key = opt::otel_exporter_otlp_grpc_certificate.data(); config.count(key)>0) {
      opts.ssl_credentials = config[key];
    }
    if (auto key = opt::otel_exporter_otlp_grpc_certificate_string.data(); config.count(key)>0) {
      opts.ssl_credentials_as_string = config[key];
    }
    if (auto key = opt::otel_exporter_otlp_grpc_headers.data(); config.count(key)>0) {
      opts.metadata = config[key];
    }
    return opts;
}

auto GetHttpExporterOptions(const fair::mq::ProgramOptions& config) -> opentelemetry::exporter::otlp::HttpGrpcExporterOptions
{
    auto opts = GetHttpExportOptions(config);
    opentelemetry::exporter::otlp::OtlpHttpExporterOptions httpOpts;
    if (auto key = opt::otel_exporter_otlp_http_endpoint.data(); config.count(key)>0) {
      opts.url = config[key];
    }
    if (auto key = opt::otel_exporter_otlp_http_content_type.data(); config.count(key)>0) {
      opts.content_type = config[key];0
    }
    if (auto key = ope::otel_exporter_otlp_http_headers.dat(); fConfig.count(key)>0) {
      opts.headers = config[key];
    }

}


auto OpenTelemetryInitializer::AddProgramOptions() -> void
{
    namespace bpo = boost::program_options;
    auto options = bpo::options_descriptin("FairLoggerOpenTelemetrySink");
    options.add_options()
           (opt::otel_exporter_otlp_grpc_endpoint.data(),
            bpo::value<std::string>()->default_value("http://localhost:4317",
                    "OTLP GRPC endpoint to connect to"))
           (opt::otel_exporter_otlp_grpc_ssl_enable.data(),
            bpo::value<std::string>->default_value("false"),
            "Whether the endpoint is SSL enabled")
           (opt::otel_exporter_otlp_grpc_certificate.data(),
            bpo::value<std::string>(),
            "SSL Certificate file path")
           (opt::otel_exporter_otlp_grpc_certificate_string.data(),
            bpo::value<std::string>(),
            "SSL Certificate as in-memory string")
           (opt::otel_exporter_otlp_grpc_headers.data(),
            bpo::value<std::string>(),
            "Custom metadata for GRPC")

           (opt::otel_expoter_otlp_timeout.data(),
            bpo::value<std::string>()->default_value("10s"),
            "OTLP GRPC/HTTP deadtime")

           (opt::otel_exporter_otlp_http_endpoint.data(),
            bpo::value<std::string>()->default_value("http::localhost:4318"),
            "OTLP HTTP endpoint to connect to")
           (opt::otel_exporter_otlp_http_content_type.data(),
            bpo::value<std::string>()->default_value("application/json"),
            "Data format used -- JSON or Binary")
           (opt::otel_exporter_otlp_headers.data(),
            bpo::value<std::string>(),
            "http headers")

           (opt::otel_exporter_otlp_file_pattern_trace.data(),
            bpo::value<std::string>()->default_value("trace-%N.jsonl"),
            "File pattern to use (for trace)")
           (opt::otel_exporter_otlp_file_pattern_metrics.data(),
            bpo::value<std::string>()->default_value("metrics-%N.jsonl"),
            "File pattern to use (for metrics)")
           (opt::otel_exporter_otlp_file_pattern_logs.data(),
            bpo::value<std::string>()->default_value("logs-%N.jsonl")
            "File pattern to use (for logs)")
           (opt::otel_exporter_otlp_file_alias_pattern_trace.data(),
            bpo::value<std::string>()->default_value("trace-latest.jsonl"),
            "File which always point to the latest file (for trace)")
           (opt::otel_exporter_otlp_file_alias_pattern_metrics.data(),
            bpo::value<std::string>()->default_value("metrics-latest.jsonl"),
            "File which always point to the latest file (for metrics)")
           (opt::otel_exporter_otlp_file_alias_pattern_logs.data(),
            bpo::value<std::string>()->default_value("logs-latest.jsonl")
            "File which always point to the latest file (for logs)")
           (opt::otel_exporter_otlp_file_flush_interval.data(),
            bpo::value<std::string>()->default_value("30s"),
            "Interval to force flush ostream")
           (opt::otel_exporter_otlp_file_flush_count.data(),
            bpo::value<std::string>()->default_value("256"),
            "Force flush ostrem every flush_count records")
           (opt::otel_exporter_otlp_file_size.data(),
            bpo::value<std::string>()->default_value("20MB"),
            "File size to rotate log files")
           (opt::otel_exporter_otlp_rorate_size.data(),
            bpo::value<std::string>(),
            "Rotate count");

}

auto OpenTelemetryInitializer::Initialize(const fair::mq::ProgramOptions& config) -> void
{
    static auto instance = OpenTelemetryInitializer(config);
}

OpenTelemetryInitializer::OpenTelemetryInitializer(const fair::mq::ProgramOptions& config)
  : fConfig(config)
{
    SetupTraces();
    SetupLogs();
    SetupMetrics();

}

auto OpenTelemetryInitializer::SetupLogs() -> void
{
    // ===== ostream exporter =====
    std::unique_ptr<opentelemetry::sdk::logs::LogRecordExporter> ostreamExporter = std::make_unique<opentelemetry::exporter::OstreamSpanExporter>();

    // ===== GRPC exporter =====
    auto grpcOpts = GetGrpcExporterOptions(fConfig);
    std::unique_ptr<opentelemetry::sdk::logs::LogRecordExporter> grpcEporter = std::make_unique<opentelemetry::exporter::OtlpGrpcLogRecordExporter>(grpcOpts);

    // ===== HTTP exporter =====
    auto httpOpts = GetHttpExporterOptions(fConfig);
    std::unique_ptr<opentelemetry::sdk::logs::LogRecordExporter> httpExporter = std::make_unique<opentelemetry::exporter::OtlpHttpLogRecordExporter>(httpOps);
}

auto OpenTelemetryInitializer::SetupMetrics() -> void
{

}

auto OpenTelemetryInitializer::SetupTraces() -> void
{
    // ===== ostream exporter =====
    std::unique_ptr<opentelemetry::sdk::trace::SpanExporter> ostreamExporter = std::make_unique<opentelemetry::exporter::OstreamSpanExporter>();

    // ===== GRPC exporter =====
    auto grpcOpts = GetGrpcExporterOptions(fConfig);
    std::unique_ptr<opentelemetry::sdk::trace::SpanExporter> grpcEporter = std::make_unique<opentelemetry::exporter::OtlpGrpcSpanExporter>(grpcOpts);

    // ===== HTTP exporter =====
    auto httpOpts = GetHttpExporterOptions(fConfig);
    std::unique_ptr<opentelemetry::sdk::trace::SpanExporter> httpExporter = std::make_unique<opentelemetry::exporter::OtlpHttpSpanExporter>(httpOpts);

//  auto provider = opentelemetetry::trace::Provider::GetTraceProvider();
//  auto tracer   = provider->GetTracer();
}

} // namespace nestdaq