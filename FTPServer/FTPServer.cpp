#define DEFAULT_PORT "21"

#include <iostream>
#include <string>
#include "FTPServer.h"


FTPServer::FTPServer(std::string port = DEFAULT_PORT) : 
    control_socket(INVALID_SOCKET), port(port), thread_pool(10)
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


int main() {
    try {
        FTPServer server = FTPServer();
        std::cout << "Listening on port 21...\n";
        server.start();
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
    return 0;
}
