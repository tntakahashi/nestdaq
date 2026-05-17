#pragma once

/**
 * @file Constants.h
 * @brief Shared Redis key names and FairMQ command strings used by plugins.
 */

#include <string_view>

namespace daq::service {

/** @brief Top-level Redis namespace for DAQ service data. */
static constexpr std::string_view TopPrefix{"daq_service"};
/** @brief Redis namespace for live service presence keys. */
static constexpr std::string_view PresencePrefix{"presence"};
/** @brief Redis namespace for service health hashes. */
static constexpr std::string_view HealthPrefix{"health"};
/** @brief Redis namespace for FairMQ state values. */
static constexpr std::string_view FairMQStatePrefix{"fair-mq-state"};
/** @brief Redis field/key prefix for last update timestamps. */
static constexpr std::string_view UpdateTimePrefix{"updatedTime"};
/** @brief Redis namespace for published FairMQ program options. */
static constexpr std::string_view ProgOptionPrefix{"option"};
/** @brief Redis key prefix used for service instance index bookkeeping. */
static constexpr std::string_view ServiceInstanceIndexPrefix{"service-instance-index"};

static constexpr std::string_view Separator{"separator"};
static constexpr std::string_view ServiceName{"service-name"};
static constexpr std::string_view ServiceRegistryUri{"registry-uri"};

static constexpr std::string_view RunInfoPrefix{"run_info"};
static constexpr std::string_view RunNumber{"run_number"};
static constexpr std::string_view StartTime{"start_time"};
static constexpr std::string_view StartTimeNS{"start_time_ns"};
static constexpr std::string_view StopTime{"stop_time"};
static constexpr std::string_view StopTimeNS{"stop_time_ns"};

static constexpr std::string_view Uuid{"uuid"};
static constexpr std::string_view MaxTtl{"max-ttl"};
static constexpr std::string_view TtlUpdateInterval{"ttl-update-interval"};
static constexpr std::string_view HostIpAddress{"host-ip"};
static constexpr std::string_view Hostname{"hostname"};
static constexpr std::string_view CommandChannelName{"daqctl"}; // daq command pubsub channel (controller -> FairMQ Deivce)
static constexpr std::string_view StateChannelName{"daqstate"}; // daq command pubsub channel (controller <- FairMQ Deivce)
}

namespace fairmq::command {
/** @brief FairMQ command string constants accepted by the service plugin. */
static constexpr std::string_view Bind{"BIND"};
static constexpr std::string_view CompleteInit{"COMPLETE INIT"};
static constexpr std::string_view Connect{"CONNECT"};
static constexpr std::string_view End{"END"};
static constexpr std::string_view InitDevice{"INIT DEVICE"};
static constexpr std::string_view InitTask{"INIT TASK"};
static constexpr std::string_view ResetDevice{"RESET DEVICE"};
static constexpr std::string_view ResetTask{"RESET TASK"};
static constexpr std::string_view Run{"RUN"};
static constexpr std::string_view Stop{"STOP"};

}

namespace daq::command {
/** @brief Redis pub/sub channel used for DAQ control commands. */
static constexpr std::string_view Channel{"daqctl"};
static constexpr std::string_view Exit{"exit"};
static constexpr std::string_view Quit{"quit"};
static constexpr std::string_view Reset{"reset"};
static constexpr std::string_view Start{"start"};
}
