#include <algorithm>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <sys/ioctl.h>
#include <unistd.h>

#include <boost/algorithm/string.hpp>
#include <boost/program_options.hpp>

#include <sw/redis++/redis++.h>

#include "plugins/Constants.h"
#include "plugins/Functions.h"
#include "plugins/MetricsData.h"

namespace bpo = boost::program_options;

using namespace std::string_literals;

namespace nestdaq {
namespace {

volatile std::sig_atomic_t g_stop_requested{0}; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

struct Options {
    std::string fServiceA;
    std::string fChannelA;
    std::string fServiceB;
    std::string fChannelB;
    std::string fRedisUrlDaqService{"127.0.0.1:6379/0"};
    std::string fRedisUrlMetrics{"127.0.0.1:6379/1"};
    std::string fSeparator{":"};
    double fDiffLow{0.0};
    double fDiffHigh{0.0};
    uint64_t fRefreshMs{1000};
    bool fNoColor{false};
};

struct EndpointKey {
    std::string fService;
    std::string fChannel;
};

struct RuntimeChannel {
    std::string fKey;
    std::string fService;
    std::string fInstance;
    std::string fChannel;
    std::unordered_map<std::string, std::string> fFields;
    std::vector<std::string> fPeers;
};

struct SocketInfo {
    std::string fKey;
    std::string fService;
    std::string fInstance;
    std::string fChannel;
    int fIndex{0};
    std::unordered_map<std::string, std::string> fFields;
};

struct InstanceInfo {
    std::string fService;
    std::string fInstance;
    std::string fState;
    std::unordered_map<std::string, std::string> fHealth;
};

struct Snapshot {
    std::vector<InstanceInfo> fInstancesA;
    std::vector<InstanceInfo> fInstancesB;
    std::map<std::string, RuntimeChannel> fChannels;
    std::vector<SocketInfo> fSockets;
    std::unordered_map<std::string, double> fMsgIn;
    std::unordered_map<std::string, double> fMsgOut;
    std::vector<std::string> fWarnings;
    std::string fMetricsError;
};

struct Terminal {
    bool fTty{false};
    int fWidth{120};
};

auto MakeOptionDescription() -> bpo::options_description
{
    bpo::options_description options{"options"};
    options.add_options()
        ("help,h", "print this help")
        ("service-a", bpo::value<std::string>(), "source-side service name")
        ("channel-a", bpo::value<std::string>(), "source-side channel name")
        ("service-b", bpo::value<std::string>(), "peer-side service name")
        ("channel-b", bpo::value<std::string>(), "peer-side channel name")
        ("redis-url-daq_service", bpo::value<std::string>()->default_value("127.0.0.1:6379/0"),
         "DAQ service Redis URL. Format: host-address:port/db")
        ("redis-url-metrics", bpo::value<std::string>()->default_value("127.0.0.1:6379/1"),
         "metrics Redis URL. Format: host-address:port/db")
        ("separator", bpo::value<std::string>()->default_value(":"), "Redis key separator")
        ("diff-low", bpo::value<double>()->default_value(0.0), "low-side threshold in msg/s")
        ("diff-high", bpo::value<double>()->default_value(0.0), "high-side threshold in msg/s")
        ("refresh", bpo::value<uint64_t>()->default_value(1000), "refresh interval in milliseconds. 0 means one-shot")
        ("no-color", bpo::bool_switch()->default_value(false), "disable ANSI colors");
    return options;
}

auto ParseOptions(int argc, char *argv[]) -> std::optional<Options> // NOLINT(cppcoreguidelines-avoid-c-arrays)
{
    auto desc = MakeOptionDescription();
    bpo::variables_map vm;
    try {
        bpo::store(bpo::parse_command_line(argc, argv, desc), vm);
        bpo::notify(vm);
    } catch (const bpo::error &e) {
        std::cerr << "error: " << e.what() << "\n\n" << desc << '\n';
        return std::nullopt;
    }

    const auto required = {"service-a", "channel-a", "service-b", "channel-b"};
    for (const auto *name : required) {
        if (vm.count(name) == 0) {
            std::cerr << "error: missing required option --" << name << "\n\n" << desc << '\n';
            return std::nullopt;
        }
    }

    Options ret;
    ret.fServiceA = vm["service-a"].as<std::string>();
    ret.fChannelA = vm["channel-a"].as<std::string>();
    ret.fServiceB = vm["service-b"].as<std::string>();
    ret.fChannelB = vm["channel-b"].as<std::string>();
    ret.fRedisUrlDaqService = vm["redis-url-daq_service"].as<std::string>();
    ret.fRedisUrlMetrics = vm["redis-url-metrics"].as<std::string>();
    ret.fSeparator = vm["separator"].as<std::string>();
    ret.fDiffLow = vm["diff-low"].as<double>();
    ret.fDiffHigh = vm["diff-high"].as<double>();
    ret.fRefreshMs = vm["refresh"].as<uint64_t>();
    ret.fNoColor = vm["no-color"].as<bool>();

    if (ret.fSeparator.empty()) {
        std::cerr << "error: --separator must not be empty\n";
        return std::nullopt;
    }
    if (ret.fDiffLow > ret.fDiffHigh) {
        std::cerr << "error: --diff-low must be less than or equal to --diff-high\n";
        return std::nullopt;
    }

    return ret;
}

auto NormalizeRedisUri(const std::string &uri) -> std::optional<std::string>
{
    constexpr std::string_view TCP_PREFIX{"tcp://"};
    std::string ret = uri;
    if (ret.rfind(TCP_PREFIX.data(), 0) == 0) {
        ret = ret.substr(TCP_PREFIX.size());
    }

    const auto slash = ret.find('/');
    const auto colon = ret.rfind(':', slash == std::string::npos ? ret.size() : slash);
    if (ret.empty() || slash == std::string::npos || colon == std::string::npos || colon == 0 || colon > slash || slash + 1 >= ret.size()) {
        return std::nullopt;
    }
    return std::string{TCP_PREFIX} + ret;
}

auto NowString() -> std::string
{
    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &time);
#else
    localtime_r(&time, &tm);
#endif
    std::ostringstream out;
    out << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return out.str();
}

auto GetTerminal() -> Terminal
{
    Terminal ret;
    ret.fTty = isatty(STDOUT_FILENO) != 0;
    winsize size{};
    if (ret.fTty && ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) == 0 && size.ws_col > 0) { // NOLINT(cppcoreguidelines-pro-type-vararg)
        ret.fWidth = size.ws_col;
    }
    return ret;
}

