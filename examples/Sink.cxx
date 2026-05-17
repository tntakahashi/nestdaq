/** @file
 *  @brief Implements the sample message-consuming NestDAQ device.
 */

#include <chrono>
#include <memory>
#include <thread>

#include <nestdaq/runDevice.h>

#include "Sink.h"

static constexpr std::string_view MyClass{"Sink"};
static constexpr int kMaxDrainRetries{10};
static constexpr std::chrono::milliseconds kDrainRetryInterval{200};

namespace bpo = boost::program_options;

//_____________________________________________________________________________
// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
Sink::Sink()
{
}

//_____________________________________________________________________________
void addCustomOptions(bpo::options_description &options)
{
    using opt = Sink::OptionKey;
    options.add_options()
           (opt::InputChannelName, bpo::value<std::string>()->default_value(opt::InputChannelName), "Name of input channel\n")
           //
           (opt::Multipart, bpo::value<std::string>()->default_value("true"), "Handle multipart message\n");
}

//_____________________________________________________________________________
std::unique_ptr<fair::mq::Device> getDevice(const fair::mq::ProgOptions& /*config*/)
{
    return std::make_unique<Sink>();
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
bool Sink::HandleData(fair::mq::MessagePtr &msg, int index)
{
    auto span = nestdaq::telemetry::GetTelemetry().StartSpan("sink.receive",
        {{"fairmq.channel.name", fInputChannelName},
         {"fairmq.channel.index", index},
         {"message.size", msg->GetSize()},
         {"message.multipart", false}});
    static_cast<void>(span);
    const auto ptr = static_cast<char*>(msg->GetData());
    std::string s(ptr, msg->GetSize());
    LOG(debug) << __FUNCTION__ << " received = " << s << " [" << index << "] " << fNumMessages;
    fMessagesReceived.Add(1, {{"fairmq.channel.name", fInputChannelName},
                              {"fairmq.channel.index", index},
                              {"message.multipart", false}});
    // These receiver metrics demonstrate counting accepted messages, observing
    // payload sizes, and tracking the current total for a single-part stream.
    fMessageSize.Record(msg->GetSize(), {{"fairmq.channel.name", fInputChannelName},
                                         {"fairmq.channel.index", index},
                                         {"message.multipart", false}});
    ++fNumMessages;
    fMessagesTotal.Record(fNumMessages, {{"fairmq.channel.name", fInputChannelName}});
    return true;
}

//_____________________________________________________________________________
bool Sink::HandleMultipartData(fair::mq::Parts &msgParts, int index)
{
    auto multipartSpan = nestdaq::telemetry::GetTelemetry().StartSpan("sink.receive.multipart",
        {{"fairmq.channel.name", fInputChannelName},
         {"fairmq.channel.index", index},
         {"message.multipart", true},
         {"message.parts", msgParts.Size()}});
    static_cast<void>(multipartSpan);
    for (const auto& msg : msgParts) {
        auto partSpan = nestdaq::telemetry::GetTelemetry().StartSpan("sink.receive.part",
            {{"fairmq.channel.name", fInputChannelName},
             {"fairmq.channel.index", index},
             {"message.size", msg->GetSize()},
             {"message.multipart", true}});
        static_cast<void>(partSpan);
        const auto ptr = static_cast<char*>(msg->GetData());
        std::string s(ptr, msg->GetSize());
        LOG(debug) << __FUNCTION__ << " received = " << s << " [" << index << "] " << fNumMessages;
        LOG(debug) << s;
        fMessagesReceived.Add(1, {{"fairmq.channel.name", fInputChannelName},
                                  {"fairmq.channel.index", index},
                                  {"message.multipart", true}});
        // The multipart path uses the same metric names with attributes that
        // distinguish multipart traffic from single-part traffic.
        fMessageSize.Record(msg->GetSize(), {{"fairmq.channel.name", fInputChannelName},
                                             {"fairmq.channel.index", index},
                                             {"message.multipart", true}});
        ++fNumMessages;
        fMessagesTotal.Record(fNumMessages, {{"fairmq.channel.name", fInputChannelName}});
    }
    return true;
}

//_____________________________________________________________________________
void Sink::Init()
{
    PrintConfig(fConfig, "channel-config", __PRETTY_FUNCTION__);
    PrintConfig(fConfig, "chans.", __PRETTY_FUNCTION__);

    fNumMessages = 0;
}

//_____________________________________________________________________________
void Sink::InitTask()
{
    PrintConfig(fConfig, "channel-config", __PRETTY_FUNCTION__);
    PrintConfig(fConfig, "chans.", __PRETTY_FUNCTION__);

    LOG(debug) << MyClass << " InitTask";
    using opt = OptionKey;

    fInputChannelName = fConfig->GetProperty<std::string>(opt::InputChannelName);
    LOG(debug) << " input channel = " << fInputChannelName;

    // These instruments show the intended consumer metrics: received message
    // count, payload size distribution, and current total received messages.
    auto telemetry = nestdaq::telemetry::GetTelemetry();
    fMessagesReceived = telemetry.Counter("examples.sink.messages.received", "{message}", "Messages received by the Sink example");
    fMessageSize = telemetry.Histogram("examples.sink.message.size", "By", "Sink example message size");
    fMessagesTotal = telemetry.Gauge("examples.sink.messages.total", "{message}", "Total messages received by the Sink example");

    const auto &isMultipart = fConfig->GetProperty<std::string>(opt::Multipart);
    if (isMultipart=="true" || isMultipart=="1") {
        LOG(warn) << " set multipart data handler";
        OnData(fInputChannelName, &Sink::HandleMultipartData);
    } else {
        LOG(warn) << " set data handler";
        OnData(fInputChannelName, &Sink::HandleData);
    }

}

//_____________________________________________________________________________
void Sink::PostRun()
{
    using opt = OptionKey;
    LOG(debug) << __func__;
    int nrecv=0;
    while (true) {
        const auto &isMultipart = fConfig->GetProperty<std::string>(opt::Multipart);
        if (isMultipart=="true" || isMultipart=="1") {
            fair::mq::Parts parts;
            if (Receive(parts, fInputChannelName) <= 0) {
                LOG(debug) << __func__ << " no data received " << nrecv;
                ++nrecv;
                if (nrecv > kMaxDrainRetries) {
                    break;
                }
                std::this_thread::sleep_for(kDrainRetryInterval);
            } else {
                LOG(debug) << __func__ << " print data";
                HandleMultipartData(parts, 0);
            }
        } else {
            fair::mq::MessagePtr msg(NewMessage());
            if (Receive(msg, fInputChannelName) <= 0) {
                LOG(debug) << __func__ << " no data received " << nrecv;
                ++nrecv;
                if (nrecv > kMaxDrainRetries) {
                    break;
                }
                std::this_thread::sleep_for(kDrainRetryInterval);
            } else {
                LOG(debug) << __func__ << " print data";
                HandleData(msg, 0);
            }
        }
        LOG(debug) << __func__ << " done";
    }
}
