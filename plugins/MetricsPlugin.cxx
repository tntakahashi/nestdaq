/** @file
 *  @brief Implements the DAQ metrics collection plugin.
 */

#include <sys/resource.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>
#include <cmath>
#include <fstream>
#include <iostream>
#include <iterator>
#include <system_error>
#include <vector>

#include <boost/algorithm/string.hpp>

#include <fairlogger/Logger.h>

#include <sw/redis++/redis++.h>
#include <sw/redis++/patterns/redlock.h>
#include <sw/redis++/errors.h>

#include "plugins/Constants.h"
#include "plugins/Functions.h"
#include "plugins/TimeUtil.h"
#include "plugins/MetricsPlugin.h"
#include "nestdaq/telemetry/FairMQThroughputLogParser.h"

static constexpr std::string_view kMyClass{"daq::service::MetricsPlugin"};

using namespace std::string_literals;

namespace {

auto timevalToSeconds(const timeval &value) -> double
{
    static constexpr auto kMicrosecondsPerSecond = 1'000'000.0;
    return static_cast<double>(value.tv_sec) + (static_cast<double>(value.tv_usec) / kMicrosecondsPerSecond);
}

} // namespace

namespace daq::service {

ProcessStatKey append(const ProcessStatKey& input, std::string_view s, std::string_view separator)
{
    ProcessStatKey ret;
    ret.cpu     = join({input.cpu,     s.data()}, separator.data());
    ret.ram     = join({input.ram,     s.data()}, separator.data());
    ret.stateId = join({input.stateId, s.data()}, separator.data());
    return ret;
}

SocketMetricsKey append(const SocketMetricsKey &input, std::string_view s, std::string_view separator)
{
    SocketMetricsKey ret;
    ret.msgIn    = join({input.msgIn,    s.data()}, separator.data());
    ret.msgOut   = join({input.msgOut,   s.data()}, separator.data());
    ret.bytesIn  = join({input.bytesIn,  s.data()}, separator.data());
    ret.bytesOut = join({input.bytesOut, s.data()}, separator.data());
    return ret;
}

ProcessStatKey prepend(const ProcessStatKey& input, std::string_view s, std::string_view separator)
{
    ProcessStatKey ret;
    ret.cpu     = join({s.data(), input.cpu},     separator.data());
    ret.ram     = join({s.data(), input.ram},     separator.data());
    ret.stateId = join({s.data(), input.stateId}, separator.data());
    return ret;
}

SocketMetricsKey prepend(const SocketMetricsKey& input, std::string_view s, std::string_view separator)
{
    SocketMetricsKey ret;
    ret.msgIn    = join({s.data(), input.msgIn},    separator.data());
    ret.msgOut   = join({s.data(), input.msgOut},   separator.data());
    ret.bytesIn  = join({s.data(), input.bytesIn},  separator.data());
    ret.bytesOut = join({s.data(), input.bytesOut}, separator.data());
    return ret;
}

ProcessStatKey replaceAll(const ProcessStatKey& input, std::string_view search, std::string format)
{
    ProcessStatKey ret;
    ret.cpu     = boost::replace_all_copy(input.cpu,     search.data(), format.data());
    ret.ram     = boost::replace_all_copy(input.ram,     search.data(), format.data());
    ret.stateId = boost::replace_all_copy(input.stateId, search.data(), format.data());
    return ret;
}

SocketMetricsKey replaceAll(const SocketMetricsKey& input, std::string_view search, std::string format)
{
    SocketMetricsKey ret;
    ret.msgIn    = boost::replace_all_copy(input.msgIn,    search.data(), format.data());
    ret.msgOut   = boost::replace_all_copy(input.msgOut,   search.data(), format.data());
    ret.bytesIn  = boost::replace_all_copy(input.bytesIn,  search.data(), format.data());
    ret.bytesOut = boost::replace_all_copy(input.bytesOut, search.data(), format.data());
    return ret;
}
} // namespace daq::service

auto daq::service::metricsPluginProgramOptions() -> fair::mq::Plugin::ProgOptions
{
    namespace bpo = boost::program_options;
    using opt = daq::service::MetricsPlugin::OptionKey;
    LOG(debug) << "daq::service::metricsPluginProgramOptions: add_options";

    auto options = bpo::options_description(kMyClass.data());
    options.add_options()
           (opt::UpdateInterval.data(), bpo::value<long long>()->default_value(1000),     "update interval in milliseconds for CPU and memory usage.")
           (opt::ServerUri.data(),      bpo::value<std::string>(),                        "Redis server URI (if empty, the same URI of the service registry is used.)")
           (opt::Retention.data(),      bpo::value<std::string>()->default_value("0"),    "Retention time in msec for time series data. When set to 0, the series is not trimmed at all.")
           (opt::RecreateTS.data(),     bpo::value<std::string>()->default_value("true"), "Recreate timeseries data on state transition to Running")
           (opt::MaxTtl.data(),         bpo::value<std::string>()->default_value("3000"), "Max TTL for metrics in milliseconds. (if zero or negative, no TTL is set.)");
    return options;
}

