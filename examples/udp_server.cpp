/**
 * @file udp_server.cpp
 * @brief Modern C++20 UDP Server example using libsocket.
 * 
 * Modernizes prof/UDP_server.c using DatagramSocket and DatagramPacket.
 * Listens on port 7000, receives a datagram packet, prints content and sender info,
 * then replies with an echo or response packet.
 */

#include <libsocket/libsocket.hpp>

#include <cstdlib>
#include <iostream>

int main(int argc, char* argv[]) {
    int port = 7000;
    if (argc > 1) {
        port = std::atoi(argv[1]);
    }

    try {
        libsocket::DatagramSocket socket(port);
        std::cout << "[UDP Server] Bound to port " << socket.get_local_port() << "...\n";

        // Pre-allocate buffer for incoming datagram packet (up to 512 bytes)
        libsocket::DatagramPacket packet(512);

        std::cout << "[UDP Server] Waiting for incoming UDP packet...\n";
        socket.receive(packet);

        std::cout << "[UDP Server] Received " << packet.get_length() << " bytes from "
                  << packet.get_address().to_string() << ":" << packet.get_port() << '\n';
        std::cout << "[UDP Server] Content: '" << packet.as_string() << "'\n";

        // Echo the packet back to the sender
        std::cout << "[UDP Server] Echoing packet back to sender...\n";
        socket.send(packet);
        std::cout << "[UDP Server] Echo sent successfully.\n";

    } catch (const libsocket::SocketException& e) {
        std::cerr << "[UDP Server] Error: " << e.what() << '\n';
        return 1;
    }

    return 0;
}
