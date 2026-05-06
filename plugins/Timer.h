#pragma once

/**
 * @file Timer.h
 * @brief Repeating Boost.Asio timer wrapper used by service plugins.
 */

#include <functional>
#include <memory>
#include <boost/asio.hpp>

namespace daq::service {
namespace net = boost::asio;
using strand_t = net::strand<net::io_context::executor_type>;

class Timer {
public:
    Timer() = default;
    Timer(const Timer&) = delete;
    Timer& operator=(const Timer&) = delete;
    Timer(Timer&&) = delete;
    Timer& operator=(Timer&&) = delete;
    ~Timer() noexcept;

    void Start(const std::shared_ptr<net::io_context> &ctx,
//           const std::shared_ptr<strand_t> &strand,
               unsigned int timeoutMS,
               std::function<bool(const std::error_code &)> f);

private:
    void Start();

    std::shared_ptr<net::io_context> fContext;
//  std::shared_ptr<strand_t> fStrand;
    std::unique_ptr<net::steady_timer> fTimer;
    unsigned int fTimeoutMS{0};
    std::function<bool(const std::error_code &)> fHandle;

};

} // namespace daq::service
