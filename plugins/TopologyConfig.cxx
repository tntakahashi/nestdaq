/** @file
 *  @brief Implements topology loading from Redis configuration.
 */

#include <algorithm>
#include <cassert>
#include <mutex>
#include <regex>
#include <thread>

#include <boost/algorithm/string.hpp>

#include <sw/redis++/redis++.h>

#include <fairmq/JSONParser.h>
#include <fairmq/SuboptParser.h>

#include "plugins/Constants.h"
#include "plugins/Functions.h"
#include "plugins/tools.h"
#include "plugins/TopologyConfig.h"

static constexpr std::string_view kMyClass{"daq::service::TopologyConfig"};

namespace topology {
static constexpr std::string_view kPrefix{"topology"};
static constexpr std::string_view kEndpointPrefix{"endpoint"};
static constexpr std::string_view kLinkPrefix{"link"};

static constexpr std::string_view kChannelPrefix{"channel"};
static constexpr std::string_view kPeerPrefix{"peer"};
static constexpr std::string_view kSocketPrefix{"socket"};

static const std::vector<std::string> kWaitDeviceReadyTargets {
    GetStateName(fair::mq::State::DeviceReady),
    GetStateName(fair::mq::State::Ready),
    GetStateName(fair::mq::State::Running),
};
}

using namespace std::string_literals;
using namespace std::chrono_literals;

void printConfig(const std::map<std::string, std::string> &p, std::string_view name)
{
    std::ostringstream ss;
    ss << " name = " << name << "\n";
    for (const auto &[k, v] : p) {
        ss << " key = " << k << ", value = " << v << "\n";
    }
    LOG(debug) << ss.str();
}

std::string makeAddress(const std::string &address, std::string_view peer_ip)
{
    // e.g. address = tcp://AAAA:XXXX
    auto pos_port = address.find_last_of(":");
    auto pos_star = address.find("*");
    auto pos0000 = address.find("0.0.0.0");
    if (address.find("tcp://")==0) {
        if ((pos_star!=std::string::npos) || (pos0000!=std::string::npos)) {
            return address.substr(0, 6) + peer_ip.data() + address.substr(pos_port);
        }
    }
    return address;
}

// convert a socket property to format of command line option of FairMQ
const std::string toChannelConfig(const daq::service::SocketProperty& p)
{
    using namespace std::string_literals;
    std::string ret;

//  LOG(debug) << __FUNCTION__ << " address = " << p.address.size() << " " << p.address;
    std::string address;
    if (!p.address.empty() && p.address!="unspecified" && p.address.find(",")==std::string::npos) {
        address = p.address;
    } else {

        if (address.empty() || address=="unspecified") {
            address="unspecified";
            for (auto i=0; i<p.num_sockets-1; ++i) {
                address += ",address=unspecified";
            }
        }
    }

    if (p.address.find(",")!=std::string::npos) {
        std::vector<std::string> res;
        boost::split(res, p.address, boost::is_any_of(","));
        const auto num_sockets = static_cast<std::vector<std::string>::size_type>(p.num_sockets);
        if (res.size()<num_sockets) {
            auto n = num_sockets - res.size();
            for (auto i=0u; i<n; ++i) {
                res.push_back("unspecified");
            }
        }
        address = boost::join(res, ",address=");
    }

    // Only FairMQ's supported parameters are allowed for channel-config
    ret = "name="s           + p.name                          //
          + ",type="s          + p.type                          //
          + ",method="s        + p.method                        //
          + ",address="s       + address                         //
          + ",transport="s     + p.transport                     //
          + ",rcvBufSize="s    + std::to_string(p.rcv_buf_size)    //
          + ",sndBufSize="s    + std::to_string(p.snd_buf_size)    //
          + ",rcvKernelSize="s + std::to_string(p.rcv_kernel_size) //
          + ",sndKernelSize="s + std::to_string(p.snd_kernel_size) //
          + ",linger="s        + std::to_string(p.linger)        //
          + ",rateLogging="s   + std::to_string(p.rate_logging)   //
          + ",portRangeMin="s  + std::to_string(p.port_range_min)  //
          + ",portRangeMax="s  + std::to_string(p.port_range_max)  //
          + ",autoBind="s      + std::to_string(p.auto_bind);      //

    LOG(debug) << __FUNCTION__ << " ret = " << ret;
    return ret;

}

// convert hash in redis to struct
template <typename Container>
const daq::service::SocketProperty toSocketProperty(const Container& c)
{
    daq::service::SocketProperty sp;
    for (const auto &[field, value] : c) {
        //ss << ", " << field << " = " << value;
        if (field=="name") {
            sp.name = value;
        } else if (field=="type") {
            sp.type = value;
        } else if (field=="method") {
            sp.method = value;
        } else if (field=="address") {
            sp.address = value;
        } else if (field=="transport") {
            sp.transport = value;
        } else if (field=="sndBufSize") {
            sp.snd_buf_size = std::stoi(value);
        } else if (field=="rcvBufSize") {
            sp.rcv_buf_size = std::stoi(value);
        } else if (field=="sndKernelSize") {
            sp.snd_kernel_size = std::stoi(value);
        } else if (field=="rcvKernelSize") {
            sp.rcv_kernel_size = std::stoi(value);
        } else if (field=="linger") {
            sp.linger = std::stoi(value);
        } else if (field=="rateLogging") {
            sp.rate_logging = std::stoi(value);
        } else if (field=="portRangeMin") {
            sp.port_range_min = std::stoi(value);
        } else if (field=="portRangeMax") {
            sp.port_range_max = std::stoi(value);
        } else if (field=="autoBind") {
            const auto& v = boost::to_lower_copy(value);
            sp.auto_bind = (v=="1") || (v=="true");
        } else if (field=="num_sockets") {
            sp.num_sockets = std::stoi(value);
        } else if (field=="autoSubChannel") {
            const auto& v = boost::to_lower_copy(value);
            sp.auto_sub_channel = (v=="1") || (v=="true");
        } else if (field=="bound") {
            const auto& v = boost::to_lower_copy(value);
            sp.bound = (v=="1") || (v=="true");
        } else if (field=="waitForPeerConnection") {
            const auto& v = boost::to_lower_copy(value);
            sp.wait_for_peer_connection = (v=="1") || (v=="true");
        }
    }
//  if (sp.auto_sub_channel) {
//    sp.num_sockets = 0;
//  }
    return sp;
}

daq::service::TopologyConfig::TopologyConfig(daq::service::Plugin& plugin)
    : fPlugin(plugin)
{
    try {
        fTopPrefix   = getProperty<std::string>("top-prefix");
        fServiceName = getProperty<std::string>(kServiceName.data());
        fId          = getProperty<std::string>("id");
        fSeparator   = getProperty<std::string>(kSeparator.data());
        fMaxTtl      = getProperty<long long>(kMaxTtl.data());

        LOG(debug) << kMyClass
                   << " top prefix = " << fTopPrefix
                   << "\n service = " << fServiceName
                   << "\n id = " << fId
                   << "\n separator = " << fSeparator
                   << "\n max ttl = " << fMaxTtl;
    } catch (const std::exception &e) {
        LOG(error) << " exception in " << kMyClass << ":" << __LINE__ << " e.what() = " << e.what();
    } catch (...) {
        LOG(error) << " exception in " << kMyClass << ":" << __LINE__ << " unknown";
    }
}

