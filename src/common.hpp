#pragma once

#include <chrono>
#include <cstdint>
#include <map>
#include <netinet/in.h>
#include <string>

namespace discovery {

class Config {
public:
    static Config load(const std::string& path);

    std::string get(const std::string& key, const std::string& fallback = "") const;
    std::uint16_t getPort(const std::string& key, std::uint16_t fallback, bool allowZero = false) const;
    int getInt(const std::string& key, int fallback) const;

private:
    std::map<std::string, std::string> values_;
};

int createUdpSocket();
void setReuseAddress(int fd);
void bindUdpSocket(int fd, const std::string& address, std::uint16_t port);
void joinMulticastGroup(int fd, const std::string& group, const std::string& interfaceAddress);
void setMulticastInterface(int fd, const std::string& interfaceAddress);
void setReceiveTimeout(int fd, std::chrono::milliseconds timeout);
sockaddr_in makeSockaddr(const std::string& address, std::uint16_t port);
std::string sockaddrToIp(const sockaddr_in& address);
std::string trim(const std::string& value);
std::string nowRequestId();

} // namespace discovery
