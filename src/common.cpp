#include "common.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <stdexcept>
#include <sys/socket.h>
#include <unistd.h>

namespace discovery {
namespace {

[[noreturn]] void throwSystemError(const std::string& action) {
    throw std::runtime_error(action + ": " + std::strerror(errno));
}

std::string stripComment(const std::string& line) {
    const auto pos = line.find('#');
    return pos == std::string::npos ? line : line.substr(0, pos);
}

in_addr parseIpv4(const std::string& address) {
    in_addr result{};
    if (inet_pton(AF_INET, address.c_str(), &result) != 1) {
        throw std::runtime_error("Invalid IPv4 address: " + address);
    }
    return result;
}

} // namespace

Config Config::load(const std::string& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("Unable to open config file: " + path);
    }

    Config config;
    std::string line;
    std::size_t lineNumber = 0;
    while (std::getline(input, line)) {
        ++lineNumber;
        line = trim(stripComment(line));
        if (line.empty()) {
            continue;
        }

        const auto equals = line.find('=');
        if (equals == std::string::npos) {
            throw std::runtime_error("Invalid config line " + std::to_string(lineNumber) + " in " + path);
        }

        const auto key = trim(line.substr(0, equals));
        const auto value = trim(line.substr(equals + 1));
        if (key.empty()) {
            throw std::runtime_error("Empty config key on line " + std::to_string(lineNumber) + " in " + path);
        }
        config.values_[key] = value;
    }

    return config;
}

std::string Config::get(const std::string& key, const std::string& fallback) const {
    const auto it = values_.find(key);
    return it == values_.end() ? fallback : it->second;
}

std::uint16_t Config::getPort(const std::string& key, std::uint16_t fallback, bool allowZero) const {
    const auto value = get(key);
    if (value.empty()) {
        return fallback;
    }

    const int port = std::stoi(value);
    if (port < 0 || port > 65535 || (!allowZero && port == 0)) {
        throw std::runtime_error("Invalid port for key " + key + ": " + value);
    }
    return static_cast<std::uint16_t>(port);
}

int Config::getInt(const std::string& key, int fallback) const {
    const auto value = get(key);
    return value.empty() ? fallback : std::stoi(value);
}

int createUdpSocket() {
    const int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        throwSystemError("socket");
    }
    return fd;
}

void setReuseAddress(int fd) {
    int enabled = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &enabled, sizeof(enabled)) < 0) {
        throwSystemError("setsockopt SO_REUSEADDR");
    }
}

void bindUdpSocket(int fd, const std::string& address, std::uint16_t port) {
    const auto bindAddress = makeSockaddr(address, port);
    if (bind(fd, reinterpret_cast<const sockaddr*>(&bindAddress), sizeof(bindAddress)) < 0) {
        throwSystemError("bind " + address + ":" + std::to_string(port));
    }
}

void joinMulticastGroup(int fd, const std::string& group, const std::string& interfaceAddress) {
    ip_mreq request{};
    request.imr_multiaddr = parseIpv4(group);
    request.imr_interface.s_addr = interfaceAddress.empty() || interfaceAddress == "0.0.0.0"
        ? htonl(INADDR_ANY)
        : parseIpv4(interfaceAddress).s_addr;

    if (setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &request, sizeof(request)) < 0) {
        throwSystemError("setsockopt IP_ADD_MEMBERSHIP");
    }
}

void setMulticastInterface(int fd, const std::string& interfaceAddress) {
    if (interfaceAddress.empty() || interfaceAddress == "0.0.0.0") {
        return;
    }

    const in_addr address = parseIpv4(interfaceAddress);
    if (setsockopt(fd, IPPROTO_IP, IP_MULTICAST_IF, &address, sizeof(address)) < 0) {
        throwSystemError("setsockopt IP_MULTICAST_IF");
    }
}

void setReceiveTimeout(int fd, std::chrono::milliseconds timeout) {
    timeval value{};
    value.tv_sec = static_cast<time_t>(timeout.count() / 1000);
    value.tv_usec = static_cast<suseconds_t>((timeout.count() % 1000) * 1000);
    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &value, sizeof(value)) < 0) {
        throwSystemError("setsockopt SO_RCVTIMEO");
    }
}

sockaddr_in makeSockaddr(const std::string& address, std::uint16_t port) {
    sockaddr_in result{};
    result.sin_family = AF_INET;
    result.sin_port = htons(port);
    result.sin_addr = parseIpv4(address);
    return result;
}

std::string sockaddrToIp(const sockaddr_in& address) {
    char buffer[INET_ADDRSTRLEN]{};
    if (!inet_ntop(AF_INET, &address.sin_addr, buffer, sizeof(buffer))) {
        throwSystemError("inet_ntop");
    }
    return buffer;
}

std::string trim(const std::string& value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return "";
    }
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

std::string nowRequestId() {
    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    std::random_device randomDevice;
    std::mt19937 generator(randomDevice());
    std::uniform_int_distribution<unsigned int> distribution(0, 0xffff);

    std::ostringstream output;
    output << std::hex << now << '-' << std::setw(4) << std::setfill('0') << distribution(generator);
    return output.str();
}

} // namespace discovery
