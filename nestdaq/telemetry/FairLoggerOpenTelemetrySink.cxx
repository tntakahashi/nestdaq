#include <thread>

#include <fairLogger/Logger.h>

#include <opentelemetry/exporters/otlp/otlp_grpc_log_record_exporter_factory.h>
#include <opentelemetry/exporters/otlp/otlp_http_log_record_exporter_factory.h>

#include <opentelemetry/logs/severity.h>
#include <opentelemetry/logs/provider.h>
#include <opentelemetry/sdk/logs/exporter.h>
#include <opentelemetry/sdk/logs/logger_provider_factory.h>
#include <opentelemetry/sdk/logs/processor.h>
#include <opentelemetry/sdk/logs/provider.h>
#include <opentelemetry/sdk/logs/simple_log_record_processor_factory.h>

#include <openteneletry/sdk/version/version.h>
#include <opentelemetry/trace/semantic_conventions.h>
//#include <opentelemetry/version.h>


#include "nestdaq/version.h"
#include "nestdaq/telemetry/FairLoggerOpenTelemetrySink.h"


namespace nestdaq {

static constexpr std::string_view kLoggerName{"FairLogger"};
static constexpr std::string_view kLibraryName{"NestDAQ"};
static constexpr std::string_view kLibraryVersion{NESTDAQ_VERSION};
static constexpr std::string_view kSchemaUrl{""};

static auto ConverteSeverity(fair::Severity severity) -> opentelemetry::logs::Severity
{
  switch (severity) {
    case fair::Severity::fatal:
    case fair::Severity::critical:
      return opentelemetry::logs::kFatal;

    case fair::Severity::error:
      return opentelemetry::logs::kError;

    case fair::Severity::alarm
    case fair::Severity::important:
    case fair::Severity::warn:
      return opentelemetry::logs::kWarn;

    case fair::Severity::state:
    case fair::Severity::info:
      return opentelemetry::logs::kInfo;

    case fair::Severity::detail:
    case fair::Severity::debug:
    case fair::Severity::debug1:
    case fair::Severity::debug2:
    case fair::Severity::debug3:
    case fair::Severity::debug4:
      return opentelemetry::logs::kDebug;

    case fair::Severity::trace:
      return opentelemetry::logs::kTrace;
    
    case fair::Severity::nolog:
    default:
      return opentelemetry::logs::Severity::kInvalid;
  }
}

FairLoggerOpenTelemetrySink::FairLoggerOpenTelemetrySink()
{
    // otel setup
    opentelemetry::exporter::otlp::OtlpGrpcLogRecordExporterOpions grpcOptions;
    grpcOptions.endpoint            = collectorEndpoint.data(); // default port is 4317 
    grpcOptions.use_ssl_credentials = useSslCredentials;


   
    // =========== fair Logger AddCustomSink ==========
    fair::Logger::AddCustomSink("otel-log-sink", 
                                fair::Severity::trace, 
                                [this](const std::string& content, const fair::LogMetaData& metaData){
      auto provider  = opentelemetry::logs::Provider::GetLoggerProvider();
      auto logger    = provider->GetLogger(kLoggerName, kLibraryName, kLibraryVersion, kSchemaUrl);
      auto logRecord = logger->CreateLogRecord();

      if (logRecord) {
        logRecord->SetSeverity(ConvertSeverity(metaData.severity));
        logRecord->SetBody(opentelemetry::nostd::string_view(content.data(), content.size()));
        logRecord->SetTimestamp(std::chrono::system_clock::time_point(std::chrono::seconds(metaData.timestamp) + std::chrono::microseconds(metaData.us)));
        if (std::stoi(metaData.line.data())>0) {
          logRecord->SetAttribute(opentelemetry::trace::SemanticConventions::kCodeFilepath, metaData.file.data());
          logRecord->SetAttribute(opentelemetry::trace::SemanticConventions::kCodeLineon,   std::stoi(metaData.line.data()));
        }
        logRocord->SetAttribute(opentelemetry::trace::SemanticConventions::kThreadId, std::this_thread::get_id());
        logger->EmitLogRecord(std::move(logRecord));
      }
    });
}


auto FairLoggerOpenTelemetrySink::AddProgramOptions() -> void
{
    namespace bpo = boost::program_options;
    auto options = bpo::options_descriptin("FairLoggerOpenTelemetrySink");
    options.add_options()
      (opt::otel_exporter_otlp_grpc_endpoint.data(), 
       bpo::value<std::string>()->default_value("http://localhost:4317", 
       "OTLP GRPC endpoint to connect to"))
      (opt::otel_exporter_otlp_grpc_ssl_enable.data(),
       bpo::value<std::string>->default_value("false"), 
       "whether the endpoint is SSL enabled")
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
       "http headers");

}

} // namespace nestdaq