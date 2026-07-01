/** @file
 *  @brief Implements the sample message-producing NestDAQ device.
 */

#include <memory>
#include <sstream>
#include <string>

#include <nestdaq/runDevice.h>

#if defined(NESTDAQ_EXAMPLES_HAVE_SPDLOG_OTEL) && __has_include(<nestdaq/telemetry/SpdlogOpenTelemetrySink.h>) && __has_include(<spdlog/spdlog.h>)
#include <nestdaq/telemetry/SpdlogOpenTelemetrySink.h>
#include <spdlog/spdlog.h>
#define NESTDAQ_EXAMPLES_USE_SPDLOG_OTEL
#endif

#include "Sampler.h"

namespace bpo = boost::program_options;

//_____________________________________________________________________________
void addCustomOptions(bpo::options_description& options)
{
    options.add_options()
           ("out-chan-name", bpo::value<std::string>()->default_value("data"), "Name of output channel")
           ("text", bpo::value<std::string>()->default_value("Hello"), "Text to send out")
           ("max-iterations", bpo::value<std::string>()->default_value("0"), "Maximum number of iterations of Run/ConditionalRun/OnData (0 - infinite)");

}

//_____________________________________________________________________________
std::unique_ptr<fair::mq::Device> getDevice(const fair::mq::ProgOptions& /*config*/)
{
    return std::make_unique<Sampler>();
}

//_____________________________________________________________________________
void PrintConfig(const fair::mq::ProgOptions* config, std::string_view name, std::string_view funcname)
{
    const auto prefix = std::string{name};
    auto c = config->GetPropertiesAsStringStartingWith(prefix);
    std::ostringstream ss;
    ss << funcname << "\n\t " << name << "\n";
    for (const auto &[k, v] : c) {
        ss << "\t key = " << k << ", value = " << v << "\n";
    }
    LOG(debug) << ss.str();
}

//_____________________________________________________________________________
// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
Sampler::Sampler()
{
    LOG(debug) << "Sampler : hello";
}

//_____________________________________________________________________________
//Sampler::~Sampler()
//{
// unsubscribe to property change
//  fConfig->UnsubscribeAsString("Sampler");
//  LOG(debug) << "Sampler : bye";
//}

//_____________________________________________________________________________
void Sampler::Init()
{
#ifdef NESTDAQ_EXAMPLES_USE_SPDLOG_OTEL
    if (!fLogger) {
        fLogger = std::make_shared<spdlog::logger>(
                      "Sampler",
                      spdlog::sinks_init_list{nestdaq::telemetry::CreateSpdlogOpenTelemetrySink()});
    }
    fLogger->info("Sampler example spdlog OTel log");
#endif
    // subscribe to property change
//  fConfig->SubscribeAsString("Sampler", [](const std::string& key, std::string value){
//    LOG(debug) << "Sampler (subscribe) : key = " << key << ", value = " << value;
//  });
    PrintConfig(fConfig, "channel-config", __PRETTY_FUNCTION__);
    PrintConfig(fConfig, "chans.", __PRETTY_FUNCTION__);
}

//_____________________________________________________________________________
void Sampler::InitTask()
{
    PrintConfig(fConfig, "channel-config", __PRETTY_FUNCTION__);
    PrintConfig(fConfig, "chans.", __PRETTY_FUNCTION__);

    fId = fConfig->GetProperty<std::string>("id");
    fOutputChannelName = fConfig->GetProperty<std::string>("out-chan-name");
    fText = fConfig->GetProperty<std::string>("text");
    fMaxIterations = std::stoull(fConfig->GetProperty<std::string>("max-iterations"));

    fNumSubChannels = static_cast<int>(GetNumSubChannels(fOutputChannelName));

    // These instruments show the intended producer metrics: successful sends,
    // failed sends, payload size distribution, and run-loop progress.
    auto telemetry = nestdaq::telemetry::GetTelemetry();
    fMessagesSent = telemetry.Counter("examples.sampler.messages.sent", "{message}", "Messages sent by the Sampler example");
    fMessagesFailed = telemetry.Counter("examples.sampler.messages.failed", "{message}", "Messages the Sampler example failed to send");
    fMessageSize = telemetry.Histogram("examples.sampler.message.size", "By", "Sampler example message size");
    fIteration = telemetry.Gauge("examples.sampler.iteration", "1", "Sampler example iteration number");
}

//_____________________________________________________________________________
bool Sampler::ConditionalRun()
{
    for (auto iSubChannel = 0; iSubChannel < fNumSubChannels; ++iSubChannel) {
        auto text = new std::string(fId + "[" + std::to_string(iSubChannel) + "]:" + fText + " : " + std::to_string(fNumIterations));

        // copy
        auto txt = *text;

        fair::mq::MessagePtr msg(NewMessage(
                                     const_cast<char*>(text->data()),
                                     text->length(),
        [](void * /*data*/, void* object) {
            auto p = static_cast<std::string*>(object);
            //LOG(debug) << " sent " << *p;
            delete p; // NOLINT(cppcoreguidelines-owning-memory)
        },
        text
                                 )
                                );

        LOG(info) << "Sending \"" << txt << "\"";

        auto span = nestdaq::telemetry::GetTelemetry().StartSpan("sampler.send",
        {   {"fairmq.channel.name", fOutputChannelName},
            {"fairmq.channel.index", iSubChannel},
            {"message.size", text->length()}
        });

        if (Send(msg, fOutputChannelName, iSubChannel) < 0) {
            LOG(warn) << "failed to send. event:  " << fNumIterations << ", sub channel = " << iSubChannel;
            // Record failures with channel attributes so send-side drops can be
            // separated by FairMQ channel and subchannel.
            fMessagesFailed.Add(1, {{"fairmq.channel.name", fOutputChannelName},
                {"fairmq.channel.index", iSubChannel}
            });
            span.SetAttribute({"send.ok", false});
            return false;
        }
        // Record normal send metrics close to the send result to demonstrate
        // how user code attaches operational context to each measurement.
        fMessagesSent.Add(1, {{"fairmq.channel.name", fOutputChannelName},
            {"fairmq.channel.index", iSubChannel}
        });
        fMessageSize.Record(text->length(), {{"fairmq.channel.name", fOutputChannelName},
            {"fairmq.channel.index", iSubChannel}
        });
        span.SetAttribute({"send.ok", true});
    }

    ++fNumIterations;
    // The gauge captures the current producer iteration so exported metrics
    // can be correlated with the message stream generated by this example.
    fIteration.Record(fNumIterations);
    if (fMaxIterations > 0 && fNumIterations >= fMaxIterations) {
        LOG(info) << "Configured maximum number of iterations reached. Leaving RUNNING state. " << fNumIterations << " / " << fMaxIterations;
        return false;
    }
    LOG(info) << " processed events:  " << fNumIterations;
    return true;
}

//_____________________________________________________________________________
void Sampler::PostRun()
{
    LOG(debug) << __FUNCTION__;
    fNumIterations = 0;
}

//_____________________________________________________________________________
void Sampler::PreRun()
{
    LOG(debug) << __FUNCTION__;
}

//_____________________________________________________________________________
void Sampler::Run()
{
    LOG(debug) << __FUNCTION__;
}
