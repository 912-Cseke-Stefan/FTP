#pragma once

#include <fstream>
#include <queue>
#include <filesystem>
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
    std::string working_directory;
    std::queue<int> available_ports;
    std::mutex access_the_queue;

    void handle_user(const std::string& username) {}
    void handle_pass(const std::string& password) {}
    void handle_list() {}
    void handle_retr() {}
    void handle_stor() {}
    SOCKET start_listening_on_port(int data_port);
    SOCKET accept_connection_on_socket(SOCKET socket);
    void get_command_from_client(SOCKET client_socket);
    void get_formatted_date_and_time_from_file_time_in_buffer_of_30_bytes(std::filesystem::file_time_type last_write, char* formatted_time);

    std::string first_word_to_lower(const char* command);
    std::string get_argument(const char* command);
    std::string clean_filename(const std::string& filename);

    bool is_username_a_user(std::string username);
    bool is_valid_password(std::string username, std::string password);

public:
    FTPServer(std::string port = DEFAULT_PORT, std::string path_to_users = DEFAULT_PATH);

    void start();
};
