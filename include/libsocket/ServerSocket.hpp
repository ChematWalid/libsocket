#pragma once

#include <libsocket/Socket.hpp>
#include <libsocket/SocketException.hpp>

#include <chrono>

namespace libsocket {

/**
 * @brief Represents a TCP server socket waiting for client connections.
 * 
 * Equivalent to Java's java.net.ServerSocket. Wraps a POSIX listening socket
 * with RAII resource lifetime management and Rule of Five move-only semantics.
 * 
 * @note Destructor automatically calls close(). Copy operations are deleted.
 */
class ServerSocket {
public:
    /**
     * @brief Constructs and binds a ServerSocket to the specified port.
     * @param port Port number to listen on (0 for ephemeral OS-allocated port).
     * @param backlog Maximum length of the queue of pending connections (default: 10).
     * @param reuse_addr If true, enables SO_REUSEADDR on the socket (default: true).
     * @throws BindException If binding to the specified port fails.
     * @throws SocketException If socket creation or listen fails.
     */
    explicit ServerSocket(int port, int backlog = 10, bool reuse_addr = true);

    /**
     * @brief Destructor. Closes the server socket if open.
     */
    ~ServerSocket() noexcept;

    // Rule of Five: non-copyable
    ServerSocket(const ServerSocket&) = delete;
    ServerSocket& operator=(const ServerSocket&) = delete;

    /**
     * @brief Move constructor. Transfers socket ownership.
     * @param other Rvalue reference to ServerSocket being moved from.
     */
    ServerSocket(ServerSocket&& other) noexcept;

    /**
     * @brief Move assignment operator. Closes existing socket and transfers ownership.
     * @param other Rvalue reference to ServerSocket being moved from.
     * @return Reference to this ServerSocket.
     */
    ServerSocket& operator=(ServerSocket&& other) noexcept;

    /**
     * @brief Listens for a connection to be made to this socket and accepts it.
     * 
     * Blocks until a connection is made or timeout expires.
     * @return Connected Socket representing the client endpoint.
     * @throws SocketTimeoutException If accept times out.
     * @throws SocketException If accept fails or socket is closed.
     */
    [[nodiscard]] Socket accept();

    /**
     * @brief Closes this server socket.
     * @note Safe to call multiple times; no-op if already closed.
     */
    void close() noexcept;

    /**
     * @brief Returns the binding state of the ServerSocket.
     * @return True if bound and open, false otherwise.
     */
    [[nodiscard]] bool is_bound() const noexcept {
        return is_bound_ && sockfd_ >= 0;
    }

    /**
     * @brief Returns the native POSIX socket file descriptor.
     * @return File descriptor integer, or -1 if closed.
     */
    [[nodiscard]] int native_handle() const noexcept {
        return sockfd_;
    }

    /**
     * @brief Returns the local port number on which this socket is listening.
     * @return Bound port number, or -1 if unbound.
     */
    [[nodiscard]] int get_local_port() const noexcept {
        return port_;
    }

    /**
     * @brief Sets the timeout for accept() operations.
     * @param ms Timeout duration in milliseconds (0 disables timeout).
     * @throws SocketException If setsockopt fails.
     */
    void set_so_timeout(std::chrono::milliseconds ms);

private:
    int sockfd_{-1};
    int port_{-1};
    bool is_bound_{false};

    void query_local_port();
};

} // namespace libsocket
