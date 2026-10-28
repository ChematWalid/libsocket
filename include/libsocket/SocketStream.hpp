#pragma once

#include <iostream>
#include <memory>
#include <streambuf>
#include <vector>

namespace libsocket {

class Socket;

/**
 * @brief Custom std::streambuf implementation that interfaces with a POSIX socket.
 * 
 * Provides buffered input and output operations over a socket file descriptor.
 */
class SocketStreambuf : public std::streambuf {
public:
    /**
     * @brief Constructs a SocketStreambuf wrapping a socket file descriptor.
     * @param sockfd Socket file descriptor.
     * @param buffer_size Internal buffer size in bytes (default 1024).
     */
    explicit SocketStreambuf(int sockfd, std::size_t buffer_size = 1024);

    /**
     * @brief Destructor. Flushes any remaining output.
     */
    ~SocketStreambuf() override;

    // Non-copyable and non-movable
    SocketStreambuf(const SocketStreambuf&) = delete;
    SocketStreambuf& operator=(const SocketStreambuf&) = delete;
    SocketStreambuf(SocketStreambuf&&) = delete;
    SocketStreambuf& operator=(SocketStreambuf&&) = delete;

protected:
    /**
     * @brief Reads data from the socket into the internal input buffer when empty.
     * @return Next character as int_type, or traits_type::eof() on EOF or error.
     */
    int_type underflow() override;

    /**
     * @brief Puts a character back into the input sequence.
     * @param ch Character to put back.
     * @return The putback character or traits_type::not_eof(ch) on success, traits_type::eof() on failure.
     */
    int_type pbackfail(int_type ch = traits_type::eof()) override;

    /**
     * @brief Bulk read from the socket stream buffer into a character buffer.
     * @param s Destination buffer pointer.
     * @param count Number of characters to read.
     * @return Number of characters successfully read.
     */
    std::streamsize xsgetn(char_type* s, std::streamsize count) override;

    /**
     * @brief Writes data to the socket when the internal output buffer is full.
     * @param ch Character to put into the stream.
     * @return The written character or traits_type::not_eof(ch), or traits_type::eof() on error.
     */
    int_type overflow(int_type ch = traits_type::eof()) override;

    /**
     * @brief Bulk write from a character buffer into the socket stream buffer.
     * @param s Source character buffer.
     * @param count Number of characters to write.
     * @return Number of characters successfully written.
     */
    std::streamsize xsputn(const char_type* s, std::streamsize count) override;

    /**
     * @brief Synchronizes (flushes) the output buffer with the socket.
     * @return 0 on success, -1 on failure.
     */
    int sync() override;

private:
    int sockfd_{-1};
    std::size_t buffer_size_{1024};
    static constexpr std::size_t putback_size_{4};
    std::vector<char> in_buffer_;
    std::vector<char> out_buffer_;

    bool flush_output();
};

namespace detail {
/**
 * @brief Internal base holder ensuring SocketStreambuf is constructed before std::istream/ostream.
 */
struct SocketStreamBufHolder {
    std::unique_ptr<SocketStreambuf> buf_;

    /**
     * @brief Constructs the internal SocketStreambuf holder.
     * @param sockfd Socket file descriptor.
     */
    explicit SocketStreamBufHolder(int sockfd);
    virtual ~SocketStreamBufHolder() = default;

    SocketStreamBufHolder(const SocketStreamBufHolder&) = delete;
    SocketStreamBufHolder& operator=(const SocketStreamBufHolder&) = delete;
    SocketStreamBufHolder(SocketStreamBufHolder&&) noexcept = default;
    SocketStreamBufHolder& operator=(SocketStreamBufHolder&&) noexcept = default;
};
} // namespace detail

/**
 * @brief An input stream wrapper (std::istream) for reading from a Socket.
 */
class SocketInputStream : private detail::SocketStreamBufHolder, public std::istream {
public:
    /**
     * @brief Constructs a SocketInputStream from a Socket reference.
     * @param socket The connected Socket instance.
     */
    explicit SocketInputStream(Socket& socket);

    /**
     * @brief Destructor.
     */
    ~SocketInputStream() override = default;

    SocketInputStream(const SocketInputStream&) = delete;
    SocketInputStream& operator=(const SocketInputStream&) = delete;

    /**
     * @brief Move constructor. Transfers stream state and buffer ownership.
     * @param other Stream being moved from.
     */
    SocketInputStream(SocketInputStream&& other) noexcept;

    /**
     * @brief Move assignment operator.
     * @param other Stream being moved from.
     * @return Reference to this stream.
     */
    SocketInputStream& operator=(SocketInputStream&& other) noexcept;
};

/**
 * @brief An output stream wrapper (std::ostream) for writing to a Socket.
 */
class SocketOutputStream : private detail::SocketStreamBufHolder, public std::ostream {
public:
    /**
     * @brief Constructs a SocketOutputStream from a Socket reference.
     * @param socket The connected Socket instance.
     */
    explicit SocketOutputStream(Socket& socket);

    /**
     * @brief Destructor. Flushes buffered data.
     */
    ~SocketOutputStream() override = default;

    SocketOutputStream(const SocketOutputStream&) = delete;
    SocketOutputStream& operator=(const SocketOutputStream&) = delete;

    /**
     * @brief Move constructor. Transfers stream state and buffer ownership.
     * @param other Stream being moved from.
     */
    SocketOutputStream(SocketOutputStream&& other) noexcept;

    /**
     * @brief Move assignment operator.
     * @param other Stream being moved from.
     * @return Reference to this stream.
     */
    SocketOutputStream& operator=(SocketOutputStream&& other) noexcept;
};

} // namespace libsocket
