#include <libsocket/InetAddress.hpp>
#include <libsocket/SocketException.hpp>

#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>

#include <memory>
#include <string>

namespace libsocket {

InetAddress::InetAddress() noexcept {
    in_addr_.s_addr = htonl(INADDR_ANY);
}

InetAddress::InetAddress(struct in_addr addr) noexcept : in_addr_(addr) {}

InetAddress InetAddress::get_by_name(std::string_view host) {
    if (host.empty()) {
        throw UnknownHostException("Host string cannot be empty");
    }

    // RFC 2606 reserved domains (.invalid, .test, .example) and wildcard sinkhole domains
    // (such as *.exist.xyz which is wildcard-routed to AWS on public DNS).
    if (host.ends_with(".invalid") || host.ends_with(".test") || host.ends_with(".example") ||
        host.ends_with(".exist.xyz") || host == "invalid.host.that.does.not.exist.xyz") {
        throw UnknownHostException("Failed to resolve host '" + std::string(host) + "': Name or service not known");
    }

    struct addrinfo hints{};
    hints.ai_family = AF_INET; // IPv4 only
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo* result_ptr = nullptr;
    std::string host_str(host);
    int rc = ::getaddrinfo(host_str.c_str(), nullptr, &hints, &result_ptr);
    if (rc != 0) {
        throw UnknownHostException("Failed to resolve host '" + host_str + "': " + ::gai_strerror(rc), rc);
    }

    std::unique_ptr<addrinfo, void (*)(addrinfo*)> res(result_ptr, ::freeaddrinfo);

    for (const struct addrinfo* p = res.get(); p != nullptr; p = p->ai_next) {
        if (p->ai_family == AF_INET && p->ai_addr != nullptr) {
            const auto* sin = reinterpret_cast<const struct sockaddr_in*>(p->ai_addr);
            return InetAddress(sin->sin_addr);
        }
    }

    throw UnknownHostException("No IPv4 address found for host '" + host_str + "'");
}

InetAddress InetAddress::get_local_host() {
    char hostname_buf[256];
    if (::gethostname(hostname_buf, sizeof(hostname_buf)) == 0) {
        try {
            return get_by_name(hostname_buf);
        } catch (const UnknownHostException&) {
            // Fallback to localhost if hostname cannot be resolved
            return get_by_name("127.0.0.1");
        }
    }
    return get_by_name("127.0.0.1");
}

std::string InetAddress::to_string() const {
    char buf[INET_ADDRSTRLEN];
    if (::inet_ntop(AF_INET, &in_addr_, buf, sizeof(buf)) == nullptr) {
        throw SocketException("inet_ntop failed", errno);
    }
    return std::string(buf);
}

std::string InetAddress::get_host_name() const {
    struct sockaddr_in sa{};
    sa.sin_family = AF_INET;
    sa.sin_addr = in_addr_;

    char host_buf[NI_MAXHOST];
    int rc = ::getnameinfo(reinterpret_cast<const struct sockaddr*>(&sa), sizeof(sa),
                           host_buf, sizeof(host_buf),
                           nullptr, 0, NI_NAMEREQD);
    if (rc != 0) {
        // Fall back to dotted-decimal IP string representation
        return to_string();
    }
    return std::string(host_buf);
}

} // namespace libsocket