daq::service::MetricsPlugin::MetricsPlugin(std::string_view name,
        const fair::mq::Plugin::Version &version,
        std::string_view maintainer,
        std::string_view homepage,
        fair::mq::PluginServices *pluginServices)
    : fair::mq::Plugin(name.data(), version, maintainer.data(), homepage.data(), pluginServices)
{
    using opt = OptionKey;
    LOG(debug) << kMyClass << "() hello " << GetName();

//  fPid          = getpid();
//  LOG(debug) << kMyClass << " pid = " << fPid;
    fPageSize     = sysconf(_SC_PAGESIZE);
    fProcessUsage = readProcessUsage();

    fId          = GetProperty<std::string>("id");
    fServiceName = GetProperty<std::string>(ServiceName.data());
    fSeparator   = GetProperty<std::string>(Separator.data());
    fTopPrefix   = kMetricsPrefix.data();

    fRetentionMS = GetProperty<std::string>(opt::Retention.data());
    fMaxTtl      = std::stoll(GetProperty<std::string>(opt::MaxTtl.data()));

    if (PropertyExists("created-time")) {
        auto t = GetProperty<int64_t>("created-time");
        std::chrono::nanoseconds dur(t);
        fCreatedTimeSystem = std::chrono::time_point<std::chrono::system_clock>(dur);
    } else {
        fCreatedTimeSystem = std::chrono::system_clock::now();
    }
    fCreatedTime = std::chrono::steady_clock::now();

    fStateKey        = join({fTopPrefix, kStatePrefix.data()},        fSeparator);
    fLastUpdateKey   = join({fTopPrefix, kLastUpdatePrefix.data()},   fSeparator);
    fLastUpdateNSKey = join({fTopPrefix, kLastUpdateNSPrefix.data()}, fSeparator);
    fProcKey.stateId = join({fTopPrefix, kStateIdPrefix.data()},      fSeparator);
    fProcKey.cpu     = join({fTopPrefix, kCpuStatPrefix.data()},      fSeparator);
    fProcKey.ram     = join({fTopPrefix, kRamStatPrefix.data()},      fSeparator);

    fSockKey.msgIn    = join({fTopPrefix, kMessageInPrefix.data()},  fSeparator);
    fSockKey.bytesIn  = join({fTopPrefix, kBytesInPrefix.data()},    fSeparator);
    fSockKey.msgOut   = join({fTopPrefix, kMessageOutPrefix.data()}, fSeparator);
    fSockKey.bytesOut = join({fTopPrefix, kBytesOutPrefix.data()},   fSeparator);

    fSockSumKey       = append(fSockKey, "sum", "-");

    fNumMessageKey    = join({fTopPrefix, kNumMessagePrefix.data()},    fSeparator);
    fBytesKey         = join({fTopPrefix, kBytesPrefix.data()},         fSeparator);
    fNumMessageSumKey = join({fTopPrefix, kNumMessageSumPrefix.data()}, fSeparator);
    fBytesSumKey      = join({fTopPrefix, kBytesSumPrefix.data()},      fSeparator);

    auto t     = replaceAll(fProcKey, std::string(fTopPrefix)+fSeparator.data(), "");
    fTsProcKey = prepend(t, join({"ts", fId}, fSeparator), fSeparator);

    /*
    LOG(debug) << " StateKey       = " << fStateKey
               << "\n LastUpdateKey     = " << fLastUpdateKey
               << "\n LastUpdateNSKey   = " << fLastUpdateNSKey
               << "\n"
               << "\n ProcKey.stateId   = " << fProcKey.stateId
               << "\n ProcKey.cpu       = " << fProcKey.cpu
               << "\n ProcKey.ram       = " << fProcKey.ram
               << "\n"
               << "\n SockKey.msgIn     = " << fSockKey.msgIn
               << "\n SockKey.bytesIn   = " << fSockKey.bytesIn
               << "\n SockKey.msgOut    = " << fSockKey.msgOut
               << "\n SockKey.bytesOut  = " << fSockKey.bytesOut
               << "\n"
               << "\n SockSumKey.msgIn     = " << fSockSumKey.msgIn
               << "\n SockSumKey.bytesIn   = " << fSockSumKey.bytesIn
               << "\n SockSumKey.msgOut    = " << fSockSumKey.msgOut
               << "\n SockSumKey.bytesOut  = " << fSockSumKey.bytesOut
               << "\n"
               << "\n fNumMessageKey    = " << fNumMessageKey
               << "\n fBytesKey         = " << fBytesKey
               << "\n fNuMMessageSumKey = " << fNumMessageSumKey
               << "\n fBytesSumKey      = " << fBytesSumKey
               << "\n"
               << "\n TsProcKey.stateId   = " << fTsProcKey.stateId
               << "\n TsProcKey.cpu       = " << fTsProcKey.cpu
               << "\n PTsrocKey.ram       = " << fTsProcKey.ram;
    */

    std::string serverUri;
    if (PropertyExists(opt::ServerUri.data())) {
        serverUri = GetProperty<std::string>(opt::ServerUri.data());
    } else if (PropertyExists(ServiceRegistryUri.data())) {
        serverUri = GetProperty<std::string>(ServiceRegistryUri.data());
    }
    if (!serverUri.empty()) {
        fClient = std::make_shared<sw::redis::Redis>(serverUri);
    }

    fCreatedTimeKey = join({fTopPrefix, kCreatedTimePrefix.data()},   fSeparator);
    fHostNameKey    = join({fTopPrefix, kHostnamePrefix.data()},      fSeparator);
    fIpAddressKey   = join({fTopPrefix, kHostIpAddressPrefix.data()}, fSeparator);

    //LOG(debug) << " createdTimeKey = " << fCreatedTimeKey
    //           << "\n hostnameKey    = " << fHostnameKey
    //           << "\n ipAddresssKey  = " << fIpAddressKey;

    fStartTimeKey   = join({fTopPrefix, StartTime.data()}, fSeparator);
    fStartTimeNSKey = join({fTopPrefix, StartTimeNS.data()}, fSeparator);
    fStopTimeKey    = join({fTopPrefix, StopTime.data()}, fSeparator);
    fStopTimeNSKey  = join({fTopPrefix, StopTimeNS.data()}, fSeparator);
    fRunNumberKey   = join({fTopPrefix, RunNumber.data()}, fSeparator);

    fRegisteredKeys.insert({fStateKey, fLastUpdateKey, fLastUpdateNSKey,
                            fStartTimeKey, fStartTimeNSKey, fStopTimeKey, fStopTimeNSKey,
                            fRunNumberKey,
                            fProcKey.stateId, fProcKey.cpu, fProcKey.ram,
                            fCreatedTimeKey, fHostNameKey, fIpAddressKey});
    //for (auto k : fRegisteredKeys) {
    //  LOG(debug) << " key = " << k;
    //}
    fRegisteredSockKeys.insert({fSockKey.msgIn, fSockKey.bytesIn, fSockKey.msgOut, fSockKey.bytesOut,
                                fSockSumKey.msgIn, fSockSumKey.bytesIn, fSockSumKey.msgOut, fSockSumKey.bytesOut,
                                fNumMessageKey, fBytesKey, fNumMessageSumKey, fBytesSumKey});

    fPipe = std::make_unique<sw::redis::Pipeline>(std::move(fClient->pipeline()));
    if (fMaxTtl>0) {
        deleteExpiredFields();
    }

    {
        //const auto &[uptimeNSec, lastUpdate] = updateDate(fCreatedTimeSystem, fCreatedTime);
        //auto lastUpdateNS = std::chrono::duration_cast<std::chrono::nanoseconds>(lastUpdate.time_since_epoch());
        std::scoped_lock<std::mutex> lock{fMutex};
        fPipe->hset(fCreatedTimeKey, fId, toDate(fCreatedTimeSystem))
        .hset(fHostNameKey,    fId, GetProperty<std::string>("hostname"))
        .hset(fIpAddressKey,   fId, GetProperty<std::string>("host-ip"))
        //.hset(fLastUpdateKey, fId, toDate(lastUpdate))
        //.hset(fLastUpdateNSKey, fId, std::to_string(lastUpdateNS.count()))
        .exec();
    }
    fair::Logger::AddCustomSink(kMyClass.data(), "info", [this](const std::string &content, const fair::LogMetaData & /*metadata*/) {
        std::scoped_lock<std::mutex> lock{fMutex};
        sendSocketMetrics(content);
    });

    SubscribeToPropertyChangeAsString([this](const std::string& key, std::string value) {
        if (
            (key==StartTime)   ||
            (key==StartTimeNS) ||
            (key==StopTime)    ||
            (key==StopTimeNS)  ||
            (key==RunNumber)) {
            //LOG(debug) << kMyClass << " (subscribed callback) key = " << key << ", value = " << value;
            std::scoped_lock<std::mutex> lock{fMutex};
            fClient->hset(join({fTopPrefix, key}, fSeparator), fId, value);

        }
    });

    SubscribeToDeviceStateChange([this](DeviceState newState) {
        auto pipelineUsed{false};
        const auto stateName = GetStateName(newState);
        LOG(debug) << kMyClass << " state change: " << stateName;
        {
            std::scoped_lock<std::mutex> lock{fMutex};
            if (fPipe) {
                fPipe->discard();
                fPipe->hset(fStateKey,        fId, stateName)
                .hset(fProcKey.stateId, {std::make_pair(fId, static_cast<int>(newState))})
                .exec();
                pipelineUsed = true;
            }
        }
        switch (newState) {
        case DeviceState::DeviceReady:
            initializeSocketProperties();
            break;
        case DeviceState::Ready:
        {
            if (isRecreateTs()) {
                deleteTsKeys();
            }
            fSocketMetrics.clear();
            fNumChannels.clear();
            break;
        }
        case DeviceState::Running:
            if (isRecreateTs()) {
                pipelineUsed |= createTimeseries(fTsProcKey.cpu,     {{kDataType.data(), kCpuStatPrefix.data()}});
                pipelineUsed |= createTimeseries(fTsProcKey.ram,     {{kDataType.data(), kRamStatPrefix.data()}});
                pipelineUsed |= createTimeseries(fTsProcKey.stateId, {{kDataType.data(), kStateIdPrefix.data()}});
                pipelineUsed |= createSocketTS();
                if (pipelineUsed) {
                    fPipe->exec();
                }
            }
            break;
        default:
            break;
        }
    });

}

