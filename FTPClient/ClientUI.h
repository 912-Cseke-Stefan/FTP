#pragma once

#include <iostream>
#include <string>

#include "FTPClient.h"
#include "Utils.h"


class ClientUI {
private:
	std::string serverAddress;
	int serverPort;


public:
	ClientUI(std::string serverAddress, int serverPort);

	void printHelp();

	void startClient(FTPClient& client);

	void login(std::string serverAddress, int serverPort);

	void start();
};