daq::service::TopologyConfig::~TopologyConfig()
{
}

/**
 * @brief Apply explicit connect configuration from the plugin property set.
 *
 * Peer references are resolved through Redis, converted into FairMQ
 * channel-config options, and written back to the device properties.
 */
void daq::service::TopologyConfig::configConnect()
{
    auto find_peer_ip = [this](const auto& service, const auto& id) {
        const auto &id_full = join({service, id}, fSeparator);
        const auto& peer_health_key = join({fTopPrefix, id_full,  kHealthPrefix.data()}, fSeparator);
        auto  peer_ip = getClient()->hget(peer_health_key, "hostIp");
        if (!peer_ip) {
            LOG(warn) << " id = " << id_full << " : hostIp not found";
            return ""s;
        } else {
            LOG(warn) << " id = " << id_full << " : hostIp found " << *peer_ip;
        }
        return *peer_ip;
    };

    auto find_address = [this, find_peer_ip](const auto& service, const auto& id, const auto& channel, const auto& sub_channel_index) {
        const auto &peer_ip = find_peer_ip(service, id);
        if (peer_ip.empty()) {
            return ""s;
        }

        const auto& ch_full = join({service, id, topology::kSocketPrefix.data(), "chans."s+channel+"."s+sub_channel_index}, fSeparator);
        std::string key = join({fTopPrefix, ch_full}, fSeparator);
        // check whether peer address exists
        std::string address;
        int n_retry = 0;
        while (true) {
            auto a = getClient()->hget(key, "address"s);
            if (a) {
                LOG(warn) << " ch = " << ch_full << " : address found " << *a;
                address = *a;
                break;
            }
            LOG(warn) << " ch = " << ch_full << " : address not found";
            if (isCanceled() || n_retry>fMaxRetryToResolveAddress) {
                LOG(warn) << " find address of peer channel = " << ch_full << " -> canceled";
                return ""s;
            }
            std::this_thread::sleep_for(1000ms);
            ++n_retry;
        }
        return makeAddress(address, peer_ip);
    };

    auto find_addresses = [this, find_peer_ip](const auto& service, const auto& id, const auto& channel) {
        std::vector<std::string> ret;
        const auto &peer_ip = find_peer_ip(service, id);
        if (peer_ip.empty()) {
            return ret;
        }

        const auto &socket_key_pattern = join({fTopPrefix, service, id, topology::kSocketPrefix.data(), channel}, fSeparator);
        const auto &socket_keys = scan(*getClient(), socket_key_pattern);
        for (const auto &socket_key : socket_keys) {
            int n_retry = 0;
            while (true) {
                auto a = getClient()->hget(socket_key, "address");
                if (a) {
                    LOG(warn) << " ch = " << socket_key << " : address found " << *a;
                    ret.push_back(makeAddress(*a, peer_ip));
                    break;
                }
                LOG(warn) << " ch = " << socket_key << " : address not found";
                if (isCanceled() || n_retry>fMaxRetryToResolveAddress) {
                    LOG(warn) << " find address of peer channel = " << socket_key << " -> canceled";
                    break;
                }
                std::this_thread::sleep_for(1000ms);
                ++n_retry;
            }
        }
        return ret;
    };

    //LOG(info) << "connect-config = " <<  fConnectConfig;
    const auto& pt = toJson(fConnectConfig);

    //LOG(info) << " connect-config (JSON) = " << toJsonString(pt);
    std::vector<std::string> channel_config_options;
    for (const auto& child : pt) {
        // child.first is string
        //LOG(info) << " channel name = " << child.first;
        auto my_channel_name = child.first;

        auto &sp = fConnectChannels[my_channel_name];

        const auto &peer = child.second.get_child("peer");
        std::vector<std::string> peer_list;
        if (const auto &s = peer.get_value<std::string>(); !s.empty()) {
            // string
            //LOG(info) << " peer : s = " << s;
            peer_list.push_back(s);
        } else {
            // array
            for (const auto &a : peer) {
                const auto &ss = a.second.get_value<std::string>();
                //LOG(info) << " peer (array) : " << ss;
                peer_list.push_back(ss);
            }
        }

        std::vector<std::string> address_list;
        for (const auto &p : peer_list) {

            int n_separators = std::count(p.begin(), p.end(), fSeparator[0]);
            bool has_sub_channel_index = (p.find("[") != std::string::npos);
            if (n_separators==2) {
                if (has_sub_channel_index) {
                    // try to match:  "service" : "instance" - "index" : "channel" ["sub_channel_index"]
                    std::regex pattern{"(\\w+)" + fSeparator + "(\\w+)-(\\d+)" + fSeparator + "(\\w+)\\[(\\d+)\\]"};
                    auto n_marks = pattern.mark_count();
                    std::smatch match_results;
                    std::regex_match(p, match_results, pattern);
                    if (!match_results.ready() || match_results.size()!=(n_marks+1)) {
                        LOG(warn) << " failed to match.  \"service\"" + fSeparator + "\"instance\"-\"index\"" + fSeparator + "\"channel\"[\"sub_channel_index\"]";
                        continue;
                    }
                    const auto& service         = match_results[1].str();
                    const auto& id              = match_results[2].str() + "-"s + match_results[3].str();
                    const auto& channel         = match_results[4].str();
                    const auto& sub_channel_index = match_results[5].str();

                    const auto& a = find_address(service, id, channel, sub_channel_index);
                    if (a.empty()) {
                        continue;
                    }
                    address_list.push_back(a);
                } else {
                    // try to match: "service" : "instance" - "index" : "channel"
                    std::regex pattern{"(\\w+)" + fSeparator + "(\\w+)-(\\d+)" + fSeparator +  "(\\w+)"};
                    auto n_marks = pattern.mark_count();
                    std::smatch match_results;
                    std::regex_match(p, match_results, pattern);
                    if (!match_results.ready() || match_results.size()!=(n_marks+1)) {
                        LOG(warn) << " failed to match.  \"service\"" + fSeparator + "\"instance\"-\"index\"" + fSeparator + "\"channel\"";
                        continue;
                    }
                    const auto& service = match_results[1].str();
                    const auto& id      = match_results[2].str() + "-"s + match_results[3].str();
                    const auto& channel = match_results[4].str();

                    if (!sp.auto_sub_channel) {
                        // infer sub_channel_index = 0
                        const auto& a = find_address(service, id, channel, "0"s);
                        if (a.empty()) {
                            continue;
                        }
                        address_list.push_back(a);
                    } else {
                        // get sub_channel_index (and full key name) from the database
                        const auto &addresses = find_addresses(service, id, channel);
                        address_list.insert(address_list.end(), addresses.begin(), addresses.end());

                    }
                }

            } else if (n_separators==1) {
                if (has_sub_channel_index) {
                    std::string service;
                    std::string id;
                    std::string channel;
                    std::string sub_channel_index;

                    // try to match: "instance" - "index" : "channel" ["sub_channel_index"]
                    std::regex pattern{"(\\w+)-(\\d+)" + fSeparator + "(\\w+)\\[(\\d+)\\]"};
                    auto n_marks = pattern.mark_count();
                    std::smatch match_results;
                    std::regex_match(p, match_results, pattern);

                    if (match_results.ready() && match_results.size()==(n_marks+1)) {
                        const auto &instance = match_results[1].str();
                        const auto &index    = match_results[2].str();
                        channel              = match_results[3].str();
                        sub_channel_index      = match_results[4].str();

                        // infer service name from instance name
                        service = instance;
                        id      = instance + "-"s + index;
                    } else {
                        //LOG(warn) << " failed to match. \"instance\"-\"index\"" + fSeparator + "\"channel\"[\"sub_channel_index\"]";

                        // try to match: "service" : "channel" ["sub_channel_index"]
                        pattern = "(\\w+)" + fSeparator + "(\\w+)\\[(\\d+)\\]";
                        n_marks = pattern.mark_count();
                        std::regex_match(p,  match_results, pattern);
                        if (!match_results.ready() || match_results.size()!=(n_marks+1)) {
                            LOG(warn) << " failed to match. \"service\"" + fSeparator + "\"channel\"[\"sub_channel_index\"]";
                            continue;
                        }

                        service         = match_results[1].str();
                        channel         = match_results[2].str();
                        sub_channel_index = match_results[3].str();

                        // infer instance id from service name
                        id = service + "-0"s;
                    }

                    const auto &a = find_address(service, id, channel, sub_channel_index);
                    if (a.empty()) {
                        continue;
                    }
                    address_list.push_back(a);
                } else {
                    std::string service;
                    std::string id;
                    std::string channel;

                    // try to match:  "instance" - "index" : "channel"
                    std::regex pattern{"(\\w+)-(\\d+)" + fSeparator + "(\\w+)"};
                    auto n_marks = pattern.mark_count();
                    std::smatch match_results;
                    std::regex_match(p, match_results, pattern);
                    if (match_results.ready() && match_results.size()==(n_marks+1)) {
                        const auto &instance = match_results[1].str();
                        const auto &index    = match_results[2].str();
                        channel              = match_results[3].str();

                        // infer service name
                        service = instance;
                        id      = instance + "-"s + index;
                    } else {
                        //LOG(warn) << " failed to match. \"instance\"-\"index\"" + fSeparator + "\"channel\"";

                        // try to match: "service" : "channel"
                        pattern = "(\\w+)" + fSeparator + "(\\w+)";
                        n_marks = pattern.mark_count();
                        std::regex_match(p,  match_results, pattern);
                        if (!match_results.ready() || match_results.size()!=(n_marks+1)) {
                            LOG(warn) << " failed to match. \"service\"" + fSeparator + "\"channel\"";
                            continue;
                        }

                        service = match_results[1].str();
                        channel = match_results[2].str();

                        // infer instance id from service name
                        id = service + "-0"s;
                    }

                    if (!sp.auto_sub_channel) {
                        // infer sub_channel_index = 0
                        const auto &a = find_address(service, id, channel, "0"s);
                        if (a.empty()) {
                            continue;
                        }
                        address_list.push_back(a);
                    } else {
                        // get sub_channel_index (and full key name) from the database
                        const auto &addresses = find_addresses(service, id, channel);
                        address_list.insert(address_list.end(), addresses.begin(), addresses.end());
                    }
                }

            }
        }

        for (const auto& address : address_list) {
            if (!address.empty()) {
                if (sp.address.empty()) {
                    sp.address = address;

                } else {
                    sp.address += ","s + address;
                }
            }
        }

        channel_config_options.emplace_back(toChannelConfig(sp));

    }

    if (channel_config_options.empty()) {
        LOG(info) << __FUNCTION__ << " done (empty)";
        return;
    }

    for (const auto &s : channel_config_options) {
        LOG(info) << " channel config option = " << s;
    }

    try {
        auto properties = fair::mq::SuboptParser(channel_config_options, fServiceName);
        for (const auto & [k, v] : properties) {

            const auto s = fair::mq::PropertyHelper::ConvertPropertyToString(v);
            fCustomChannelProperties[k] = s;
            LOG(debug) << " id = " << fId << " set property : " << k << " " << s;
        }

        setProperties(properties);
    } catch (const std::exception& e) {
        LOG(error) << kMyClass << " error on SetProperty(chans.) : id = " << fId << ": " << e.what();
    } catch (...) {
        LOG(error) << kMyClass << " unknown exception on SetProperty(chans.) :";
    }
    LOG(debug) << __FUNCTION__ << " done";
    //LOG(debug) << " after update";
    //printConfig(getPropertiesAsStringStartingWith("channel-config"), "channel-config");
    //printConfig(getPropertiesAsStringStartingWith("chans."), "chans.");

}

