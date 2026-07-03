/** @file
 *  @brief Implements Redis-backed parameter configuration loading.
 */

#include <iostream>
#include <iterator>
#include <unordered_map>
#include <vector>

#include <boost/algorithm/string.hpp>

#include <fairlogger/Logger.h>

#include <sw/redis++/redis++.h>

#include "plugins/Constants.h"
#include "plugins/Functions.h"
#include "plugins/ParameterConfigPlugin.h"

using namespace std::literals::string_literals;

static constexpr std::string_view kMyClass{"daq::service::ParameterConfigPlugin"};

static constexpr std::string_view kRedisKeySpacePrefix{"__keyspace@"};

const std::unordered_set<std::string_view> reservedOptionsString
{   "id",  //
    "transport", //
    "network-interface", //
    "shm-allocation", //
    "session", //
    "config-key", //
    "mq-config", //
    "severity", //
    "file-severity", //
    "verbosity", //
    "log-to-file", //
};

const std::unordered_set<std::string_view> reservedOptionsInt
{   "io-threads", //
    "init-timeout", //
};

const std::unordered_set<std::string_view> reservedOptionsBool
{   "print-channels", //
    "shm-mlock-segment", //
    "shm-zero-segment", //
    "shm-throw-bad-alloc", //
    "shm-monitor", //
};

const std::unordered_set<std::string_view> reservedOptionsSize
{   "shm-segment-size", //
    "color", //
};

const std::unordered_set<std::string_view> reservedOptionsUint16
{   "shm-segment-id", //
};

const std::unordered_set<std::string_view> reservedOptionsFloat
{   "rate", //
};

