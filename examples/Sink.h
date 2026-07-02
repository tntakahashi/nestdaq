#pragma once

/**
 * @file Sink.h
 * @brief Example consumer device that receives single-part or multipart FairMQ messages.
 */

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>

#include <fairmq/Device.h>
#include <nestdaq/telemetry/Telemetry.h>

#if __has_include(<spdlog/spdlog.h>)
namespace spdlog {
class logger;
} // namespace spdlog
#endif

class Sink : public fair::mq::Device {
public:

    struct OptionKey {
        static constexpr const char* InputChannelName{"in"};
        static constexpr const char* Multipart{"multipart"};
    };

    Sink();
    Sink(const Sink&) = delete;
    Sink &operator=(const Sink&) = delete;
    Sink(Sink&&) = delete;
    Sink& operator=(Sink&&) = delete;
    ~Sink() override = default;

private:
    bool HandleData(fair::mq::MessagePtr &msg, int index);
    bool HandleMultipartData(fair::mq::Parts &msgParts, int index);
    void Init() override;
    void InitTask() override;
    void PostRun() override;

    std::string fInputChannelName;
    // Telemetry handles are kept as members to demonstrate receiver-side
    // metrics through the NestDAQ facade, without linking this example to the
    // OTel SDK.
    nestdaq::telemetry::Counter fMessagesReceived;
    nestdaq::telemetry::Histogram fMessageSize;
    nestdaq::telemetry::Gauge fMessagesTotal;
#if __has_include(<spdlog/spdlog.h>)
    std::shared_ptr<spdlog::logger> fLogger;
#endif
    uint64_t fNumMessages{0};

};
