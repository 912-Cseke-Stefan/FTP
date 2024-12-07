#include "FTPClient.h"

const int BUFFER_SIZE = 8192;


FTPClient::FTPClient(const std::string& address, int port) : serverAddress(address), serverPort(port) {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        throw std::runtime_error("WSAStartup failed");
    }

    controlSocket = createSocket();

    sockaddr_in serverAddr = {};
    serverAddr.sin_family = AF_INET;
    inet_pton(AF_INET, serverAddress.c_str(), &serverAddr.sin_addr);
    serverAddr.sin_port = htons(serverPort);

    if (connect(controlSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        throw std::runtime_error("Failed to connect: " + std::to_string(WSAGetLastError()));
    }

    std::cout << readResponse();
}


SOCKET FTPClient::createSocket() {
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) {
        throw std::runtime_error("Failed to create socket: " + std::to_string(WSAGetLastError()));
    }
    return s;
}


void FTPClient::sendCommand(const std::string& cmd)  const{
    std::string message = cmd + "\r\n";
    send(controlSocket, message.c_str(), message.length(), 0);
}


std::string FTPClient::readResponse() const {
    char buffer[BUFFER_SIZE];
    int bytesReceived = recv(controlSocket, buffer, BUFFER_SIZE - 1, 0);
    if (bytesReceived == SOCKET_ERROR) {
        throw std::runtime_error("Failed to read response: " + std::to_string(WSAGetLastError()));
    }
    buffer[bytesReceived] = '\0';
    return std::string(buffer);
}


SOCKET FTPClient::enterPassiveMode() {
    sendCommand("PASV");
    std::string response = readResponse();
    if (response.substr(0, 3) != "227") {
        throw std::runtime_error("Failed to enter passive mode: " + response);
    }

    size_t start = response.find('(');
    size_t end = response.find(')', start);
    std::string data = response.substr(start + 1, end - start - 1);

    std::vector<int> values;
    size_t pos = 0;
    while ((pos = data.find(',')) != std::string::npos) {
        values.push_back(std::stoi(data.substr(0, pos)));
        data.erase(0, pos + 1);
    }
    values.push_back(std::stoi(data));

    std::string ip = std::to_string(values[0]) + "." + std::to_string(values[1]) + "." +
        std::to_string(values[2]) + "." + std::to_string(values[3]);
    int port = values[4] * 256 + values[5];

    sockaddr_in dataAddr = {};
    dataAddr.sin_family = AF_INET;
    inet_pton(AF_INET, ip.c_str(), &dataAddr.sin_addr);
    dataAddr.sin_port = htons(port);

    SOCKET dataSocket = createSocket();
    if (connect(dataSocket, (sockaddr*)&dataAddr, sizeof(dataAddr)) == SOCKET_ERROR) {
        throw std::runtime_error("Failed to connect to data socket: " + std::to_string(WSAGetLastError()));
    }

    return dataSocket;
}


FTPClient::~FTPClient() {
    closesocket(controlSocket);
    WSACleanup();
}


std::string FTPClient::user(const std::string& username) {
    sendCommand("USER " + username);
	return readResponse();
}


std::string FTPClient::pass(const std::string& password) {
	sendCommand("PASS " + password);
	return readResponse();
}


std::string FTPClient::logout() {
    sendCommand("QUIT");
	return readResponse();
}


void FTPClient::uploadFile(const std::string& localPath, const std::string& remotePath) {
    SOCKET dataSocket = enterPassiveMode();

    sendCommand("STOR " + remotePath);
    std::cout << readResponse();

    std::ifstream file(localPath, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + localPath);
    }

    char buffer[BUFFER_SIZE];
    while (file.read(buffer, BUFFER_SIZE) || file.gcount() > 0) {
        send(dataSocket, buffer, static_cast<int>(file.gcount()), 0);
    }

    file.close();
    closesocket(dataSocket);
    std::cout << readResponse();
}


void FTPClient::downloadFile(const std::string& remotePath, const std::string& localPath) {
    SOCKET dataSocket = enterPassiveMode();

    sendCommand("RETR " + remotePath);
    std::cout << readResponse();

    std::ofstream file(localPath, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to create file: " + localPath);
    }

    char buffer[BUFFER_SIZE];
    int bytesRead;
    while ((bytesRead = recv(dataSocket, buffer, BUFFER_SIZE, 0)) > 0) {
        file.write(buffer, bytesRead);
    }

    file.close();
    closesocket(dataSocket);
    std::cout << readResponse();
}


void FTPClient::listFiles() {
	SOCKET dataSocket = enterPassiveMode();
	sendCommand("LIST");
	std::cout << readResponse();
	char buffer[BUFFER_SIZE];
	int bytesRead;
	while ((bytesRead = recv(dataSocket, buffer, BUFFER_SIZE, 0)) > 0) {
		std::cout.write(buffer, bytesRead);
	}
	closesocket(dataSocket);
	std::cout << readResponse();
}