#pragma once

/**
 * @file MetricsPlugin.h
 * @brief FairMQ plugin that publishes device and channel metrics to Redis.
 */

#include <chrono>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

#include <boost/asio.hpp>

#include <fairmq/Plugin.h>

#include "plugins/Timer.h"
#include "plugins/TopologyData.h"

// forward declaration
namespace sw::redis {
class Redis;
template <typename Impl> class QueuedRedis;
class PipelineImpl;
using Pipeline = QueuedRedis<PipelineImpl>;
}

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

/** @brief RedisTimeSeries label names attached to socket metric series. */
static constexpr std::string_view DataType{"data"};
static constexpr std::string_view SocketName{"name"};
static constexpr std::string_view SocketType{"socket"};
static constexpr std::string_view SocketTransport{"transport"};
static constexpr std::string_view SocketMethod{"method"};

/**
 * @brief Process CPU sample used to compute CPU usage between timer ticks.
 */
struct ProcessUsageSample {
    double cpuSeconds{0.0};
    std::chrono::steady_clock::time_point timestamp;
};

/**
 * @brief Per-socket throughput values parsed from FairMQ rate log lines.
 */
struct SocketMetrics {
    double msgIn{0};
    double msgOut{0};
    double bytesIn{0};
    double bytesOut{0};
};

/** @brief Redis hash and time-series keys for process-level metrics. */
struct ProcessStatKey {
    std::string cpu;
    std::string ram;
    std::string stateId;
};

/** @brief Redis hash and time-series keys for one socket metric group. */
struct SocketMetricsKey {
    std::string msgIn;
    std::string msgOut;
    std::string bytesIn;
    std::string bytesOut;
};

/**
 * @brief FairMQ plugin that exports process and socket metrics to Redis.
 *
 * The plugin samples CPU/RSS on a timer and parses FairMQ throughput log lines
 * for channel metrics. It can also manage RedisTimeSeries keys when the Redis
 * module is available.
 */
class MetricsPlugin : public fair::mq::Plugin
{
public:
    using work_guard_t = net::executor_work_guard<net::io_context::executor_type>;

    /** @brief Command-line option names for metrics plugin configuration. */
    struct OptionKey {
        static constexpr std::string_view UpdateInterval{"proc-stat-update-interval"};
        static constexpr std::string_view ServerUri{"metrics-uri"};
        static constexpr std::string_view Retention{"retention"};
        static constexpr std::string_view RecreateTS{"recreate-ts"};
        static constexpr std::string_view MaxTtl{"metrics-max-ttl"};
    };

    /** @brief Construct and initialize the Redis-backed metrics plugin. */
    MetricsPlugin(std::string_view name,
                  const fair::mq::Plugin::Version &version,
                  std::string_view maintainer,
                  std::string_view homepage,
                  fair::mq::PluginServices *pluginServices);
    MetricsPlugin(const MetricsPlugin&) = delete;
    MetricsPlugin& operator=(const MetricsPlugin&) = delete;
    MetricsPlugin(MetricsPlugin&&) = delete;
    MetricsPlugin& operator=(MetricsPlugin&&) = delete;
    ~MetricsPlugin() override;

private:
    /** @brief Create RedisTimeSeries entries for one socket metric pair. */
    bool CreateSocketTS(std::string_view keyMsg,
                        std::string_view keyBytes,
                        std::string_view labelMsg,
                        std::string_view labelBytes,
                        const std::unordered_map<std::string, std::string> &labels);
    /** @brief Create all configured socket RedisTimeSeries entries. */
    bool CreateSocketTS();
    /** @brief Create one RedisTimeSeries key with labels and retention. */
    bool CreateTimeseries(std::string_view key,
                          const std::unordered_map<std::string, std::string> &labels);
    /** @brief Remove stale hash fields whose update timestamp exceeded max TTL. */
    void DeleteExpiredFields();
    /** @brief Delete RedisTimeSeries keys owned by this metrics instance. */
    void DeleteTSKeys();
    /** @brief Load socket metadata from topology keys for metric labels. */
    void InitializeSocketProperties();
    /** @brief Return whether time-series keys should be recreated on startup. */
    bool IsRecreateTS();
    /** @brief Read process user/system CPU time for usage deltas. */
    ProcessUsageSample ReadProcessUsage() const;
    /** @brief Read resident memory in MiB from `/proc/self/stat`. */
    double ReadResidentMemoryMiB() const;
    /** @brief Publish process CPU, RSS, and FairMQ state metrics to Redis. */
    void SendProcessMetrics();
    /** @brief Parse and publish socket throughput metrics from a FairMQ log line. */
    void SendSocketMetrics(const std::string &content);

    //pid_t fPid;
    std::string fId;
    std::unordered_map<std::string, SocketMetrics> fSocketMetrics;
    ProcessUsageSample fProcessUsage;
    long fPageSize;

    std::unique_ptr<work_guard_t> fWorkGuard;
    std::shared_ptr<net::io_context> fContext;
    std::unique_ptr<Timer> fTimer;
    std::thread fTimerThread;

    // milliseconds
    static constexpr long long kDefaultUpdateIntervalMs{1000};
    long long fUpdateInterval{kDefaultUpdateIntervalMs};
    long long fMaxTtl{0};

    std::string fStartTimeKey;
    std::string fStartTimeNSKey;
    std::string fStopTimeKey;
    std::string fStopTimeNSKey;
    std::string fRunNumberKey;

    std::chrono::system_clock::time_point fCreatedTimeSystem;
    std::chrono::steady_clock::time_point fCreatedTime;
    std::string fCreatedTimeKey;
    std::string fHostNameKey;
    std::string fIpAddressKey;

    std::mutex fMutex;
    std::shared_ptr<sw::redis::Redis> fClient;
    std::unique_ptr<sw::redis::Pipeline> fPipe;
    std::string fSeparator;
    std::string fServiceName;
    std::string fTopPrefix;

    // keys for hash (displayed in table)
    ProcessStatKey fProcKey;
    std::string fStateKey;
    std::string fLastUpdateKey;
    std::string fLastUpdateNSKey;

    SocketMetricsKey fSockKey;
    SocketMetricsKey fSockSumKey;
    std::string fNumMessageKey;
    std::string fBytesKey;
    std::string fNumMessageSumKey;
    std::string fBytesSumKey;

    // keys for time series data
    ProcessStatKey   fTsProcKey;

    std::unordered_map<std::string, SocketProperty> fSocketProperties;
    std::unordered_map<std::string, SocketMetricsKey> fTsSockKey;
    std::unordered_map<std::string, SocketMetricsKey> fTsSockSumKey;
    std::unordered_map<std::string, int> fNumChannels;
    std::string fRetentionMS{"0"};
    std::unordered_set<std::string> fRegisteredTSKeys;
    std::unordered_set<std::string> fRegisteredKeys;
    std::unordered_set<std::string> fRegisteredSockKeys;
};

/**
 * @brief Declare metrics plugin command-line options.
 */
auto MetricsPluginProgramOptions() -> fair::mq::Plugin::ProgOptions;

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables,cppcoreguidelines-pro-type-reinterpret-cast,performance-no-int-to-ptr)
REGISTER_FAIRMQ_PLUGIN(
    MetricsPlugin,
    metrics,
(fair::mq::Plugin::Version{0, 0, 0}),
"Metrics <maintainer@daq.service.net>",
"https://github.com/spadi-alliance/nestdaq",
daq::service::MetricsPluginProgramOptions
) // end of macro: REGISTER_FAIRMQ_PLUGIN

} // namespace daq::service