auto SplitKey(std::string_view key, std::string_view separator) -> std::vector<std::string>
{
    std::vector<std::string> ret;
    boost::split(ret, key, boost::is_any_of(std::string{separator}));
    return ret;
}

auto Equals(const std::string &lhs, std::string_view rhs) -> bool
{
    return lhs == std::string{rhs};
}

auto ParseEndpointKey(std::string_view key, std::string_view separator) -> std::optional<EndpointKey>
{
    const auto parts = SplitKey(key, separator);
    if (parts.size() != 5 || !Equals(parts[0], daq::service::TopPrefix) || parts[1] != "topology" || parts[2] != "endpoint") {
        return std::nullopt;
    }
    return EndpointKey{parts[3], parts[4]};
}

auto ParseRuntimeChannelKey(std::string_view key, std::string_view separator) -> std::optional<RuntimeChannel>
{
    const auto parts = SplitKey(key, separator);
    if (parts.size() != 5 || !Equals(parts[0], daq::service::TopPrefix) || parts[3] != "channel") {
        return std::nullopt;
    }
    RuntimeChannel ret;
    ret.fKey = std::string{key};
    ret.fService = parts[1];
    ret.fInstance = parts[2];
    ret.fChannel = parts[4];
    return ret;
}

auto ParseSocketKey(std::string_view key, std::string_view separator) -> std::optional<SocketInfo>
{
    const auto parts = SplitKey(key, separator);
    if (parts.size() != 5 || !Equals(parts[0], daq::service::TopPrefix) || parts[3] != "socket") {
        return std::nullopt;
    }

    constexpr std::string_view CHANS_PREFIX{"chans."};
    const auto &local = parts[4];
    if (local.rfind(CHANS_PREFIX.data(), 0) != 0) {
        return std::nullopt;
    }
    const auto last_dot = local.find_last_of('.');
    if (last_dot == std::string::npos || last_dot <= CHANS_PREFIX.size()) {
        return std::nullopt;
    }

    SocketInfo ret;
    ret.fKey = std::string{key};
    ret.fService = parts[1];
    ret.fInstance = parts[2];
    ret.fChannel = local.substr(CHANS_PREFIX.size(), last_dot - CHANS_PREFIX.size());
    try {
        ret.fIndex = std::stoi(local.substr(last_dot + 1));
    } catch (const std::exception &) {
        return std::nullopt;
    }
    return ret;
}

