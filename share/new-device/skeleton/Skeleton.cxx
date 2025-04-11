#include <memory>

#include <fairmq/runDevice.h>

#include "Skeleton.h"

namespace po = boost::program_options;

auto addCustomOptions(po::options_description &options) -> void
{
    options.add_options()
    ("in-chan-name",  po::value<std::string>()->default_value("in"), "Name of the input channel")
    ("out-chan-name", po::value<std::string>()->default_value("out"), "Name of the output channel")
    ("dqm-chan-name", po::value<std::string>()->default_valeu("dqm"), "Name of the data quality monitoring channel")
    ("poll-timeout", po::value<std::string>()->default_value("1", "Timeout for polling (in msec)"))
    ;
}

auto getDevice(fair::mq::ProgOptions &) -> std::unique_ptr<fair::mq::Device>
{
    return std::make_unique<Skeleton>();
}

auto Skeleton::Bind() -> void
{
}

auto Skeleton::ConditionalRun() -> bool
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
    fInputChannelName  = fConfig->GetProperty<std::string>("in-chan-name");
    fOutputChannelName = fConfig->GetProperty<std::string>("out-chan-name");
    fDQMChannelName    = fConfig->GetProperty<std::string>("dqm-chan-name");
    fPollTimeoutMS     = std::stoi(fConfig->GetProperty<std::string>("poll-timeout"));
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

auto Skeleton::ResetTask() -> void
{
}

auto Skeleton::Run() -> void
{
}