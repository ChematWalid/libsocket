/**
 * @file generic_type_demo.cpp
 * @brief Demonstrates typed object transmission over sockets using libsocket.
 * 
 * Shows how send_object<T>() and receive_object<T>() safely transmit trivially copyable
 * C++ structs over TCP using a network-order size header and binary payload.
 */

#include <libsocket/libsocket.hpp>

#include <cassert>
#include <cstring>
#include <iostream>
#include <thread>

// Trivially copyable struct representing game player state
struct PlayerData {
    int id;
    float x;
    float y;
    char name[32];
};

static_assert(std::is_trivially_copyable_v<PlayerData>, "PlayerData must be trivially copyable");

int main() {
    try {
        std::cout << "[Generic Type Demo] Starting demo...\n";

        // Bind server to an ephemeral port
        libsocket::ServerSocket server(0);
        int port = server.get_local_port();
        std::cout << "[Generic Type Demo] Server bound to port " << port << '\n';

        // Server thread receives object, updates coordinates, and sends back
        std::jthread server_thread([&server]() {
            try {
                libsocket::Socket client = server.accept();
                std::cout << "[Server] Accepted client connection.\n";

                // Receive PlayerData object
                PlayerData player = client.receive_object<PlayerData>();
                std::cout << "[Server] Received PlayerData: id=" << player.id
                          << ", name='" << player.name << "', pos=("
                          << player.x << ", " << player.y << ")\n";

                // Update player data
                player.x += 10.5f;
                player.y += 20.0f;
                std::strncpy(player.name, "Archmage-Updated", sizeof(player.name) - 1);

                // Send updated object back
                client.send_object(player);
                std::cout << "[Server] Sent updated PlayerData back to client.\n";
            } catch (const libsocket::SocketException& e) {
                std::cerr << "[Server] Error: " << e.what() << '\n';
            }
        });

        // Client connects and sends initial player state
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        libsocket::Socket client("127.0.0.1", port);
        std::cout << "[Client] Connected to server.\n";

        PlayerData initial_player{};
        initial_player.id = 1337;
        initial_player.x = 100.0f;
        initial_player.y = 250.0f;
        std::strncpy(initial_player.name, "Archmage", sizeof(initial_player.name) - 1);

        std::cout << "[Client] Sending initial PlayerData: id=" << initial_player.id
                  << ", name='" << initial_player.name << "', pos=("
                  << initial_player.x << ", " << initial_player.y << ")\n";

        client.send_object(initial_player);

        // Receive modified player data back from server
        PlayerData returned_player = client.receive_object<PlayerData>();
        std::cout << "[Client] Received returned PlayerData: id=" << returned_player.id
                  << ", name='" << returned_player.name << "', pos=("
                  << returned_player.x << ", " << returned_player.y << ")\n";

        // Verify fields
        assert(returned_player.id == 1337);
        assert(returned_player.x == 110.5f);
        assert(returned_player.y == 270.0f);
        assert(std::strcmp(returned_player.name, "Archmage-Updated") == 0);

        std::cout << "[Generic Type Demo] Verification passed! Struct round-trip successful.\n";

    } catch (const libsocket::SocketException& e) {
        std::cerr << "[Generic Type Demo] Error: " << e.what() << '\n';
        return 1;
    }

    return 0;
}
