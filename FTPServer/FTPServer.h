#pragma once

#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")

class FTPServer {
private:
    SOCKET control_socket;
    std::string port;

    void handleUser(const std::string& username) {}
    void handlePass(const std::string& password) {}
    void handleList() {}
    void handleRetr() {}
    void handleStor() {}

public:
    FTPServer(std::string port) :
        m_control_socket(),
        m_port(port) {

        sockaddr_in serverAddr{};
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(m_port);
        serverAddr.sin_addr.s_addr = INADDR_ANY;

        if (bind(m_control_socket.get(),
            reinterpret_cast<sockaddr*>(&serverAddr),
            sizeof(serverAddr)) == SOCKET_ERROR) {
            throw std::runtime_error("Bind failed");
        }

        listen(m_control_socket.get(), SOMAXCONN);
    }

    void start();
};
