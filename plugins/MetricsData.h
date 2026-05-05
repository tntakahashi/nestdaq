#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace daq::service {

static constexpr std::string_view MetricsPrefix{"metrics"};
static constexpr std::string_view StatePrefix{"state"};
static constexpr std::string_view StateIdPrefix{"state-id"};
static constexpr std::string_view CpuStatPrefix{"cpu-stat"};
static constexpr std::string_view RamStatPrefix{"ram-stat"};

static constexpr std::string_view MessageInPrefix{"msg-in"};
static constexpr std::string_view BytesInPrefix{"mb-in"};
static constexpr std::string_view MessageOutPrefix{"msg-out"};
static constexpr std::string_view BytesOutPrefix{"mb-out"};

static constexpr std::string_view NumMessagePrefix{"num-msg"};
static constexpr std::string_view BytesPrefix{"mb"};
static constexpr std::string_view NumMessageSumPrefix{"num-msg-sum"};
static constexpr std::string_view BytesSumPrefix{"mb-sum"};

static constexpr std::string_view CreatedTimePrefix{"created-time"};
static constexpr std::string_view LastUpdatePrefix{"last-update"};
static constexpr std::string_view LastUpdateNSPrefix{"last-update-ns"};

static constexpr std::string_view HostnamePrefix{"hostname"};
static constexpr std::string_view HostIpAddressPrefix{"host-ip"};

// labels for time series data
static constexpr std::string_view DataType{"data"};
static constexpr std::string_view SocketName{"name"};
static constexpr std::string_view SocketType{"socket"};
static constexpr std::string_view SocketTransport{"transport"};
static constexpr std::string_view SocketMethod{"method"};

struct ProcStat_t {
    uint64_t user{0};
    uint64_t nice{0};
    uint64_t system{0};
    uint64_t idle{0};
    auto sum() const -> uint64_t {
        return user + nice + system + idle;
    }
};

struct ProcSelfStat_t {
    uint64_t utime{0};
    uint64_t stime{0};
    uint64_t vsize{0};
    uint64_t rss{0};
    auto sum() const -> uint64_t {
        return utime + stime;
    }
};

struct SocketMetrics {
    double msgIn{0};
    double msgOut{0};
    double bytesIn{0};
    double bytesOut{0};
};

struct ProcessStatKey {
    std::string cpu;
    std::string ram;
    std::string stateId;
};

struct SocketMetricsKey {
    std::string msgIn;
    std::string msgOut;
    std::string bytesIn;
    std::string bytesOut;
};

inline auto MakeSocketMetricField(const std::string &instance,
                                  const std::string &channel,
                                  int index,
                                  std::string_view direction,
                                  std::string_view separator) -> std::string
{
    return instance + std::string{separator}
           + channel + "[" + std::to_string(index) + "]." + std::string{direction};
}

} // namespace daq::service
