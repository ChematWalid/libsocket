#include <libsocket/ServerSocket.hpp>
#include <libsocket/SocketException.hpp>
#include <libsocket/detail/SocketUtil.hpp>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <utility>

namespace libsocket {

ServerSocket::ServerSocket(int port, int backlog, bool reuse_addr)
    : sockfd_(-1), port_(port), is_bound_(false) {
    if (port < 0 || port > 65535) {
        throw SocketException("Invalid port number: " + std::to_string(port));
    }

    sockfd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd_ < 0) {
        throw SocketException("Failed to create server socket", errno);
    }

    if (reuse_addr) {
        try {
            detail::set_socket_reuse_addr(sockfd_, true);
        } catch (const SocketException&) {
            int err = errno;
            close();
            throw SocketException("Failed to set SO_REUSEADDR on server socket", err);
        }
    }

    struct sockaddr_in serv_addr{};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(static_cast<uint16_t>(port));

    if (::bind(sockfd_, reinterpret_cast<const struct sockaddr*>(&serv_addr), sizeof(serv_addr)) < 0) {
        int err = errno;
        close();
        throw BindException("Failed to bind server socket to port " + std::to_string(port), err);
    }

    if (::listen(sockfd_, backlog) < 0) {
        int err = errno;
        close();
        throw SocketException("Failed to listen on server socket", err);
    }

    is_bound_ = true;
    query_local_port();
}

ServerSocket::~ServerSocket() noexcept {
    close();
}

ServerSocket::ServerSocket(ServerSocket&& other) noexcept
    : sockfd_(std::exchange(other.sockfd_, -1)),
      port_(std::exchange(other.port_, -1)),
      is_bound_(std::exchange(other.is_bound_, false)) {}

ServerSocket& ServerSocket::operator=(ServerSocket&& other) noexcept {
    if (this != &other) {
        close();
        sockfd_ = std::exchange(other.sockfd_, -1);
        port_ = std::exchange(other.port_, -1);
        is_bound_ = std::exchange(other.is_bound_, false);
    }
    return *this;
}

Socket ServerSocket::accept() {
    if (!is_bound() || sockfd_ < 0) {
        throw SocketException("Cannot accept on closed or unbound server socket");
    }

    struct sockaddr_in cli_addr{};
    socklen_t clilen = sizeof(cli_addr);

    while (true) {
        int newsockfd = ::accept(sockfd_, reinterpret_cast<struct sockaddr*>(&cli_addr), &clilen);
        if (newsockfd < 0) {
            if (errno == EINTR) {
                continue;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                throw SocketTimeoutException("ServerSocket accept timed out", errno);
            }
            throw SocketException("ServerSocket accept failed", errno);
        }
        return Socket(newsockfd);
    }
}

void ServerSocket::close() noexcept {
    if (sockfd_ >= 0) {
        ::close(sockfd_);
        sockfd_ = -1;
    }
    is_bound_ = false;
    port_ = -1;
}

void ServerSocket::set_so_timeout(std::chrono::milliseconds ms) {
    if (sockfd_ < 0) {
        throw SocketException("Cannot set timeout on closed server socket");
    }
    detail::set_socket_timeout(sockfd_, SO_RCVTIMEO, ms);
}

void ServerSocket::query_local_port() {
    port_ = detail::query_socket_local_port(sockfd_);
}

} // namespace libsocket