auto ParseInstanceFromStateKey(std::string_view key, std::string_view separator) -> std::optional<InstanceInfo>
{
    const auto parts = SplitKey(key, separator);
    if (parts.size() != 4 || !Equals(parts[0], daq::service::TopPrefix) || !Equals(parts[3], daq::service::FairMQStatePrefix)) {
        return std::nullopt;
    }
    InstanceInfo ret;
    ret.fService = parts[1];
    ret.fInstance = parts[2];
    return ret;
}

auto GetHash(sw::redis::Redis &redis, const std::string &key) -> std::unordered_map<std::string, std::string>
{
    std::unordered_map<std::string, std::string> ret;
    redis.hgetall(key, std::inserter(ret, ret.begin()));
    return ret;
}

auto GetList(sw::redis::Redis &redis, const std::string &key) -> std::vector<std::string>
{
    std::vector<std::string> ret;
    redis.lrange(key, 0, -1, std::back_inserter(ret));
    return ret;
}

auto ReadDoubleHash(sw::redis::Redis &redis, const std::string &key) -> std::unordered_map<std::string, double>
{
    const auto values = GetHash(redis, key);
    std::unordered_map<std::string, double> ret;
    for (const auto &[field, value] : values) {
        try {
            ret.emplace(field, std::stod(value));
        } catch (const std::exception &) {
            // Ignore malformed metrics fields. They are not useful for numeric display.
        }
    }
    return ret;
}

auto AggregateMetric(const std::unordered_map<std::string, double> &metrics,
                     const std::string &instance,
                     const std::string &channel,
                     std::string_view direction,
                     const std::vector<SocketInfo> &sockets,
                     std::string_view separator) -> std::optional<double>
{
    bool found = false;
    double ret = 0.0;
    std::set<int> indexes;
    for (const auto &socket : sockets) {
        if (socket.fInstance == instance && socket.fChannel == channel) {
            indexes.emplace(socket.fIndex);
        }
    }
    if (indexes.empty()) {
        indexes.emplace(0);
    }

    for (const auto index : indexes) {
        const auto field = daq::service::MakeSocketMetricField(instance, channel, index, direction, separator);
        const auto iter = metrics.find(field);
        if (iter != metrics.end()) {
            ret += iter->second;
            found = true;
        }
    }
    return found ? std::optional<double>{ret} : std::nullopt;
}

auto StatusForDiff(double diff, const Options &options) -> std::string_view
{
    if (diff > options.fDiffHigh) {
        return "HIGH";
    }
    if (diff < options.fDiffLow) {
        return "LOW";
    }
    return "OK";
}

auto ColorForStatus(std::string_view status, bool use_color) -> std::string
{
    if (!use_color) {
        return "";
    }
    if (status == "HIGH") {
        return "\033[31m";
    }
    if (status == "LOW") {
        return "\033[34m";
    }
    if (status == "OK") {
        return "\033[32m";
    }
    if (status == "WAIT" || status == "MISSING" || status == "UNKNOWN") {
        return "\033[33m";
    }
    return "";
}

auto ResetColor(bool use_color) -> std::string
{
    return use_color ? "\033[0m" : "";
}

auto FormatDiff(std::optional<double> value, const Options &options, bool use_color) -> std::string
{
    if (!value) {
        return "n/a";
    }
    const auto status = StatusForDiff(*value, options);
    std::ostringstream out;
    out << ColorForStatus(status, use_color) << std::fixed << std::setprecision(1) << *value << ResetColor(use_color);
    return out.str();
}

auto Shorten(std::string value, std::size_t width) -> std::string
{
    if (value.size() <= width) {
        return value;
    }
    if (width <= 1) {
        return value.substr(0, width);
    }
    return value.substr(0, width - 1) + "~";
}

auto HasPeer(const RuntimeChannel &channel, const RuntimeChannel &peer) -> bool
{
    return std::find(channel.fPeers.cbegin(), channel.fPeers.cend(), peer.fKey) != channel.fPeers.cend();
}

