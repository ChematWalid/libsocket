/**
 * @file udp_client.cpp
 * @brief Modern C++20 UDP Client example using libsocket.
 * 
 * Modernizes prof/UDP_client.c using DatagramSocket and DatagramPacket.
 * Sends a datagram to 127.0.0.1:7000 and waits for the echo reply.
 */

#include <libsocket/libsocket.hpp>

#include <cstdlib>
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    std::string host = "127.0.0.1";
    int port = 7000;

    if (argc > 1) {
        host = argv[1];
    }
    if (argc > 2) {
        port = std::atoi(argv[2]);
    }

    try {
        libsocket::DatagramSocket client_socket;
        libsocket::InetAddress server_addr = libsocket::InetAddress::get_by_name(host);

        const std::string message = "Hello from UDP client!";
        std::cout << "[UDP Client] Sending message to " << host << ":" << port << "...\n";

        libsocket::DatagramPacket send_packet(message, server_addr, port);
        client_socket.send(send_packet);

        // Set 5-second timeout on receive so client does not hang indefinitely
        client_socket.set_so_timeout(std::chrono::milliseconds(5000));

        // Prepare packet buffer for the response
        libsocket::DatagramPacket recv_packet(512);
        std::cout << "[UDP Client] Waiting for reply...\n";
        client_socket.receive(recv_packet);

        std::cout << "[UDP Client] Received reply from "
                  << recv_packet.get_address().to_string() << ":" << recv_packet.get_port() << '\n';
        std::cout << "[UDP Client] Content: '" << recv_packet.as_string() << "'\n";

    } catch (const libsocket::SocketTimeoutException&) {
        std::cerr << "[UDP Client] Timeout waiting for server response.\n";
        return 1;
    } catch (const libsocket::SocketException& e) {
        std::cerr << "[UDP Client] Error: " << e.what() << '\n';
        return 1;
    }

    return 0;
}
