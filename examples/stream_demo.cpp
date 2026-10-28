/**
 * @file stream_demo.cpp
 * @brief Demonstrates std::iostream streaming over sockets using libsocket.
 * 
 * Shows how get_input_stream() and get_output_stream() allow formatted C++ streaming
 * with operator<<, operator>>, and std::getline() across a client/server pair.
 */

#include <libsocket/libsocket.hpp>

#include <iostream>
#include <string>
#include <thread>

int main() {
    try {
        std::cout << "[Stream Demo] Starting loopback stream demonstration...\n";

        // Bind server to an ephemeral port (port 0)
        libsocket::ServerSocket server(0);
        int port = server.get_local_port();
        std::cout << "[Stream Demo] Server bound to ephemeral port " << port << '\n';

        // Server thread
        std::jthread server_thread([&server]() {
            try {
                libsocket::Socket client = server.accept();
                std::cout << "[Server Thread] Accepted connection.\n";

                // Read formatted data from client stream
                std::string greeting;
                int client_magic = 0;
                double client_ratio = 0.0;

                std::getline(client.get_input_stream(), greeting);
                client.get_input_stream() >> client_magic >> client_ratio;

                std::cout << "[Server Thread] Received line: '" << greeting << "'\n";
                std::cout << "[Server Thread] Received formatted values: magic=" << client_magic
                          << ", ratio=" << client_ratio << '\n';

                // Send stream response back
                client.get_output_stream() << "Server response: acknowledge!\n"
                                           << (client_magic * 2) << " "
                                           << (client_ratio + 10.5) << '\n'
                                           << std::flush;
                std::cout << "[Server Thread] Response stream sent.\n";
            } catch (const libsocket::SocketException& e) {
                std::cerr << "[Server Thread] Error: " << e.what() << '\n';
            }
        });

        // Client in main thread
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        libsocket::Socket client("127.0.0.1", port);
        std::cout << "[Client Thread] Connected to server.\n";

        // Write formatted data to socket output stream
        client.get_output_stream() << "Hello through C++ iostream!\n"
                                   << 42 << " " << 3.14159 << '\n'
                                   << std::flush;

        // Read response using client input stream
        std::string reply_line;
        int reply_magic = 0;
        double reply_val = 0.0;

        std::getline(client.get_input_stream(), reply_line);
        client.get_input_stream() >> reply_magic >> reply_val;

        std::cout << "[Client Thread] Server reply line: '" << reply_line << "'\n";
        std::cout << "[Client Thread] Server reply numbers: magic=" << reply_magic
                  << ", val=" << reply_val << '\n';

        // Join server thread (automatic with jthread)
        std::cout << "[Stream Demo] Stream demo finished successfully.\n";

    } catch (const libsocket::SocketException& e) {
        std::cerr << "[Stream Demo] Error: " << e.what() << '\n';
        return 1;
    }

    return 0;
}