/**
 * @brief Read FairMQ states for peers connected to the given channels.
 */
auto daq::service::TopologyConfig::getPeerState(const MQChannel & channels) -> std::map<std::string, std::string>
{
    std::unordered_set<std::string> peer_keys;
    for (const auto &[name, sp] : channels) {
        for (const auto& [lk, lp] : fLinks) {
            //LOG(debug) << kMyClass << " " << __FUNCTION__ << ":" << __LINE__ << " bind endpoint = " << sp.name
            //           << ", link property = " << lp.my_service << ":" << lp.my_channel
            //           << ", " << lp.peer_service << ":" << lp.peer_channel;
            if ((fServiceName == lp.my_service) && (sp.name == lp.my_channel)) {
                auto k = join({fTopPrefix, lp.peer_service, "*"}, fSeparator);
                peer_keys.emplace(k);
            } else if ((fServiceName == lp.peer_service) && (sp.name == lp.peer_channel)) {
                auto k = join({fTopPrefix, lp.my_service, "*"}, fSeparator);
                peer_keys.emplace(k);
            }
        }
    }


    std::unordered_set<std::string> state_keys;
    auto client = getClient();
    for (const auto &k : peer_keys) {
        //LOG(debug) << " peer key = " << k;
        auto s = daq::service::scan(*client, {k, daq::service::kFairMQStatePrefix.data()}, fSeparator);
        state_keys.merge(s);
    }

    if (state_keys.empty()) {
        return {};
    }

    std::vector<sw::redis::OptionalString> state_values;
    client->mget(state_keys.begin(), state_keys.end(), std::back_inserter(state_values));

    std::map<std::string, std::string> result;
    int i=0;
    std::stringstream ss;
    //ss << " scan result\n";
    for (const auto & k : state_keys) {
        const auto & v = state_values[i];
        if (v) {
            //ss << " key = " << k << ", value = " << *v << "\n";
            // remove the last part ":fairm-mq-state"
            auto s = k.substr(0, k.find_last_of(':'));
            result.emplace(s, *v);
        }
        ++i;
    }
    LOG(debug) << ss.str();
    return result;
}

/**
 * @brief Load topology endpoint/link definitions and install channel properties.
 */