auto HasAddress(const SocketInfo &socket) -> bool
{
    const auto iter = socket.fFields.find("address");
    return iter != socket.fFields.end() && !iter->second.empty() && iter->second != "unspecified";
}

auto SocketIndexes(const Snapshot &snapshot,
                   const std::string &service,
                   const std::string &instance,
                   const std::string &channel) -> std::set<int>
{
    std::set<int> ret;
    for (const auto &socket : snapshot.fSockets) {
        if (socket.fService == service && socket.fInstance == instance && socket.fChannel == channel && HasAddress(socket)) {
            ret.emplace(socket.fIndex);
        }
    }
    return ret;
}

auto FindChannel(const Snapshot &snapshot, const std::string &service, const std::string &instance, const std::string &channel)
    -> const RuntimeChannel *
{
    const auto key = service + "\n" + instance + "\n" + channel;
    const auto iter = snapshot.fChannels.find(key);
    if (iter == snapshot.fChannels.end()) {
        return nullptr;
    }
    return &iter->second;
}

auto ChannelMapKey(const RuntimeChannel &channel) -> std::string
{
    return channel.fService + "\n" + channel.fInstance + "\n" + channel.fChannel;
}

auto CompareInstance(const InstanceInfo &lhs, const InstanceInfo &rhs) -> bool
{
    if (lhs.fService != rhs.fService) {
        return lhs.fService < rhs.fService;
    }
    return lhs.fInstance < rhs.fInstance;
}

auto ReadInstances(sw::redis::Redis &redis, const Options &options, const std::string &service) -> std::vector<InstanceInfo>
{
    std::map<std::string, InstanceInfo> instances;
    const auto state_keys = daq::service::scan(redis,
                           {std::string{daq::service::TopPrefix}, service, "*", std::string{daq::service::FairMQStatePrefix}},
                           options.fSeparator);
    for (const auto &key : state_keys) {
        auto parsed = ParseInstanceFromStateKey(key, options.fSeparator);
        if (!parsed) {
            continue;
        }
        auto state = redis.get(key);
        if (state) {
            parsed->fState = *state;
        }
        instances[parsed->fInstance] = *parsed;
    }

    const auto presence_keys = daq::service::scan(redis,
                              {std::string{daq::service::TopPrefix}, service, "*", std::string{daq::service::PresencePrefix}},
                              options.fSeparator);
    for (const auto &key : presence_keys) {
        const auto parts = SplitKey(key, options.fSeparator);
        if (parts.size() != 4) {
            continue;
        }
        auto &inst = instances[parts[2]];
        inst.fService = service;
        inst.fInstance = parts[2];
    }

    for (auto &[instance, info] : instances) {
        const auto health_key = daq::service::join({std::string{daq::service::TopPrefix}, service, instance, std::string{daq::service::HealthPrefix}},
                               options.fSeparator);
        info.fHealth = GetHash(redis, health_key);
        if (info.fState.empty()) {
            if (const auto iter = info.fHealth.find("fair:mq:state"); iter != info.fHealth.end()) {
                info.fState = iter->second;
            }
        }
    }

    std::vector<InstanceInfo> ret;
    ret.reserve(instances.size());
    for (auto &[instance, info] : instances) {
        ret.push_back(std::move(info));
    }
    std::sort(ret.begin(), ret.end(), CompareInstance);
    return ret;
}

