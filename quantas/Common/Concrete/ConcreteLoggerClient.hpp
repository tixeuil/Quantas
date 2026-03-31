#ifndef CONCRETE_LOGGER_CLIENT_HPP
#define CONCRETE_LOGGER_CLIENT_HPP

#include <chrono>
#include <thread>
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

    static bool sendReport(const ConcreteBootstrapConfig& bootstrap, const json& report) {
        if (!bootstrap.hasLogger()) {
            return false;
        }

        return sendJson(bootstrap, report);
    }

private:
    static bool sendAll(int sock, const std::string& body) {
        size_t totalSent = 0;
        while (totalSent < body.size()) {
            int sent = send(sock,
                            body.c_str() + totalSent,
                            static_cast<int>(body.size() - totalSent),
                            0);
            if (sent <= 0) {
                return false;
            }
            totalSent += static_cast<size_t>(sent);
        }
        return true;
    }

    static bool sendJson(const ConcreteBootstrapConfig& bootstrap, const json& payload) {
        if (!bootstrap.hasLogger()) {
            return false;
        }

        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            return false;
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
        bool sent = false;
        for (int attempt = 0; attempt < 3000 && !sent; ++attempt) {
            if (connect(sock, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) == 0) {
                sent = sendAll(sock, body);
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }

#ifdef _WIN32
        closesocket(sock);
#else
        close(sock);
#endif
        return sent;
    }
};

} // namespace quantas

#endif // CONCRETE_LOGGER_CLIENT_HPP