void daq::service::TopologyConfig::initialize()
{
//  fNSubscribed = -1;
    if (fDefaultChannelProperties.empty()) {
        initializeDefaultChannelProperties();
    }
    if (!fConnectConfig.empty()) {
        LOG(info) << "connect-config = " <<  fConnectConfig;
        const auto& pt = toJson(fConnectConfig);

        LOG(info) << " connect-config (JSON) = " << toJsonString(pt);
        for (const auto& child : pt) {
            // child.first is string
            //LOG(info) << " channel name = " << child.first;
            const auto my_channel_name = child.first;

            std::unordered_map<std::string, std::string> cont;
            for (const auto &k : {
                        "type", "transport", "sndBufSize", "rcvBufSize", "sndKernelSize", "rcvKernelSize", "linger", "rateLogging", "num_sockets", "autoSubChannel"
                    }) {
                if (const auto &v = child.second.get_optional<std::string>(k); v) {
                    cont[k] = *v;
                }
            }
            auto sp = toSocketProperty(cont);
            sp.name = my_channel_name;
            sp.method = "connect"s;
            fConnectChannels.emplace(sp.name, sp);
        }
    }

    auto endpoints = readEndpoints();

    for (const auto& k : endpoints) {
        const auto sp = readEndpointProperty(k);
        if (sp.method=="bind") {
            fBindChannels.emplace(sp.name, sp);
        } else if (sp.method=="connect") {
            fConnectChannels.emplace(sp.name, sp);
        } else {
            LOG(error) << "MQ channel name = " << sp.name <<  ": unknown method = " << (sp.method.empty() ? "(empty)" : sp.method);
        }
    }

    auto links = readLinks();
    for (const auto& k : links) {
        const auto lp = readLinkProperty(k);
        const auto kk = lp.my_service + fSeparator + lp.my_channel + "," + lp.peer_service + fSeparator + lp.peer_channel;
        LOG(debug) << " link = " << kk;
        if (fLinks.count(kk)) {
            fLinks[kk].options += "," + lp.options;
        } else {
            fLinks[kk] = lp;
        }
    }

    std::vector<SocketProperty*> channelList;
    for (auto &[k, v] : fBindChannels) {
        channelList.push_back(&v);
    }
    for (auto &[k, v] : fConnectChannels) {
        channelList.push_back(&v);
    }

    LOG(debug) << kMyClass << " " << __FUNCTION__ << " number of channels : bind = " //
               << fBindChannels.size() << ", connect = " << fConnectChannels.size();
    std::vector<std::string> channel_config_options;
    for (auto p : channelList) {
        auto &sp = *p;
        std::vector<std::string> peers;
        // check number of peer instances
        for (const auto& [pairName, l] : fLinks) {
            LOG(warn) << __FILE__ << ":" << __LINE__ << "\n"
                      << pairName << " " << l.my_service << ":" << l.my_channel << " " << l.peer_service << ":" << l.peer_channel
                      << " " << sp.name;
            if ((l.my_service!=l.peer_service) && (l.my_channel!=sp.name)) {
                continue;
            }
            auto use_l = ((l.my_service==l.peer_service) && (l.peer_channel==sp.name));
            const auto &peer_service = (use_l) ? l.my_service : l.peer_service;
            const auto &peer_channel = (use_l) ? l.my_channel : l.peer_channel;
            // scan keys by a pattern = "daq_servie:service:*:presence"
            const auto &keys = scan(*getClient(), {fTopPrefix, peer_service, "*", kPresencePrefix.data()}, fSeparator);
            LOG(debug) << kMyClass << " " << __FUNCTION__ << " scan-service : peer name = " << peer_service << ", n peers " << keys.size();
            for (const auto &a: keys) {
                auto k = a.substr(0, a.find_last_of(fSeparator));
                k = join({k, topology::kChannelPrefix.data(), peer_channel}, fSeparator);
                LOG(debug) << " " << k;
                peers.push_back(k);
            }
            if (sp.auto_sub_channel) {
                sp.num_sockets += keys.size();
            }
        }
        std::sort(peers.begin(), peers.end());
        peers.erase(std::unique(peers.begin(), peers.end()), peers.end());

        LOG(debug) << " channel = " << sp.name << " autoSubChannel set num_sockets = " << sp.num_sockets;

        if (isUdsAvailable(peers) && fEnableUds && (sp.method=="bind") && (sp.transport=="zeromq")) {
            sp.address += "ipc://@/tmp/nestdaq/"s + "/" + fServiceName + "/" + fId + "/" + sp.name + "[0]";
            for (auto i=1; i<sp.num_sockets; ++i) {
                sp.address += ",ipc://@/tmp/nestdaq/"s + "/" + fServiceName + "/" + fId + "/" + sp.name + "[" + std::to_string(i) + "]";
            }
            //LOG(debug4) << " uds address =  " << sp.address;
        }
        channel_config_options.emplace_back(toChannelConfig(sp));

        writeChannel(sp, peers);
    }

    try {
        auto properties = fair::mq::SuboptParser(channel_config_options, fServiceName);
        for (auto it = properties.begin(); it!=properties.end();) {
            if (fDefaultChannelProperties.count(it->first)>0) {
                it = properties.erase(it);
            } else {
                fCustomChannelProperties[it->first] = fair::mq::PropertyHelper::ConvertPropertyToString(it->second);
                ++it;
            }
        }
        setProperties(properties);
    } catch (const std::exception& e) {
        LOG(error) << kMyClass << " error on SetProperty(chans.) : " << e.what();
    } catch (...) {
        LOG(error) << kMyClass << " unknown exception on SetProperty(chans.) :";
    }

    LOG(debug) << kMyClass << " initialize() done";
}

/**
 * @brief Capture existing FairMQ channel properties as defaults.
 */
void daq::service::TopologyConfig::initializeDefaultChannelProperties()
{
    //printConfig(getPropertiesAsStringStartingWith("channel-config"), "(default) channel-config"); // available in InitializingDevice
    //printConfig(getPropertiesAsStringStartingWith("mq-config"), "(default) mq-config"); // available in InitializingDevice
    std::string id_for_parser;
    if (propertyExists("config-key")) {
        id_for_parser = getProperty<std::string>("config-key");
    } else if (propertyExists("id")) {
        id_for_parser = getProperty<std::string>("id");
    }

    if (!id_for_parser.empty()) {
        try {
            if (propertyExists("mq-config")) {
                auto properties = fair::mq::JSONParser(getProperty<std::string>("mq-config"), id_for_parser);
                for (auto &[k, v] : properties) {
                    fDefaultChannelProperties[k] = fair::mq::PropertyHelper::ConvertPropertyToString(v);
                }
            } else if (propertyExists("channel-config")) {
                auto properties = fair::mq::SuboptParser(getProperty<std::vector<std::string>>("channel-config"), id_for_parser);
                for (auto &[k, v] : properties) {
                    LOG(debug) << " property name = " << k;
                    fDefaultChannelProperties[k] = fair::mq::PropertyHelper::ConvertPropertyToString(v);
                }
            }
        }
        catch (const std::exception& e) {
            LOG(error) << kMyClass << " " << __FUNCTION__ << " : " << e.what();
        }
        catch (...) {
            LOG(error) << kMyClass << " " << __FUNCTION__ << " : unknown exception";
        }
    }
    // printConfig(fDefaultChannelProperties, "(default) chans.");
}

/**
 * @brief Check whether all peers are on the same host IP and can use UDS.
 */
