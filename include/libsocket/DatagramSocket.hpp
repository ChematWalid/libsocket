#pragma once

#include <libsocket/DatagramPacket.hpp>
#include <libsocket/SocketException.hpp>

#include <chrono>

namespace libsocket {

/**
 * @brief Represents a datagram (UDP) socket.
 * 
 * Equivalent to Java's java.net.DatagramSocket. Manages a UDP socket descriptor
 * with RAII resource lifetime management and Rule of Five move-only semantics.
 * 
 * @note Destructor automatically calls close(). Copy operations are deleted.
 */
class DatagramSocket {
public:
    /**
     * @brief Constructs an unbound UDP socket.
     * @throws SocketException If socket creation fails.
     */
    DatagramSocket();

    /**
     * @brief Constructs a UDP socket bound to the specified local port.
     * @param port Port number to bind to (0 for ephemeral OS-assigned port).
     * @param reuse_addr If true, enables SO_REUSEADDR on the socket (default: true).
     * @throws BindException If binding to the port fails.
     * @throws SocketException If socket creation fails.
     */
    explicit DatagramSocket(int port, bool reuse_addr = true);

    /**
     * @brief Destructor. Closes the socket descriptor if open.
     */
    ~DatagramSocket() noexcept;

    // Rule of Five: non-copyable
    DatagramSocket(const DatagramSocket&) = delete;
    DatagramSocket& operator=(const DatagramSocket&) = delete;

    /**
     * @brief Move constructor. Transfers socket ownership.
     * @param other Rvalue reference to DatagramSocket being moved from.
     */
    DatagramSocket(DatagramSocket&& other) noexcept;

    /**
     * @brief Move assignment operator. Closes existing socket and transfers ownership.
     * @param other Rvalue reference to DatagramSocket being moved from.
     * @return Reference to this DatagramSocket.
     */
    DatagramSocket& operator=(DatagramSocket&& other) noexcept;

    /**
     * @brief Sends a datagram packet from this socket.
     * @param packet DatagramPacket containing data, destination address, and port.
     * @throws SocketException If send fails or socket is closed.
     */
    void send(const DatagramPacket& packet);

    /**
     * @brief Receives a datagram packet from this socket.
     * 
     * Fills the packet buffer with received data, and updates the packet's
     * length, sender address, and sender port.
     * @param packet Destination DatagramPacket with pre-allocated buffer.
     * @throws SocketTimeoutException If receive times out.
     * @throws SocketException If receive fails or socket is closed.
     */
    void receive(DatagramPacket& packet);

    /**
     * @brief Closes this datagram socket.
     * @note Safe to call multiple times; no-op if already closed.
     */
    void close() noexcept;

    /**
     * @brief Checks if the datagram socket is closed.
     * @return True if closed, false if open.
     */
    [[nodiscard]] bool is_closed() const noexcept {
        return is_closed_ || sockfd_ < 0;
    }

    /**
     * @brief Returns the native POSIX socket file descriptor.
     * @return File descriptor integer, or -1 if closed.
     */
    [[nodiscard]] int native_handle() const noexcept {
        return sockfd_;
    }

    /**
     * @brief Returns the local port to which this socket is bound.
     * @return Bound local port, or -1 if unbound or closed.
     */
    [[nodiscard]] int get_local_port() const noexcept {
        return local_port_;
    }

    /**
     * @brief Sets the timeout for receive() operations.
     * @param ms Timeout duration in milliseconds (0 disables timeout).
     * @throws SocketException If setsockopt fails.
     */
    void set_so_timeout(std::chrono::milliseconds ms);

    /**
     * @brief Enables or disables SO_BROADCAST option.
     * @param on True to enable broadcast, false to disable.
     * @throws SocketException If setsockopt fails.
     */
    void set_broadcast(bool on);

private:
    int sockfd_{-1};
    int local_port_{-1};
    bool is_closed_{false};

    void query_local_port();
};

} // namespace libsocket