auto ReadSnapshot(sw::redis::Redis &redis, sw::redis::Redis &metrics, const Options &options) -> Snapshot
{
    Snapshot ret;
    ret.fInstancesA = ReadInstances(redis, options, options.fServiceA);
    ret.fInstancesB = ReadInstances(redis, options, options.fServiceB);

    for (const auto &[service, channel] : {std::pair{options.fServiceA, options.fChannelA}, std::pair{options.fServiceB, options.fChannelB}}) {
        const auto endpoint_key = daq::service::join({std::string{daq::service::TopPrefix}, "topology", "endpoint", service, channel},
                                options.fSeparator);
        if (!ParseEndpointKey(endpoint_key, options.fSeparator)) {
            ret.fWarnings.push_back("malformed endpoint key pattern: " + endpoint_key);
        } else if (!redis.exists(endpoint_key)) {
            ret.fWarnings.push_back("endpoint is not configured: " + service + ":" + channel);
        }
    }

    const auto channel_keys = daq::service::scan(redis, {std::string{daq::service::TopPrefix}, "*", "*", "channel", "*"}, options.fSeparator);
    for (const auto &key : channel_keys) {
        if (boost::algorithm::ends_with(key, options.fSeparator + "peer")) {
            continue;
        }
        auto parsed = ParseRuntimeChannelKey(key, options.fSeparator);
        if (!parsed) {
            ret.fWarnings.push_back("malformed channel key: " + key);
            continue;
        }
        const auto wanted = (parsed->fService == options.fServiceA && parsed->fChannel == options.fChannelA)
                         || (parsed->fService == options.fServiceB && parsed->fChannel == options.fChannelB);
        if (!wanted) {
            continue;
        }
        parsed->fFields = GetHash(redis, key);
        const auto peer_key = key + options.fSeparator + "peer";
        if (redis.exists(peer_key) > 0) {
            parsed->fPeers = GetList(redis, peer_key);
        }
        ret.fChannels.emplace(ChannelMapKey(*parsed), std::move(*parsed));
    }

    const auto socket_keys = daq::service::scan(redis, {std::string{daq::service::TopPrefix}, "*", "*", "socket", "chans.*"}, options.fSeparator);
    for (const auto &key : socket_keys) {
        auto parsed = ParseSocketKey(key, options.fSeparator);
        if (!parsed) {
            ret.fWarnings.push_back("malformed socket key: " + key);
            continue;
        }
        const auto wanted = (parsed->fService == options.fServiceA && parsed->fChannel == options.fChannelA)
                         || (parsed->fService == options.fServiceB && parsed->fChannel == options.fChannelB);
        if (!wanted) {
            continue;
        }
        parsed->fFields = GetHash(redis, key);
        ret.fSockets.push_back(std::move(*parsed));
    }
    std::sort(ret.fSockets.begin(), ret.fSockets.end(), [](const auto &lhs, const auto &rhs) {
        return std::tie(lhs.fService, lhs.fInstance, lhs.fChannel, lhs.fIndex) < std::tie(rhs.fService, rhs.fInstance, rhs.fChannel, rhs.fIndex);
    });

    try {
        const auto num_msg = ReadDoubleHash(metrics, daq::service::join({std::string{daq::service::MetricsPrefix},
                                                                        std::string{daq::service::NumMessagePrefix}},
                                                                       options.fSeparator));
        ret.fMsgIn = num_msg;
        ret.fMsgOut = num_msg;
    } catch (const std::exception &e) {
        ret.fMetricsError = e.what();
    }
    return ret;
}

auto ConnectionStatus(const Snapshot &snapshot,
                      const InstanceInfo &left,
                      const std::string &left_channel_name,
                      const InstanceInfo &right,
                      const std::string &right_channel_name) -> std::string
{
    const auto *left_channel = FindChannel(snapshot, left.fService, left.fInstance, left_channel_name);
    const auto *right_channel = FindChannel(snapshot, right.fService, right.fInstance, right_channel_name);
    if (left_channel == nullptr || right_channel == nullptr) {
        return "MISSING";
    }
    const auto left_has_right = HasPeer(*left_channel, *right_channel);
    const auto right_has_left = HasPeer(*right_channel, *left_channel);
    if (left_has_right || right_has_left) {
        const auto left_indexes = SocketIndexes(snapshot, left.fService, left.fInstance, left_channel_name);
        const auto right_indexes = SocketIndexes(snapshot, right.fService, right.fInstance, right_channel_name);
        if (!left_indexes.empty() && !right_indexes.empty()) {
            return "OK";
        }
        return "WAIT";
    }
    const auto left_bound = left_channel->fFields.find("bound");
    const auto right_bound = right_channel->fFields.find("bound");
    if ((left_bound != left_channel->fFields.end() && left_bound->second == "1")
        || (right_bound != right_channel->fFields.end() && right_bound->second == "1")) {
        return "WAIT";
    }
    return "UNKNOWN";
}

