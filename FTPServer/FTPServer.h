#pragma once

#include <fstream>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
#include "ThreadPool.h"

#define DEFAULT_PORT "22"
#define DEFAULT_PATH "D:\\FTP\\users"

class FTPServer {
private:
    SOCKET control_socket;
    std::string port;
    ThreadPool thread_pool;
    std::vector<std::pair<std::string, std::string>> users_and_passwords;

    void handle_user(const std::string& username) {}
    void handle_pass(const std::string& password) {}
    void handle_list() {}
    void handle_retr() {}
    void handle_stor() {}
    void handle_pasv() {}
    void get_command_from_client(SOCKET client_socket);

    std::string first_word_to_lower(const char* command);
    std::string get_argument(const char* command);

    bool is_username_a_user(std::string username);
    bool is_valid_password(std::string username, std::string password);

public:
    FTPServer(std::string port = DEFAULT_PORT, std::string path_to_users = DEFAULT_PATH);

    void start();
};