daq::service::MetricsPlugin::~MetricsPlugin()
{

    UnsubscribeFromDeviceStateChange();
    UnsubscribeFromPropertyChangeAsString();
    fair::Logger::RemoveCustomSink(kMyClass.data());
    LOG(debug) << kMyClass << "UnsubscribeFromDeviceStateChange()";
    //fContext->stop();
    //if (fTimerThread.joinable()) {
    //  fTimerThread.join();
    //  LOG(debug) << kMyClass << " timer thread joined.";
    //}
    if (fPipe) {
        fPipe.reset();
    }
    LOG(debug) << "~" << kMyClass << "() bye";
}

/**
 * @brief Create RedisTimeSeries keys for one socket direction and its sum.
 */
bool daq::service::MetricsPlugin::createSocketTS(std::string_view keyMsg,
        std::string_view keyBytes,
        std::string_view labelMsg,
        std::string_view labelBytes,
        const std::unordered_map<std::string, std::string>& labels)
{
    //LOG(warn) << __func__ << ":" << __LINE__;
    bool pipelineUsed=false;

    auto keyMsgSum     = join({keyMsg.data(),     "sum"}, "-");
    auto keyBytesSum   = join({keyBytes.data(),   "sum"}, "-");
    auto labelMsgSum   = join({labelMsg.data(),   "sum"}, "-");
    auto labelBytesSum = join({labelBytes.data(), "sum"}, "-");

    auto labelsMsg      = labels;
    auto labelsBytes    = labels;
    auto labelsMsgSum   = labels;
    auto labelsBytesSum = labels;

    //LOG(debug) << __func__ << ":"
    //           << "\n keyMsgSum     = " << keyMsgSum
    //           << "\n keyBytesSum   = " << keyBytesSum
    //           << "\n labelMsgSum   = " << labelMsgSum
    //           << "\n labelBytesSum = " << labelBytesSum;

    labelsMsg.emplace(kDataType.data(),      labelMsg);
    labelsBytes.emplace(kDataType.data(),    labelBytes);
    labelsMsgSum.emplace(kDataType.data(),   labelMsgSum);
    labelsBytesSum.emplace(kDataType.data(), labelBytesSum);
    pipelineUsed |= createTimeseries(keyMsg,      labelsMsg);
    pipelineUsed |= createTimeseries(keyBytes,    labelsBytes);
    pipelineUsed |= createTimeseries(keyMsgSum,   labelsMsgSum);
    pipelineUsed |= createTimeseries(keyBytesSum, labelsBytesSum);
    return pipelineUsed;
}

