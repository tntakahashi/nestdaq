#pragma once

#include <chrono>

#include <string>
#include <string_view>


// Mapping fair::Severity 16 -> opentelemetry::logs::Severity 25
//
// fair::Severity
//   nolog     =  0
//   trace     =  1
//   debug4    =  2
//   debug3    =  3
//   debug2    =  4
//   debug1    =  5
//   debug     =  6
//   detail    =  7
//   info      =  8
//   state     =  9
//   warn      = 10
//   important = 11
//   alarm     = 12
//   error     = 13
//   critical  = 14
//   fatal     = 15
//
// opentelemetry::logs::Severity
//   kInvalid =  0
//   kTrace   =  1
//   kTrace2  =  2
//   kTrace3  =  3
//   kTrace4  =  4
//   kDebug   =  5
//   kDebug2  =  6
//   kDebug3  =  7
//   kDebug4  =  8
//   kInfo    =  9
//   kInfo2   = 10
//   kInfo3   = 11
//   kInfo4   = 12
//   kWarn    = 13
//   kWarn2   = 14
//   kWarn3   = 15
//   kWarn4   = 16
//   kError   = 17
//   kError2  = 18
//   kError3  = 19
//   kError4  = 20
//   kFatal   = 21
//   kFatal2  = 22
//   kFatal3  = 23
//   kFatal4  = 24

// spdlog level
//   trace
//   debug
//   info
//   warn
//   err (error)
//   critical (fatal)
//   off

namespace nestdaq {

class FairLoggerOpenTelemetrySink {
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


  FairLoggerOpenTelemetrySink();

  static auto AddProgramOptions() -> void;
  

private:

};

} // namespace nestdaq