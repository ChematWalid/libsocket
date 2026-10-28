#pragma once

#include <libsocket/SocketException.hpp>
#include <netinet/in.h>
#include <string>
#include <string_view>

namespace libsocket {

/**
 * @brief Represents an Internet Protocol (IPv4) address.
 * 
 * Provides static factories to resolve hostnames or IP address strings,
 * reverse DNS lookup, and string formatting.
 */
class InetAddress {
public:
    /**
     * @brief Constructs a default InetAddress representing INADDR_ANY (0.0.0.0).
     */
    InetAddress() noexcept;

    /**
     * @brief Constructs an InetAddress from a raw POSIX in_addr structure.
     * @param addr The in_addr structure representing an IPv4 address.
     */
    explicit InetAddress(struct in_addr addr) noexcept;

    /**
     * @brief Resolves a hostname or dotted-decimal IPv4 string to an InetAddress.
     * @param host Hostname (e.g. "localhost", "example.com") or dotted IP ("127.0.0.1").
     * @return InetAddress corresponding to the resolved host.
     * @throws UnknownHostException If resolution fails.
     */
    [[nodiscard]] static InetAddress get_by_name(std::string_view host);

    /**
     * @brief Retrieves the local host address.
     * @return InetAddress corresponding to the local host.
     * @throws UnknownHostException If the local host cannot be resolved.
     */
    [[nodiscard]] static InetAddress get_local_host();

    /**
     * @brief Converts the IP address to a dotted-decimal string representation (e.g. "127.0.0.1").
     * @return Formatted IP string.
     * @throws SocketException If string conversion fails.
     */
    [[nodiscard]] std::string to_string() const;

    /**
     * @brief Performs a reverse DNS lookup to obtain the host name.
     * 
     * If the hostname cannot be determined, returns the dotted-decimal IP address.
     * @return Hostname string or IP string.
     */
    [[nodiscard]] std::string get_host_name() const;

    /**
     * @brief Returns the underlying POSIX in_addr structure.
     * @return Const reference to struct in_addr.
     */
    [[nodiscard]] const struct in_addr& raw_address() const noexcept {
        return in_addr_;
    }

    /**
     * @brief Equality comparison operator.
     * @param other The other InetAddress to compare against.
     * @return True if both addresses represent the same IPv4 address.
     */
    bool operator==(const InetAddress& other) const noexcept {
        return in_addr_.s_addr == other.in_addr_.s_addr;
    }

    /**
     * @brief Inequality comparison operator.
     * @param other The other InetAddress to compare against.
     * @return True if addresses differ.
     */
    bool operator!=(const InetAddress& other) const noexcept {
        return !(*this == other);
    }

private:
    struct in_addr in_addr_{};
};

} // namespace libsocket