const std::unordered_set<std::string_view> reservedOptionsVectorString
{   "channel-config", //
};
namespace daq::service {

/**
 * @brief Return command-line options provided by the parameter configuration plugin.
 */
auto parameterConfigPluginProgramOptions() -> fair::mq::Plugin::ProgOptions
{
    namespace bpo = boost::program_options;
    using opt = ParameterConfigPlugin::OptionKey;
    auto options = bpo::options_description(kMyClass.data());
    options.add_options()
           (opt::ServerUri.data(), bpo::value<std::string>(), "Redis server URI (if empty, the same URI of the service registry is used.)");
    return options;
}

/**
 * @brief Construct the plugin, load Redis-backed parameters, and start watching for changes.
 */
ParameterConfigPlugin::ParameterConfigPlugin(std::string_view name,
        const fair::mq::Plugin::Version &version,
        std::string_view maintainer,
        std::string_view homepage,
        fair::mq::PluginServices *pluginServices)
    : fair::mq::Plugin(name.data(), version, maintainer.data(), homepage.data(), pluginServices)
{
    LOG(debug) << kMyClass << " hello";
    using opt = ParameterConfigPlugin::OptionKey;
    std::string serverUri;
    if (PropertyExists(opt::ServerUri.data())) {
        serverUri = GetProperty<std::string>(opt::ServerUri.data());
    } else if (PropertyExists(ServiceRegistryUri.data())) {
        serverUri = GetProperty<std::string>(ServiceRegistryUri.data());
    }
    if (!serverUri.empty()) {
        fClient = std::make_shared<sw::redis::Redis>(serverUri);
    }

    SubscribeToDeviceStateChange([this](DeviceState newState) {
        // LOG(debug) << kMyClass << " state change: " << GetStateName(newState);
        try {
            switch (newState) {
            case DeviceState::Error:
            case DeviceState::Exiting:
                fPluginShutdownRequested = true;
            default:
                break;
            }
        } catch (const std::exception &e) {
            LOG(error) << kMyClass << " exception during device state change: " << e.what();
        } catch (...) {
            LOG(error) << kMyClass << " exception during device state change: unknow exception";
        }
    });

    if (fClient) {
        readParameters();
    }
    fSubscriberThread = std::thread([this]() {
        try {
            subscribeToParameterChange();
        } catch (const std::exception &e) {
            LOG(error) << kMyClass << " in CheckThread" << e.what();
        } catch (...) {
            LOG(error) << kMyClass << " unknown error in CheckThread";
        }
    });
    fSubscriberThread.detach();
}

ParameterConfigPlugin::~ParameterConfigPlugin()
{
    UnsubscribeFromDeviceStateChange();
    LOG(debug) << kMyClass << " bye";
}

/**
 * @brief Check whether a parameter name maps to a FairMQ reserved option type.
 */
bool ParameterConfigPlugin::isReservedOption(std::string_view name)
{
    if (reservedOptionsString.count(name)>0) {
        return true;
    }
    if (reservedOptionsInt.count(name)>0) {
        return true;
    }
    if (reservedOptionsBool.count(name)>0) {
        return true;
    }
    if (reservedOptionsSize.count(name)>0) {
        return true;
    }
    if (reservedOptionsUint16.count(name)>0) {
        return true;
    }
    if (reservedOptionsFloat.count(name)>0) {
        return true;
    }
    if (reservedOptionsVectorString.count(name)>0) {
        return true;
    }
    return false;
}

/**
 * @brief Parse one Redis parameter value and store it as a FairMQ property.
 *
 * Reserved FairMQ options are converted to their known target types. Other
 * values are interpreted as strings, arrays, or maps based on comma and equals
 * separators.
 */
void ParameterConfigPlugin::parse(std::string_view name, std::string line)
{
    //LOG(debug) << " parameter: field = " << name << ", value = " << line;
    if (isReservedOption(name)) {
        setPropertyOfReservedOption(name, line);
    } else if (line.find(",")==std::string::npos) {
        setPropertyFromString<std::string>(name, line);
    } else if (line.find("=")==std::string::npos) {
        // <value> has "," but doesn't have "=".
        toArray(name, line);
    } else {
        // <value> has "," and "=".
        toMap(name, line);
    }
}

/**
 * @brief Read a Redis hash and apply its fields as parameters.
 */
void ParameterConfigPlugin::readHash(const std::string& name)
{
    std::unordered_map<std::string, std::string> h;
    fClient->hgetall(name, std::inserter(h, h.begin()));
    std::string prefix = (fKey==name)||(fGroupKey==name) ? ""s : name.substr(name.find_last_of(fSeparator)+1).data();
    //LOG(info) << " prefix = " << prefix;
    for (const auto &[field, value] : h) {
        auto f = prefix.empty() ? field : (prefix + fSeparator + field);
        //LOG(info) << " f = " << f << ", field = " << field << ", value = " << value;
        parse(f, value);
    }
}

/**
 * @brief Read a Redis list and apply it as a vector property.
 */
void ParameterConfigPlugin::readList(const std::string& name)
{
    std::vector<std::string> v;
    fClient->lrange(name, 0, -1, std::back_inserter(v));

    std::string ss;
    for (const auto& x : v) {
        ss += x + ", ";
    }
    LOG(debug) << " list: name = " << name << ", values = " << ss;
    if (!v.empty()) {
        SetProperty(name, v);
    }
}

/**
 * @brief Load group, instance, and nested parameter keys from Redis.
 */
void ParameterConfigPlugin::readParameters()
{
    //LOG(debug) << kMyClass << " " << __FUNCTION__;

    if (fId.empty()) {
        if (PropertyExists("id")>0) {
            fId = GetProperty<std::string>("id");
        } else {
            return;
        }
    }

    if (fSeparator.empty()) {
        if (PropertyExists(Separator.data())>0) {
            fSeparator = GetProperty<std::string>(Separator.data());
        } else {
            return;
        }
    }

    //LOG(debug) << " separator  = " << fSeparator;

    if (fKey.empty()) {
        fKey = kParametersPrefix.data() + fSeparator + fId;
    }
    if (fGroupKey.empty()) {
        auto lastHyphen = fKey.find_last_of("-");
        auto idx = fKey.substr(lastHyphen+1);
        bool isNumber{true};
        for (const auto& c : idx) {
            if (!std::isdigit(c)) {
                isNumber = false;
                break;
            }
        }
        if (isNumber) {
            fGroupKey = fKey.substr(0, lastHyphen);
        }
    }

    //LOG(debug) << " parameter config key = " << fKey;
    readHash(fGroupKey);
    readHash(fKey);

    for (const auto &k : {
                fGroupKey, fKey
            }) {
        if (k.empty()) {
            continue;
        }
        auto scanKey = k + fSeparator + "*";
        //LOG(debug) << " parameter read hash done. scanning additional parameters ... : " << scanKey;
        const auto keys = scan(*fClient, scanKey);
        if (!keys.empty()) {
            LOG(debug) << " additional parameters found.";
            for (const auto & x : keys) {
                auto t = fClient->type(x);
                LOG(debug) << " key = " << x << ", type = " << t;
                if (t=="string") {
                    readString(x);
                } else if (t=="list") {
                    readList(x);
                } else if (t=="hash") {
                    readHash(x);
                } else if (t=="set") {
                    readSet(x);
                } else if (t=="zset") {
                    readZset(x);
                }
            }
        }
    }

    //LOG(debug) << kMyClass << " " << __FUNCTION__ << " done";
}

/**
 * @brief Read a Redis set and apply it as an unordered-set property.
 */
void ParameterConfigPlugin::readSet(const std::string& name)
{
    std::unordered_set<std::string> members;
    fClient->smembers(name, std::inserter(members, members.begin()));

    std::string ss;
    for (const auto & x : members) {
        ss += x + ", ";
    }
    LOG(debug) << " set: name = " << name << ", values = " << ss;

    if (!members.empty()) {
        SetProperty(name, members);
    }
}

/**
 * @brief Read a Redis string and parse it as a parameter value.
 */
void ParameterConfigPlugin::readString(const std::string& name)
{
    auto value = fClient->get(name);
    if (!value) {
        return;
    }
    std::string field = name.substr(name.find_last_of(fSeparator)+1).data();
    LOG(debug) << " string: name = " << field << ", value = " << *value;
    parse(field, *value);
}

/**
 * @brief Read a Redis sorted set and apply it as a value-to-score map property.
 */
void ParameterConfigPlugin::readZset(const std::string& name)
{
    std::unordered_map<std::string, double> m;
    fClient->zrange(name, 0, -1, std::inserter(m, m.end()));
    std::string ss;
    for (const auto & [k, v] : m) {
        ss += "{" + k + ": " + std::to_string(v) + "}, ";
    }
    LOG(debug) << " zset: name = " << name << ", values = " << ss;

    if (!m.empty()) {
        SetProperty(name, m);
    }
}

/**
 * @brief Convert and store a Redis value for a FairMQ reserved option.
 */
void ParameterConfigPlugin::setPropertyOfReservedOption(std::string_view name, std::string_view value)
{
    if (reservedOptionsString.count(name)>0) {
        setPropertyFromString<std::string>(name,  value);
        return;
    }

    if (reservedOptionsInt.count(name)>0) {
        setPropertyFromString<int>(name, value);
        return;
    }

    if (reservedOptionsSize.count(name)>0) {
        setPropertyFromString<uint64_t>(name, value);
        return;
    }

    if (reservedOptionsBool.count(name)>0) {
        setPropertyFromString<bool>(name, value);
        return;
    }

    if (reservedOptionsFloat.count(name)>0) {
        setPropertyFromString<float>(name, value);
        return;
    }

    if (reservedOptionsVectorString.count(name)>0) {
        toArray(name, value.data());
        return;
    }
}

/**
 * @brief Subscribe to Redis keyspace notifications for parameter changes.
 */
void ParameterConfigPlugin::subscribeToParameterChange()
{
    using opt = ParameterConfigPlugin::OptionKey;
    LOG(debug) << " create a subscriber. (parameter change)";
    auto sub = fClient->subscriber();

    const auto &serverUri = GetProperty<std::string>(opt::ServerUri.data());
    const auto dbNumber = serverUri.substr(serverUri.find_last_of("/")+1);
    LOG(debug) << " db number = " << dbNumber;
    const std::string redisKeySpaceNotificationChannel = kRedisKeySpacePrefix.data() + dbNumber + "__:"s + fKey;
    const std::string redisKeySpaceNotificationGroupChannel = kRedisKeySpacePrefix.data() + dbNumber + "__:"s + fGroupKey;
    LOG(debug) << " key-space-notification channel = " << redisKeySpaceNotificationChannel << ", " << redisKeySpaceNotificationGroupChannel;

    sub.on_message([this, &redisKeySpaceNotificationChannel, &redisKeySpaceNotificationGroupChannel](auto channel, auto /*msg*/) {
        //LOG(debug) << kMyClass << " on_message(MESSAGE): channel = " << channel << " msg = " << msg;
        if (redisKeySpaceNotificationChannel!=channel && redisKeySpaceNotificationGroupChannel!=channel) {
            return;
        }
        readParameters();
    });

    sub.subscribe({redisKeySpaceNotificationChannel, redisKeySpaceNotificationGroupChannel});

    while (!fPluginShutdownRequested) {
        try {
            sub.consume();
        } catch (const sw::redis::TimeoutError &e) {
            // try again.
        } catch (const sw::redis::Error &e) {
            LOG(error) << kMyClass << "::" << __func__ << ": error in consume(): " << e.what();
            break;
        } catch (const std::exception& e) {
            LOG(error) << kMyClass << "::" << __func__ << ": error in consume(): " << e.what();
            break;
        } catch (...) {
            LOG(error) << kMyClass << "::" << __func__ << ": unknown exception";
            break;
        }
    }
    LOG(debug) << " " << __func__ << " exit.";
}

/**
 * @brief Convert a delimited Redis value into a vector property.
 */
void ParameterConfigPlugin::toArray(std::string_view name, std::string line)
{
    std::vector<std::string> v;
    // remove left-space and right space
    boost::trim_if(line, boost::is_space());
    // split at "," or " "  (ignore empty string)
    boost::split(v, line, boost::is_any_of(", "), boost::token_compress_on);
    std::string ss;
    for (const auto& x : v) {
        ss += x + ", ";
    }
    LOG(debug) << " parameter (vector): field = " << name << ", values = " << ss;
    SetProperty(name.data(), v);
}

/**
 * @brief Convert a delimited key-value Redis value into a map property.
 */
void ParameterConfigPlugin::toMap(std::string_view name, std::string line)
{
    // Assuming the counts of "," and "=" are the same.
    std::vector<std::string> v;
    // remove left-space and right space
    boost::trim_if(line, boost::is_space());
    // split at "," (ignore empty string)
    boost::split(v, line, boost::is_any_of(","), boost::token_compress_on);
    std::unordered_map<std::string, std::string> m;
    std::string ss;
    for (auto& x : v) {
        LOG(debug) << " x = " << x;
        std::vector<std::string> s;
        // remove left-space and right space
        boost::trim_if(x, boost::is_space());
        // split at "=" or " " (ignore empty string)
        boost::split(s, x, boost::is_any_of("= "), boost::token_compress_on);
        if (s.size()==2) {
            m.emplace(s[0], s[1]);
            ss+="{" + s[0] + ": " + s[1] + "}, ";
        }
    }
    LOG(debug) << " parameter (map): field = " << name << ", values = " << ss;
    SetProperty(name.data(), m);
}

} // namespace daq::service
