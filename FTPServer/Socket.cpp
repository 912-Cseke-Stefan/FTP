#include "Socket.h"

Socket::Socket(int domain = AF_INET, int type = SOCK_STREAM) {
    m_socket = socket(domain, type, 0);
    if (m_socket < 0) {
        throw std::runtime_error("Socket creation failed");
    }
}

Socket::~Socket() {
    closesocket(m_socket);
}
