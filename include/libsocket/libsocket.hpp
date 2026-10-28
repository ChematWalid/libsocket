#pragma once

/**
 * @file libsocket.hpp
 * @brief Main umbrella header for the libsocket C++20 network library.
 * 
 * Provides Java-like, RAII-safe object-oriented socket abstractions:
 * - InetAddress: IPv4 address resolution and representation
 * - Socket: TCP client socket and stream I/O
 * - ServerSocket: TCP server socket
 * - SocketInputStream & SocketOutputStream: iostream integration
 * - DatagramPacket & DatagramSocket: UDP packet exchange
 * - SocketException hierarchy: Exception-based error handling
 */

#include <libsocket/DatagramPacket.hpp>
#include <libsocket/DatagramSocket.hpp>
#include <libsocket/InetAddress.hpp>
#include <libsocket/ServerSocket.hpp>
#include <libsocket/Socket.hpp>
#include <libsocket/SocketException.hpp>
#include <libsocket/SocketStream.hpp>
