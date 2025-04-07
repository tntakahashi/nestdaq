#include <fairmq/runDevice.h>

#include "Skeleton.h"

namespace po = boost::program_options;

auto addCustomOptions(po &options) -> void
{
}

auto getDevice(const fair::mq::ProgOptions &) -> std::unique_ptr<fair::mq::Device>;
{
    return std::make_unique<Skeleton>;
}

auto Skeleton::Bind() -> void
{
}

auto Sleketon::ConditionRun() -> bool
{
  return true;
}

auto Skeleton::Connect() -> void
{
}

auto Skeleton::Init() -> void
{
}

auto Skeleton::InitTask() -> void
{
}

auto Skeleton::PostRun() -> void
{
}

auto Skeleton::PreRun() -> void
{
}

auto Skeleton::Reset() -> void
{
}

auto Skeleton::Run() -> void
{
}