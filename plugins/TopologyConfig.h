#pragma once

/**
 * @file TopologyConfig.h
 * @brief Runtime topology resolver for Redis-backed FairMQ channel setup.
 */

//#include <initializer_list>
#include <memory>
#include <mutex>
#include <string>
#include <sstream>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <fairmq/Plugin.h>

#include "plugins/TopologyData.h"
#include "plugins/DaqServicePlugin.h"

// forward declaration
namespace sw::redis {
class Redis;
template <typename Impl> class QueuedRedis;
class PipelineImpl;
using Pipeline = QueuedRedis<PipelineImpl>;
}

namespace daq::service {

/**
 * @brief Resolves Redis topology definitions into FairMQ channel properties.
 *
 * The object is owned by the DAQ service plugin and uses its Redis client,
 * mutex, and FairMQ property accessors. It writes bind/connect addresses back
 * to Redis so peers can discover each other at runtime.
 */
class TopologyConfig {
public:
    using DeviceState = fair::mq::Plugin::DeviceState;
    /** @brief Bind topology configuration to the owning DAQ service plugin. */
    explicit TopologyConfig(daq::service::Plugin &plugin);
    TopologyConfig(const TopologyConfig&) = delete;
    TopologyConfig& operator=(const TopologyConfig&) = delete;
    TopologyConfig(TopologyConfig&&) = delete;
    TopologyConfig& operator=(TopologyConfig&&) = delete;
    ~TopologyConfig();

    /** Resolve and apply channel connection properties from Redis. */
    void ConfigConnect();
    /** @brief Enable or disable UDS address preference where available. */
    void EnableUds(bool f=true) {
        fEnableUds = f;
    }
    /** @brief Return current peer states for local bind channels. */
    auto GetPeerStateOfBindChannels() -> std::map<std::string, std::string> {
        return GetPeerState(fBindChannels);
    }
    /** @brief Return current peer states for local connect channels. */
    auto GetPeerStateOfConnectChannels() -> std::map<std::string, std::string> {
        return GetPeerState(fConnectChannels);
    }

    /** React to FairMQ state changes and update topology-related Redis state. */
    void OnDeviceStateChange(DeviceState newState);
    /** @brief Reset transient topology state owned by this instance. */
    void Reset();
    /** @brief Refresh TTLs for Redis topology keys owned by this instance. */
    void ResetTtl(sw::redis::Pipeline& pipe);
    /** @brief Set raw connect configuration from command-line or property input. */
    void SetConnectConfig(std::string_view arg) {
        fConnectConfig = arg;
    }
    /** @brief Set retry count used while resolving peer addresses. */
    void SetMaxRetryToResolveAddress(int arg) {
        fMaxRetryToResolveAddress = arg;
    }

private:
    void DeleteProperty(const std::string& key) {
        fPlugin.DeleteProperty(key);
    }
    std::shared_ptr<sw::redis::Redis> GetClient() const {
        return fPlugin.GetClient();
    }
    std::mutex& GetMutex() {
        return fPlugin.GetMutex();
    }
    auto GetPeerState(const MQChannel & channels) -> std::map<std::string, std::string>;
    std::map<std::string, std::string> GetPropertiesAsStringStartingWith(const std::string& q) const {
        return fPlugin.GetPropertiesAsStringStartingWith(q);
    }
    template <typename T> T GetProperty(const std::string& key) const {
        return fPlugin.GetProperty<T>(key);
    }
    /** @brief Load Redis topology and initialize local FairMQ channel properties. */
    void Initialize();
    /** @brief Populate default channel properties from FairMQ options. */
    void InitializeDefaultChannelProperties();
    bool IsCanceled() const {
        return fPlugin.IsCanceled();
    }
    /** @brief Return true when all peers can use UDS transport. */
    bool IsUdsAvailable(const std::vector<std::string> &peers);
    int PropertyExists(const std::string& key) {
        return fPlugin.PropertyExists(key);
    }
    /** @brief Read one endpoint definition from Redis. */
    const SocketProperty ReadEndpointProperty(std::string_view key);
    /** @brief Read all endpoint keys relevant to this topology. */
    std::unordered_set<std::string> ReadEndpoints();
    /** @brief Read one logical link definition from Redis. */
    const LinkProperty ReadLinkProperty(std::string_view key);
    /** @brief Read all logical link keys relevant to this topology. */
    std::unordered_set<std::string> ReadLinks();
    /** @brief Read candidate addresses published by a peer service. */
    const std::vector<std::string> ReadPeerAddress(const std::string& peer);
    /** @brief Read the peer service IP address from Redis health data. */
    const std::string ReadPeerIP(const std::string& peer);
    /** @brief Resolve connect channel addresses from peer bind publications. */
    void ResolveConnectAddress();
    void SetProperties(const fair::mq::Properties &props) {
        fPlugin.SetProperties(props);
    }
    /** @brief Remove topology keys owned by this service instance. */
    void Unregister();
    /** @brief Wait until local bind addresses have been published. */
    void WaitBindAddress();
    /** @brief Wait until required peers reach a connectable state. */
    void WaitForPeerConnection();
    /** @brief Write channel addresses through the provided Redis pipeline callback. */
    void WriteAddress(MQChannel &channels, std::function<void (sw::redis::Pipeline&, std::string_view)> f = nullptr);
    /** @brief Publish local bind channel addresses to Redis. */
    void WriteBindAddress();
    /** @brief Write one socket property and associated peer metadata. */
    void WriteChannel(SocketProperty &sp, const std::vector<std::string> &peers);
    /** @brief Publish resolved connect channel addresses to FairMQ properties. */
    void WriteConnectAddress();

    daq::service::Plugin &fPlugin;
    std::string fServiceName;
    std::string fId;
    std::string fSeparator;
    std::string fTopPrefix;
    long long   fMaxTtl{0};
    bool        fEnableUds{true};
    std::string fConnectConfig;
    int         fMaxRetryToResolveAddress{0};

    // channel properties configured by command line option or JSON
    std::map<std::string, std::string> fDefaultChannelProperties;

    // channel properties configured by this plugin
    std::map<std::string, std::string> fCustomChannelProperties;

    MQChannel fBindChannels;
    MQChannel fConnectChannels;
    std::map<std::string, LinkProperty> fLinks;

    std::vector<std::string> fRegisteredKeys;
};

} // namespace daq::service
