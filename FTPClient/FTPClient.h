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

	void user(const std::string& username);
	void pass(const std::string& password);
    void logout();
    void uploadFile(const std::string& localPath, const std::string& remotePath);
    void downloadFile(const std::string& remotePath, const std::string& localPath);
	void listFiles();
};