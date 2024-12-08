#include <iostream>
#include <string>

#include "ClientUI.h"


int main() {
    
	std::string serverAddress;
	int serverPort;
	std::cout << "Enter the server address: ";
	std::cin >> serverAddress;
	std::cout << "Enter the server port: ";
	std::cin >> serverPort;

	ClientUI clientUI = ClientUI(serverAddress, serverPort);
	clientUI.start();
    return 0;
}
