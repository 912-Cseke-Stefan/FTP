
#include <iostream>
#include "FTPServer.h"


int main() {
    try {
        FTPServer server = FTPServer();
        std::cout << "Listening on port " << DEFAULT_PORT << "...\n";
        server.start();
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
    return 0;
}
