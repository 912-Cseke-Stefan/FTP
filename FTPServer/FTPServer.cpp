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

void FTPServer::get_command_from_client(SOCKET client_socket)
{
    std::string current_user;

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
                send(client_socket, "500", 3, 0);  // Syntax error, command unrecognized. In this case, command line too long.
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
                            send(client_socket, "331", 3, 0);  // User name okay, need password.
                        }
                        else
                            send(client_socket, "530", 3, 0);  // Not logged in.
                    }
                    else if (command == "pass")
                    {
                        std::string passwd = get_argument(command_builder);
                        if (is_valid_password(current_user, passwd))
                            send(client_socket, "230", 3, 0);  // User logged in, proceed.
                        else
                        {
                            current_user = "";
                            send(client_socket, "530", 3, 0);  // Not logged in.
                        }
                    }
                    else if (command == "quit")
                    {
                        current_user = "";
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

                    }
                    else
                    {
                        send(client_socket, "502", 3, 0);  // Command not implemented.
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
