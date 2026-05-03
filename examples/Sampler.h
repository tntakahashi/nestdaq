#ifndef Examples_Sampler_h
#define Exapmles_Sampler_h

#include <cstdint>
#include <string>

#if __has_include(<fairmq/Device.h>)
#include <fairmq/Device.h>  // since v1.4.34
#else
#include <fairmq/FairMQDevice.h>
#endif

class Sampler : public FairMQDevice
{
public:
    Sampler();
    Sampler(const Sampler&) = delete;
    Sampler& operator=(const Sampler&) = delete;
    Sampler(Sampler&&) = delete;
    Sampler& operator=(Sampler&&) = delete;
    ~Sampler() override = default;

private:
    std::string fId;
    std::string fOutputChannelName;
    std::string fText;
    uint64_t fMaxIterations{0};
    uint64_t fNumIterations{0};
    int fNumSubChannels{0};

    void Init() override;
    void InitTask() override;
    bool ConditionalRun() override;
    void PostRun() override;
    void PreRun() override;
    void Run() override;

};

#endif
