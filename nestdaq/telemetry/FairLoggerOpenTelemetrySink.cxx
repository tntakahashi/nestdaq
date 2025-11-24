#include <fairLogger/Logger.h>

#include <opentelemetry/exporters/otlp/otlp_grpc_log_record_exporter_factory.h>
#include <opentelemetry/exporters/otlp/otlp_http_log_record_exporter_factory.h>

#include <opentelemetry/logs/provider.h>
#include <opentelemetry/sdk/logs/simple_log_record_processor_factory.h>

#include <openteneletry/sdk/version/version.h>
#include <opentelemetry/trace/semantic_conventions.h>
#include <opentelemetry/version.h>
#include <opentelemetry/logs/severity.h>



#include "nestdaq/telemetry/FairLoggerOpenTelemetrySink.h"


namespace nestdaq {

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

FairLoggerOpenTelemetrySink::FairLoggerOpenTelemetrySink(std::string_view collectorEndpoint, bool useSslCredentials, bool consoleDebug)
  : fCollectorEndpoint(collectorEndpoint.data())
  , fUseSslCredentials(useSslCredentials)
  , fConsoleDebug(fConsoleDebug)
{
    // otel setup


   
    // =========== fair Logger AddCustomSink ==========

    fair::Logger::AddCustomSink("otel-log-sink-nolog", fair::Severity::nolog, []());

    fair::Logger::AddCustomSink("otel-log-sink-trace", fair::Severity::trace, []());

    fair::Logger::AddCustomSink("otel-log-sink-debug4", fair::Severity::debug4, []());
    fair::Logger::AddCustomSink("otel-log-sink-debug3", fair::Severity::debug3, []());
    fair::Logger::AddCustomSink("otel-log-sink-debug2", fair::Severity::debug2, []());
    fair::Logger::AddCustomSink("otel-log-sink-debug1", fair::Severity::debug1, []());
    fair::Logger::AddCustomSink("otel-log-sink-debug0", fair::Severity::debug,  []());
    fair::Logger::AddCustomSink("otel-log-sink-detail", fair::Severity::detail, []());

    fair::Logger::AddCustomSink("otel-log-sink-info",  fair::Severity::info,  []());
    fair::Logger::AddCustomSink("otel-log-sink-state", fair::Severity::state, []());

    fair::Logger::AddCustomSink("otel-log-sink-warn",      fair::Severity::warn,      []());
    fair::Logger::AddCustomSink("otel-log-sink-important", fair::Severity::important, []());
    fair::Logger::AddCustomSink("otel-log-sink-alerm",     fair::Severity::alerm,     []());

    fair::Logger::AddCustomSink("otel-log-sink-error",    fair::Severity::error, []());

    fair::Logger::AddCustomSink("otel-log-sink-critical", fair::Severity::critial, []());
    fair::Logger::AddCustomSink("otel-log-sink-fatal",    fair::Severity::fatal, []());

}

} // namespace nestdaq