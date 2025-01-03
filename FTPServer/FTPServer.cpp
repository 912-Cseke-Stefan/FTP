#define WORKING_DIRECTORY "D:\\FTP\\"
#define _CRT_SECURE_NO_WARNINGS

#include <string>
#include <filesystem>
#include <chrono>
#include <sstream>
#include "FTPServer.h"


FTPServer::FTPServer(std::string port, std::string path_to_users) : 
    control_socket(INVALID_SOCKET), port(port), thread_pool(10), working_directory(WORKING_DIRECTORY)
{
    WSADATA wsaData;
    struct addrinfo* result = NULL, hints;
    int iResult;

    // Initialize Winsock
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        throw std::exception("WSAStartup failed with error: %d\n", iResult);
    }

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    // Resolve the local address and port to be used by the server
    iResult = getaddrinfo(NULL, port.c_str(), &hints, &result);
    if (iResult != 0) {
        std::string msg("getaddrinfo failed: " + std::to_string(iResult) + "\n");
        WSACleanup();
        throw std::exception(msg.c_str());
    }

    // Create a SOCKET for the server to listen for client connections.
    control_socket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (control_socket == INVALID_SOCKET) {
        std::string msg("Error at socket(): " + std::to_string(WSAGetLastError()) + "\n");
        freeaddrinfo(result);
        WSACleanup();
        throw std::exception(msg.c_str());
    }

    // Setup the TCP listening socket
    iResult = bind(control_socket, result->ai_addr, (int)result->ai_addrlen);
    if (iResult == SOCKET_ERROR) {
        std::string msg("bind failed with error: " + std::to_string(WSAGetLastError()) + "\n");
        freeaddrinfo(result);
        closesocket(control_socket);
        WSACleanup();
    }

    freeaddrinfo(result);

    iResult = listen(control_socket, SOMAXCONN);
    if (iResult == SOCKET_ERROR) {
        std::string msg("listen failed with error: " + std::to_string(WSAGetLastError()) + "\n");
        closesocket(control_socket);
        WSACleanup();
        throw std::exception(msg.c_str());
    }

    // Prepare file with users
    std::ifstream file_of_users(path_to_users);
    std::string user, pass;
    std::getline(file_of_users, user);
    while (!file_of_users.eof())
    {
        std::getline(file_of_users, pass);
        users_and_passwords.push_back({ user, pass });
        std::getline(file_of_users, user);
    }
    file_of_users.close();

    // Prepare ports for passive mode
    for (int i = 5000; i < 5010; i++)  // 10 ports are enough since the server has only 10 threads
        available_ports.push(i);
}

void FTPServer::start() 
{
    while (true) {
        SOCKET clientSocket = accept(control_socket, nullptr, nullptr);
        if (clientSocket == INVALID_SOCKET) 
            continue;

        // Spawn thread or use async to handle client
        thread_pool.enqueue([clientSocket, this]() { get_command_from_client(clientSocket); });
    }
}

SOCKET FTPServer::start_listening_on_port(int data_port)
{
    SOCKET data_connection = INVALID_SOCKET;
    struct addrinfo* result = NULL, hints;
    int iResult;

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    // Resolve the local address and port to be used by the server
    iResult = getaddrinfo(NULL, std::to_string(data_port).c_str(), &hints, &result);
    if (iResult != 0) {
        std::string msg("getaddrinfo failed: " + std::to_string(iResult) + "\n");
        WSACleanup();
        throw std::exception(msg.c_str());
    }

    // Create a SOCKET for the server to listen for client connections.
    data_connection = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (data_connection == INVALID_SOCKET) {
        std::string msg("Error at socket(): " + std::to_string(WSAGetLastError()) + "\n");
        freeaddrinfo(result);
        WSACleanup();
        throw std::exception(msg.c_str());
    }

    // Setup the TCP listening socket
    iResult = bind(data_connection, result->ai_addr, (int)result->ai_addrlen);
    if (iResult == SOCKET_ERROR) {
        std::string msg("bind failed with error: " + std::to_string(WSAGetLastError()) + "\n");
        freeaddrinfo(result);
        closesocket(data_connection);
        WSACleanup();
    }

    freeaddrinfo(result);

    iResult = listen(data_connection, SOMAXCONN);
    if (iResult == SOCKET_ERROR) {
        std::string msg("listen failed with error: " + std::to_string(WSAGetLastError()) + "\n");
        closesocket(data_connection);
        WSACleanup();
        throw std::exception(msg.c_str());
    }

    return data_connection;
}

