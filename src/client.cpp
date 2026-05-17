#include "common.hpp"

#include <array>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

int main(int argc, char* argv[]) {
    const std::string configPath = argc > 1 ? argv[1] : "config/client.conf";

    try {
        const auto config = discovery::Config::load(configPath);
        const auto group = config.get("multicast_group", "239.255.0.1");
        const auto interfaceAddress = config.get("interface", "0.0.0.0");
        const auto bindAddress = config.get("bind_address", "0.0.0.0");
        const auto multicastPort = config.getPort("multicast_port", 30001);
        const auto listenPort = config.getPort("listen_port", 0, true);
        const auto timeoutMs = config.getInt("timeout_ms", 3000);

        const int fd = discovery::createUdpSocket();
        discovery::setReuseAddress(fd);
        discovery::bindUdpSocket(fd, bindAddress, listenPort);
        discovery::setMulticastInterface(fd, interfaceAddress);
        discovery::setReceiveTimeout(fd, std::chrono::milliseconds(timeoutMs));

        const auto requestId = discovery::nowRequestId();
        const std::string request = "DISCOVER_ZMQ_CONFIG v1 request_id=" + requestId;
        const auto destination = discovery::makeSockaddr(group, multicastPort);

        const ssize_t sent = sendto(
            fd,
            request.data(),
            request.size(),
            0,
            reinterpret_cast<const sockaddr*>(&destination),
            sizeof(destination));
        if (sent < 0) {
            throw std::runtime_error(std::string("sendto: ") + std::strerror(errno));
        }

        std::array<char, 2048> buffer{};
        sockaddr_in serverAddress{};
        socklen_t serverLength = sizeof(serverAddress);
        const ssize_t received = recvfrom(
            fd,
            buffer.data(),
            buffer.size() - 1,
            0,
            reinterpret_cast<sockaddr*>(&serverAddress),
            &serverLength);

        if (received < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                throw std::runtime_error("Timed out waiting for unicast ZeroMQ config response");
            }
            throw std::runtime_error(std::string("recvfrom: ") + std::strerror(errno));
        }

        const std::string response(buffer.data(), static_cast<std::size_t>(received));
        if (response.find("request_id=" + requestId) == std::string::npos) {
            throw std::runtime_error("Received response for a different request: " + response);
        }

        std::cout << "Received unicast response from " << discovery::sockaddrToIp(serverAddress)
                  << ':' << ntohs(serverAddress.sin_port) << std::endl;
        std::cout << response << std::endl;

        close(fd);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "client error: " << error.what() << std::endl;
        return 1;
    }
}
