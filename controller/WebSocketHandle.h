#pragma once

/**
 * @file WebSocketHandle.h
 * @brief Global connection callbacks used by Beast WebSocket sessions.
 */

#include <memory>
#include <string>
#include <vector>

class websocket_session;

void OnClose(unsigned int id);
void OnConnect(const std::shared_ptr<websocket_session> &session);
void OnRead(unsigned int id, const std::string& message);
void OnRead(unsigned int id, const std::vector<char>& message);
void Write(unsigned int id, const std::string& message);