bool daq::service::TopologyConfig::isUdsAvailable(const std::vector<std::string> &peers)
{
    const auto& my_ip = fPlugin.getHealth().ip_address;
    for (const auto& x : peers) {
        const auto& ip = readPeerIp(x);
        if (my_ip!=ip) {
            LOG(debug4) << __func__ << " different ip: me =  " << my_ip << ", peer = " << ip;
            return false;
        }
    }
    LOG(debug4) << __func__ << " all IP is same";
    return true;
}

/**
 * @brief React to FairMQ lifecycle states that require topology synchronization.
 */
void daq::service::TopologyConfig::onDeviceStateChange(DeviceState newState)
{
    try {
        switch (newState) {
        case DeviceState::InitializingDevice:
            initialize();
            break;
        case DeviceState::Bound:
            writeBindAddress();
            if (isCanceled()) break;
            waitBindAddress();
            if (isCanceled()) break;
            if (!fConnectConfig.empty()) {
                configConnect();
            } else {
                resolveConnectAddress();
            }
            if (isCanceled()) break;
            writeConnectAddress();
            waitForPeerConnection();
            break;
        case DeviceState::ResettingDevice:
            reset();
            break;
        default:
            break;
        }
    } catch (const std::exception &e) {
        LOG(error) << kMyClass << " exception during device state change: " << e.what();
    } catch (...) {
        LOG(error) << kMyClass << " exception during device state change: unknow exception";
    }
}

/**
 * @brief Read one endpoint definition from Redis.
 */
const daq::service::SocketProperty daq::service::TopologyConfig::readEndpointProperty(std::string_view key)
{
    const auto& prefix = join({fTopPrefix, topology::kPrefix.data(), topology::kEndpointPrefix.data(), fServiceName, ""}, fSeparator);
    LOG(debug) << __FUNCTION__ << " prefix = " << prefix;
    const auto& channel_name = key.substr(prefix.size());
    std::unordered_map<std::string, std::string> h;
    getClient()->hgetall(key, std::inserter(h, h.begin()));
    // std::ostringstream ss;
    // ss << " name = " << channel_name;
    SocketProperty sp = toSocketProperty(h);
    sp.name = channel_name;
    return sp;
}

/**
 * @brief Scan Redis for endpoint definitions for this service.
 */
std::unordered_set<std::string> daq::service::TopologyConfig::readEndpoints()
{
    // scan keys by a pattern = "daq_service:topology:endpoint:service:*"
    auto keys = scan(*getClient(), {fTopPrefix, topology::kPrefix.data(), topology::kEndpointPrefix.data(), fServiceName, "*"}, fSeparator);

    auto n = keys.size();
    std::ostringstream ss;
    ss << fServiceName << ":" << __func__ << " n keys = " << n << ",";
    if (n>0) {
        for (const auto& a : keys) {
            ss << " " << a;
        }
        LOG(debug) << ss.str();
    } else {
        LOG(warn) << fServiceName << ":" << __func__ << " no endpoint entiries";
    }

    return keys;
}

/**
 * @brief Read and normalize one topology link definition from Redis.
 */
const daq::service::LinkProperty daq::service::TopologyConfig::readLinkProperty(std::string_view key)
{
    // key = ...:link:service0:channel0,service1:channel1

    auto val = getClient()->get(key);

    const auto& prefix = join({fTopPrefix, topology::kPrefix.data(), topology::kLinkPrefix.data(), ""}, fSeparator);
    // LOG(debug) << " readLinkProperty prefix = " << prefix;

    // socket_pair_name = service0:channel0,service1:channel1
    const auto& socket_pair_name = key.substr(prefix.size());
    std::ostringstream ss;
    ss << " link = " << socket_pair_name;
    LinkProperty lp;
    const auto comma     = socket_pair_name.find_first_of(",");
    const auto first_sep  = socket_pair_name.find_last_of(fSeparator, comma);
    const auto second_sep = socket_pair_name.find_last_of(fSeparator);
    //  LOG(debug) << " 1st sep = " << first_sep << ", comma = " << comma << ", 2nd sep = " << second_sep;
    const auto &service_l = socket_pair_name.substr(0, first_sep);
    const auto &channel_l = socket_pair_name.substr(first_sep+1, comma-(first_sep+1));
    const auto &service_r = socket_pair_name.substr(comma+1, second_sep-(comma+1));
    const auto &channel_r = socket_pair_name.substr(second_sep+1);

    // LOG(debug) << " LinkProperty parse result = " << service_l << " " << channel_l << " " << service_r << " " << channel_r;

    if (service_l==service_r) {
        lp.my_service   = service_l;
        lp.peer_service = service_r;
        if (channel_l < channel_r) {
            lp.my_channel   = channel_l;
            lp.peer_channel = channel_r;
        } else {
            lp.my_channel   = channel_r;
            lp.peer_channel = channel_l;
        }
    }

    if (service_l == fServiceName) {
        lp.my_service   = service_l;
        lp.my_channel   = channel_l;
        lp.peer_service = service_r;
        lp.peer_channel = channel_r;
    } else {
        lp.my_service   = service_r;
        lp.my_channel   = channel_r;
        lp.peer_service = service_l;
        lp.peer_channel = channel_l;
    }
    lp.options = *val;

    return lp;
}

/**
 * @brief Scan Redis for topology links involving this service.
 */
std::unordered_set<std::string> daq::service::TopologyConfig::readLinks()
{
    auto &r         = *getClient();
    // scan keys by a pattern = "daq_service:topology:link:service:*,*:*"
    auto keys = scan(r, {fTopPrefix, topology::kPrefix.data(), topology::kLinkPrefix.data(), fServiceName + "*,*", "*"}, fSeparator);

    // scan keys by a pattern = "daq_service:topology:link:*:*,service:*"
    keys.merge(scan(r, {fTopPrefix, topology::kPrefix.data(), topology::kLinkPrefix.data(), "*", "*,"+fServiceName, "*"}, fSeparator));

    auto n = keys.size();
    std::ostringstream ss;
    ss << fServiceName << ":" << __func__ << " n keys = " << n << ",";
    if (n>0) {
        for (const auto& a : keys) {
            ss << " " << a;
        }
        LOG(debug) << ss.str();
    } else {
        LOG(warn) << fServiceName << ":" << __func__ << " no link entries";
    }

    return keys;
}

/**
 * @brief Read bound socket addresses published by a peer channel.
 */
