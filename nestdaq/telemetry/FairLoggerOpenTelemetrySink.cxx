#include <thread>

#include <fairLogger/Logger.h>


#include <opentelemetry/logs/severity.h>
#include <opentelemetry/logs/provider.h>

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

static constexpr std::string_view kSinkKey{"otel-log-sink"};

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
    // =========== fair Logger AddCustomSink ==========
    fair::Logger::AddCustomSink(kSinkKey.data(), 
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

FairLoggerOpenTelemetrySink::~FairLoggerOpenTelemetrySink()
{
    fair::Logger::RemoveCustomSink(kSinkKey.data());
}

auto FairLoggerOpenTelemetrySink::Initialize() -> void
{
    static auto instance = FairLoggerOpenTelemetrySink();
}

} // namespace nestdaq