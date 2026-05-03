#ifndef Examples_Sampler_h
#define Exapmles_Sampler_h

#include <cstdint>
#include <string>

#include <fairmq/Device.h>

class Sampler : public fair::mq::Device
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
