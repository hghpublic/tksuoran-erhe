#pragma once

#include "erhe/net/socket.hpp"

namespace erhe::net
{

class Client
{
public:
    auto connect            (const char* address, int port) -> bool;
    void disconnect         ();
    auto send               (const std::string& message) -> bool;
    void set_receive_handler(Receive_handler receive_handler);
    auto poll               (int timeout_ms) -> bool;
    auto get_state          () -> Socket::State;

private:
    Socket m_socket;
};

} // namespace erhe::net
