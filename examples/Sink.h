#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>

#include <fairmq/Device.h>

class Sink : public fair::mq::Device {
public:

    struct OptionKey {
        static constexpr const char* InputChannelName{"in"};
        static constexpr const char* Multipart{"multipart"};
    };

    Sink() = default;
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
    uint64_t fNumMessages{0};

};
