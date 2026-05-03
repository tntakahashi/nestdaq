#ifndef Examples_NullDevice_h
#define Examples_NullDevice_h

#include <fairmq/Device.h>

class NullDevice : public fair::mq::Device
{
public:
    NullDevice() = default;
    NullDevice(const NullDevice&) = delete;
    NullDevice& operator=(const NullDevice&) = delete;
    NullDevice(NullDevice&&) = delete;
    NullDevice& operator=(NullDevice&&) = delete;
    ~NullDevice() override = default;

protected:
    void Bind() override;
    bool ConditionalRun() override;
    void Connect() override;
    void Init() override;
    void InitTask() override;
    void PostRun() override;
    void PreRun() override;
    void Reset() override;
    void ResetTask() override;
    void Run() override;

};

#endif