SOCKET FTPServer::accept_connection_on_socket(SOCKET socket)
{
    SOCKET data_transmission_socket = accept(socket, nullptr, nullptr);
    if (data_transmission_socket == INVALID_SOCKET) {
        std::string msg("accept failed with error: " + std::to_string(WSAGetLastError()) + "\n");
        throw std::exception(msg.c_str());
    }

    return data_transmission_socket;
}

void FTPServer::get_command_from_client(SOCKET client_socket)
{
    send(client_socket, "220 Connected, and you only get one line\r\n", 42, 0);

    std::string current_user;
    std::string directory_of_user;
    bool passive = false;
    int passive_port = -1;
    SOCKET data_connection = INVALID_SOCKET;
    SOCKET listening_socket;

    int iResult;
    char recvbuf[101];
    int recvbuflen = 100;

    char command_builder[105];
    int current_index = 0;

    do
    {
        iResult = recv(client_socket, recvbuf, recvbuflen, 0);
        if (iResult > 0)
        {
            recvbuf[iResult] = 0;
            strncpy(command_builder + current_index, recvbuf, 100 - current_index);
            command_builder[100] = '\0';
            if (strlen(command_builder) >= 100)
                send(client_socket, "500 Command is too long\r\n", 25, 0);  // Syntax error, command unrecognized. In this case, command line too long.
            else
            {
                current_index += iResult;
                if (command_builder[strlen(command_builder) - 1] == '\n')  // this is a valid command ended in <CRLF>
                {
                    std::string command = first_word_to_lower(command_builder);
                    if (command == "user")
                    {
                        std::string username = get_argument(command_builder);
                        if (is_username_a_user(username))
                        {
                            current_user = username;
                            send(client_socket, "331 User name okay, need password\r\n", 35, 0);  // User name okay, need password.
                        }
                        else
                            send(client_socket, "530 Not logged in\r\n", 19, 0);  // Not logged in.
                    }
                    else if (command == "pass")
                    {
                        if (current_user == "")
                            send(client_socket, "530 Not logged in\r\n", 19, 0);  // Not logged in.
                        else
                        {
                            std::string passwd = get_argument(command_builder);
                            if (is_valid_password(current_user, passwd))
                            {
                                directory_of_user = working_directory + current_user + '\\';  // no path traversal should be possible
                                send(client_socket, "230 User logged in, proceed\r\n", 29, 0);  // User logged in, proceed.
                            }
                            else
                            {
                                current_user = "";
                                send(client_socket, "530 Not logged in\r\n", 19, 0);  // Not logged in.
                            }
                        }
                    }
                    else if (command == "quit")
                    {
                        if (directory_of_user == "")
                            send(client_socket, "530 Not logged in\r\n", 19, 0);  // Not logged in.
                        else
                        {
                            current_user = "";
                            directory_of_user = "";
                            send(client_socket, "221 Logged out\r\n", 16, 0);  // Logged out.
                            access_the_queue.lock();
                            available_ports.push(passive_port);
                            access_the_queue.unlock();
                            passive_port = -1;
                        }
                    }
                    else if (command == "retr")
                    {
                        if (directory_of_user == "")
                            send(client_socket, "530 Not logged in\r\n", 19, 0);  // Not logged in.
                        else
                        {
                            std::string dirty_filename = get_argument(command_builder);
                            std::string filename = clean_filename(dirty_filename);
                            std::string path_to_file = directory_of_user + filename;

                            std::ifstream file_reader(path_to_file, std::ifstream::binary);
                            if (!file_reader.is_open())
                                send(client_socket, "550 File not found\r\n", 20, 0);
                            else
                            {
                                if (passive)
                                {
                                    if (data_connection != INVALID_SOCKET)
                                        send(client_socket, "125 Data connection already open; transfer starting\r\n", 53, 0);
                                    else
                                    {
                                        data_connection = accept_connection_on_socket(listening_socket);
                                        send(client_socket, "150 Connection accepted\r\n", 25, 0);
                                    }
                                    char buffer[1000];
                                    while (file_reader.read(buffer, 1000) || file_reader.gcount() > 0) 
                                        send(data_connection, buffer, static_cast<int>(file_reader.gcount()), 0);

                                    send(client_socket, "226 Transfer OK\r\n", 17, 0);

                                    // deallocate resources for this data socket
                                    closesocket(data_connection);
                                    passive = false;
                                    data_connection = INVALID_SOCKET;
                                    access_the_queue.lock();
                                    available_ports.push(passive_port);
                                    access_the_queue.unlock();
                                    passive_port = -1;
                                }
                                else
                                    send(client_socket, "425 Can't open data connection\r\n", 32, 0);  // should have received port from client

                                file_reader.close();
                            }
                        }
                    }
                    else if (command == "stor")
                    {
                        if (directory_of_user == "")
                            send(client_socket, "530 Not logged in\r\n", 19, 0);  // Not logged in.
                        else
                        {
                            std::string dirty_filename = get_argument(command_builder);
                            std::string filename = clean_filename(dirty_filename);
                            std::string path_to_file = directory_of_user + filename;

                            std::ofstream file_writer(path_to_file, std::ofstream::binary);
                            if (!file_writer.is_open())
                                send(client_socket, "550 File was not created\r\n", 26, 0);
                            else
                            {
                                if (passive)
                                {
                                    if (data_connection != INVALID_SOCKET)
                                        send(client_socket, "125 Data connection already open; transfer starting\r\n", 53, 0);
                                    else
                                    {
                                        data_connection = accept_connection_on_socket(listening_socket);
                                        send(client_socket, "150 Connection accepted\r\n", 25, 0);
                                    }
                                    char buffer[1000];
                                    int bytes_read;
                                    while ((bytes_read = recv(data_connection, buffer, 1000, 0)) > 0)
                                        file_writer.write(buffer, bytes_read);

                                    send(client_socket, "226 Transfer OK\r\n", 17, 0);

                                    // deallocate resources for this data socket
                                    if (data_connection != INVALID_SOCKET)  // client should have already closed the connection, but anyway
                                    {
                                        closesocket(data_connection);
                                        data_connection = INVALID_SOCKET;
                                    }
                                    passive = false;
                                    access_the_queue.lock();
                                    available_ports.push(passive_port);
                                    access_the_queue.unlock();
                                    passive_port = -1;
                                }
                                else
                                    send(client_socket, "425 Can't open data connection\r\n", 32, 0);  // should have received port from client

                                file_writer.close();
                            }
                        }
                    }
                    else if (command == "list")
                    {
                        if (directory_of_user == "")
                            send(client_socket, "530 Not logged in\r\n", 19, 0);  // Not logged in.
                        else
                        {
                            // we ignore the argument for now, as we won't have any folder hierarchy
                            if (passive)
                            {
                                if (data_connection != INVALID_SOCKET)
                                    send(client_socket, "125 Data connection already open; transfer starting\r\n", 53, 0);
                                else
                                {
                                    data_connection = accept_connection_on_socket(listening_socket);
                                    send(client_socket, "150 Connection accepted\r\n", 25, 0);
                                }

                                char buffer[1000];
                                char zero_buffer[1000] = {};
                                for (const auto& entry : std::filesystem::directory_iterator(directory_of_user))
                                {
                                    strncpy(buffer, zero_buffer, 1000);
                                    int length = 0;
                                        
                                    strncpy(buffer, "-rw-rw-rw- 1 ", 15);
                                    length += 13;

                                    strncpy(buffer + length, current_user.c_str(), current_user.size());
                                    length += static_cast<int>(current_user.size());
                                    buffer[length] = ' ';
                                    length++;

                                    strncpy(buffer + length, current_user.c_str(), current_user.size());
                                    length += static_cast<int>(current_user.size());
                                    buffer[length] = ' ';
                                    length++;

                                    std::string size_of_file = std::to_string(std::filesystem::file_size(entry.path()));
                                    std::string padding_of_spaces = std::string(13 - size_of_file.size(), ' ');
                                    std::string size_string = padding_of_spaces + size_of_file;
                                    strncpy(buffer + length,
                                            size_string.c_str(),
                                            size_string.size());
                                    length += static_cast<int>(size_string.size());
                                    buffer[length] = ' ';
                                    length++;

                                    char formatted_date[30];
                                    get_formatted_date_and_time_from_file_time_in_buffer_of_30_bytes(
                                        std::filesystem::last_write_time(entry.path()), formatted_date);
                                    strncpy(buffer + length,
                                            formatted_date,
                                            13);
                                    length += 13;

                                    strncpy(buffer + length, 
                                            entry.path().string().substr(directory_of_user.size()).c_str(), 
                                            997 - length);
                                    buffer[strlen(buffer)] = 13;
                                    buffer[strlen(buffer)] = 10;

                                    send(data_connection, buffer, static_cast<int>(strlen(buffer)), 0);
                                }

                                send(client_socket, "226 Transfer OK\r\n", 17, 0);

                                // deallocate resources for this data socket
                                closesocket(data_connection);
                                passive = false;
                                data_connection = INVALID_SOCKET;
                                access_the_queue.lock();
                                available_ports.push(passive_port);
                                access_the_queue.unlock();
                                passive_port = -1;
                            }
                            else
                                send(client_socket, "425 Can't open data connection\r\n", 32, 0);  // should have received port from client
                        }
                    }
                    else if (command == "pasv")
                    {
                        if (directory_of_user == "")
                            send(client_socket, "530 Not logged in\r\n", 19, 0);  // Not logged in.
                        else
                        {
                            passive = true;
                            access_the_queue.lock();
                            passive_port = available_ports.front();
                            available_ports.pop();
                            access_the_queue.unlock();
                            // prepare to listen on the offered port
                            listening_socket = start_listening_on_port(passive_port);
                            
                            struct addrinfo* result = NULL, * ptr = NULL, hints;
                            char hostname[256];

                            if (gethostname(hostname, sizeof(hostname)) != 0) {
                                //std::cerr << "gethostname failed with error " << WSAGetLastError() << std::endl;
                                WSACleanup();
                                return;
                            }

                            ZeroMemory(&hints, sizeof(hints));
                            hints.ai_family = AF_INET;  // IPv4
                            hints.ai_socktype = SOCK_STREAM;
                            hints.ai_protocol = IPPROTO_TCP;

                            // Get address info for the local machine's hostname
                            int iResult = getaddrinfo(hostname, NULL, &hints, &result);
                            if (iResult != 0) {
                                //std::cerr << "getaddrinfo failed with error " << iResult << std::endl;
                                WSACleanup();
                                return;
                            }

                            char ipStr[INET_ADDRSTRLEN];
                            // Loop through all the results and print the first valid address
                            for (ptr = result; ptr != NULL; ptr = ptr->ai_next) 
                            {
                                struct sockaddr_in* sockaddr_ipv4 = (struct sockaddr_in*)ptr->ai_addr;

                                // Convert the IP address to a string
                                inet_ntop(AF_INET, &sockaddr_ipv4->sin_addr, ipStr, sizeof(ipStr));
                                break; // We found a valid address, so we break
                            }

                            freeaddrinfo(result);

                            std::string correct_ip_string = std::string(ipStr);
                            std::ranges::replace(correct_ip_string, '.', ',');

                            send(client_socket, 
                                 (std::string("227 Entering Passive Mode (") + correct_ip_string + std::string(",") + 
                                    std::to_string(passive_port/256) + std::string(",") + 
                                    std::to_string(passive_port%256) + std::string(")") + std::string("\r\n")).c_str(), 
                                 32 +
                                    static_cast<int>(correct_ip_string.size()) + 
                                    static_cast<int>(std::to_string(passive_port/256).size()) + 
                                    static_cast<int>(std::to_string(passive_port%256).size()),
                                 0);  // Entering Passive Mode (h1,h2,h3,h4,p1,p2).
                        }
                    }
                    else if (command == "pwd")
                    {
                        if (directory_of_user == "")
                            send(client_socket, "530 Not logged in\r\n", 19, 0);  // Not logged in.
                        else
                            send(client_socket, "257 \"/\" is current directory\r\n", 30, 0);
                    }
                    else if (command == "type")
                    {
                        if (directory_of_user == "")
                            send(client_socket, "530 Not logged in\r\n", 19, 0);  // Not logged in.
                        else
                            send(client_socket, "200 Everything is fine...\r\n", 27, 0);
                    }
                    else if (command == "cwd")
                    {
                        if (directory_of_user == "")
                            send(client_socket, "530 Not logged in\r\n", 19, 0);  // Not logged in.
                        else
                            send(client_socket, "250 Everything is fine...\r\n", 27, 0);
                    }
                    else
                    {
                        send(client_socket, "502 Command not implemented\r\n", 29, 0);  // Command not implemented.
                    }

                    // clear the command_builder
                    current_index = 0;
                }
            }
        }
        else if (iResult == 0)
        {
            //socket was closed
        }
        else 
        {
            std::string msg("recv failed with error: " + std::to_string(WSAGetLastError()) + "\n");
            closesocket(client_socket);
            WSACleanup();
            throw std::exception(msg.c_str());
        }
    } while (iResult > 0);

    if (passive_port != -1)
    {
        access_the_queue.lock();
        available_ports.push(passive_port);
        access_the_queue.unlock();
    }
}

