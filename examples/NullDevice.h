#pragma once

/**
 * @file NullDevice.h
 * @brief Example FairMQ device with lifecycle hooks and no data processing.
 */

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
