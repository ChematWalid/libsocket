#include <libsocket/SocketStream.hpp>
#include <libsocket/Socket.hpp>

#include <sys/socket.h>
#include <unistd.h>
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <string_view>

namespace libsocket {

SocketStreambuf::SocketStreambuf(int sockfd, std::size_t buffer_size)
    : sockfd_(sockfd),
      buffer_size_(buffer_size > 0 ? buffer_size : 1024),
      in_buffer_(buffer_size_ + putback_size_),
      out_buffer_(buffer_size_) {
    char* in_start = in_buffer_.data() + putback_size_;
    setg(in_start, in_start, in_start);
    setp(out_buffer_.data(), out_buffer_.data() + out_buffer_.size());
}

SocketStreambuf::~SocketStreambuf() {
    flush_output();
}

SocketStreambuf::int_type SocketStreambuf::underflow() {
    if (gptr() < egptr()) {
        return traits_type::to_int_type(*gptr());
    }
    if (sockfd_ < 0) {
        return traits_type::eof();
    }

    // Preserve up to putback_size_ characters for putback/unget
    std::size_t num_putback = std::min(static_cast<std::size_t>(gptr() - eback()), putback_size_);
    if (num_putback > 0) {
        std::memmove(
            in_buffer_.data() + (putback_size_ - num_putback),
            gptr() - num_putback,
            num_putback);
    }

    while (true) {
        ssize_t bytes_read = ::recv(sockfd_, in_buffer_.data() + putback_size_, buffer_size_, 0);
        if (bytes_read < 0) {
            if (errno == EINTR) {
                continue;
            }
            return traits_type::eof();
        }
        if (bytes_read == 0) {
            return traits_type::eof();
        }

        setg(
            in_buffer_.data() + (putback_size_ - num_putback),
            in_buffer_.data() + putback_size_,
            in_buffer_.data() + putback_size_ + bytes_read);
        return traits_type::to_int_type(*gptr());
    }
}

SocketStreambuf::int_type SocketStreambuf::pbackfail(int_type ch) {
    if (gptr() <= eback()) {
        return traits_type::eof();
    }
    gbump(-1);
    if (ch != traits_type::eof()) {
        *gptr() = traits_type::to_char_type(ch);
    }
    return traits_type::not_eof(ch);
}

std::streamsize SocketStreambuf::xsgetn(char_type* s, std::streamsize count) {
    std::streamsize read_bytes = 0;
    while (read_bytes < count) {
        std::ptrdiff_t available = egptr() - gptr();
        if (available <= 0) {
            if (underflow() == traits_type::eof()) {
                break;
            }
            available = egptr() - gptr();
        }
        std::streamsize to_copy = std::min(static_cast<std::streamsize>(available), count - read_bytes);
        std::memcpy(s + read_bytes, gptr(), static_cast<std::size_t>(to_copy));
        gbump(static_cast<int>(to_copy));
        read_bytes += to_copy;
    }
    return read_bytes;
}

SocketStreambuf::int_type SocketStreambuf::overflow(int_type ch) {
    if (sockfd_ < 0) {
        return traits_type::eof();
    }
    if (!flush_output()) {
        return traits_type::eof();
    }
    if (ch != traits_type::eof()) {
        *pptr() = traits_type::to_char_type(ch);
        pbump(1);
        if (ch == '\n') {
            if (!flush_output()) {
                return traits_type::eof();
            }
        }
    }
    return traits_type::not_eof(ch);
}

std::streamsize SocketStreambuf::xsputn(const char_type* s, std::streamsize count) {
    std::streamsize written = 0;
    while (written < count) {
        std::ptrdiff_t available = epptr() - pptr();
        if (available <= 0) {
            if (!flush_output()) {
                break;
            }
            available = epptr() - pptr();
        }
        std::streamsize to_copy = std::min(static_cast<std::streamsize>(available), count - written);
        std::memcpy(pptr(), s + written, static_cast<std::size_t>(to_copy));
        pbump(static_cast<int>(to_copy));
        written += to_copy;

        std::string_view chunk(s + written - to_copy, static_cast<std::size_t>(to_copy));
        if (chunk.find('\n') != std::string_view::npos) {
            if (!flush_output()) {
                break;
            }
        }
    }
    return written;
}

int SocketStreambuf::sync() {
    return flush_output() ? 0 : -1;
}

bool SocketStreambuf::flush_output() {
    char* start = pbase();
    char* end = pptr();
    std::ptrdiff_t len = end - start;
    if (len <= 0) {
        setp(out_buffer_.data(), out_buffer_.data() + out_buffer_.size());
        return true;
    }
    if (sockfd_ < 0) {
        return false;
    }

    std::size_t total_sent = 0;
    bool success = true;
    while (total_sent < static_cast<std::size_t>(len)) {
        ssize_t sent = ::send(sockfd_, start + total_sent, static_cast<std::size_t>(len) - total_sent, MSG_NOSIGNAL);
        if (sent < 0) {
            if (errno == EINTR) {
                continue;
            }
            success = false;
            break;
        }
        if (sent == 0) {
            success = false;
            break;
        }
        total_sent += static_cast<std::size_t>(sent);
    }

    if (total_sent > 0 && total_sent < static_cast<std::size_t>(len)) {
        std::size_t remaining = static_cast<std::size_t>(len) - total_sent;
        std::memmove(out_buffer_.data(), start + total_sent, remaining);
        setp(out_buffer_.data(), out_buffer_.data() + out_buffer_.size());
        pbump(static_cast<int>(remaining));
    } else if (total_sent == static_cast<std::size_t>(len)) {
        setp(out_buffer_.data(), out_buffer_.data() + out_buffer_.size());
    }

    return success;
}

namespace detail {
SocketStreamBufHolder::SocketStreamBufHolder(int sockfd)
    : buf_(std::make_unique<SocketStreambuf>(sockfd)) {}
} // namespace detail

SocketInputStream::SocketInputStream(Socket& socket)
    : detail::SocketStreamBufHolder(socket.native_handle()),
      std::istream(buf_.get()) {}

SocketInputStream::SocketInputStream(SocketInputStream&& other) noexcept
    : detail::SocketStreamBufHolder(std::move(other)),
      std::istream(std::move(other)) {
    set_rdbuf(buf_.get());
    other.set_rdbuf(nullptr);
}

SocketInputStream& SocketInputStream::operator=(SocketInputStream&& other) noexcept {
    if (this != &other) {
        detail::SocketStreamBufHolder::operator=(std::move(other));
        std::istream::operator=(std::move(other));
        set_rdbuf(buf_.get());
        other.set_rdbuf(nullptr);
    }
    return *this;
}

SocketOutputStream::SocketOutputStream(Socket& socket)
    : detail::SocketStreamBufHolder(socket.native_handle()),
      std::ostream(buf_.get()) {}

SocketOutputStream::SocketOutputStream(SocketOutputStream&& other) noexcept
    : detail::SocketStreamBufHolder(std::move(other)),
      std::ostream(std::move(other)) {
    set_rdbuf(buf_.get());
    other.set_rdbuf(nullptr);
}

SocketOutputStream& SocketOutputStream::operator=(SocketOutputStream&& other) noexcept {
    if (this != &other) {
        detail::SocketStreamBufHolder::operator=(std::move(other));
        std::ostream::operator=(std::move(other));
        set_rdbuf(buf_.get());
        other.set_rdbuf(nullptr);
    }
    return *this;
}

} // namespace libsocket
