/**
 * @file tcp_server.cpp
 * @brief Modern C++20 TCP Server example using libsocket.
 * 
 * Modernizes prof/server_TCP.c using ServerSocket and Socket RAII abstractions.
 * Listens on port 6000, sends a greeting to incoming clients, receives and displays
 * their response, then safely closes the client connection.
 */

#include <libsocket/libsocket.hpp>

#include <cstdlib>
#include <iostream>

int main(int argc, char* argv[]) {
    int port = 6000;
    if (argc > 1) {
        port = std::atoi(argv[1]);
    }

    try {
        std::cout << "[TCP Server] Starting on port " << port << "...\n";
        libsocket::ServerSocket server(port);
        std::cout << "[TCP Server] Listening for incoming connections on port "
                  << server.get_local_port() << "...\n";

        // Accept a client connection (can loop or accept single for demonstration)
        while (true) {
            std::cout << "[TCP Server] Waiting for client...\n";
            libsocket::Socket client = server.accept();
            std::cout << "[TCP Server] Client connected (fd: " << client.native_handle() << ")\n";

            // Send greeting message to client
            client.send("Message from server");

            // Receive response from client
            std::string response = client.receive(100);
            std::cout << "[TCP Server] Received from client: " << response << '\n';

            // RAII handles closing client socket when client leaves scope
            std::cout << "[TCP Server] Client handled.\n";
            break; // Exit after one client for demo purposes
        }

        std::cout << "[TCP Server] Server shutting down cleanly.\n";
    } catch (const libsocket::SocketException& e) {
        std::cerr << "[TCP Server] Error: " << e.what() << '\n';
        return 1;
    }

    return 0;
}
