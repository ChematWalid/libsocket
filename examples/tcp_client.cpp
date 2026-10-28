/**
 * @file tcp_client.cpp
 * @brief Modern C++20 TCP Client example using libsocket.
 * 
 * Modernizes prof/TCP client.c using Socket RAII abstraction.
 * Connects to 127.0.0.1:6000, receives server greeting, and sends reply.
 */

#include <libsocket/libsocket.hpp>

#include <cstdlib>
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    std::string host = "127.0.0.1";
    int port = 6000;

    if (argc > 1) {
        host = argv[1];
    }
    if (argc > 2) {
        port = std::atoi(argv[2]);
    }

    try {
        std::cout << "[TCP Client] Connecting to " << host << ":" << port << "...\n";
        libsocket::Socket socket(host, port);
        std::cout << "[TCP Client] Connected successfully (fd: " << socket.native_handle() << ")\n";

        // Receive greeting from server
        std::string greeting = socket.receive(100);
        std::cout << "[TCP Client] Received from server: '" << greeting << "'\n";

        // Send reply back to server
        const std::string message = "Hello from TCP client!";
        socket.send(message);
        std::cout << "[TCP Client] Sent: '" << message << "'\n";

        // RAII closes the socket automatically upon exiting scope
    } catch (const libsocket::SocketException& e) {
        std::cerr << "[TCP Client] Error: " << e.what() << '\n';
        return 1;
    }

    return 0;
}
