#pragma once

#include <libsocket/InetAddress.hpp>
#include <libsocket/SocketException.hpp>

#include <array>
#include <bit>
#include <cstddef>
#include <cstring>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace libsocket {

/**
 * @brief Represents a datagram packet for UDP communication.
 * 
 * Equivalent to Java's java.net.DatagramPacket. Contains binary payload data,
 * a length indicator, and target/source IP address and port.
 */
class DatagramPacket {
public:
    /**
     * @brief Constructs an empty DatagramPacket.
     */
    DatagramPacket() noexcept;

    /**
     * @brief Constructs a DatagramPacket for sending binary data.
     * @param data Byte vector containing the datagram payload.
     * @param address Destination IP address.
     * @param port Destination port number (1-65535).
     */
    DatagramPacket(std::vector<std::byte> data, InetAddress address, int port);

    /**
     * @brief Constructs a DatagramPacket for sending text data.
     * @param text Text payload.
     * @param address Destination IP address.
     * @param port Destination port number (1-65535).
     */
    DatagramPacket(std::string_view text, InetAddress address, int port);

    /**
     * @brief Constructs a DatagramPacket for receiving data with pre-allocated buffer size.
     * @param max_size Maximum expected packet size in bytes.
     */
    explicit DatagramPacket(std::size_t max_size);

    /**
     * @brief Returns the remote IP address.
     * @return Const reference to InetAddress.
     */
    [[nodiscard]] const InetAddress& get_address() const noexcept {
        return address_;
    }

    /**
     * @brief Returns the remote port number.
     * @return Port integer.
     */
    [[nodiscard]] int get_port() const noexcept {
        return port_;
    }

    /**
     * @brief Returns the underlying data buffer.
     * @return Const reference to vector of bytes.
     */
    [[nodiscard]] const std::vector<std::byte>& get_data() const noexcept {
        return data_;
    }

    /**
     * @brief Returns mutable reference to the underlying data buffer.
     * @return Reference to vector of bytes.
     */
    [[nodiscard]] std::vector<std::byte>& get_data() noexcept {
        return data_;
    }

    /**
     * @brief Returns the length of the data in the packet.
     * @return Number of payload bytes.
     */
    [[nodiscard]] std::size_t get_length() const noexcept {
        return length_;
    }

    /**
     * @brief Sets the remote IP address.
     * @param address Target or source InetAddress.
     */
    void set_address(const InetAddress& address) noexcept {
        address_ = address;
    }

    /**
     * @brief Sets the remote port number.
     * @param port Port integer (1-65535).
     */
    void set_port(int port) noexcept {
        port_ = port;
    }

    /**
     * @brief Sets the packet payload data from a byte vector.
     * @param data Payload byte vector.
     */
    void set_data(std::vector<std::byte> data);

    /**
     * @brief Sets the packet payload data from a string.
     * @param text String data.
     */
    void set_data(std::string_view text);

    /**
     * @brief Sets the payload length indicator.
     * @param length Number of valid bytes in the packet.
     */
    void set_length(std::size_t length);

    /**
     * @brief Converts the packet's valid data payload to a std::string.
     * @return String representation of payload bytes up to get_length().
     */
    [[nodiscard]] std::string as_string() const;

    /**
     * @brief Serializes a trivially copyable C++ object into the packet payload.
     * @tparam T Trivially copyable type to store.
     * @param obj Object instance to copy into the buffer.
     */
    template<typename T>
    void set_object(const T& obj) {
        static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable for set_object");
        data_.resize(sizeof(T));
        std::memcpy(data_.data(), &obj, sizeof(T));
        length_ = sizeof(T);
    }

    /**
     * @brief Deserializes a trivially copyable C++ object from the packet payload.
     * @tparam T Trivially copyable type to extract.
     * @return Deserialized object instance.
     * @throws SerializationException If packet length is less than sizeof(T).
     */
    template<typename T>
    [[nodiscard]] T get_object() const {
        static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable for get_object");
        if (length_ < sizeof(T)) {
            throw SerializationException(
                "DatagramPacket length (" + std::to_string(length_) +
                ") is smaller than required object size (" + std::to_string(sizeof(T)) + ")");
        }
        std::array<std::byte, sizeof(T)> storage;
        std::memcpy(storage.data(), data_.data(), sizeof(T));
        return std::bit_cast<T>(storage);
    }

private:
    std::vector<std::byte> data_;
    std::size_t length_{0};
    InetAddress address_{};
    int port_{0};
};

} // namespace libsocket