/**
 * @brief Create RedisTimeSeries keys for all configured FairMQ sockets.
 */
bool daq::service::MetricsPlugin::createSocketTS()
{
    //LOG(warn) << __func__ << ":" << __LINE__;
    bool pipelineUsed=false;
    for (const auto &[name, property] : fSocketProperties) {
        auto hasInput  = (property.type!="push") && (property.type!="pub");
        auto hasOutput = (property.type!="pull") && (property.type!="sub");
        if (!hasInput && !hasOutput) {
            continue;
        }
        const auto prefix = join({"ts", fId, name}, fSeparator);
        auto t = replaceAll(fSockKey, std::string(fTopPrefix)+fSeparator.data(), "");
        auto tsKey = prepend(t, prefix, fSeparator);
        fTsSockKey[name]    = tsKey;
        auto sumKey = append(tsKey, "sum", "-");
        fTsSockSumKey[name] =  sumKey;

        //std::string s{" socket TS keys for "};
        //s += name + "\n";
        //s += " " + tsKey.msgIn  + ", " + tsKey.bytesIn  + ", " + tsKey.msgOut  + ", " + tsKey.bytesOut + "\n";
        //s += " " + sumKey.msgIn + ", " + sumKey.bytesIn + ", " + sumKey.msgOut + ", " + sumKey.bytesOut;
        //LOG(debug) << kMyClass << s;

        std::unordered_map<std::string, std::string> labels{{"name",     property.name},
            {"socket",    property.type},
            {"transport", property.transport}};
        if (hasInput) {
            pipelineUsed |= createSocketTS(tsKey.msgIn, tsKey.bytesIn, kMessageInPrefix, kBytesInPrefix, labels);
        }
        if (hasOutput) {
            pipelineUsed |= createSocketTS(tsKey.msgOut, tsKey.bytesOut, kMessageOutPrefix, kBytesOutPrefix, labels);
        }
    }
    return pipelineUsed;
}

