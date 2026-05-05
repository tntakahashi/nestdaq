#pragma once

#include <chrono>
#include <cstdint>
#include <deque>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

#include <boost/asio.hpp>

#include <fairmq/Plugin.h>

#include "plugins/MetricsData.h"
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

class MetricsPlugin : public fair::mq::Plugin
{
public:
    using work_guard_t = net::executor_work_guard<net::io_context::executor_type>;

    struct OptionKey {
        static constexpr std::string_view UpdateInterval{"proc-stat-update-interval"};
        static constexpr std::string_view ServerUri{"metrics-uri"};
        static constexpr std::string_view Retention{"retention"};
        static constexpr std::string_view RecreateTS{"recreate-ts"};
        static constexpr std::string_view MaxTtl{"metrics-max-ttl"};
    };

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
    bool CreateSocketTS(std::string_view keyMsg,
                        std::string_view keyBytes,
                        std::string_view labelMsg,
                        std::string_view labelBytes,
                        const std::unordered_map<std::string, std::string> &labels);
    bool CreateSocketTS();
    bool CreateTimeseries(std::string_view key,
                          const std::unordered_map<std::string, std::string> &labels);
    void DeleteExpiredFields();
    void DeleteTSKeys();
    void InitializeSocketProperties();
    bool IsRecreateTS();
    ProcSelfStat_t ReadProcSelfStat();
    ProcStat_t     ReadProcStat();
    void SendProcessMetrics();
    void SendSocketMetrics(const std::string &content);

    //pid_t fPid;
    std::string fId;
    std::unordered_map<std::string, SocketMetrics> fSocketMetrics;
    std::ifstream fProcStatFile;
    std::ifstream fProcSelfStatFile;
    ProcStat_t     fProcStat;
    ProcSelfStat_t fProcSelfStat;
    unsigned int fNCpuCores;
    long fClockTick;
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

//_____________________________________________________________________________
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
