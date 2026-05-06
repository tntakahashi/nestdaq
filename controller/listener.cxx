
/** @file
 *  @brief Implements TCP listener setup for controller HTTP sessions.
 */

#include <iostream>

#include "controller/http_session.h"
#include "controller/listener.h"

//_____________________________________________________________________________
listener::listener(const std::shared_ptr<net::io_context> &ioc, const tcp::endpoint& endpoint, std::shared_ptr<std::string const> const& doc_root)
    : ioc_(ioc)
    , acceptor_(net::make_strand(*ioc))
    , doc_root_(doc_root)
    , status_(StatusGood)
{
    beast::error_code ec;

    // Open the acceptor
    const auto openResult = acceptor_.open(endpoint.protocol(), ec);
    boost::ignore_unused(openResult);
    if(ec) {
        fail(ec, "listener open");
        status_ = ec.message();
        return;
    }

    // Allow address reuse
    const auto setOptionResult = acceptor_.set_option(net::socket_base::reuse_address(true), ec);
    boost::ignore_unused(setOptionResult);
    if(ec) {
        fail(ec, "listener set_option");
        status_ = ec.message();
        return;
    }

    // Bind to the server address
    const auto bindResult = acceptor_.bind(endpoint, ec);
    boost::ignore_unused(bindResult);
    if(ec) {
        fail(ec, "listener bind");
        status_ = ec.message();
        return;
    }

    // Start listening for connections
    const auto listenResult = acceptor_.listen(net::socket_base::max_listen_connections, ec);
    boost::ignore_unused(listenResult);
    if(ec) {
        fail(ec, "listener listen");
        status_ = ec.message();
        return;
    }
}

//_____________________________________________________________________________
void listener::do_accept()
{
    // The new connection gets its own strand
    acceptor_.async_accept(net::make_strand(*ioc_),
                           beast::bind_front_handler(&listener::on_accept, shared_from_this())
                          );
}

//_____________________________________________________________________________
void listener::on_accept(beast::error_code ec, tcp::socket socket)
{
    if(ec) {
        fail(ec, "listener accept");
    } else {
        // Create the http session and run it
        std::make_shared<http_session>(std::move(socket), doc_root_)->run();
    }

    // Accept another connection
    do_accept();
}