const std::vector<std::string> daq::service::TopologyConfig::readPeerAddress(const std::string& peer)
{
    const auto &peer_instance_key = peer.substr(0, peer.find(fSeparator+topology::kChannelPrefix.data()));
    const auto &peer_health_key   = join({peer_instance_key, kHealthPrefix.data()}, fSeparator);
    const auto &peer_channel     = peer.substr(peer.find_last_of(fSeparator)+1);
    auto &r = *getClient();
    LOG(debug) << "peer_instance_key = " << peer_instance_key << ", peer_health_key =  " << peer_health_key << ", peer_channel = " << peer_channel;
    auto peer_ip = r.hget(peer_health_key, "hostIp");
    LOG(debug) << "id = " << fId << " peer health = " << peer_health_key;
    if (!peer_ip) {
        LOG(warn) << "id = " << fId << " hostIp not found";
    } else {
        LOG(warn) << "id = " << fId << " hostIp found " << peer_ip.value();
    }

    auto scan_pattern = join({peer_instance_key.data(), topology::kSocketPrefix.data(), "chans."s + peer_channel.data() + ".*"s}, fSeparator);
    LOG(debug) << kMyClass << " " << __FUNCTION__ << " id = " << fId<<  " scan_pattern = " << scan_pattern;
    auto sub_socket_keys = scan(r, scan_pattern);
    LOG(debug) << kMyClass << " " << __FUNCTION__ << " id = " << fId << " subSokectKeys = " << sub_socket_keys.size();
    std::set<std::string> sorted(sub_socket_keys.cbegin(), sub_socket_keys.cend());

    std::vector<std::string> ret;
    for (const auto &k : sorted) {
        LOG(debug) << kMyClass << " " << __FUNCTION__ << " id = " << fId << " k = " << k;
        std::string address;
        int n_retry = 0;
        while (true) {
            auto a = getClient()->hget(k, "address");
            if (a) {
                address = makeAddress(*a, peer_ip->data());
                break;
            }
            LOG(warn) << " address not found for " << k;
            if (isCanceled() || n_retry>fMaxRetryToResolveAddress) {
                LOG(warn) << " find address of peer channel = " << k << " -> canceled";
                break;
            }
            std::this_thread::sleep_for(1000ms);
            ++n_retry;
        }
        LOG(debug) << kMyClass << " " << __FUNCTION__ << ":" << __LINE__ << " id = " << fId << " address = " << address;

        ret.push_back(address);
    }
    return ret;
}

/**
 * @brief Read the host IP of a peer service instance.
 */
const std::string daq::service::TopologyConfig::readPeerIp(const std::string& peer)
{
    const auto &peer_instance_key = peer.substr(0, peer.find(fSeparator+topology::kChannelPrefix.data()));
    const auto &peer_health_key   = join({peer_instance_key, kHealthPrefix.data()}, fSeparator);
    auto &r = *getClient();
    LOG(debug4) << "peer_instance_key = " << peer_instance_key << ", peer_health_key =  " << peer_health_key;
    auto peer_ip = r.hget(peer_health_key, "hostIp");
    LOG(debug4) << "id = " << fId << " peer health = " << peer_health_key;
    if (!peer_ip) {
        LOG(warn) << "id = " << fId << " hostIp not found";
        return {};
    } else {
        LOG(warn) << "id = " << fId << " hostIp found " << peer_ip.value();
    }
    return peer_ip.value();
}

/**
 * @brief Clear installed channel properties and remove topology registry keys.
 */
void daq::service::TopologyConfig::reset()
{
    LOG(debug) << kMyClass << " " << __FUNCTION__;
    fBindChannels.clear();
    fConnectChannels.clear();
    for (const auto& [k, v] : fCustomChannelProperties) {
        deleteProperty(k);
    }
    fCustomChannelProperties.clear();
    unregisterService();
}

/**
 * @brief Queue TTL refreshes for topology keys owned by this instance.
 */
void daq::service::TopologyConfig::resetTtl(sw::redis::Pipeline& pipe)
{
    //LOG(debug) << kMyClass << " " << __FUNCTION__ << " num registered = " << fRegisteredKeys.size();
    std::for_each(fRegisteredKeys.cbegin(), fRegisteredKeys.cend(),
    [&pipe, ttl = fMaxTtl](const auto& key) {
        pipe.expire(key, ttl);
    });
}

/**
 * @brief Resolve connect socket addresses from peer bind channel registry data.
 */
void daq::service::TopologyConfig::resolveConnectAddress()
{
    //LOG(debug) << __PRETTY_FUNCTION__;
    if (fConnectChannels.empty()) {
        return;
    }

    LOG(debug) << __PRETTY_FUNCTION__ << " id = " << fId << " wait done";
    auto &r = *getClient();

    // list of instances with the same service name
    //     auto same_services = scan(r, {fTopPrefix, fServiceName, "*", kPresencePrefix.data()}, fSeparator);
    //     std::vector<std::string> sorted_same_services;
    //     for (const auto &k : same_services) {
    //       auto instance_key = k.substr(0, k.find_last_of(fSeparator));
    //       instance_key      = instance_key.substr(instance_key.find_last_of(fSeparator));
    //       sorted_same_services.push_back(instance_key);
    //     }
    //     std::sort(sorted_same_services.begin(), sorted_same_services.end());
    //     int my_instance_index = 0;
    //     for (const auto &k : sorted_same_services) {
    //       if (k != fServiceName) {
    //         continue;
    //       }
    //       ++my_instance_index;
    //     }

    std::unordered_map<std::string, std::vector<std::string>> options;
    for (auto &[name, sp] : fConnectChannels) {
        if (!sp.address.empty() && sp.address!="unspecified") {
            continue;
        }
        LOG(debug) << kMyClass << " " << __FUNCTION__ << " id = " << fId << " find peer of " << sp.name << " num_sockets = " << sp.num_sockets;
        const auto &my_instance_key = join({fTopPrefix, fServiceName, fId}, fSeparator);
        const auto &my_channel_key  = join({my_instance_key, topology::kChannelPrefix.data(), sp.name}, fSeparator);

        std::vector<std::string> peers;
        const auto &my_peer_key = join({my_channel_key, topology::kPeerPrefix.data()}, fSeparator);
        r.lrange(my_peer_key, 0, -1, std::back_inserter(peers));
        int peer_index{0};
        SocketProperty res(sp);
        bool is1to1{false};
        for (const auto& p : peers) {
            LOG(debug) << kMyClass << " " << __FUNCTION__ << " id = " << fId << " peer of " << name << " : " << p;
            const auto &k = join({p, topology::kPeerPrefix.data()}, fSeparator);
            std::vector<std::string> neighbors;
            r.lrange(k, 0, -1, std::back_inserter(neighbors));
            LOG(debug) << kMyClass << " " << __FUNCTION__ << ":" << __LINE__ << " id = " << fId << " n neighbors " << neighbors.size();
            int my_index = 0; // index viewed from the peer
            // for (const auto& n : neighbors) {
            //   LOG(debug) << kMyClass << " " << __FUNCTION__ << ":" << __LINE__ << " id = " << fId << " neighbor: " << n;
            // }
            for (const auto& n : neighbors) {
                if (n==my_channel_key) {
                    break;
                }
                ++my_index;
            }
            if (is1to1) {
                if (my_index!=peer_index) {
                    ++peer_index;
                    continue;
                }
            }
            LOG(debug) << kMyClass << " " << __FUNCTION__ << ":" << __LINE__ << " id = " << fId << " my_index = " << my_index;
            std::unordered_map<std::string, std::string> h;
            r.hgetall(p, std::inserter(h, h.begin()));
            const auto &peer_property = toSocketProperty(h);

            LOG(debug) << "id = " << fId << " numSocket (me) = " << sp.num_sockets << ", (peer) = " << peer_property.num_sockets;
            const auto address = readPeerAddress(p); //peer_health_key, *peer_ip, peer_channel);
            const auto my_address_index = static_cast<decltype(address)::size_type>(my_index);
            if ((sp.num_sockets<=1) && (peer_property.num_sockets<=1)) {
                is1to1 = true;
                // 1:1 or fan-in/fan-out
                LOG(debug) << kMyClass << " " << __FUNCTION__ << ":" << __LINE__ << " id = " << fId << " 1:1 or fan-in/fan-out ";
                LOG(debug) << kMyClass << " " << __FUNCTION__ << ":" << __LINE__  << " id = " << fId
                           << " peer size = " << peers.size() << " my_index = " << my_index << " peer_index = " << peer_index
                           << " address.size() = " << address.size();
                if ((my_index==peer_index) || (peers.size()==1)) {
                    res.address = address[0];
                    break;
                }
            } else if ((sp.num_sockets<=1) && (peer_property.num_sockets>1)) {
                // 1:m
                LOG(debug) << kMyClass << " " << __FUNCTION__ << ":" << __LINE__ << " id = " << fId << " 1:m ";
                res.address = address[my_address_index];
            } else if ((sp.num_sockets>1) && (peer_property.num_sockets<=1)) {
                // n:1
                LOG(debug) << kMyClass << " " << __FUNCTION__ << ":" << __LINE__ << " id = " << fId << " n:1 ";
                assert(address.size()==1);
                res.address += (res.address.empty()) ? address[0] : ("," + address[0]);
            } else if ((sp.num_sockets>1) && (peer_property.num_sockets>1)) {
                // n:m
                LOG(debug) << kMyClass << " " << __FUNCTION__ << ":" << __LINE__ << " id = " << fId << " n:m ";
                assert(address.size()>my_address_index);
                res.address += (res.address.empty()) ? address[my_address_index] : ("," + address[my_address_index]);
            }
            ++peer_index;
        }
        LOG(debug) << " id = " << fId << " add socket property : " << res.name << " " << res.address;
        options[res.name].emplace_back(toChannelConfig(res));
    }

//  LOG(debug) << " before update";
//  printConfig(getPropertiesAsStringStartingWith("channel-config"), "channel-config");
//  printConfig(getPropertiesAsStringStartingWith("chans."), "chans.");

    if (options.empty()) {
        return;
    }

    try {
        for (const auto& [name, channel_config] : options) {
            auto properties = fair::mq::SuboptParser(channel_config, fServiceName);
            for (const auto & [k, v] : properties) {
                const auto s = fair::mq::PropertyHelper::ConvertPropertyToString(v);
                fCustomChannelProperties[k] = s;
                LOG(debug) << " id = " << fId << " set property : " << k << " " << s;
            }

            setProperties(properties);
        }
    } catch (const std::exception& e) {
        LOG(error) << kMyClass << " error on SetProperty(chans.) : id = " << fId << ": " << e.what();
    } catch (...) {
        LOG(error) << kMyClass << " unknown exception on SetProperty(chans.) :";
    }
//  LOG(debug) << " after update";
//  printConfig(getPropertiesAsStringStartingWith("channel-config"), "channel-config");
//  printConfig(getPropertiesAsStringStartingWith("chans."), "chans.");
}

