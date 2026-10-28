#pragma once

#include <libsocket/SocketException.hpp>

#include <arpa/inet.h>
#include <chrono>
#include <sys/socket.h>
#include <unistd.h>

namespace libsocket::detail {

/**
 * @brief Converts a std::chrono::milliseconds duration to a POSIX struct timeval.
 * @param ms Duration in milliseconds.
 * @return struct timeval structure.
 */
inline struct timeval to_timeval(std::chrono::milliseconds ms) noexcept {
    struct timeval tv{};
    tv.tv_sec = static_cast<time_t>(ms.count() / 1000);
    tv.tv_usec = static_cast<suseconds_t>((ms.count() % 1000) * 1000);
    return tv;
}

/**
 * @brief Sets a socket receive or send timeout.
 * @param sockfd Socket file descriptor.
 * @param optname Socket option (SO_RCVTIMEO or SO_SNDTIMEO).
 * @param ms Timeout duration in milliseconds.
 * @throws SocketException If setsockopt fails.
 */
inline void set_socket_timeout(int sockfd, int optname, std::chrono::milliseconds ms) {
    if (sockfd < 0) {
        throw SocketException("Cannot set timeout on invalid socket descriptor");
    }
    struct timeval tv = to_timeval(ms);
    if (::setsockopt(sockfd, SOL_SOCKET, optname, &tv, sizeof(tv)) < 0) {
        throw SocketException("Failed to set socket timeout option", errno);
    }
}

/**
 * @brief Sets the SO_REUSEADDR option on a socket.
 * @param sockfd Socket file descriptor.
 * @param reuse True to enable SO_REUSEADDR, false to disable.
 * @throws SocketException If setsockopt fails.
 */
inline void set_socket_reuse_addr(int sockfd, bool reuse) {
    if (sockfd < 0) {
        throw SocketException("Cannot set SO_REUSEADDR on invalid socket descriptor");
    }
    int opt = reuse ? 1 : 0;
    if (::setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        throw SocketException("Failed to set SO_REUSEADDR on socket", errno);
    }
}

/**
 * @brief Queries the bound local port of a socket using getsockname.
 * @param sockfd Socket file descriptor.
 * @return Local port in host byte order, or -1 on error.
 */
inline int query_socket_local_port(int sockfd) noexcept {
    if (sockfd < 0) {
        return -1;
    }
    struct sockaddr_in sa{};
    socklen_t len = sizeof(sa);
    if (::getsockname(sockfd, reinterpret_cast<struct sockaddr*>(&sa), &len) == 0) {
        return ntohs(sa.sin_port);
    }
    return -1;
}

} // namespace libsocket::detail
