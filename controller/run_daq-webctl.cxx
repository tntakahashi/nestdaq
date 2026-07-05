/** @file
 *  @brief Provides the daq-webctl executable entry point.
 */

#include <cstdlib>
#include <iostream>
#include <mutex>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>

#include <boost/asio.hpp>
#include <boost/program_options.hpp>

#include <fairlogger/Logger.h>

#include <nestdaq/telemetry/FairLoggerTelemetryLoader.h>

#include "plugins/Constants.h"
#include "plugins/tools.h"
#include "controller/DaqWebControlDefaultDocRootPath.h"
#include "controller/HttpWebSocketServer.h"
#include "controller/WebSocketHandle.h"
#include "controller/websocket_session.h"
#include "controller/WebGui.h"

namespace bpo = boost::program_options;

using namespace std::string_literals;

static constexpr uint64_t kDefaultPollIntervalMs{500};
static constexpr int kWebSocketRetryIntervalMs{1000};

std::mutex gWsMutex; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
std::unordered_map<unsigned int, std::pair<std::shared_ptr<WebSocketSession>, std::string>> gWsSessions; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
std::unique_ptr<WebGui> gDaqControl; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

bpo::options_description makeOption() {
    bpo::options_description options("options");
    bpo::options_description wsOptions("websocket handler options");
    bpo::options_description redisOptions("redis options");
    bpo::options_description logOptions("log options");
    wsOptions.add_options()
             //
             //
             ("http-uri",  bpo::value<std::string>()->default_value("http://0.0.0.0:8080"), "http server URI. (scheme://address:port)")
             //
             ("threads", bpo::value<unsigned int>()->default_value(1), "number of threads for http server")
             //
             ("doc-root", bpo::value<std::string>()->default_value(std::string{DefaultDocRootPath}), "Directory of the document root, which is the starting point when looking for html")
             //
             ("pre-run", bpo::value<std::string>()->default_value("echo \"pre-run command\""), "Path to a script file (starting with shebang) or a command line to execute before publishing RUN command")
             //
             ("post-run", bpo::value<std::string>()->default_value("echo \"post-run command\""), "Path to a script file (starting with shebang) or a comamnd line to execute after publishing RUN command")
             //
             ("pre-stop", bpo::value<std::string>()->default_value("echo \"pre-stop command\""), "Path to a script file (starting with shebang) or a command line to execute before publishing STOP command")
             //
             ("post-stop", bpo::value<std::string>()->default_value("echo \"post-stop command\""), "Path to the script file (starting with shebang) or a comamnd line to execute after publishing STOP command");

    redisOptions.add_options()
                //
                ("redis-uri", bpo::value<std::string>()->default_value("tcp://127.0.0.1:6379"), "URI of redis-server")
                //
                ("separator", bpo::value<std::string>()->default_value(":"), "namespace separator for redis keys")
                //
                ("poll-interval", bpo::value<uint64_t>()->default_value(kDefaultPollIntervalMs), "state polling interval in millisecond");

    logOptions.add_options()
              //
              ("log-to-file", bpo::value<std::string>()->default_value(""), "FairLogger Log output to a file")
              //
              ("file-severity", bpo::value<std::string>()->default_value("info"), "FairLogger Log severity level (file) : trace, debug, info, state, warn, error, fatal, nolog")
              //
              ("severity", bpo::value<std::string>()->default_value("info"), "FairLogger Log severity level (console): trace, debug, info, state, warn, error, fatal, nolog")
              //
              ("verbosity", bpo::value<std::string>()->default_value("medium"), "FairLogger Log verbosity level: veryhigh, high, medium, low")
              //
              ("color", bpo::value<bool>()->default_value(true), "FairLogger Log color (true/false)");

    bpo::options_description otelOptions("OpenTelemetry log options");
    nestdaq::telemetry::addTelemetryOptions(otelOptions, "daq-webctl");

    options.add_options()
           //
           ("help,h", "print this help");

    options.add(wsOptions)
           .add(redisOptions)
           .add(logOptions)
           .add(otelOptions);
    return options;
}

