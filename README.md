# libsocket — Modern C++20 Java-like Socket Library

`libsocket` is a modern, object-oriented C++20 networking library designed to provide an intuitive API modeled after Java's `java.net` socket architecture (`Socket`, `ServerSocket`, `DatagramSocket`, `DatagramPacket`, `InetAddress`).

It encapsulates low-level POSIX Berkeley sockets into safe, RAII-managed, move-only abstractions with full exception safety, standard stream integration (`std::iostream`), and generic type transmission.

---

## Features

- **Java-like API**:
  - `Socket` — TCP client connection with direct buffer I/O, `read_line()`, and `send_object<T>()`/`receive_object<T>()`.
  - `ServerSocket` — TCP server with `accept()` returning an RAII-managed `Socket`.
  - `DatagramSocket` & `DatagramPacket` — UDP connectionless networking with generic type packet helpers.
  - `InetAddress` — DNS name resolution and IPv4 address representation.
  - `SocketInputStream` & `SocketOutputStream` — C++ standard stream adapters (`std::istream` / `std::ostream`) backed by `SocketStreambuf`.
- **Generic Type Transmission & Casting**:
  - Transmit any trivially copyable C++ struct or type across sockets safely using network-byte-order size framing and `std::bit_cast`.
  - Works even for types without default constructors.
- **Modern C++20 & C++ Core Guidelines**:
  - **RAII**: Automatic socket closure on destruction; zero file descriptor leaks.
  - **Rule of Five**: Explicit move constructors and move assignment operators with `std::exchange`; copy operations explicitly deleted.
  - **No duplicate logic**: Centralized socket option routines (`SO_RCVTIMEO`, `SO_REUSEADDR`, `getsockname`).
  - **Exception Hierarchy**: `SocketException`, `BindException`, `UnknownHostException`, `SocketTimeoutException`, and `SerializationException`.
  - Fully documented with **Doxygen** comments.

---

## Project Structure

```
.
├── CMakeLists.txt              # CMake build configuration
├── include/libsocket/          # Public header files
│   ├── libsocket.hpp           # Umbrella header
│   ├── InetAddress.hpp         # Hostname & IP resolution
│   ├── Socket.hpp              # TCP client socket
│   ├── ServerSocket.hpp        # TCP listening server socket
│   ├── SocketStream.hpp        # std::iostream stream wrappers
│   ├── DatagramSocket.hpp      # UDP socket
│   ├── DatagramPacket.hpp      # UDP datagram packet
│   ├── SocketException.hpp     # Custom exception hierarchy
│   └── detail/
│       └── SocketUtil.hpp      # Internal socket helpers
├── src/                        # Implementation files
├── examples/                   # Practical examples
│   ├── tcp_server.cpp
│   ├── tcp_client.cpp
│   ├── tcp_concurrent_server.cpp
│   ├── udp_server.cpp
│   ├── udp_client.cpp
│   ├── stream_demo.cpp
│   └── generic_type_demo.cpp
├── tests/                      # 25-suite automated test runner
├── .gitignore
└── README.md
```

---

## Building and Testing

### Prerequisites
- C++20 compatible compiler (GCC 11+, Clang 13+)
- CMake 3.20+
- Linux (POSIX socket interface)

### Build Instructions

```bash
# Configure the build
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Compile library, examples, and test suite
cmake --build build -- -j$(nproc)

# Run test suite
ctest --test-dir build --output-on-failure
```

---

## Quick Start Examples

### TCP Client / Server

```cpp
#include <libsocket/libsocket.hpp>
#include <iostream>

// Server
libsocket::ServerSocket server(6000);
libsocket::Socket client = server.accept();
client.send("Hello from server!\n");

// Client
libsocket::Socket socket("127.0.0.1", 6000);
std::string greeting = socket.read_line();
std::cout << greeting << '\n';
```

### Generic Struct Transfer

```cpp
#include <libsocket/libsocket.hpp>

struct PlayerData {
    int id;
    float x, y;
    char name[32];
};

// Sender
PlayerData player{1, 10.5f, 20.0f, "Hero"};
socket.send_object(player);

// Receiver
PlayerData received = socket.receive_object<PlayerData>();
```

### Standard C++ Streams

```cpp
#include <libsocket/libsocket.hpp>

// Stream output
socket.get_output_stream() << "HTTP/1.1 200 OK\r\n\r\nHello World!" << std::flush;

// Stream input
std::string line;
std::getline(socket.get_input_stream(), line);
```

---

## License
MIT License