auto ConnectionDetail(const Snapshot &snapshot,
                      const InstanceInfo &left,
                      const std::string &left_channel_name,
                      const InstanceInfo &right,
                      const std::string &right_channel_name) -> std::string
{
    const auto status = ConnectionStatus(snapshot, left, left_channel_name, right, right_channel_name);
    const auto left_indexes = SocketIndexes(snapshot, left.fService, left.fInstance, left_channel_name);
    const auto right_indexes = SocketIndexes(snapshot, right.fService, right.fInstance, right_channel_name);
    std::ostringstream out;
    out << status << "(" << left_indexes.size() << "/" << right_indexes.size() << ")";
    return out.str();
}

auto PrintConnectionMatrix(std::ostream &out, const Snapshot &snapshot, const Options &options, bool use_color) -> void
{
    out << "\nConnection sub-channel status\n";
    out << "Rows: " << options.fServiceA << ":" << options.fChannelA
        << "  Columns: " << options.fServiceB << ":" << options.fChannelB << "\n\n";
    out << std::left << std::setw(18) << "A\\B";
    for (const auto &b : snapshot.fInstancesB) {
        out << std::setw(14) << Shorten(b.fInstance, 13);
    }
    out << '\n';
    for (const auto &a : snapshot.fInstancesA) {
        out << std::left << std::setw(18) << Shorten(a.fInstance, 17);
        for (const auto &b : snapshot.fInstancesB) {
            const auto status = ConnectionStatus(snapshot, a, options.fChannelA, b, options.fChannelB);
            const auto detail = ConnectionDetail(snapshot, a, options.fChannelA, b, options.fChannelB);
            out << ColorForStatus(status, use_color) << std::setw(14) << detail << ResetColor(use_color);
        }
        out << '\n';
    }
}

auto PrintTrafficMatrix(std::ostream &out,
                        const Snapshot &snapshot,
                        const Options &options,
                        bool use_color,
                        std::string_view title,
                        const std::string &row_channel,
                        const std::unordered_map<std::string, double> &row_metric,
                        std::string_view row_direction,
                        const std::string &column_channel,
                        const std::unordered_map<std::string, double> &column_metric,
                        std::string_view column_direction) -> void
{
    out << "\n" << title << " message-rate diff [msg/s]\n";
    out << std::left << std::setw(18) << "A\\B";
    for (const auto &b : snapshot.fInstancesB) {
        out << std::setw(14) << Shorten(b.fInstance, 13);
    }
    out << '\n';

    for (const auto &a : snapshot.fInstancesA) {
        out << std::left << std::setw(18) << Shorten(a.fInstance, 17);
        for (const auto &b : snapshot.fInstancesB) {
            const auto row_value = AggregateMetric(row_metric, a.fInstance, row_channel, row_direction, snapshot.fSockets, options.fSeparator);
            const auto column_value = AggregateMetric(column_metric, b.fInstance, column_channel, column_direction, snapshot.fSockets, options.fSeparator);
            std::optional<double> diff;
            if (row_value && column_value) {
                diff = *row_value - *column_value;
            }
            out << std::setw(14) << FormatDiff(diff, options, use_color);
        }
        out << '\n';
    }
}

auto PrintReverseTrafficMatrix(std::ostream &out, const Snapshot &snapshot, const Options &options, bool use_color) -> void
{
    out << "\n" << options.fServiceB << ":" << options.fChannelB << " -> "
        << options.fServiceA << ":" << options.fChannelA << " message-rate diff [msg/s]\n";
    out << std::left << std::setw(18) << "A\\B";
    for (const auto &b : snapshot.fInstancesB) {
        out << std::setw(14) << Shorten(b.fInstance, 13);
    }
    out << '\n';

    for (const auto &a : snapshot.fInstancesA) {
        out << std::left << std::setw(18) << Shorten(a.fInstance, 17);
        for (const auto &b : snapshot.fInstancesB) {
            const auto b_out = AggregateMetric(snapshot.fMsgOut, b.fInstance, options.fChannelB, "out", snapshot.fSockets, options.fSeparator);
            const auto a_in = AggregateMetric(snapshot.fMsgIn, a.fInstance, options.fChannelA, "in", snapshot.fSockets, options.fSeparator);
            std::optional<double> diff;
            if (b_out && a_in) {
                diff = *b_out - *a_in;
            }
            out << std::setw(14) << FormatDiff(diff, options, use_color);
        }
        out << '\n';
    }
}

