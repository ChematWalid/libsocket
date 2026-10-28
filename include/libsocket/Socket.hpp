#pragma once

#include <libsocket/SocketException.hpp>
#include <libsocket/SocketStream.hpp>

#include <arpa/inet.h>
#include <array>
#include <bit>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>

namespace libsocket {

class SocketInputStream;
class SocketOutputStream;

/**
 * @brief Represents a TCP client endpoint socket.
 * 
 * Equivalent to Java's java.net.Socket. Wraps a POSIX stream socket descriptor
 * with RAII resource lifetime management and Rule of Five move-only semantics.
 * 
 * @note Destructor automatically calls close(). Copy operations are deleted.
 */
class Socket {
public:
    /**
     * @brief Constructs an unconnected Socket.
     * 
     * Call connect() later to establish a connection.
     */
    Socket() noexcept;

    /**
     * @brief Constructs a Socket and connects to the specified remote host and port.
     * @param host Remote hostname or IP address string.
     * @param port Remote port number (1-65535).
     * @throws UnknownHostException If the host name resolution fails.
     * @throws SocketException If socket creation or connection fails.
     */
    Socket(std::string_view host, int port);

    /**
     * @brief Adopts an existing connected native socket file descriptor.
     * @param fd POSIX socket file descriptor.
     * @throws SocketException If fd is invalid.
     * @note Used internally by ServerSocket::accept(). Takes ownership of fd.
     */
    explicit Socket(int fd);

    /**
     * @brief Destructor. Closes the socket descriptor if open.
     */
    ~Socket() noexcept;

    // Rule of Five: non-copyable
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    /**
     * @brief Move constructor. Transfers socket ownership.
     * @param other Rvalue reference to Socket being moved from.
     */
    Socket(Socket&& other) noexcept;

    /**
     * @brief Move assignment operator. Closes existing socket and transfers ownership.
     * @param other Rvalue reference to Socket being moved from.
     * @return Reference to this Socket.
     */
    Socket& operator=(Socket&& other) noexcept;

    /**
     * @brief Connects this socket to a remote host and port.
     * @param host Remote hostname or IP address string.
     * @param port Remote port number (1-65535).
     * @throws UnknownHostException If host resolution fails.
     * @throws SocketException If already connected or connection fails.
     */
    void connect(std::string_view host, int port);

    /**
     * @brief Closes the socket descriptor and releases associated streams.
     * @note Safe to call multiple times; no-op if already closed.
     */
    void close() noexcept;

    /**
     * @brief Checks if the socket is currently connected.
     * @return True if connected, false otherwise.
     */
    [[nodiscard]] bool is_connected() const noexcept {
        return is_connected_ && sockfd_ >= 0;
    }

    /**
     * @brief Returns the native POSIX socket file descriptor.
     * @return File descriptor integer, or -1 if closed.
     */
    [[nodiscard]] int native_handle() const noexcept {
        return sockfd_;
    }

    /**
     * @brief Sends string data over the TCP connection.
     * @param data Data string_view to send.
     * @throws SocketTimeoutException If send times out.
     * @throws SocketException If send fails or connection is closed.
     */
    void send(std::string_view data);

    /**
     * @brief Sends raw byte buffer over the TCP connection.
     * @param buf Pointer to the data buffer.
     * @param len Number of bytes to send.
     * @throws SocketTimeoutException If send times out.
     * @throws SocketException If send fails or connection is closed.
     */
    void send(const void* buf, std::size_t len);

    /**
     * @brief Receives incoming data from the socket.
     * @param max_bytes Maximum number of bytes to receive (default: 4096).
     * @return String containing the received bytes, or empty string on EOF.
     * @throws SocketTimeoutException If receive times out.
     * @throws SocketException If receive fails.
     */
    [[nodiscard]] std::string receive(std::size_t max_bytes = 4096);

    /**
     * @brief Reads a line of text terminated by '\n' from the socket.
     * 
     * Any trailing '\r' preceding '\n' is stripped.
     * @return String line content without the newline character.
     * @throws SocketTimeoutException If reading times out.
     * @throws SocketException If reading fails.
     */
    [[nodiscard]] std::string read_line();

    /**
     * @brief Reads exactly len bytes from the socket into buf.
     * @param buf Destination buffer pointer.
     * @param len Number of bytes to read.
     * @throws SocketTimeoutException If reading times out.
     * @throws SocketException If connection closes before len bytes are received.
     */
    void receive_all(void* buf, std::size_t len);

    /**
     * @brief Sends a trivially copyable C++ object over the socket.
     * 
     * Transmits a 4-byte network-order size header followed by the object's raw bytes.
     * @tparam T Trivially copyable type to send.
     * @param obj The object instance to send.
     * @throws SocketTimeoutException If send times out.
     * @throws SocketException If send fails.
     */
    template<typename T>
    void send_object(const T& obj) {
        static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable for send_object");
        uint32_t net_size = ::htonl(static_cast<uint32_t>(sizeof(T)));
        send(&net_size, sizeof(net_size));
        send(&obj, sizeof(T));
    }

    /**
     * @brief Receives a trivially copyable C++ object from the socket.
     * 
     * Reads a 4-byte network-order size header and verifies it matches sizeof(T),
     * then reads the object bytes into a newly created instance.
     * @tparam T Trivially copyable type to receive.
     * @return Deserialized object instance.
     * @throws SerializationException If the received size header does not match sizeof(T).
     * @throws SocketTimeoutException If reading times out.
     * @throws SocketException If reading fails or peer disconnects prematurely.
     */
    template<typename T>
    [[nodiscard]] T receive_object() {
        static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable for receive_object");
        uint32_t net_size = 0;
        receive_all(&net_size, sizeof(net_size));
        uint32_t host_size = ::ntohl(net_size);
        if (host_size != sizeof(T)) {
            throw SerializationException(
                "Received object size mismatch: expected " + std::to_string(sizeof(T)) +
                " bytes, but header specified " + std::to_string(host_size) + " bytes");
        }
        std::array<std::byte, sizeof(T)> storage;
        receive_all(storage.data(), sizeof(T));
        return std::bit_cast<T>(storage);
    }

    /**
     * @brief Returns a reference to the output stream backed by this socket.
     * @return Reference to std::ostream.
     * @throws SocketException If socket is closed.
     */
    [[nodiscard]] std::ostream& get_output_stream();

    /**
     * @brief Returns a reference to the input stream backed by this socket.
     * @return Reference to std::istream.
     * @throws SocketException If socket is closed.
     */
    [[nodiscard]] std::istream& get_input_stream();

    /**
     * @brief Sets read and write timeout on the socket.
     * @param ms Timeout duration in milliseconds (0 disables timeout).
     * @throws SocketException If setsockopt fails.
     */
    void set_so_timeout(std::chrono::milliseconds ms);

    /**
     * @brief Enables or disables Nagle's algorithm (TCP_NODELAY).
     * @param on True to disable Nagle (send packets immediately), false to enable.
     * @throws SocketException If setsockopt fails.
     */
    void set_tcp_no_delay(bool on);

private:
    int sockfd_{-1};
    bool is_connected_{false};

    std::unique_ptr<SocketInputStream> input_stream_;
    std::unique_ptr<SocketOutputStream> output_stream_;
};

} // namespace libsocket