/**
 * @brief Queue creation of one RedisTimeSeries key with standard labels.
 */
bool daq::service::MetricsPlugin::createTimeseries(std::string_view key,
        const std::unordered_map<std::string, std::string> &labels)
{
    //LOG(warn) << __func__ << ":" << __LINE__;
    if (fClient->exists(key.data())>0) {
        //LOG(warn) << " TS key = " << key << " already exists in DB";
        fClient->del(key.data());
        fRegisteredTSKeys.erase(key.data());
    }
    std::vector<std::string> cmd;
    cmd.push_back("ts.create");
    cmd.push_back(key.data());
    cmd.push_back("retention");
    cmd.push_back(fRetentionMS);
    cmd.push_back("labels");
    cmd.push_back("service");
    cmd.push_back(fServiceName);
    cmd.push_back("id");
    cmd.push_back(fId);
    for (const auto& [k, v] : labels) {
        cmd.push_back(k);
        cmd.push_back(v);
    }

    //std::string s{" create time series data:\n"};
    //for (const auto& x : cmd) {
    //  s += " " + x + "\n";
    //}
    //LOG(debug) << s;

    fPipe->command(cmd.cbegin(), cmd.cend());
    fRegisteredTSKeys.emplace(key.data());
    return true;
}

/**
 * @brief Delete stale Redis hash fields for service instances past the metrics TTL.
 */