/**
 * @brief Remove topology registry keys owned by this instance.
 */
void daq::service::TopologyConfig::unregisterService()
{
    if (!fRegisteredKeys.empty()) {
        auto ndeleted = getClient()->del(fRegisteredKeys.cbegin(), fRegisteredKeys.cend());
        fRegisteredKeys.clear();
        LOG(debug) << kMyClass << " " << __FUNCTION__ << " n deleted = " << ndeleted;
    }
}

/**
 * @brief Wait until peer bind channels have published their bound addresses.
 */
void daq::service::TopologyConfig::waitBindAddress()
{
    //LOG(debug) << __PRETTY_FUNCTION__;
    if (fConnectChannels.empty()) {
        return;
    }

    auto &r = *getClient();
    // find bind channels of peers
    std::unordered_set<std::string> channels;
    for (const auto& [name, sp] : fConnectChannels) {
        for (const auto& [lk, lp] : fLinks) {
            LOG(debug) << kMyClass << " " << __FUNCTION__ << ":" << __LINE__ << " connect " << sp.name
                       << ", " << lp.my_service << ":" << lp.my_channel
                       << ", " << lp.peer_service << ":" << lp.peer_channel;
            if ((fServiceName == lp.my_service) && (sp.name == lp.my_channel)) {
                auto k = join({fTopPrefix, lp.peer_service, "*", kPresencePrefix.data()}, fSeparator);
                LOG(debug) << kMyClass << " " << __FUNCTION__ << ":" << __LINE__ << " : k = " << k;
                auto presence_keys = scan(r, {fTopPrefix, lp.peer_service, "*", kPresencePrefix.data()}, fSeparator);
                LOG(debug) << __LINE__ << ": n presence: " << presence_keys.size();
                for (auto &a : presence_keys) {
                    auto c =  a.substr(0, a.find_last_of(fSeparator));
                    // e.g.: daq_service:peer-service:peer-instance-id:endpoint:peer-chanenl
                    channels.emplace(join({c, topology::kChannelPrefix.data(), lp.peer_channel}, fSeparator));
                }
            } else if ((fServiceName == lp.peer_service) && (sp.name == lp.peer_channel)) {
                auto k = join({fTopPrefix, lp.my_service, "*", kPresencePrefix.data()}, fSeparator);
                LOG(debug) << kMyClass << " " << __FUNCTION__ << ":" << __LINE__ << " : k = " << k;
                auto presence_keys = scan(r, {fTopPrefix, lp.my_service, "*", kPresencePrefix.data()}, fSeparator);
                LOG(debug) << __LINE__ << ": n presence: " << presence_keys.size();
                for (auto &a : presence_keys) {
                    auto c = a.substr(0, a.find_last_of(fSeparator));
                    channels.emplace(join({c, topology::kChannelPrefix.data(), lp.my_channel}, fSeparator));
                }
            }
        }
    }

    for (const auto &c : channels) {
        while (true) {
            LOG(warn) << kMyClass << " " << __FUNCTION__ << " wait channel : " << c;
            auto v = r.hget(c, "bound");
            if (v) {
                auto s = boost::to_lower_copy(*v);
                if ((s=="1") || (s=="true")) {
                    break;
                }
            }
            if (isCanceled()) {
                return;
            }
            std::this_thread::sleep_for(1000ms);
        }
    }
}

/**
 * @brief Wait for configured peer devices to reach a connection-ready state.
 */