void FTPServer::get_formatted_date_and_time_from_file_time_in_buffer_of_30_bytes(
    std::filesystem::file_time_type last_write, char* formatted_time)
{
    std::filesystem::file_time_type myfmtime = last_write;

    // file_clock::file_time_type -> utc_clock::time_point
    // -> system_clock::time_point -> time_t

    std::chrono::time_point<std::chrono::system_clock> ftsys;

#ifdef  __GNUC__
    ftsys = chrono::file_clock::to_sys(myfmtime);
#else
    // MSVC, CLANG
    ftsys = std::chrono::utc_clock::to_sys(std::chrono::file_clock::to_utc(myfmtime));
#endif

    time_t ftt = std::chrono::system_clock::to_time_t(ftsys);

    struct tm ftm; ftm = { 0 };
    localtime_s(&ftm, &ftt);
    strftime(formatted_time, 30, "%b %d %H:%M ", &ftm);
}

std::string FTPServer::first_word_to_lower(const char* command)
{
    // assume command is null-terminated byte string
    int index = 0;
    while (command[index] != ' ' && command[index] != '\r' && index < strlen(command))
        index++;

    char word[105];
    strncpy(word, command, index);
    word[index] = '\0';
    for (int i = 0; word[i] != 0; i++)
        word[i] = std::tolower(word[i]);

    return std::string(word);
}

