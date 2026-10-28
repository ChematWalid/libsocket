#include <libsocket/Socket.hpp>
#include <libsocket/InetAddress.hpp>
#include <libsocket/SocketException.hpp>
#include <libsocket/SocketStream.hpp>
#include <libsocket/detail/SocketUtil.hpp>

#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

#include <iostream>
#include <utility>

namespace libsocket {

Socket::Socket() noexcept : sockfd_(-1), is_connected_(false) {}

Socket::Socket(std::string_view host, int port) : sockfd_(-1), is_connected_(false) {
    connect(host, port);
}

Socket::Socket(int fd) : sockfd_(fd), is_connected_(fd >= 0) {
    if (fd < 0) {
        throw SocketException("Invalid socket file descriptor provided to Socket constructor");
    }
}

Socket::~Socket() noexcept {
    close();
}

Socket::Socket(Socket&& other) noexcept
    : sockfd_(std::exchange(other.sockfd_, -1)),
      is_connected_(std::exchange(other.is_connected_, false)),
      input_stream_(std::move(other.input_stream_)),
      output_stream_(std::move(other.output_stream_)) {}

Socket& Socket::operator=(Socket&& other) noexcept {
    if (this != &other) {
        close();
        sockfd_ = std::exchange(other.sockfd_, -1);
        is_connected_ = std::exchange(other.is_connected_, false);
        input_stream_ = std::move(other.input_stream_);
        output_stream_ = std::move(other.output_stream_);
    }
    return *this;
}

void Socket::connect(std::string_view host, int port) {
    if (is_connected()) {
        throw SocketException("Socket is already connected");
    }
    if (port <= 0 || port > 65535) {
        throw SocketException("Port number out of range: " + std::to_string(port));
    }

    InetAddress address = InetAddress::get_by_name(host);

    if (sockfd_ < 0) {
        sockfd_ = ::socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd_ < 0) {
            throw SocketException("Failed to create TCP socket", errno);
        }
    }

    struct sockaddr_in serv_addr{};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(static_cast<uint16_t>(port));
    serv_addr.sin_addr = address.raw_address();

    int rc = ::connect(sockfd_, reinterpret_cast<const struct sockaddr*>(&serv_addr), sizeof(serv_addr));
    if (rc < 0) {
        int err = errno;
        close();
        throw SocketException("Failed to connect to " + std::string(host) + ":" + std::to_string(port), err);
    }

    is_connected_ = true;
}

void Socket::close() noexcept {
    input_stream_.reset();
    output_stream_.reset();
    if (sockfd_ >= 0) {
        ::close(sockfd_);
        sockfd_ = -1;
    }
    is_connected_ = false;
}

void Socket::send(std::string_view data) {
    send(data.data(), data.size());
}

void Socket::send(const void* buf, std::size_t len) {
    if (!is_connected() || sockfd_ < 0) {
        throw SocketException("Cannot send on closed or unconnected socket");
    }
    if (len == 0) {
        return;
    }
    const auto* ptr = static_cast<const char*>(buf);
    std::size_t total_sent = 0;
    while (total_sent < len) {
        ssize_t sent = ::send(sockfd_, ptr + total_sent, len - total_sent, MSG_NOSIGNAL);
        if (sent < 0) {
            if (errno == EINTR) {
                continue;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                throw SocketTimeoutException("Socket send timed out", errno);
            }
            throw SocketException("Socket send failed", errno);
        }
        if (sent == 0) {
            throw SocketException("Connection closed by peer during send");
        }
        total_sent += static_cast<std::size_t>(sent);
    }
}

std::string Socket::receive(std::size_t max_bytes) {
    if (!is_connected() || sockfd_ < 0) {
        throw SocketException("Cannot receive on closed or unconnected socket");
    }
    if (max_bytes == 0) {
        return {};
    }
    std::string buffer(max_bytes, '\0');
    while (true) {
        ssize_t bytes_read = ::recv(sockfd_, buffer.data(), max_bytes, 0);
        if (bytes_read < 0) {
            if (errno == EINTR) {
                continue;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                throw SocketTimeoutException("Socket receive timed out", errno);
            }
            throw SocketException("Socket receive failed", errno);
        }
        buffer.resize(static_cast<std::size_t>(bytes_read));
        return buffer;
    }
}

std::string Socket::read_line() {
    if (!is_connected() || sockfd_ < 0) {
        throw SocketException("Cannot read line on closed or unconnected socket");
    }
    if (input_stream_) {
        std::string line;
        if (std::getline(*input_stream_, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            return line;
        }
        return {};
    }
    std::string line;
    char c = '\0';
    while (true) {
        ssize_t bytes_read = ::recv(sockfd_, &c, 1, 0);
        if (bytes_read < 0) {
            if (errno == EINTR) {
                continue;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                throw SocketTimeoutException("Socket read_line timed out", errno);
            }
            throw SocketException("Socket read_line failed", errno);
        }
        if (bytes_read == 0) {
            // Peer closed socket
            break;
        }
        if (c == '\n') {
            break;
        }
        if (c != '\r') {
            line.push_back(c);
        }
    }
    return line;
}

void Socket::receive_all(void* buf, std::size_t len) {
    if (!is_connected() || sockfd_ < 0) {
        throw SocketException("Cannot receive on closed or unconnected socket");
    }
    auto* ptr = static_cast<char*>(buf);
    std::size_t total = 0;
    while (total < len) {
        ssize_t bytes_read = ::recv(sockfd_, ptr + total, len - total, 0);
        if (bytes_read < 0) {
            if (errno == EINTR) {
                continue;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                throw SocketTimeoutException("Socket receive_all timed out", errno);
            }
            throw SocketException("Socket receive_all failed", errno);
        }
        if (bytes_read == 0) {
            throw SocketException("Peer closed connection before reading requested bytes");
        }
        total += static_cast<std::size_t>(bytes_read);
    }
}

std::ostream& Socket::get_output_stream() {
    if (!is_connected() || sockfd_ < 0) {
        throw SocketException("Cannot get output stream for closed or unconnected socket");
    }
    if (!output_stream_) {
        output_stream_ = std::make_unique<SocketOutputStream>(*this);
    }
    return *output_stream_;
}

std::istream& Socket::get_input_stream() {
    if (!is_connected() || sockfd_ < 0) {
        throw SocketException("Cannot get input stream for closed or unconnected socket");
    }
    if (!input_stream_) {
        input_stream_ = std::make_unique<SocketInputStream>(*this);
    }
    return *input_stream_;
}

void Socket::set_so_timeout(std::chrono::milliseconds ms) {
    if (sockfd_ < 0) {
        sockfd_ = ::socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd_ < 0) {
            throw SocketException("Failed to allocate socket for set_so_timeout", errno);
        }
    }
    detail::set_socket_timeout(sockfd_, SO_RCVTIMEO, ms);
    detail::set_socket_timeout(sockfd_, SO_SNDTIMEO, ms);
}

void Socket::set_tcp_no_delay(bool on) {
    if (sockfd_ < 0) {
        sockfd_ = ::socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd_ < 0) {
            throw SocketException("Failed to allocate socket for set_tcp_no_delay", errno);
        }
    }
    int flag = on ? 1 : 0;
    if (::setsockopt(sockfd_, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag)) < 0) {
        throw SocketException("Failed to set TCP_NODELAY", errno);
    }
}

} // namespace libsocket
