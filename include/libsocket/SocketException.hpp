#pragma once

#include <cstring>
#include <stdexcept>
#include <string>
#include <system_error>

namespace libsocket {

/**
 * @brief Base exception class for all libsocket network errors.
 * 
 * Derives from std::runtime_error and optionally formats an OS errno
 * error code into the exception message.
 * 
 * @note Exceptions are thrown by value and should be caught by const reference.
 */
class SocketException : public std::runtime_error {
public:
    /**
     * @brief Constructs a SocketException with a custom message.
     * @param message Detailed error description.
     */
    explicit SocketException(const std::string& message)
        : std::runtime_error(message), error_code_(0) {}

    /**
     * @brief Constructs a SocketException with a message prefix and an OS error code.
     * @param prefix Descriptive error prefix.
     * @param error_code OS errno value.
     */
    SocketException(const std::string& prefix, int error_code)
        : std::runtime_error(format_system_error(prefix, error_code)), error_code_(error_code) {}

    /**
     * @brief Gets the associated OS error code, or 0 if not set.
     * @return OS error code integer.
     */
    [[nodiscard]] int error_code() const noexcept {
        return error_code_;
    }

private:
    int error_code_{0};

    static std::string format_system_error(const std::string& prefix, int error_code) {
        if (error_code == 0) {
            return prefix;
        }
        std::string sys_msg = std::system_category().message(error_code);
        return prefix + ": " + sys_msg + " (errno " + std::to_string(error_code) + ")";
    }
};

/**
 * @brief Thrown when socket binding to an address or port fails.
 */
class BindException : public SocketException {
public:
    /**
     * @brief Constructs a BindException with a custom message.
     * @param message Detailed error description.
     */
    explicit BindException(const std::string& message)
        : SocketException(message) {}

    /**
     * @brief Constructs a BindException with a message prefix and an OS error code.
     * @param prefix Descriptive error prefix.
     * @param error_code OS errno value.
     */
    BindException(const std::string& prefix, int error_code)
        : SocketException(prefix, error_code) {}
};

/**
 * @brief Thrown when a hostname or IP address cannot be resolved.
 */
class UnknownHostException : public SocketException {
public:
    /**
     * @brief Constructs an UnknownHostException with a custom message.
     * @param message Detailed error description.
     */
    explicit UnknownHostException(const std::string& message)
        : SocketException(message) {}

    /**
     * @brief Constructs an UnknownHostException with a message prefix and an error code.
     * @param prefix Descriptive error prefix.
     * @param error_code Error code (such as gai_strerror code or errno).
     */
    UnknownHostException(const std::string& prefix, int error_code)
        : SocketException(prefix, error_code) {}
};

/**
 * @brief Thrown when a socket read, write, or accept operation times out.
 */
class SocketTimeoutException : public SocketException {
public:
    /**
     * @brief Constructs a SocketTimeoutException with a custom message.
     * @param message Detailed error description.
     */
    explicit SocketTimeoutException(const std::string& message)
        : SocketException(message) {}

    /**
     * @brief Constructs a SocketTimeoutException with a message prefix and an OS error code.
     * @param prefix Descriptive error prefix.
     * @param error_code OS errno value.
     */
    SocketTimeoutException(const std::string& prefix, int error_code)
        : SocketException(prefix, error_code) {}
};

/**
 * @brief Thrown when an error occurs during object serialization or deserialization over a socket.
 */
class SerializationException : public SocketException {
public:
    /**
     * @brief Constructs a SerializationException with a custom message.
     * @param message Detailed error description.
     */
    explicit SerializationException(const std::string& message)
        : SocketException(message) {}

    /**
     * @brief Constructs a SerializationException with a message prefix and an error code.
     * @param prefix Descriptive error prefix.
     * @param error_code OS errno or serialization error code.
     */
    SerializationException(const std::string& prefix, int error_code)
        : SocketException(prefix, error_code) {}
};

} // namespace libsocket
