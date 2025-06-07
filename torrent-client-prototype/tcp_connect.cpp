#include "tcp_connect.h"
#include "byte_tools.h"

#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdexcept>
#include <cstring>
#include <iostream>
#include <chrono>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/poll.h>
#include <limits>
#include <utility>
#include <exception>

const std::string &TcpConnect::GetIp() const {
    return ip_;
}

int TcpConnect::GetPort() const {
    return port_;
}

TcpConnect::TcpConnect(std::string ip, int port, std::chrono::milliseconds connectTimeout, std::chrono::milliseconds readTimeout):
    ip_(std::move(ip)), port_(port),
    connectTimeout_(connectTimeout),
    readTimeout_(readTimeout),
    sock_(-1) 
    {
    sock_ = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_ < 0) {
        throw std::runtime_error("Socket creation failed: " + std::string(strerror(errno)));
    }
    
    int flags = fcntl(sock_, F_GETFL, 0);
    if (flags < 0 || fcntl(sock_, F_SETFL, flags | O_NONBLOCK) < 0) {
        close(sock_);
        throw std::runtime_error("Can't set non-blocking mode: " + std::string(strerror(errno)));
    }
}

TcpConnect::~TcpConnect() {
    CloseConnection();
}

void TcpConnect::EstablishConnection() {
    sockaddr_in addr{};
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port_);
    
    if (inet_pton(AF_INET, ip_.c_str(), &addr.sin_addr) <= 0) {
        throw std::runtime_error("Invalid IP address: " + ip_);
    }
    
    int res = connect(sock_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    if (res == 0) {
        return;
    }
    
    if (errno != EINPROGRESS) {
        throw std::runtime_error("Connect error: " + std::string(strerror(errno)));
    }
    
    pollfd pfd{};
    pfd.fd = sock_;
    pfd.events = POLLOUT;
    
    int pollRes = poll(&pfd, 1, static_cast<int>(connectTimeout_.count()));
    if (pollRes == 0) {
        throw std::runtime_error("Connection timeout");
    }
    if (pollRes < 0) {
        throw std::runtime_error("Poll error: " + std::string(strerror(errno)));
    }
    
    
    int so_error = 0;
    socklen_t len = sizeof(so_error);
    if (getsockopt(sock_, SOL_SOCKET, SO_ERROR, &so_error, &len) < 0) {
        throw std::runtime_error("Getsockopt error: " + std::string(strerror(errno)));
    }
    
    if (so_error != 0) {
        throw std::runtime_error("Connection failed: " + std::string(strerror(so_error)));
    }
}

void TcpConnect::SendData(const std::string& data) const {
    size_t totalSent = 0;
    const char* buf = data.data();
    size_t size = data.size();
    
    while (totalSent < size) {
        int res = send(sock_, buf + totalSent, size - totalSent, 0);
        if (res < 0) {
            throw std::runtime_error("Send error: " + std::string(strerror(errno)));
        }
        totalSent += res;
    }
}

std::string TcpConnect::ReceiveData(size_t bufferSize) const {
    if (bufferSize == 0) {
        char header[4];
        size_t read = 0;
        auto start = std::chrono::steady_clock::now();
        while (read < sizeof(header)) {
            auto elapsed = std::chrono::steady_clock::now() - start;
            if (elapsed > readTimeout_) {
                throw std::runtime_error("Header read timeout");
            }
            pollfd pfd{};
            pfd.fd = sock_;
            pfd.events = POLLIN;
            int remaining = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(readTimeout_ - elapsed).count());
            if (poll(&pfd, 1, remaining) <= 0) {
                throw std::runtime_error("Header poll error");
            }
            int res = recv(sock_, header + read, sizeof(header) - read, 0);
            if (res == 0) {
                throw std::runtime_error("Connection closed");
            }
            if (res < 0 && !(errno == EAGAIN || errno == EWOULDBLOCK)) {
                throw std::runtime_error("Header read error: " + std::string(strerror(errno)));
            }
            if (res > 0) {
                read += res;
            }
        }
        
        bufferSize = BytesToInt(header);
    }
    std::string buffer(bufferSize, '\0');
    size_t totalRead = 0;
    auto start = std::chrono::steady_clock::now();
    
    while (totalRead < bufferSize) {
        auto elapsed = std::chrono::steady_clock::now() - start;
        if (elapsed > readTimeout_) {
            throw std::runtime_error("Body read timeout");
        }
        pollfd pfd{};
        pfd.fd = sock_;
        pfd.events = POLLIN;
        int remaining = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(readTimeout_ - elapsed).count());
        if (poll(&pfd, 1, remaining) <= 0) {
            throw std::runtime_error("Body poll error");
        }
        int res = recv(sock_, &buffer[totalRead], bufferSize - totalRead, 0);
        if (res == 0) {
            throw std::runtime_error("Connection closed");
        }
        if (res < 0 && !(errno == EAGAIN || errno == EWOULDBLOCK)) {
            throw std::runtime_error("Body read error: " + std::string(strerror(errno)));
        }
        if (res > 0) {
            totalRead += res;
        }
    }
    return buffer;
}

void TcpConnect::CloseConnection() {
    if (sock_ >= 0) {
        close(sock_);
        sock_ = -1;
    }
}
