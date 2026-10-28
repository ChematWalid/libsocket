#include <libsocket/DatagramPacket.hpp>

namespace libsocket {

DatagramPacket::DatagramPacket() noexcept : length_(0), port_(0) {}

DatagramPacket::DatagramPacket(std::vector<std::byte> data, InetAddress address, int port)
    : data_(std::move(data)), length_(data_.size()), address_(address), port_(port) {}

DatagramPacket::DatagramPacket(std::string_view text, InetAddress address, int port)
    : length_(text.size()), address_(address), port_(port) {
    data_.resize(text.size());
    std::memcpy(data_.data(), text.data(), text.size());
}

DatagramPacket::DatagramPacket(std::size_t max_size)
    : data_(max_size), length_(max_size), port_(0) {}

void DatagramPacket::set_data(std::vector<std::byte> data) {
    data_ = std::move(data);
    length_ = data_.size();
}

void DatagramPacket::set_data(std::string_view text) {
    data_.resize(text.size());
    std::memcpy(data_.data(), text.data(), text.size());
    length_ = text.size();
}

void DatagramPacket::set_length(std::size_t length) {
    length_ = length;
    if (data_.size() < length) {
        data_.resize(length);
    }
}

std::string DatagramPacket::as_string() const {
    if (length_ == 0 || data_.empty()) {
        return {};
    }
    return std::string(reinterpret_cast<const char*>(data_.data()), length_);
}

} // namespace libsocket
