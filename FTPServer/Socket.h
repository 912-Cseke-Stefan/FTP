#pragma once

#include <stdexcept>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")

class Socket {
private:
    SOCKET m_socket;

public:
    explicit Socket(int domain, int type);

    ~Socket();
};

