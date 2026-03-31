#ifndef CONCRETE_LOGGER_CLIENT_HPP
#define CONCRETE_LOGGER_CLIENT_HPP

#include <string>

#include "ConcreteBootstrap.hpp"
#include "ConcreteLoggerProtocol.hpp"
#include "ipUtil.hpp"

namespace quantas {

class ConcreteLoggerClient {
public:
    static void sendRegistration(const ConcreteBootstrapConfig& bootstrap, const std::string& peerIp, int peerPort) {
        sendJson(bootstrap, ConcreteLoggerProtocol::makePeerRegistration(peerIp, peerPort));
    }

    static void sendReport(const ConcreteBootstrapConfig& bootstrap, const json& report) {
        if (!bootstrap.hasLogger()) {
            return;
        }

        sendJson(bootstrap, report);
    }

private:
    static void sendJson(const ConcreteBootstrapConfig& bootstrap, const json& payload) {
        if (!bootstrap.hasLogger()) {
            return;
        }

        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            return;
        }

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(bootstrap.loggerPort);
        inet_pton(AF_INET, bootstrap.loggerIp.c_str(), &addr.sin_addr);

#ifdef __APPLE__
        int set = 1;
        setsockopt(sock, SOL_SOCKET, SO_NOSIGPIPE, &set, sizeof(set));
#endif

        const std::string body = payload.dump();
        if (connect(sock, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) == 0) {
            send(sock, body.c_str(), static_cast<int>(body.size()), 0);
        }

#ifdef _WIN32
        closesocket(sock);
#else
        close(sock);
#endif
    }
};

} // namespace quantas

#endif // CONCRETE_LOGGER_CLIENT_HPP