std::string FTPServer::get_argument(const char* command)
{
    // assume command is null-terminated byte string
    int index = 0;
    while (command[index] != ' ' && index < strlen(command))
        index++;

    if (index == strlen(command))
        return std::string();  // should never get here since this method is only called for valid commands

    int index2 = ++index;  // moved to second word
    while (command[index2] != '\r' && index2 < strlen(command))
        index2++;

    char word[105];
    strncpy(word, command + index, index2 - index);
    word[index2 - index] = '\0';

    return std::string(word);
}

std::string FTPServer::clean_filename(const std::string& filename)
{
    std::string result = filename;//"/sadf/asdf//asdf////dsf../../dsf/././/..../d"
    size_t index_of_bad_characters;
    while ((index_of_bad_characters = result.find("..")) != std::string::npos)
        result.erase(index_of_bad_characters, 2);
    while ((index_of_bad_characters = result.find("\\")) != std::string::npos)
        result.erase(index_of_bad_characters, 1);
    while ((index_of_bad_characters = result.find("/")) != std::string::npos)
        result.erase(index_of_bad_characters, 1);
    return result;
}

bool FTPServer::is_username_a_user(std::string username)
{
    for (std::pair<std::string, std::string>& entry: users_and_passwords)
        if (entry.first == username)
            return true;

    return false;
}

bool FTPServer::is_valid_password(std::string username, std::string password)
{
    for (std::pair<std::string, std::string>& entry : users_and_passwords)
        if (entry.first == username)
        {
            if (entry.second == password)
                return true;
            else
                return false;
        }

    return false;
}
