#include <libsocket/DatagramSocket.hpp>
#include <libsocket/SocketException.hpp>
#include <libsocket/detail/SocketUtil.hpp>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <utility>

namespace libsocket {

DatagramSocket::DatagramSocket() : sockfd_(-1), local_port_(-1), is_closed_(false) {
    sockfd_ = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd_ < 0) {
        throw SocketException("Failed to create UDP socket", errno);
    }
}

DatagramSocket::DatagramSocket(int port, bool reuse_addr)
    : sockfd_(-1), local_port_(port), is_closed_(false) {
    if (port < 0 || port > 65535) {
        throw SocketException("Invalid port number: " + std::to_string(port));
    }

    sockfd_ = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd_ < 0) {
        throw SocketException("Failed to create UDP socket", errno);
    }

    if (reuse_addr) {
        try {
            detail::set_socket_reuse_addr(sockfd_, true);
        } catch (const SocketException&) {
            int err = errno;
            close();
            throw SocketException("Failed to set SO_REUSEADDR on UDP socket", err);
        }
    }

    struct sockaddr_in serv_addr{};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(static_cast<uint16_t>(port));

    if (::bind(sockfd_, reinterpret_cast<const struct sockaddr*>(&serv_addr), sizeof(serv_addr)) < 0) {
        int err = errno;
        close();
        throw BindException("Failed to bind UDP socket to port " + std::to_string(port), err);
    }

    query_local_port();
}

DatagramSocket::~DatagramSocket() noexcept {
    close();
}

DatagramSocket::DatagramSocket(DatagramSocket&& other) noexcept
    : sockfd_(std::exchange(other.sockfd_, -1)),
      local_port_(std::exchange(other.local_port_, -1)),
      is_closed_(std::exchange(other.is_closed_, true)) {}

DatagramSocket& DatagramSocket::operator=(DatagramSocket&& other) noexcept {
    if (this != &other) {
        close();
        sockfd_ = std::exchange(other.sockfd_, -1);
        local_port_ = std::exchange(other.local_port_, -1);
        is_closed_ = std::exchange(other.is_closed_, true);
    }
    return *this;
}

void DatagramSocket::send(const DatagramPacket& packet) {
    if (is_closed()) {
        throw SocketException("Cannot send on closed DatagramSocket");
    }
    if (packet.get_port() <= 0 || packet.get_port() > 65535) {
        throw SocketException("Invalid packet destination port: " + std::to_string(packet.get_port()));
    }

    struct sockaddr_in dest_addr{};
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(static_cast<uint16_t>(packet.get_port()));
    dest_addr.sin_addr = packet.get_address().raw_address();

    ssize_t sent = ::sendto(
        sockfd_,
        packet.get_data().data(),
        packet.get_length(),
        0,
        reinterpret_cast<const struct sockaddr*>(&dest_addr),
        sizeof(dest_addr));

    if (sent < 0) {
        throw SocketException("DatagramSocket send failed", errno);
    }
}

void DatagramSocket::receive(DatagramPacket& packet) {
    if (is_closed()) {
        throw SocketException("Cannot receive on closed DatagramSocket");
    }
    if (packet.get_data().empty()) {
        throw SocketException("DatagramPacket has zero capacity buffer for receive");
    }

    struct sockaddr_in src_addr{};
    socklen_t addr_len = sizeof(src_addr);

    while (true) {
        ssize_t bytes = ::recvfrom(
            sockfd_,
            packet.get_data().data(),
            packet.get_data().size(),
            0,
            reinterpret_cast<struct sockaddr*>(&src_addr),
            &addr_len);

        if (bytes < 0) {
            if (errno == EINTR) {
                continue;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                throw SocketTimeoutException("DatagramSocket receive timed out", errno);
            }
            throw SocketException("DatagramSocket receive failed", errno);
        }

        packet.set_length(static_cast<std::size_t>(bytes));
        packet.set_address(InetAddress(src_addr.sin_addr));
        packet.set_port(ntohs(src_addr.sin_port));
        break;
    }
}

void DatagramSocket::close() noexcept {
    if (sockfd_ >= 0) {
        ::close(sockfd_);
        sockfd_ = -1;
    }
    is_closed_ = true;
    local_port_ = -1;
}

void DatagramSocket::set_so_timeout(std::chrono::milliseconds ms) {
    if (is_closed()) {
        throw SocketException("Cannot set timeout on closed DatagramSocket");
    }
    detail::set_socket_timeout(sockfd_, SO_RCVTIMEO, ms);
}

void DatagramSocket::set_broadcast(bool on) {
    if (is_closed()) {
        throw SocketException("Cannot set broadcast on closed DatagramSocket");
    }
    int opt = on ? 1 : 0;
    if (::setsockopt(sockfd_, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(opt)) < 0) {
        throw SocketException("Failed to set SO_BROADCAST on DatagramSocket", errno);
    }
}

void DatagramSocket::query_local_port() {
    local_port_ = detail::query_socket_local_port(sockfd_);
}

} // namespace libsocket
