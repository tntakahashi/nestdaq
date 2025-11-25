#pragma once

#include <string_view>

namespace nestdaq {

class OpenTelemetryInitializer {

public:
  struct OptionKey {
    static constexpr std::string_view otel_exporter_otlp_grpc_endpoint            {"otel_exporter_otlp_grpc_endpoint"};
    static constexpr std::string_view otel_exporter_otlp_grpc_ssl_enable          {"otel_exporter_otlp_grpc_ssl_enable"};
    static constexpr std::string_view otel_exporter_otlp_grpc_certificate         {"otel_exporter_otlp_grpc_certificate"}; 
    static constexpr std::string_view otel_exporter_otlp_grpc_certificate_string  {"otel_exporter_otlp_grpc_certificate_string"};
    static constexpr std::string_view otel_exporter_otlp_grpc_headers             {"otel_exporter_otlp_grpc_headers"};

    static constexpr std::string_view otel_exporter_otlp_timeout                  {"otel_exporter_otlp_timeout"};

    static constexpr std::string_view otel_exporter_otlp_http_endpoint            {"otel_exporter_otlp_http_endpoint"};
    static constexpr std::string_view otel_exporter_otlp_http_content_type        {"otel_exporter_otlp_http_content_type"};
    static constexpr std::string_view otel_exporter_otlp_http_headers             {"otel_exporter_otlp_http_headers"};

    static constexpr std::string_view otel_exporter_otlp_file_pattern_trace       {"otel_exporter_otlp_file_pattern_trace"};
    static constexpr std::string_view otel_exporter_otlp_file_pattern_metrics     {"otel_exporter_otlp_file_pattern_metrics"};
    static constexpr std::string_view otel_exporter_otlp_file_pattern_logs        {"otel_exporter_otlp_file_pattern_logs"};
    static constexpr std::string_view otel_exporter_otlp_file_alias_pattern_trace{"otel_exporter_otlp_file_alias_pattern_trace"};
    static constexpr std::string_view otel_exporter_otlp_file_alias_pattern_metris {"otel_exporter_otlp_file_alias_pattern_metrics"};
    static constexpr std::string_view otel_exporter_otlp_file_alias_pattern      {"otel_exporter_otlp_file_alias_pattern_logs"};
    static constexpr std::string_view olel_exporter_otlp_file_flush_interval     {"otel_exporter_otlp_file_flush_interval"};
    static constexpr std::string_view otel_exporter_otlp_file_flush_count        {"otel_exporter_otlp_file_flush_count"};
    static constexpr std::string_view otel_exporter_otlp_file_size               {"otel_exporter_otlp_file_size"};
    static constexpr std::string_view otel_exporter_otlp_file_rotate             {"otel_exporter_otlp_file_rotate"};
  };

  static auto AddProgramOptions() -> void;
  

};
  
} // namespace