void daq::service::TopologyConfig::waitForPeerConnection()
{
    LOG(debug) << __FUNCTION__ << " ...";
    std::unordered_set<std::string> peer_keys;
    for (const auto &[name, sp] : fBindChannels) {
        //LOG(debug) << name << " waitForPeerConnection = " << sp.wait_for_peer_connection;
        if (!sp.wait_for_peer_connection) {
            continue;
        }
        for (const auto& [lk, lp] : fLinks) {
            //LOG(debug) << kMyClass << " " << __FUNCTION__ << ":" << __LINE__ << " bind endpoint = " << sp.name
            //           << ", link property = " << lp.my_service << ":" << lp.my_channel
            //           << ", " << lp.peer_service << ":" << lp.peer_channel;
            if ((fServiceName == lp.my_service) && (sp.name == lp.my_channel)) {
                auto k = join({fTopPrefix, lp.peer_service, "*"}, fSeparator);
                peer_keys.emplace(k);
            } else if ((fServiceName == lp.peer_service) && (sp.name == lp.peer_channel)) {
                auto k = join({fTopPrefix, lp.my_service, "*"}, fSeparator);
                peer_keys.emplace(k);
            }
        }
    }


    bool done{false};
    auto client = getClient();
    while (!done && !isCanceled()) {
        std::unordered_set<std::string> state_keys;
        for (const auto &k : peer_keys) {
            auto s = daq::service::scan(*client, {k, daq::service::kFairMQStatePrefix.data()}, fSeparator);
            state_keys.merge(s);
        }

        if (state_keys.empty()) {
            return;
        }

        // {
        //     std::string k;
        //     for (const auto &x : state_keys) {
        //         k += x + ", ";
        //     }
        //     LOG(debug) << " state_keys = " << k;
        // }

        std::vector<sw::redis::OptionalString> state_values;
        client->mget(state_keys.begin(), state_keys.end(), std::back_inserter(state_values));

        std::vector<std::string> states;
        for (const auto & x : state_values) {
            if (!x) {
                continue;
            }
            states.push_back(*x);
        }

        // {
        //      std::string s;
        //      for (const auto &x : states) {
        //          s += x + ", ";
        //      }
        //      LOG(debug) << " states = " << s;
        // }

        for (const auto &w : topology::kWaitDeviceReadyTargets) {
            if (std::all_of(states.begin(), states.end(), [&w](const auto &x) {
            return x == w;
        })) {
                done = true;
                break;
            }
        }
        std::this_thread::sleep_for(100ms);
    }
    LOG(debug) << __FUNCTION__ << " done";
}

/**
 * @brief Publish current FairMQ socket addresses for a channel set.
 */
void daq::service::TopologyConfig::writeAddress(MQChannel &channels, std::function<void (sw::redis::Pipeline&, std::string_view)> f)
{
    auto &r    = *getClient();
    auto pipe  = r.pipeline();

    std::scoped_lock<std::mutex> lock{getMutex()};
    try {
        for (auto &[name, sp] : channels) {
            auto local_key_prefix = "chans." + sp.name + ".";
            for (auto index=0; ; ++index) {
                auto local_key = local_key_prefix + std::to_string(index);
                const auto &chans = getPropertiesAsStringStartingWith(local_key);
                if (chans.empty()) {
                    break;
                }
                const auto &key = join({fTopPrefix, fServiceName, fId, topology::kSocketPrefix.data(), local_key}, fSeparator);
                std::ostringstream ss;
                ss << kMyClass << " " << __FUNCTION__ << ":" << __LINE__ << " key = " << key << " :\n";
                std::map<std::string, std::string> h;
                for (const auto &[k, v] : chans) {
                    auto hk = k.substr(k.find_last_of(".")+1);
                    h[hk] = v;
                    ss << " " << hk << ", " << v << "\n";
                }
                LOG(debug1) << ss.str();

                h["num_sockets"]     = std::to_string(sp.num_sockets);
                h["autoSubChannel"] = std::to_string(sp.auto_sub_channel);

                pipe.hset(key, h.cbegin(), h.cend());
                pipe.expire(key, fMaxTtl);
                fRegisteredKeys.push_back(key);

            }
            if (f) {
                f(pipe, name);
            }
        }
        pipe.exec();
    } catch (const std::exception &e) {
        LOG(error) << kMyClass << " " << __FUNCTION__ << " caught exception : " << e.what();
    } catch (...) {
        LOG(error) << kMyClass << " " << __FUNCTION__ << " caught unknown exception";
    }
}

/**
 * @brief Publish bind socket addresses and mark channels as bound.
 */
void daq::service::TopologyConfig::writeBindAddress()
{
    //LOG(debug) << __PRETTY_FUNCTION__;
    if (fBindChannels.empty()) {
        return;
    }

    LOG(debug) << kMyClass << " write bind address to the registry. (n =  " << fBindChannels.size() << ")";
    writeAddress(fBindChannels, [this](auto &pipe, auto name) {
        auto channel = join({fTopPrefix, fServiceName, fId, topology::kChannelPrefix.data(), name.data()}, fSeparator);
        pipe.hset(channel, "bound", "1");
        LOG(warn) << kMyClass << " " << __FUNCTION__ << " bound channel: " << channel;
    });

    //LOG(debug) << __PRETTY_FUNCTION__ << " done";
}

/**
 * @brief Publish one logical channel and its peer list to Redis.
 */
void daq::service::TopologyConfig::writeChannel(SocketProperty &sp, const std::vector<std::string> &peers)
{
    if (peers.empty()) {
        //LOG(debug) << " empty peers";
        return;
    }
    const auto &key = join({fTopPrefix, fServiceName, fId, topology::kChannelPrefix.data(), sp.name}, fSeparator);

    LOG(debug) << kMyClass << " " << __FUNCTION__ << " channel : " << sp.name << " : n peers = " << peers.size();
    fPlugin.SetProperty("n-peers:"s+sp.name, std::to_string(peers.size()));

    auto pipe = getClient()->pipeline();
    pipe.hset(key, {
        std::make_pair("name",                  sp.name),
        std::make_pair("type",                  sp.type),
        std::make_pair("method",                sp.method),
        std::make_pair("address",               sp.address),
        std::make_pair("transport",             sp.transport),
        std::make_pair("sndBufSize",            std::to_string(sp.snd_buf_size)),
        std::make_pair("rcvBufSize",            std::to_string(sp.rcv_buf_size)),
        std::make_pair("sndKernelSize",         std::to_string(sp.snd_kernel_size)),
        std::make_pair("rcvKernelSize",         std::to_string(sp.rcv_kernel_size)),
        std::make_pair("linger",                std::to_string(sp.linger)),
        std::make_pair("rateLogging",           std::to_string(sp.rate_logging)),
        std::make_pair("portRangeMin",          std::to_string(sp.port_range_min)),
        std::make_pair("portRangeMax",          std::to_string(sp.port_range_max)),
        std::make_pair("autoBind",              std::to_string(sp.auto_bind)),
        std::make_pair("num_sockets",            std::to_string(sp.num_sockets)),
        std::make_pair("autoSubChannel",        std::to_string(sp.auto_sub_channel)),
        std::make_pair("bound",                 std::to_string(sp.bound)),
        std::make_pair("waitForPeerConnection", std::to_string(sp.wait_for_peer_connection)),
    });
    pipe.expire(key, fMaxTtl);

    auto list_key = join({key, topology::kPeerPrefix.data()}, fSeparator);
    pipe.rpush(list_key, peers.cbegin(), peers.cend());
    pipe.expire(list_key, fMaxTtl);

    pipe.exec();

    fRegisteredKeys.push_back(key);
    fRegisteredKeys.push_back(list_key);

}

/**
 * @brief Publish connect socket addresses to Redis.
 */
void daq::service::TopologyConfig::writeConnectAddress()
{
    // LOG(debug) << __PRETTY_FUNCTION__;
    if (fConnectChannels.empty()) {
        return;
    }

    LOG(debug) << kMyClass << " write connect address to the registry. (n =  " << fConnectChannels.size() << ")";
    writeAddress(fConnectChannels);
    //LOG(debug) << __PRETTY_FUNCTION__ << " done";
}
