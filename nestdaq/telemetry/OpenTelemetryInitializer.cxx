#include "nestdaq/telemetry/OpenTelemetryInitializer.h"


namespace nestdaq {

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

auto OpenTelemetryInitializer::Initialize() -> void
{
  static auto instance = OpenTelemetryInitializer();
}

auto OpenTelemetryInitializer::OpenTelemetryInitializer()
{

}

} // namespace nestdaq