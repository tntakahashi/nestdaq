/** @file
 *  @brief Implements WebSocket session management for browser clients.
 */

#include <algorithm>
#include <mutex>

#include <fairlogger/Logger.h>

#include "controller/WebSocketHandle.h"
#include "controller/websocket_session.h"

//_____________________________________________________________________________
WebSocketSession::WebSocketSession(tcp::socket&& socket)
    : fWebSocket(std::move(socket))
{
}

//_____________________________________________________________________________
void WebSocketSession::onAccept(beast::error_code ec)
{
    LOG(debug) << " websocket session : new connection\n";
    if(ec) {
        fail(ec, "websocket accept");
        return;
    }

    static unsigned int gLastId{0};
    static std::mutex gMutex;
    {
        std::scoped_lock<std::mutex> lock{gMutex};
        fId = ++gLastId;
        handleWebSocketConnect(shared_from_this());
    }

    // Read a message
    doRead();
}

//_____________________________________________________________________________
void WebSocketSession::doRead()
{
    // Read a message into our buffer
    fWebSocket.async_read(fBuffer,
                   beast::bind_front_handler(&WebSocketSession::onRead, shared_from_this())
                  );
}

//_____________________________________________________________________________
void WebSocketSession::onRead(beast::error_code ec, std::size_t bytes_transferred)
{
    boost::ignore_unused(bytes_transferred);

    // This indicates that the WebSocketSession was closed
    if(ec == websocket::error::closed) {
        LOG(warn) << "websocket session : " << ec.what();
        handleWebSocketClose(fId);
        return;
    }

    if(ec) {
        LOG(warn) << "websocket session : " << ec.what();
        fail(ec, "websocket read");
        handleWebSocketClose(fId);
        return;
    }

    fWebSocket.text(fWebSocket.got_text());
    if (bytes_transferred>0) {
        if (fWebSocket.got_text()) {
            const std::string m(beast::buffers_to_string(fBuffer.data()));
//      std::cout << "received message: got_text() ? " << fWebSocket.got_text()
//                << " buffer (size = " << m.size() << " bytes): " << m << std::endl;
            fBuffer.consume(fBuffer.size());
            handleWebSocketRead(fId, m);

        } else {
            const auto m = beast::buffers_to_string(fBuffer.data());
            std::vector<char> buf(m.begin(), m.end());
//      std::cout << "received message: got_text() ? " << fWebSocket.got_text() << "\n"
//                << " buffer (" << fBuffer.size() << " bytes, "
//                << " transferred: " << bytes_transferred << " bytes)\n";
//      std::for_each(buf.begin(), buf.end(),
//                   [](auto x) { std::cout << static_cast<uint16_t>(x) << " "; });
//      std::cout << std::endl;
            fBuffer.consume(fBuffer.size());
            handleWebSocketRead(fId, buf);
        }
    } else {
    }

    // Do another read
    doRead();
}

//_____________________________________________________________________________
void WebSocketSession::write(const std::string &message)
{
    // synchronous write
    fWebSocket.write(net::buffer(message));
}