auto parseHttpUri(const std::string& uri) -> const std::tuple<std::string, std::string, std::string> {
    // pattern = (scheme)://(address):(port)
    std::regex pattern{R"(^([^:\/?#]+)://([^\/?#]+):(\d+))"};

    std::smatch match_result;
    if (std::regex_match(uri, match_result, pattern)) {
        // int count=0;
        //  for (const auto & s : match_result) {
        //    std::cout << count++ << " : " << s << std::endl;
        //  }
        //std::cout << " count = " << count << std::endl;
    } else {
        LOG(error) << "error: " << __FILE__ << ":" << __LINE__ << " " << __func__
                   << "\n  std::regex_match failed: uri = " << uri
                   << "\n  It should be (scheme)://(address):(port)";
        return {};
    }

    // LOG(debug) << " match_result.size() = " << match_result.size();
    if (match_result.size()!=4) {
        LOG(error) << "error: " << __FILE__ << ":" << __LINE__ << " " << __func__
                   << "\n  http server URI format is invalid. URI = " << uri
                   << "\n  number of matched parts = " << match_result.size()
                   << "\n  It should be (scheme)://(address):(port)";
        return {};
    }
    return {match_result[1], match_result[2], match_result[3]};
}

int main(int argc, char* argv[]) { // NOLINT(bugprone-exception-escape)
    std::cin.tie(nullptr);
    std::ios::sync_with_stdio(false);

    bpo::variables_map vm;
    auto ret = parseCommandLine(argc, argv, makeOption(), vm);
    if (ret!=EXIT_SUCCESS) {
        return ret;
    }

    {
        const auto log_file = vm["log-to-file"].as<std::string>();
        const auto verbosity = vm["verbosity"].as<std::string>();
        fair::Logger::SetVerbosity(verbosity);
        if (log_file.empty()) {
            fair::Logger::SetConsoleColor(vm["color"].as<bool>());
            fair::Logger::SetConsoleSeverity(vm["severity"].as<std::string>());
        } else {
            fair::Logger::InitFileSink(vm["file-severity"].as<std::string>(), log_file);
            fair::Logger::SetConsoleSeverity("nolog");
        }
    }

    const auto telemetry_options = nestdaq::telemetry::readTelemetryOptions(vm, "daq-webctl");
    nestdaq::telemetry::setSpdlogConsolePattern(telemetry_options.spdlog_console_pattern);
    nestdaq::telemetry::setSpdlogNativeConsoleEnabled(telemetry_options.spdlog_native_console);
    nestdaq::telemetry::setSpdlogAsyncOptions({
        .enabled = telemetry_options.spdlog_async,
        .queue_size = telemetry_options.spdlog_async_queue_size,
        .thread_count = telemetry_options.spdlog_async_thread_count,
        .overflow_policy = telemetry_options.spdlog_async_overflow_policy,
    });
    auto telemetry = std::make_unique<nestdaq::telemetry::TelemetryLibrary>();
    auto telemetry_loaded = false;
    if (!telemetry_options.library.empty()) {
        telemetry_loaded = telemetry->load(telemetry_options.library);
        if (!telemetry_loaded) {
            LOG(error) << "Failed to load telemetry library '" << telemetry_options.library
                       << "': " << telemetry->getLastError();
            if (telemetry_options.required) {
                return EXIT_FAILURE;
            }
        } else {
            const auto telemetry_config = nestdaq::telemetry::makeConfig(telemetry_options);
            if (!telemetry->initializeWith(telemetry_config)) {
                LOG(error) << "Failed to initialize telemetry library '" << telemetry_options.library
                           << "': " << telemetry->getLastError();
                if (telemetry_options.required) {
                    return EXIT_FAILURE;
                }
                telemetry_loaded = false;
            } else {
                nestdaq::telemetry::warnUnknownSeverityFallback(telemetry_options.severity);
            }
        }
    }

    // ============================================
    // redis client setup
    const auto redis_uri  = vm["redis-uri"].as<std::string>();
    const auto channel   = daq::service::kCommandChannelName.data();
    const auto sep       = vm["separator"].as<std::string>();
    LOG(info) << "redis-server URI  = " << redis_uri;
    LOG(info) << "command-channel   = " << channel;
    LOG(info) << "separator         = " << sep;

    gDaqControl = std::make_unique<WebGui>();

    gDaqControl->setPollIntervalMs(vm["poll-interval"].as<uint64_t>());
    if (!gDaqControl->connectToRedis(redis_uri, channel, sep)) {
        return EXIT_FAILURE;
    }
    // ============================================
    gDaqControl->setSendFunction([](auto conn_id, const auto& arg) {
        if (gWsSessions.empty()) {
            LOG(debug) << " no websocket clients";
            std::this_thread::sleep_for(std::chrono::milliseconds(kWebSocketRetryIntervalMs));
            return;
        }
        if (conn_id==0) { // broadcast message to registered clients
            for (const auto& [i, t] : gWsSessions) {
                LOG(debug) << "send message to websocket client id = " << i << ", msg = " << arg;
                writeWebSocketMessage(i, arg);
            }
        } else {
            writeWebSocketMessage(conn_id, arg);
        }
    });
    gDaqControl->setTerminateFunction([]() {
        LOG(info) << " Termination is requested.";
    });

    gDaqControl->setPreRunCommand(vm["pre-run"].as<std::string>());
    gDaqControl->setPostRunCommand(vm["post-run"].as<std::string>());
    gDaqControl->setPreStopCommand(vm["pre-stop"].as<std::string>());
    gDaqControl->setPostStopCommand(vm["post-stop"].as<std::string>());

    // ============================================
    // http server setup
    const auto http_uri = vm["http-uri"].as<std::string>();
    LOG(info) << "http serve URI = " << http_uri;
    const auto &[http_scheme, http_address, http_port] = parseHttpUri(http_uri);
    LOG(info) << "http server scheme  = " << http_scheme;
    LOG(info) << "http server address = " << http_address;
    LOG(info) << "http server port    = " << http_port;
    const auto n_threads = vm["threads"].as<unsigned int>();
    LOG(info) << "http threads = " << n_threads;
    const auto doc_root = vm["doc-root"].as<std::string>();
    LOG(info) << "doc-root = " << doc_root;

    HttpWebSocketServer server(static_cast<int>(n_threads));
    server.run(http_scheme, http_address, http_port, doc_root);
    if (telemetry_loaded) {
        telemetry->shutdownTelemetry(telemetry_options.timeout_ms);
    }
    return ret;
}

