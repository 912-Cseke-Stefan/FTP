#pragma once

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <winsock2.h>
#include <WS2tcpip.h>


#pragma comment(lib, "Ws2_32.lib")

class FTPClient {
private:
    SOCKET controlSocket;
    std::string serverAddress;
    int serverPort;

    SOCKET createSocket();
    void sendCommand(const std::string& cmd) const;
    std::string readResponse() const;
	SOCKET enterPassiveMode();

public:
    FTPClient(const std::string& address, int port);
    ~FTPClient();

	std::string user(const std::string& username);
	std::string pass(const std::string& password);
    std::string logout();
    void uploadFile(const std::string& localPath, const std::string& remotePath);
    void downloadFile(const std::string& remotePath, const std::string& localPath);
	void listFiles();
};