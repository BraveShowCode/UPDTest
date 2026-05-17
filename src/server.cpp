#include "common.hpp"

#include <array>
#include <csignal>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

namespace {

volatile std::sig_atomic_t keepRunning = 1;

void handleSignal(int) {
    keepRunning = 0;
}

std::string extractRequestId(const std::string& request) {
    const std::string key = "request_id=";
    const auto pos = request.find(key);
    if (pos == std::string::npos) {
        return "unknown";
    }
    const auto begin = pos + key.size();
    const auto end = request.find_first_of(" \t\r\n", begin);
    return request.substr(begin, end == std::string::npos ? std::string::npos : end - begin);
}

std::string buildResponse(const discovery::Config& config, const std::string& requestId) {
    return "ZMQ_CONFIG v1 request_id=" + requestId
        + " endpoint=" + config.get("zeromq_endpoint", "tcp://127.0.0.1:5555")
        + " pub_endpoint=" + config.get("zeromq_pub_endpoint", "tcp://127.0.0.1:5556")
        + " sub_endpoint=" + config.get("zeromq_sub_endpoint", "tcp://127.0.0.1:5557")
        + " topic=" + config.get("zeromq_topic", "default");
}

} // namespace

int main(int argc, char* argv[]) {
    const std::string configPath = argc > 1 ? argv[1] : "config/server.conf";

    try {
        const auto config = discovery::Config::load(configPath);
        const auto group = config.get("multicast_group", "239.255.0.1");
        const auto interfaceAddress = config.get("interface", "0.0.0.0");
        const auto bindAddress = config.get("bind_address", "0.0.0.0");
        const auto port = config.getPort("multicast_port", 30001);

        std::signal(SIGINT, handleSignal);
        std::signal(SIGTERM, handleSignal);

        const int fd = discovery::createUdpSocket();
        discovery::setReuseAddress(fd);
        discovery::bindUdpSocket(fd, bindAddress, port);
        discovery::joinMulticastGroup(fd, group, interfaceAddress);
        discovery::setReceiveTimeout(fd, std::chrono::milliseconds(config.getInt("poll_timeout_ms", 1000)));

        std::cout << "UDP multicast ZeroMQ discovery server listening on " << group << ':' << port << std::endl;

        std::array<char, 2048> buffer{};
        while (keepRunning) {
            sockaddr_in clientAddress{};
            socklen_t clientLength = sizeof(clientAddress);
            const ssize_t received = recvfrom(
                fd,
                buffer.data(),
                buffer.size() - 1,
                0,
                reinterpret_cast<sockaddr*>(&clientAddress),
                &clientLength);

            if (received < 0) {
                if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
                    continue;
                }
                throw std::runtime_error(std::string("recvfrom: ") + std::strerror(errno));
            }

            const std::string request(buffer.data(), static_cast<std::size_t>(received));
            if (request.rfind("DISCOVER_ZMQ_CONFIG v1", 0) != 0) {
                std::cerr << "Ignoring unsupported discovery request from "
                          << discovery::sockaddrToIp(clientAddress) << std::endl;
                continue;
            }

            const auto requestId = extractRequestId(request);
            const auto response = buildResponse(config, requestId);
            const ssize_t sent = sendto(
                fd,
                response.data(),
                response.size(),
                0,
                reinterpret_cast<const sockaddr*>(&clientAddress),
                clientLength);
            if (sent < 0) {
                throw std::runtime_error(std::string("sendto: ") + std::strerror(errno));
            }

            std::cout << "Sent ZeroMQ config to " << discovery::sockaddrToIp(clientAddress)
                      << ':' << ntohs(clientAddress.sin_port) << " request_id=" << requestId << std::endl;
        }

        close(fd);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "server error: " << error.what() << std::endl;
        return 1;
    }
}
