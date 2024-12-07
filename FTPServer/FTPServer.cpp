#define WORKING_DIRECTORY "D:\\FTP\\"
#define _CRT_SECURE_NO_WARNINGS

#include <string>
#include "FTPServer.h"


FTPServer::FTPServer(std::string port, std::string path_to_users) : 
    control_socket(INVALID_SOCKET), port(port), thread_pool(10), users_and_passwords()
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
                        current_user = "";
                        directory_of_user = "";
                        send(client_socket, "221 Logged out\r\n", 16, 0);  // Logged out.
                        access_the_queue.lock();
                        available_ports.push(passive_port);
                        access_the_queue.unlock();
                        passive_port = -1;
                    }
                    else if (command == "retr")
                    {

                    }
                    else if (command == "stor")
                    {

                    }
                    else if (command == "list")
                    {

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
                            SOCKET listening_socket = start_listening_on_port(passive_port);
                            send(client_socket, (std::string("227 ") + std::to_string(passive_port)).c_str(), 8, 0);  // Entering Passive Mode (h1,h2,h3,h4,p1,p2).
                            data_connection = accept_connection_on_socket(listening_socket);
                        }
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

std::string FTPServer::first_word_to_lower(const char* command)
{
    // assume command is null-terminated byte string
    int index = 0;
    while (command[index] != ' ' && index < strlen(command))
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
