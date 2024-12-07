#pragma once

#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
#include "ThreadPool.h"

class FTPServer {
private:
    SOCKET control_socket;
    std::string port;
    ThreadPool thread_pool;

    void handle_user(const std::string& username) {}
    void handle_pass(const std::string& password) {}
    void handle_list() {}
    void handle_retr() {}
    void handle_stor() {}
    void handle_pasv() {}
    void get_command_from_client(SOCKET client_socket);

    std::string first_word_to_lower(const char* command);

public:
    FTPServer(std::string port);

    void start();
};
