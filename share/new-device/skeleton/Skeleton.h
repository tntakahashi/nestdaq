#ifndef Skeleton_h
#define Skeleton_h

#include <fairmq/Device.h>

class Skeleton : public fair::mq::Device
{
public:
    Skeleton() = default;
    ~Skeleton() override = default;

protected:
    auto Bind() -> void override;
    auto ConditionalRun() -> bool override;
    auto Connect() -> void override;
    auto Init() -> void override;
    auto InitTask() -> void override;
    auto PostRun() -> void override;
    auto PreRun() -> void override;
    auto Reset() -> void override;
    auto ResetTask() -> void override;
    auto Run() -> void override;
};

#endif