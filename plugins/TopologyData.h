#pragma once

/**
 * @file TopologyData.h
 * @brief Data structures for the current Redis-backed DAQ topology schema.
 */

#include <map>
#include <string>
#include <vector>

namespace daq::service {

/** @brief Default FairMQ socket buffer size used when topology omits one. */
static constexpr int kDefaultSocketBufferSize{1000};
/** @brief Default FairMQ socket linger value used when topology omits one. */
static constexpr int kDefaultSocketLinger{500};
/** @brief Lower bound for automatically assigned TCP ports. */
static constexpr int kDefaultPortRangeMin{22000};
/** @brief Upper bound for automatically assigned TCP ports. */
static constexpr int kDefaultPortRangeMax{32000};

/**
 * @brief Current socket/channel properties exchanged through Redis topology keys.
 */
struct SocketProperty {
    std::string name; // channel name
    std::string type;
    std::string method; // bind or connect
    std::string address;
    std::string transport{"zeromq"};
    int sndBufSize{kDefaultSocketBufferSize};
    int rcvBufSize{kDefaultSocketBufferSize};
    int sndKernelSize{0};
    int rcvKernelSize{0};
    int linger{kDefaultSocketLinger};
    int rateLogging{1};
    int portRangeMin{kDefaultPortRangeMin};
    int portRangeMax{kDefaultPortRangeMax};
    bool autoBind{true};
    int numSockets{0};

    /** @brief True when TopologyConfig should derive subchannels automatically. */
    bool autoSubChannel{false};
    /** @brief True after a bind address has been resolved and published. */
    bool bound{false};
    /** @brief True when connect setup must wait for peer state. */
    bool waitForPeerConnection{true};
};

/**
 * @brief Current logical link between a local channel and a peer channel.
 */
struct LinkProperty {
    std::string myService; // near
    std::string myChannel;
    std::string peerService; // far
    std::string peerChannel;
    std::string options;
};

using MQChannel = std::map<std::string, SocketProperty>;

} // namespae daq::service