auto PrintSnapshot(std::ostream &out, const Snapshot &snapshot, const Options &options, const Terminal &terminal) -> void
{
    const auto use_color = terminal.fTty && !options.fNoColor;
    out << "NestDAQ link check  " << options.fServiceA << ":" << options.fChannelA << " <-> "
        << options.fServiceB << ":" << options.fChannelB
        << "  updated=" << NowString()
        << "  diff-low=" << options.fDiffLow
        << "  diff-high=" << options.fDiffHigh << "\n";

    if (snapshot.fInstancesA.empty()) {
        out << "warning: no live instances found for " << options.fServiceA << '\n';
    }
    if (snapshot.fInstancesB.empty()) {
        out << "warning: no live instances found for " << options.fServiceB << '\n';
    }

    PrintConnectionMatrix(out, snapshot, options, use_color);
    PrintTrafficMatrix(out,
                       snapshot,
                       options,
                       use_color,
                       options.fServiceA + ":" + options.fChannelA + " -> " + options.fServiceB + ":" + options.fChannelB,
                       options.fChannelA,
                       snapshot.fMsgOut,
                       "out",
                       options.fChannelB,
                       snapshot.fMsgIn,
                       "in");
    PrintReverseTrafficMatrix(out, snapshot, options, use_color);

    out << "\nLegend: connection OK=peer key found, WAIT=runtime channel exists but peer unresolved, "
        << "MISSING=runtime channel missing, UNKNOWN=insufficient data\n";
    out << "Traffic cells are sender msg-out rate minus receiver msg-in rate. Colors: LOW/OK/HIGH by thresholds.\n";

    if (!snapshot.fMetricsError.empty()) {
        out << "\nMetrics warning: " << snapshot.fMetricsError << '\n';
    }
    if (!snapshot.fWarnings.empty()) {
        out << "\nWarnings\n";
        for (const auto &warning : snapshot.fWarnings) {
            out << "  " << warning << '\n';
        }
    }
}

auto OnSignal(int /*signal*/) -> void
{
    g_stop_requested = 1;
}

} // namespace
} // namespace nestdaq

int main(int argc, char *argv[]) // NOLINT(bugprone-exception-escape,cppcoreguidelines-avoid-c-arrays)
{
    std::cin.tie(nullptr);
    std::ios::sync_with_stdio(false);

    for (int i = 1; i < argc; ++i) {
        const std::string arg{argv[i]}; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        if (arg == "--help" || arg == "-h") {
            std::cout << nestdaq::MakeOptionDescription() << '\n';
            return EXIT_SUCCESS;
        }
    }

    const auto parsed = nestdaq::ParseOptions(argc, argv);
    if (!parsed) {
        return EXIT_FAILURE;
    }
    const auto &options = *parsed;

    std::signal(SIGINT, nestdaq::OnSignal);
    std::signal(SIGTERM, nestdaq::OnSignal);

    const auto redis_uri = nestdaq::NormalizeRedisUri(options.fRedisUrlDaqService);
    if (!redis_uri) {
        std::cerr << "error: --redis-url-daq_service must be host-address:port/db or tcp://host-address:port/db\n";
        return EXIT_FAILURE;
    }

    const auto metrics_uri = nestdaq::NormalizeRedisUri(options.fRedisUrlMetrics);
    if (!metrics_uri) {
        std::cerr << "error: --redis-url-metrics must be host-address:port/db or tcp://host-address:port/db\n";
        return EXIT_FAILURE;
    }

    auto terminal = nestdaq::GetTerminal();
    if (!terminal.fTty && options.fRefreshMs == 1000) {
        terminal.fTty = false;
    }

    try {
        sw::redis::Redis redis{*redis_uri};
        sw::redis::Redis metrics{*metrics_uri};

        while (nestdaq::g_stop_requested == 0) {
            if (terminal.fTty && options.fRefreshMs > 0) {
                std::cout << "\033[2J\033[H";
            }
            const auto snapshot = nestdaq::ReadSnapshot(redis, metrics, options);
            nestdaq::PrintSnapshot(std::cout, snapshot, options, terminal);
            std::cout << std::flush;

            if (options.fRefreshMs == 0 || !terminal.fTty) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(options.fRefreshMs));
        }
    } catch (const std::exception &e) {
        std::cerr << "error: Redis read failed: " << e.what() << '\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
