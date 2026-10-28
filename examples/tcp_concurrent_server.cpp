/**
 * @file tcp_concurrent_server.cpp
 * @brief Modern C++20 Concurrent TCP Server example using libsocket and std::jthread.
 * 
 * Modernizes prof/server_TCP_concurrent.c by replacing raw fork() with modern
 * C++20 std::jthread and ServerSocket/Socket RAII abstractions.
 */

#include <libsocket/libsocket.hpp>

#include <atomic>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <vector>

namespace {
std::atomic<bool> g_running{true};

void handle_client(libsocket::Socket client, int client_id) {
    try {
        std::cout << "[Worker " << client_id << "] Handling connection (fd: "
                  << client.native_handle() << ")\n";

        // Send greeting to client
        client.send("Message from concurrent server");

        // Receive client's response
        std::string response = client.receive(100);
        std::cout << "[Worker " << client_id << "] Received: '" << response << "'\n";

        std::cout << "[Worker " << client_id << "] Finished handling client.\n";
    } catch (const libsocket::SocketException& e) {
        std::cerr << "[Worker " << client_id << "] Socket error: " << e.what() << '\n';
    }
}
} // namespace

int main(int argc, char* argv[]) {
    int port = 6000;
    if (argc > 1) {
        port = std::atoi(argv[1]);
    }

    try {
        libsocket::ServerSocket server(port);
        std::cout << "[Concurrent Server] Listening on port " << server.get_local_port() << "...\n";

        // Set timeout on accept so the server loop can periodically check running condition
        server.set_so_timeout(std::chrono::milliseconds(500));

        std::vector<std::jthread> worker_threads;
        int client_counter = 0;

        std::cout << "[Concurrent Server] Ready. Press Ctrl+C or wait for connections.\n";

        // Serve up to 3 clients for automated demo, or continue if running
        int max_clients = 3;
        while (client_counter < max_clients && g_running) {
            try {
                libsocket::Socket client = server.accept();
                ++client_counter;
                std::cout << "[Concurrent Server] Accepted client " << client_counter << '\n';

                // Spawn a modern C++20 std::jthread to handle the client concurrently
                worker_threads.emplace_back(handle_client, std::move(client), client_counter);
            } catch (const libsocket::SocketTimeoutException&) {
                // Timeout allows checking loop condition
                continue;
            }
        }

        std::cout << "[Concurrent Server] Stopping server, waiting for workers to complete...\n";
        // std::jthread destructors automatically join threads
    } catch (const libsocket::SocketException& e) {
        std::cerr << "[Concurrent Server] Fatal error: " << e.what() << '\n';
        return 1;
    }

    std::cout << "[Concurrent Server] Clean exit.\n";
    return 0;
}
