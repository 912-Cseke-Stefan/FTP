#include <iostream>
#include <string>

#include "FTPClient.h"
#include "Utils.h"


static void printHelp() {
	std::cout << "Commands:" << std::endl;
	std::cout << "list - List files in the current directory" << std::endl;
	std::cout << "stor <local file> <remote file> - Upload a file" << std::endl;
	std::cout << "retr <remote file> <local file> - Download a file" << std::endl;
	std::cout << "exit - Exit the program" << std::endl;
}

int main() {
    
	std::string serverAddress;
	int serverPort;
	std::cout << "Enter the server address: ";
	std::cin >> serverAddress;
	std::cout << "Enter the server port: ";
	std::cin >> serverPort;

	try {
		std::string username, password;
		FTPClient client(serverAddress, serverPort);


		std::cout << "Enter username: ";
		std::cin >> username;
		if (!validateUserCredentials(username)) {
			std::cerr << "Invalid username" << std::endl;
			return 1;
		}
		client.user(username);

		std::cout << "Enter password: ";
		std::cin >> password;
		if (!validateUserCredentials(password)) {
			std::cerr << "Invalid password" << std::endl;
			return 1;
		}
		client.pass(password);

		while (true) {
			std::string command;
			std::cout << "> ";
			std::getline(std::cin, command);
			std::vector<std::string> tokens;

			if (command.empty()) {
				continue;
			}

			tokens = getTokens(command);

			if (tokens[0] == "list") {
				client.listFiles();
			}

			else if (tokens[0] == "exit") {
				client.logout();
				break;
			}

			else if (tokens[0] == "stor") {
				if (tokens.size() != 3) {
					std::cout << "Invalid number of arguments" << std::endl;
					continue;
				}
				client.uploadFile(tokens[1], tokens[2]);
			}

			else if (tokens[0] == "retr") {
				if (tokens.size() != 3) {
					std::cout << "Invalid number of arguments" << std::endl;
					continue;
				}
				client.downloadFile(tokens[1], tokens[2]);
			}

			else if (tokens[0] == "help") {
				printHelp();
			}

			else {
				std::cout << "Invalid command" << std::endl;
			}
		}
	}
	catch (const std::exception& ex) {
		std::cerr << ex.what() << std::endl;
	}
    return 0;
}