//=============================================================================
// WebSocketHandle functions

void handleWebSocketClose(unsigned int id) {
    std::vector<std::pair<unsigned int, std::string>> v;
    {
        std::scoped_lock<std::mutex> lock{gWsMutex};
        gWsSessions.erase(id);
        for (const auto& [i, t] : gWsSessions) {
            v.emplace_back(i, t.second);
        }
    }
    gDaqControl->sendWebSocketIdList(v);
    LOG(info) << __func__ << " websocket id = " << id << " done";
}

void handleWebSocketConnect(const std::shared_ptr<WebSocketSession> &session) {
    unsigned int id{0};
    std::string msg{"My WebSocket Connection ID: "};
    auto d = currentDate();
    std::vector<std::pair<unsigned int, std::string>> v;
    {
        std::scoped_lock<std::mutex> lock{gWsMutex};
        id = session->id();
        msg += std::to_string(id) + " (Date: " + d + ")";
        gWsSessions.emplace(id, std::make_pair(session, d));
        for (const auto& [i, t] : gWsSessions) {
            v.emplace_back(i, t.second);
        }
    }
    gDaqControl->send(id, msg);
    gDaqControl->sendWebSocketIdList(v);
    LOG(info) << __func__ << " websocket id = " << id << " done";
}

void handleWebSocketRead(unsigned int id, const std::string& message) {
    gDaqControl->processData(id, message);
    LOG(trace) << __func__ << " websocket id = " << id << " done";
}

void handleWebSocketRead(unsigned int /*id*/, const std::vector<char>& /*message*/) {
}

void writeWebSocketMessage(unsigned int id, const std::string& message) {
    auto &[session, d] = gWsSessions[id];
    if (session) {
        session->write(message);
    }
    LOG(trace) << __func__ << " websocket id = " << id << " done";
}
