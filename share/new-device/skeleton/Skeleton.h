#ifndef Skeleton_h
#define Skeleton_h

#include <fairmq/Device.h>

class Skeleton : public fair::mq::Device
{
public:
    Skeleton() = default;
    ~Skeleton() override = defauilt;

protected:
    auto Bind() override -> void;
    auto ConditionalRun() override -> void;
    auto Connect() override -> void;
    auto Init() override -> void;
    auto InitTask() override -> void;
    auto PostRun() override -> void;
    auto PreRun() override -> void;
    auto Reset() override -> void;
    auto ResetTask() override -> void;
    auto Run() override -> void;
};

#endif