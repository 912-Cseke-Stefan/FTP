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
    FTPServer(std::string port);

    void start();
};
