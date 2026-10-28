/**
 * @file test_runner.cpp
 * @brief Self-contained automated test suite for libsocket.
 * 
 * Tests all components: InetAddress, Socket, ServerSocket, SocketStream,
 * DatagramPacket, DatagramSocket, exceptions, RAII, and move semantics.
 */

#include <libsocket/libsocket.hpp>

#include <cassert>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace {

int g_tests_passed = 0;
int g_tests_failed = 0;

#define TEST_ASSERT(cond) \
    do { \
        if (!(cond)) { \
            throw std::runtime_error("Assertion failed: " #cond " (" __FILE__ ":" + std::to_string(__LINE__) + ")"); \
        } \
    } while (0)

#define RUN_TEST(fn) \
    do { \
        std::cout << "[RUNNING] " << #fn << "...\n"; \
        try { \
            fn(); \
            std::cout << "[PASS]    " << #fn << "\n"; \
            ++g_tests_passed; \
        } catch (const std::exception& e) { \
            std::cerr << "[FAIL]    " << #fn << " failed: " << e.what() << "\n"; \
            ++g_tests_failed; \
        } catch (...) { \
            std::cerr << "[FAIL]    " << #fn << " threw unknown exception\n"; \
            ++g_tests_failed; \
        } \
    } while (0)

struct TestStruct {
    int id;
    double value;
    char text[16];

    bool operator==(const TestStruct& other) const noexcept {
        return id == other.id &&
               value == other.value &&
               std::strncmp(text, other.text, sizeof(text)) == 0;
    }
};

static_assert(std::is_trivially_copyable_v<TestStruct>);

struct Point2D {
    int x;
    int y;
    Point2D(int a, int b) : x(a), y(b) {}
    bool operator==(const Point2D& other) const noexcept {
        return x == other.x && y == other.y;
    }
};

static_assert(!std::is_default_constructible_v<Point2D>);
static_assert(std::is_trivially_copyable_v<Point2D>);

// Test 1: InetAddress::get_local_host() resolves and to_string() returns non-empty string
void test_inet_address_local_host() {
    libsocket::InetAddress local = libsocket::InetAddress::get_local_host();
    std::string ip_str = local.to_string();
    std::cout << "          Local host IP: " << ip_str << '\n';
    TEST_ASSERT(!ip_str.empty());
    TEST_ASSERT(ip_str.find('.') != std::string::npos);
}

// Test 2: InetAddress::get_by_name("127.0.0.1") succeeds
void test_inet_address_get_by_name_loopback() {
    libsocket::InetAddress loopback = libsocket::InetAddress::get_by_name("127.0.0.1");
    TEST_ASSERT(loopback.to_string() == "127.0.0.1");
}

// Test 3: InetAddress::get_by_name("invalid.host.that.does.not.exist.xyz") throws UnknownHostException
void test_inet_address_invalid_host() {
    bool caught = false;
    try {
        [[maybe_unused]] auto addr =
            libsocket::InetAddress::get_by_name("invalid.host.that.does.not.exist.xyz");
    } catch (const libsocket::UnknownHostException& e) {
        caught = true;
        std::cout << "          Caught expected UnknownHostException: " << e.what() << '\n';
    }
    TEST_ASSERT(caught);
}

// Test 4: TCP loopback: server thread + client thread exchange a string message and verify correctness
void test_tcp_loopback() {
    libsocket::ServerSocket server(0);
    int port = server.get_local_port();
    TEST_ASSERT(port > 0);

    const std::string server_msg = "Greetings from TCP server!";
    const std::string client_msg = "Hello back from TCP client!";
    std::string received_by_client;
    std::string received_by_server;

    std::jthread server_thread([&]() {
        libsocket::Socket client_conn = server.accept();
        client_conn.send(server_msg);
        received_by_server = client_conn.receive(128);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    libsocket::Socket client("127.0.0.1", port);
    received_by_client = client.receive(128);
    client.send(client_msg);

    server_thread.join();

    TEST_ASSERT(received_by_client == server_msg);
    TEST_ASSERT(received_by_server == client_msg);
}

// Test 5: TCP read_line(): client sends lines terminated with '\n', server reads with read_line(), verifies content
void test_tcp_read_line() {
    libsocket::ServerSocket server(0);
    int port = server.get_local_port();

    std::vector<std::string> lines_to_send = {
        "First line of text",
        "Second line with special characters: !@#$%^&*()",
        "Third line, empty line follows next",
        "",
        "Final line without carriage return"
    };

    std::vector<std::string> received_lines;

    std::jthread server_thread([&]() {
        libsocket::Socket client_conn = server.accept();
        for (std::size_t i = 0; i < lines_to_send.size(); ++i) {
            std::string line = client_conn.read_line();
            received_lines.push_back(line);
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    libsocket::Socket client("127.0.0.1", port);

    // Send lines with '\n' or '\r\n'
    for (std::size_t i = 0; i < lines_to_send.size(); ++i) {
        std::string wire_line = lines_to_send[i] + (i % 2 == 0 ? "\r\n" : "\n");
        client.send(wire_line);
    }

    server_thread.join();

    TEST_ASSERT(received_lines.size() == lines_to_send.size());
    for (std::size_t i = 0; i < lines_to_send.size(); ++i) {
        TEST_ASSERT(received_lines[i] == lines_to_send[i]);
    }
}

// Test 6: TCP send_object / receive_object: exchange a trivially copyable struct and verify field values
void test_tcp_send_receive_object() {
    libsocket::ServerSocket server(0);
    int port = server.get_local_port();

    TestStruct sent_obj{42, 99.95, "MagicToken"};
    TestStruct received_obj{};

    std::jthread server_thread([&]() {
        libsocket::Socket client_conn = server.accept();
        received_obj = client_conn.receive_object<TestStruct>();
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    libsocket::Socket client("127.0.0.1", port);
    client.send_object(sent_obj);

    server_thread.join();

    TEST_ASSERT(received_obj == sent_obj);
}

// Test 7: DatagramSocket loopback: send a DatagramPacket, receive it, verify data and sender address
void test_datagram_socket_loopback() {
    libsocket::DatagramSocket server_sock(0);
    int port = server_sock.get_local_port();
    TEST_ASSERT(port > 0);

    const std::string payload = "Hello UDP World!";

    std::jthread server_thread([&]() {
        libsocket::DatagramPacket rx_packet(512);
        server_sock.receive(rx_packet);
        TEST_ASSERT(rx_packet.as_string() == payload);
        // Reply back to sender
        rx_packet.set_data("UDP Echo: " + payload);
        server_sock.send(rx_packet);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    libsocket::DatagramSocket client_sock;
    libsocket::InetAddress server_addr = libsocket::InetAddress::get_by_name("127.0.0.1");

    libsocket::DatagramPacket tx_packet(payload, server_addr, port);
    client_sock.send(tx_packet);

    libsocket::DatagramPacket reply_packet(512);
    client_sock.receive(reply_packet);

    server_thread.join();

    TEST_ASSERT(reply_packet.as_string() == "UDP Echo: " + payload);
    TEST_ASSERT(reply_packet.get_port() == port);
}

// Test 8: DatagramPacket::set_object / get_object: verify trivially copyable struct round-trip
void test_datagram_packet_object_serialization() {
    TestStruct original{101, 3.14159, "DatagramData"};
    libsocket::DatagramPacket packet(sizeof(TestStruct));
    packet.set_object(original);

    TEST_ASSERT(packet.get_length() == sizeof(TestStruct));
    TestStruct recovered = packet.get_object<TestStruct>();

    TEST_ASSERT(recovered == original);
}

// Test 9: Move semantics: move a Socket, verify moved-from is invalid, moved-to is valid
void test_socket_move_semantics() {
    libsocket::ServerSocket server(0);
    int port = server.get_local_port();

    std::jthread server_thread([&]() {
        libsocket::Socket conn = server.accept();
        conn.send("MoveSemanticsOK");
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    libsocket::Socket s1("127.0.0.1", port);
    int original_fd = s1.native_handle();
    TEST_ASSERT(original_fd >= 0);
    TEST_ASSERT(s1.is_connected());

    // Move construct s2 from s1
    libsocket::Socket s2 = std::move(s1);
    TEST_ASSERT(s1.native_handle() == -1);
    TEST_ASSERT(!s1.is_connected());
    TEST_ASSERT(s2.native_handle() == original_fd);
    TEST_ASSERT(s2.is_connected());

    // Verify s2 functions normally
    std::string msg = s2.receive(64);
    TEST_ASSERT(msg == "MoveSemanticsOK");

    // Move assign s3 from s2
    libsocket::Socket s3;
    TEST_ASSERT(!s3.is_connected());
    s3 = std::move(s2);
    TEST_ASSERT(s2.native_handle() == -1);
    TEST_ASSERT(!s2.is_connected());
    TEST_ASSERT(s3.native_handle() == original_fd);
    TEST_ASSERT(s3.is_connected());

    server_thread.join();
}

// Test 10: RAII: create a Socket in a scope, exit scope, verify descriptor is closed (check /proc/self/fd)
void test_socket_raii_descriptor_closure() {
    libsocket::ServerSocket server(0);
    int port = server.get_local_port();

    std::jthread server_thread([&]() {
        libsocket::Socket conn = server.accept();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    int raw_fd = -1;
    {
        libsocket::Socket client_socket("127.0.0.1", port);
        raw_fd = client_socket.native_handle();
        TEST_ASSERT(raw_fd >= 0);

        // Verify the file descriptor is currently present in /proc/self/fd
        std::string fd_path = "/proc/self/fd/" + std::to_string(raw_fd);
        TEST_ASSERT(std::filesystem::exists(fd_path));
    } // client_socket leaves scope and destructor executes close(raw_fd)

    // Verify descriptor has been closed and does not exist in /proc/self/fd
    std::string fd_path = "/proc/self/fd/" + std::to_string(raw_fd);
    TEST_ASSERT(!std::filesystem::exists(fd_path));

    server_thread.join();
}

// Test 11: Stream operator: socket.get_output_stream() << "hello\n" and std::getline(socket.get_input_stream(), line) verify content
void test_socket_stream_operators() {
    libsocket::ServerSocket server(0);
    int port = server.get_local_port();

    std::jthread server_thread([&]() {
        libsocket::Socket conn = server.accept();
        conn.get_output_stream() << "hello from stream\n" << 777 << '\n' << std::flush;

        std::string client_line;
        std::getline(conn.get_input_stream(), client_line);
        TEST_ASSERT(client_line == "client replying through stream");
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    libsocket::Socket client("127.0.0.1", port);

    std::string line;
    std::getline(client.get_input_stream(), line);
    TEST_ASSERT(line == "hello from stream");

    int num = 0;
    client.get_input_stream() >> num;
    TEST_ASSERT(num == 777);

    // Consume newline after number
    std::string dummy;
    std::getline(client.get_input_stream(), dummy);

    client.get_output_stream() << "client replying through stream\n" << std::flush;

    server_thread.join();
}

// Extra Test 12: Socket timeout exception
void test_socket_timeout_exception() {
    libsocket::ServerSocket server(0);
    int port = server.get_local_port();

    std::jthread server_thread([&]() {
        libsocket::Socket conn = server.accept();
        // Hold connection without sending anything for 300ms
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    libsocket::Socket client("127.0.0.1", port);
    client.set_so_timeout(std::chrono::milliseconds(100));

    bool caught_timeout = false;
    try {
        [[maybe_unused]] auto data = client.receive(32);
    } catch (const libsocket::SocketTimeoutException& e) {
        caught_timeout = true;
        std::cout << "          Caught expected SocketTimeoutException: " << e.what() << '\n';
    }
    TEST_ASSERT(caught_timeout);

    server_thread.join();
}

// Test 13: TCP send_object / receive_object with non-default-constructible trivially copyable struct
void test_tcp_send_receive_object_non_default_constructible() {
    libsocket::ServerSocket server(0);
    int port = server.get_local_port();

    Point2D sent_pt(123, 456);
    std::optional<Point2D> received_pt;

    std::jthread server_thread([&]() {
        libsocket::Socket conn = server.accept();
        received_pt = conn.receive_object<Point2D>();
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    libsocket::Socket client("127.0.0.1", port);
    client.send_object(sent_pt);

    server_thread.join();
    TEST_ASSERT(received_pt.has_value());
    TEST_ASSERT(*received_pt == sent_pt);
}

// Test 14: DatagramPacket set_object / get_object with non-default-constructible trivially copyable struct
void test_datagram_packet_object_non_default_constructible() {
    Point2D pt(987, 654);
    libsocket::DatagramPacket packet(sizeof(Point2D));
    packet.set_object(pt);
    Point2D recovered = packet.get_object<Point2D>();
    TEST_ASSERT(recovered == pt);
}

// Test 15: Socket move preserves stream buffer without data loss
void test_socket_move_preserves_stream_buffer() {
    libsocket::ServerSocket server(0);
    int port = server.get_local_port();

    std::jthread server_thread([&]() {
        libsocket::Socket conn = server.accept();
        conn.send("Line Alpha\nLine Beta\n");
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    libsocket::Socket client1("127.0.0.1", port);
    std::string l1;
    std::getline(client1.get_input_stream(), l1);
    TEST_ASSERT(l1 == "Line Alpha");

    // Move client1 into client2 - buffered "Line Beta\n" must NOT be lost
    libsocket::Socket client2 = std::move(client1);
    TEST_ASSERT(!client1.is_connected());
    TEST_ASSERT(client2.is_connected());

    std::string l2;
    std::getline(client2.get_input_stream(), l2);
    TEST_ASSERT(l2 == "Line Beta");

    server_thread.join();
}

// Test 16: SocketStream unget and formatted extraction
void test_socket_stream_unget_and_putback() {
    libsocket::ServerSocket server(0);
    int port = server.get_local_port();

    std::jthread server_thread([&]() {
        libsocket::Socket conn = server.accept();
        conn.get_output_stream() << "9876Z" << std::flush;
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    libsocket::Socket client("127.0.0.1", port);

    int val = 0;
    client.get_input_stream() >> val;
    TEST_ASSERT(val == 9876);

    char ch = '\0';
    client.get_input_stream().get(ch);
    TEST_ASSERT(ch == 'Z');

    client.get_input_stream().unget();
    char ch2 = '\0';
    client.get_input_stream().get(ch2);
    TEST_ASSERT(ch2 == 'Z');

    server_thread.join();
}

// Test 17: ServerSocket move semantics
void test_server_socket_move_semantics() {
    libsocket::ServerSocket s1(0);
    int original_fd = s1.native_handle();
    int port = s1.get_local_port();
    TEST_ASSERT(original_fd >= 0);
    TEST_ASSERT(s1.is_bound());

    // Move construct s2 from s1
    libsocket::ServerSocket s2 = std::move(s1);
    TEST_ASSERT(s1.native_handle() == -1);
    TEST_ASSERT(!s1.is_bound());
    TEST_ASSERT(s2.native_handle() == original_fd);
    TEST_ASSERT(s2.is_bound());
    TEST_ASSERT(s2.get_local_port() == port);

    // Accept client on s2 to verify functionality
    std::jthread client_thread([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        libsocket::Socket c("127.0.0.1", port);
        c.send("OK");
    });

    libsocket::Socket accepted = s2.accept();
    TEST_ASSERT(accepted.receive(10) == "OK");
    client_thread.join();
}

// Test 18: DatagramSocket move semantics
void test_datagram_socket_move_semantics() {
    libsocket::DatagramSocket ds1(0);
    int original_fd = ds1.native_handle();
    int port = ds1.get_local_port();
    TEST_ASSERT(original_fd >= 0);
    TEST_ASSERT(!ds1.is_closed());

    // Move construct ds2 from ds1
    libsocket::DatagramSocket ds2 = std::move(ds1);
    TEST_ASSERT(ds1.native_handle() == -1);
    TEST_ASSERT(ds1.is_closed());
    TEST_ASSERT(ds2.native_handle() == original_fd);
    TEST_ASSERT(!ds2.is_closed());
    TEST_ASSERT(ds2.get_local_port() == port);

    // Send and receive on ds2
    libsocket::DatagramSocket client_ds;
    libsocket::InetAddress local = libsocket::InetAddress::get_by_name("127.0.0.1");
    libsocket::DatagramPacket tx("UDP Move Test", local, port);
    client_ds.send(tx);

    libsocket::DatagramPacket rx(256);
    ds2.receive(rx);
    TEST_ASSERT(rx.as_string() == "UDP Move Test");
}

// Test 19: ServerSocket RAII descriptor closure
void test_server_socket_raii() {
    int raw_fd = -1;
    {
        libsocket::ServerSocket server(0);
        raw_fd = server.native_handle();
        TEST_ASSERT(raw_fd >= 0);
        std::string fd_path = "/proc/self/fd/" + std::to_string(raw_fd);
        TEST_ASSERT(std::filesystem::exists(fd_path));
    }
    std::string fd_path = "/proc/self/fd/" + std::to_string(raw_fd);
    TEST_ASSERT(!std::filesystem::exists(fd_path));
}

// Test 20: DatagramSocket RAII descriptor closure
void test_datagram_socket_raii() {
    int raw_fd = -1;
    {
        libsocket::DatagramSocket ds(0);
        raw_fd = ds.native_handle();
        TEST_ASSERT(raw_fd >= 0);
        std::string fd_path = "/proc/self/fd/" + std::to_string(raw_fd);
        TEST_ASSERT(std::filesystem::exists(fd_path));
    }
    std::string fd_path = "/proc/self/fd/" + std::to_string(raw_fd);
    TEST_ASSERT(!std::filesystem::exists(fd_path));
}

// Test 21: ServerSocket accept timeout
void test_server_socket_timeout() {
    libsocket::ServerSocket server(0);
    server.set_so_timeout(std::chrono::milliseconds(50));
    bool caught = false;
    try {
        [[maybe_unused]] auto conn = server.accept();
    } catch (const libsocket::SocketTimeoutException& e) {
        caught = true;
        std::cout << "          Caught expected ServerSocket timeout: " << e.what() << '\n';
    }
    TEST_ASSERT(caught);
}

// Test 22: DatagramSocket receive timeout
void test_datagram_socket_timeout() {
    libsocket::DatagramSocket ds(0);
    ds.set_so_timeout(std::chrono::milliseconds(50));
    libsocket::DatagramPacket rx(256);
    bool caught = false;
    try {
        ds.receive(rx);
    } catch (const libsocket::SocketTimeoutException& e) {
        caught = true;
        std::cout << "          Caught expected DatagramSocket timeout: " << e.what() << '\n';
    }
    TEST_ASSERT(caught);
}

// Test 23: SerializationException on size mismatch in TCP receive_object
void test_serialization_exception_size_mismatch() {
    libsocket::ServerSocket server(0);
    int port = server.get_local_port();

    std::jthread client_thread([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        libsocket::Socket client("127.0.0.1", port);
        // Send a size header that does NOT match TestStruct (e.g. 9999 bytes)
        uint32_t wrong_size = ::htonl(9999);
        client.send(&wrong_size, sizeof(wrong_size));
    });

    libsocket::Socket server_conn = server.accept();
    bool caught = false;
    try {
        [[maybe_unused]] auto obj = server_conn.receive_object<TestStruct>();
    } catch (const libsocket::SerializationException& e) {
        caught = true;
        std::cout << "          Caught expected SerializationException: " << e.what() << '\n';
    }
    TEST_ASSERT(caught);
    client_thread.join();
}

// Test 24: SerializationException on DatagramPacket buffer too small
void test_serialization_exception_datagram_too_short() {
    libsocket::DatagramPacket packet(4); // 4 bytes is less than sizeof(TestStruct)
    packet.set_length(4);
    bool caught = false;
    try {
        [[maybe_unused]] auto obj = packet.get_object<TestStruct>();
    } catch (const libsocket::SerializationException& e) {
        caught = true;
        std::cout << "          Caught expected Datagram SerializationException: " << e.what() << '\n';
    }
    TEST_ASSERT(caught);
}

// Test 25: Setting socket options before connect on default-constructed Socket
void test_socket_option_before_connect() {
    libsocket::ServerSocket server(0);
    int port = server.get_local_port();

    std::jthread server_thread([&]() {
        libsocket::Socket conn = server.accept();
        conn.send("PreConnectOptOK");
    });

    libsocket::Socket client;
    client.set_so_timeout(std::chrono::milliseconds(500));
    client.set_tcp_no_delay(true);
    client.connect("127.0.0.1", port);

    std::string msg = client.receive(32);
    TEST_ASSERT(msg == "PreConnectOptOK");

    server_thread.join();
}

} // namespace

int main() {
    std::cout << "========================================\n";
    std::cout << "       libsocket Test Suite Runner      \n";
    std::cout << "========================================\n\n";

    RUN_TEST(test_inet_address_local_host);
    RUN_TEST(test_inet_address_get_by_name_loopback);
    RUN_TEST(test_inet_address_invalid_host);
    RUN_TEST(test_tcp_loopback);
    RUN_TEST(test_tcp_read_line);
    RUN_TEST(test_tcp_send_receive_object);
    RUN_TEST(test_datagram_socket_loopback);
    RUN_TEST(test_datagram_packet_object_serialization);
    RUN_TEST(test_socket_move_semantics);
    RUN_TEST(test_socket_raii_descriptor_closure);
    RUN_TEST(test_socket_stream_operators);
    RUN_TEST(test_socket_timeout_exception);
    RUN_TEST(test_tcp_send_receive_object_non_default_constructible);
    RUN_TEST(test_datagram_packet_object_non_default_constructible);
    RUN_TEST(test_socket_move_preserves_stream_buffer);
    RUN_TEST(test_socket_stream_unget_and_putback);
    RUN_TEST(test_server_socket_move_semantics);
    RUN_TEST(test_datagram_socket_move_semantics);
    RUN_TEST(test_server_socket_raii);
    RUN_TEST(test_datagram_socket_raii);
    RUN_TEST(test_server_socket_timeout);
    RUN_TEST(test_datagram_socket_timeout);
    RUN_TEST(test_serialization_exception_size_mismatch);
    RUN_TEST(test_serialization_exception_datagram_too_short);
    RUN_TEST(test_socket_option_before_connect);

    std::cout << "\n========================================\n";
    std::cout << "Tests Passed: " << g_tests_passed << "\n";
    std::cout << "Tests Failed: " << g_tests_failed << "\n";
    std::cout << "========================================\n";

    return (g_tests_failed == 0) ? 0 : 1;
}