void daq::service::MetricsPlugin::deleteExpiredFields()
{
    while (true) {
        try {
            sw::redis::RedMutex mtx(fClient, "metrics");
            std::unique_lock<sw::redis::RedMutex> redLock(mtx, std::defer_lock);
            if (redLock.try_lock()) {
                LOG(debug) << "got lock: " << kMyClass << " " << fId;

                std::unordered_map<std::string, std::string> hashInstanceToLastUpdateNS;
                fClient->hgetall(fLastUpdateNSKey, std::inserter(hashInstanceToLastUpdateNS, hashInstanceToLastUpdateNS.begin()));
                std::vector<std::string> expiredInstances;
                for (const auto& [k, v] : hashInstanceToLastUpdateNS) {
                    auto tNS = std::stoull(v); // nanoseconds -> milliseconds
                    auto tNow = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
                    if ((tNow - tNS)/1e6 > fMaxTtl) {
                        expiredInstances.push_back(k);
                    }
                }

                if (!expiredInstances.empty()) {
                    for (const auto &k : fRegisteredKeys) {
                        LOG(debug) << __func__ << ":" << __LINE__ << " delete " << k;
                        fPipe->hdel(k, expiredInstances.begin(), expiredInstances.end());
                    }

                    std::unordered_map<std::string, std::string> sockets;
                    for (const auto &k : fRegisteredSockKeys) {
                        fClient->hgetall(k, std::inserter(sockets, sockets.begin()));
                        std::vector<std::string> a;
                        for (const auto &instName : expiredInstances) {
                            for (const auto &[sockName, v] : sockets) {
                                if (sockName.find(instName) == 0) {
                                    LOG(debug) << __func__ << ":" << __LINE__ << " delete " << k << " " << sockName;
                                    fPipe->hdel(k, sockName);
                                }
                            }
                        }
                    }
                }
                fPipe->exec();
                if (redLock.owns_lock()) {
                    LOG(debug) << "unlock: " << kMyClass << " " << fId;
                    break;
                } else {
                    std::this_thread::yield();
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
            }
        } catch (const sw::redis::Error& e) {
            LOG(error) << " caught exception (redis++) : " << e.what();
        } catch (const std::exception& e) {
            LOG(error) << " caught exception (std) : " << e.what();
        } catch (...) {
            LOG(error) << " caught exception : unknown";
        }
    }
}

/**
 * @brief Delete RedisTimeSeries keys created by this plugin instance.
 */
void daq::service::MetricsPlugin::deleteTsKeys()
{
    if (!fRegisteredTSKeys.empty()) {
        auto ndeleted = fClient->del(fRegisteredTSKeys.cbegin(), fRegisteredTSKeys.cend());
        fRegisteredTSKeys.clear();
        LOG(debug) << kMyClass << " " << __FUNCTION__ << " n deleted = " << ndeleted;
    }
}

/**
 * @brief Read FairMQ channel properties and cache per-socket metadata.
 */
void daq::service::MetricsPlugin::initializeSocketProperties()
{
    // Get parameters of channel configuration as std::map<sstd::tring, std::1string>
    const auto properties = GetPropertiesAsStringStartingWith("chans.");
    fSocketProperties.clear();
    for (const auto& [k, v] : properties) {
        std::vector<std::string> c;
        boost::split(c, k, boost::is_any_of("."), boost::token_compress_on);
        // k = chans.<channel-name>.<subchannel-index>.<field>
        if (c.size()<4) {
            LOG(error) << " invalid channel property : key = " << k << ", value = " << v;
            continue;
        }
        auto name  = c[1] + "[" + c[2] + "]";
        const auto &field = c[3];
        auto &p = fSocketProperties[name];
        if (p.name.empty()) {
            p.name = name;
        } else if (field=="type") {
            p.type = v;
        } else if (field=="method") {
            p.method = v;
        } else if (field=="address") {
            p.address = v;
        } else if (field=="transport") {
            p.transport = v;
        } else if (field=="sndBufSize") {
            p.sndBufSize = std::stoi(v);
        } else if (field=="rcvBufSize") {
            p.rcvBufSize = std::stoi(v);
        } else if (field=="sndKernelSize") {
            p.sndKernelSize = std::stoi(v);
        } else if (field=="rcvKernelSize") {
            p.rcvKernelSize = std::stoi(v);
        } else if (field=="linger") {
            p.linger = std::stoi(v);
        } else if (field=="rateLogging") {
            p.rateLogging = std::stoi(v);
        } else if (field=="portRangeMin") {
            p.portRangeMin = std::stoi(v);
        } else if (field=="portRangeMax") {
            p.portRangeMax = std::stoi(v);
        } else if (field=="autoBind") {
            p.autoBind = (v=="true") || (v=="1");
        }
    }
}

/**
 * @brief Check whether time-series keys should be recreated when running starts.
 */
bool daq::service::MetricsPlugin::isRecreateTs()
{
    //LOG(warn) << __func__ << ":" << __LINE__;
    using opt = OptionKey;
    if (PropertyExists(opt::RecreateTS.data())) {
        auto f = GetProperty<std::string>(opt::RecreateTS.data());
        boost::to_lower(f);
        return (f=="true") || (f=="1");
    }
    //LOG(warn) << __func__ << ":" << __LINE__;
    return false;
}

/**
 * @brief Read cumulative CPU time consumed by this process.
 */
auto daq::service::MetricsPlugin::readProcessUsage() const -> ProcessUsageSample
{
    rusage usage{};
    if (getrusage(RUSAGE_SELF, &usage) != 0) {
        const auto error = std::error_code{errno, std::generic_category()};
        LOG(error) << kMyClass << " " << __FUNCTION__ << " getrusage failed: " << error.message();
        return {.cpuSeconds = fProcessUsage.cpuSeconds, .timestamp = std::chrono::steady_clock::now()};
    }

    return {
        .cpuSeconds = timevalToSeconds(usage.ru_utime) + timevalToSeconds(usage.ru_stime),
        .timestamp = std::chrono::steady_clock::now(),
    };
}

/**
 * @brief Read resident memory usage of this process in MiB.
 */
auto daq::service::MetricsPlugin::readResidentMemoryMiB() const -> double
{
    std::ifstream input{"/proc/self/statm"};
    uint64_t totalPages = 0;
    uint64_t residentPages = 0;
    if (!(input >> totalPages >> residentPages)) {
        LOG(error) << kMyClass << " " << __FUNCTION__ << " failed to read /proc/self/statm";
        return 0.0;
    }

    static constexpr auto kBytesPerMiB = 1024.0 * 1024.0;
    return static_cast<double>(residentPages) * static_cast<double>(fPageSize) / kBytesPerMiB;
}

/**
 * @brief Record CPU, memory, state, and last-update metrics.
 */
void daq::service::MetricsPlugin::sendProcessMetrics()
{
    //std::cout << kMyClass << " " << __FUNCTION__;

    auto nowProcessUsage = readProcessUsage();

    const auto cpuSeconds = nowProcessUsage.cpuSeconds - fProcessUsage.cpuSeconds;
    const auto wallSeconds =
        std::chrono::duration<double>(nowProcessUsage.timestamp - fProcessUsage.timestamp).count();

    // Top/htop style percent: one fully used CPU core is 100%, two cores are 200%.
    const auto cpuUsage = wallSeconds > 0.0 ? cpuSeconds / wallSeconds * 100.0 : 0.0;
    const auto ramUsage = readResidentMemoryMiB();

//  std::cout << " diff (self) = " << diffSelf
//             << ", diff (all) = " << diffAll << "\n"
//             << "cpu = " << cpuUsage
//             << ", memory = " << ramUsage;

    fProcessUsage = nowProcessUsage;
    auto stateId  = static_cast<int>(GetCurrentDeviceState());

    const auto &[uptimeNSec, lastUpdate] = updateDate(fCreatedTimeSystem, fCreatedTime);
    auto lastUpdateNS = std::chrono::duration_cast<std::chrono::nanoseconds>(lastUpdate.time_since_epoch());
    try {
        if (fPipe) {
            fPipe->hset(fProcKey.cpu, {std::make_pair(fId, cpuUsage)})
            .hset(fProcKey.ram, {std::make_pair(fId, ramUsage)})
            .hset(fLastUpdateKey, fId, toDate(lastUpdate))
            .hset(fLastUpdateNSKey, fId, std::to_string(lastUpdateNS.count()))
            .command("ts.add", fTsProcKey.cpu,        "*", std::to_string(cpuUsage))
            .command("ts.add", fTsProcKey.ram,        "*", std::to_string(ramUsage))
            .command("ts.add", fTsProcKey.stateId,    "*", std::to_string(stateId));
            //std::cout << " "   << fTsProcKey.cpu       << "\t " << cpuUsage
            //          << "\n " << fTsProcKey.ram       << "\t " << ramUsage
            //          << "\n " << fTsProcKey.stateId   << "\t " << stateId << std::endl;
        }
    } catch (const std::exception& e) {
        LOG(error) << kMyClass << " " << __FUNCTION__ << " exception : what() " << e.what();
    } catch (...) {
        LOG(error) << kMyClass << " " << __FUNCTION__ << " exception : unknown ";
    }
    //std::cout << kMyClass << " " << __FUNCTION__ << " done";
}

/**
 * @brief Parse a FairMQ throughput log line and record socket metrics.
 */
void daq::service::MetricsPlugin::sendSocketMetrics(const std::string &content)
{
    //LOG(debug) << kMyClass << " " << __FUNCTION__;
    //return;
    //std::cout << kMyClass << " content = \n" << content << "\n length = " << content.size() << std::endl;
    const auto sample = nestdaq::telemetry::parseFairMQThroughputLog(content);
    if (!sample || !sample->subChannelIndex) {
        return;
    }
    //std::cout << kMyClass << " " << __FUNCTION__ << " (passed) content = \n" << content << std::endl;

    const auto &channelName = sample->channelName;
    const auto subChannelIndex = std::to_string(*sample->subChannelIndex);
    const auto &subChannelName = sample->subChannelName;
    auto channelId       = join({fId, subChannelName}, fSeparator);

    SocketMetrics now;
    now.msgIn    = sample->messagesPerSecondIn;
    now.msgOut   = sample->messagesPerSecondOut;
    // mega bytes
    now.bytesIn  = sample->megabytesPerSecondIn;
    now.bytesOut = sample->megabytesPerSecondOut;

    auto& sum = fSocketMetrics[subChannelName];
    sum.msgIn    += now.msgIn;
    sum.msgOut   += now.msgOut;
    sum.bytesIn  += now.bytesIn;
    sum.bytesOut += now.bytesOut;

    //std::cout << __LINE__ << " " << channelName << " (sum) in = " << sum.msgIn << " " << sum.bytesIn << " MB, out = " << sum.msgOut << " " << sum.bytesOut << " MB" << std::endl;
    auto msgIn     = static_cast<uint64_t>(std::nearbyint(now.msgIn));
    auto msgOut    = static_cast<uint64_t>(std::nearbyint(now.msgOut));

    auto msgInSum  = static_cast<uint64_t>(std::nearbyint(sum.msgIn));
    auto msgOutSum = static_cast<uint64_t>(std::nearbyint(sum.msgOut));

    try {
        if (fPipe) {
            const auto &socketTypeKey = join({"chans", channelName, subChannelIndex, "type"},  ".");
            // std::cout << " channel type key = " << socketTypeKey << std::endl;
            std::string socketType;
            if (PropertyExists(socketTypeKey)) {
                socketType = GetProperty<std::string>(socketTypeKey);
            } else {
                return;
            }
            bool hasInput  = (socketType!="push") && (socketType!="pub");
            bool hasOutput = (socketType!="pull") && (socketType!="sub");
            if (!hasInput && !hasOutput) {
                return;
            }

            // LOG(debug) << " subChannelName = " << subChannelName;

            const auto tsKey       = fTsSockKey[subChannelName];
            const auto tsSumKey    = fTsSockSumKey[subChannelName];

            if (hasInput) {
                fPipe->hset(fSockKey.msgIn,       {std::make_pair(channelId, msgIn)})
                .hset(fSockKey.bytesIn,     {std::make_pair(channelId, now.bytesIn)})  // mega bytes
                .hset(fSockSumKey.msgIn,    {std::make_pair(channelId, msgInSum)})
                .hset(fSockSumKey.bytesIn,  {std::make_pair(channelId, sum.bytesIn)})  // mega bytes
                .hset(fNumMessageKey,       {std::make_pair(channelId+".in",  msgIn)})
                .hset(fBytesKey,            {std::make_pair(channelId+".in",  now.bytesIn)})
                .hset(fNumMessageSumKey,    {std::make_pair(channelId+".in",  msgInSum)})
                .hset(fBytesSumKey,         {std::make_pair(channelId+".in",  sum.bytesIn)})
                .command("ts.add", tsKey.msgIn,         "*", std::to_string(msgIn))
                .command("ts.add", tsKey.bytesIn,       "*", std::to_string(now.bytesIn))
                .command("ts.add", tsSumKey.msgIn,      "*", std::to_string(msgInSum))
                .command("ts.add", tsSumKey.bytesIn,    "*", std::to_string(sum.bytesIn));
                //std::cout << __LINE__ << " has input: "
                //          << tsKey.msgIn       << "\t " << msgIn
                //          << "\n " << tsKey.bytesIn     << "\t " << now.bytesIn
                //          << "\n " << tsSumKey.msgIn    << "\t " << msgInSum
                //          << "\n " << tsSumKey.bytesIn  << "\t " << sum.bytesIn << std::endl;
            }

            if (hasOutput) {
                fPipe->hset(fSockKey.msgOut,      {std::make_pair(channelId, msgOut)})
                .hset(fSockKey.bytesOut,    {std::make_pair(channelId, now.bytesOut)}) // mega bytes
                .hset(fSockSumKey.msgOut,   {std::make_pair(channelId, msgOutSum)})
                .hset(fSockSumKey.bytesOut, {std::make_pair(channelId, sum.bytesOut)}) // mega bytes
                .hset(fNumMessageKey,       {std::make_pair(channelId+".out", msgOut)})
                .hset(fBytesKey,            {std::make_pair(channelId+".out", now.bytesOut)})
                .hset(fNumMessageSumKey,    {std::make_pair(channelId+".out", msgOutSum)})
                .hset(fBytesSumKey,         {std::make_pair(channelId+".out", sum.bytesOut)})
                .command("ts.add", tsKey.msgOut,         "*", std::to_string(msgOut))
                .command("ts.add", tsKey.bytesOut,       "*", std::to_string(now.bytesOut))
                .command("ts.add", tsSumKey.msgOut,      "*", std::to_string(msgOutSum))
                .command("ts.add", tsSumKey.bytesOut,    "*", std::to_string(sum.bytesOut));
                //std::cout << __LINE__ << " has output: "
                //          << tsKey.msgOut      << "\t " << msgOut
                //          << "\n " << tsKey.bytesOut    << "\t " << now.bytesOut
                //          << "\n " << tsSumKey.msgOut   << "\t " << msgOutSum
                //          << "\n " << tsSumKey.bytesOut << "\t " << sum.bytesOut << std::endl;

            }

            auto &count = fNumChannels[subChannelName];
            if (count==0) {
                ++count;
            }
            std::size_t countAll = 0;
            for (const auto &[k, v] : fNumChannels) {
                countAll += static_cast<std::size_t>(v);
            }
            if (countAll==fSocketMetrics.size()) {
                sendProcessMetrics();
                fPipe->exec();
                fNumChannels.clear();
            }
        } //else {
        //std::cout << __LINE__ << " no pipeline is created " << std::endl;
        //}
    } catch (const std::exception &e) {
        LOG(error) << kMyClass << " " << __FUNCTION__ << " exception : what() = " << e.what();
    } catch (...) {
        LOG(error) << kMyClass << " " << __FUNCTION__ << " exception : unknown";
    }
}
