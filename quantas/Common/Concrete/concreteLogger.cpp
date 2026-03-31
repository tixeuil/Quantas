#include <iostream>
#include <algorithm>
#include <vector>

#include "ConcreteBootstrap.hpp"
#include "ConcreteExperiment.hpp"
#include "ConcreteLoggerProtocol.hpp"
#include "ConcreteTopologyPlan.hpp"
#include "ipUtil.hpp"

using namespace quantas;

namespace {
struct LoggerPeerEndpoint {
    interfaceId id = NO_PEER_ID;
    std::string ip;
    int port = -1;

    json jsonify() const {
        return {
            {"id", id},
            {"ip", ip},
            {"port", port}
        };
    }
};

std::string readSocketPayload(int client) {
    std::string payload;
    char buffer[65536];
    while (true) {
#ifdef _WIN32
        int bytes = recv(client, buffer, sizeof(buffer), 0);
#else
        int bytes = read(client, buffer, sizeof(buffer));
#endif
        if (bytes <= 0) {
            break;
        }
        payload.append(buffer, static_cast<size_t>(bytes));
    }
    return payload;
}

bool sendAll(int sock, const std::string& payload) {
    size_t totalSent = 0;
    while (totalSent < payload.size()) {
        int sent = send(sock,
                        payload.c_str() + totalSent,
                        static_cast<int>(payload.size() - totalSent),
                        0);
        if (sent <= 0) {
            return false;
        }
        totalSent += static_cast<size_t>(sent);
    }
    return true;
}
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: ./concrete-logger <experiment.json> <bootstrap.json> [experiment_index]\n";
        return 1;
    }

    size_t experimentIndex = 0;
    if (argc >= 4) {
        experimentIndex = static_cast<size_t>(std::stoul(argv[3]));
    }

    ConcreteExperiment experiment = ConcreteExperiment::load(argv[1], experimentIndex);
    ConcreteBootstrapConfig bootstrap = ConcreteBootstrapConfig::load(argv[2]);
    if (!bootstrap.hasLogger()) {
        std::cerr << "Bootstrap file must define logger.ip and logger.port for concrete-logger.\n";
        return 1;
    }

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        std::cerr << "Failed to create logger socket.\n";
        return 1;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(bootstrap.loggerPort);

    if (bind(server_fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::cerr << "Failed to bind logger port " << bootstrap.loggerPort << ".\n";
        return 1;
    }

    if (listen(server_fd, std::max(experiment.initialPeers(), 128)) < 0) {
        std::cerr << "Failed to listen on logger socket.\n";
        return 1;
    }

    std::vector<LoggerPeerEndpoint> registrations;
    registrations.reserve(static_cast<size_t>(experiment.initialPeers()));
    ConcreteTopologyPlan topologyPlan = ConcreteTopologyPlan::fromTopology(experiment.topology());

    std::vector<json> reports;
    reports.reserve(static_cast<size_t>(experiment.initialPeers()));

    while (registrations.size() < static_cast<size_t>(experiment.initialPeers())) {
        sockaddr_in client_addr{};
        socklen_t addrlen = sizeof(client_addr);
        int client = accept(server_fd, reinterpret_cast<struct sockaddr*>(&client_addr), &addrlen);
        if (client < 0) {
            continue;
        }

        std::string payload = readSocketPayload(client);
    #ifdef _WIN32
        closesocket(client);
    #else
        close(client);
    #endif
        if (payload.empty()) {
            continue;
        }

        try {
            json message = json::parse(payload);
            if (message.value("type", std::string()) == "peer_registration") {
                registrations.push_back(LoggerPeerEndpoint{
                    NO_PEER_ID,
                    message.value("peerIp", std::string()),
                    message.value("peerPort", -1)});
            }
        } catch (const std::exception& ex) {
            std::cerr << "Logger failed to parse registration payload of size "
                      << payload.size() << ": " << ex.what() << std::endl;
        }
    }

    std::sort(registrations.begin(), registrations.end(), [](const LoggerPeerEndpoint& lhs, const LoggerPeerEndpoint& rhs) {
        if (lhs.ip == rhs.ip) {
            return lhs.port < rhs.port;
        }
        return lhs.ip < rhs.ip;
    });

    json peers = json::array();
    std::vector<LoggerPeerEndpoint> assignedPeers;
    assignedPeers.reserve(registrations.size());
    for (size_t slot = 0; slot < registrations.size(); ++slot) {
        LoggerPeerEndpoint assigned{topologyPlan.publicIdForSlot(slot), registrations[slot].ip, registrations[slot].port};
        assignedPeers.push_back(assigned);
        peers.push_back(assigned.jsonify());
    }

    json assignment = ConcreteLoggerProtocol::makePeerAssignment(peers, topologyPlan.toJson());
    for (const LoggerPeerEndpoint& assigned : assignedPeers) {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            continue;
        }

        sockaddr_in peerAddr{};
        peerAddr.sin_family = AF_INET;
        peerAddr.sin_port = htons(assigned.port);
        inet_pton(AF_INET, assigned.ip.c_str(), &peerAddr.sin_addr);

#ifdef __APPLE__
        int set = 1;
        setsockopt(sock, SOL_SOCKET, SO_NOSIGPIPE, &set, sizeof(set));
#endif

        const std::string payload = assignment.dump();
        if (connect(sock, reinterpret_cast<struct sockaddr*>(&peerAddr), sizeof(peerAddr)) == 0) {
            sendAll(sock, payload);
        }

#ifdef _WIN32
        closesocket(sock);
#else
        close(sock);
#endif
    }

    while (reports.size() < static_cast<size_t>(experiment.initialPeers())) {
        sockaddr_in client_addr{};
        socklen_t addrlen = sizeof(client_addr);
        int client = accept(server_fd, reinterpret_cast<struct sockaddr*>(&client_addr), &addrlen);
        if (client < 0) {
            continue;
        }

        std::string payload = readSocketPayload(client);
    #ifdef _WIN32
        closesocket(client);
    #else
        close(client);
    #endif
        if (payload.empty()) {
            continue;
        }

        try {
            json report = json::parse(payload);
            if (report.value("type", std::string()) == "peer_report") {
                reports.push_back(std::move(report));
            }
        } catch (const std::exception& ex) {
            std::cerr << "Logger failed to parse report payload of size "
                      << payload.size() << ": " << ex.what() << std::endl;
        }
    }

    json aggregate = ConcreteLoggerProtocol::aggregateReports(reports,
                                                             experiment.initialPeerType(),
                                                             experiment.rounds());
    json detailed = ConcreteLoggerProtocol::makeDetailedReportBundle(reports);
    detailed["aggregated"] = aggregate;
    detailed["inputFile"] = experiment.inputFile;
    detailed["experimentIndex"] = experimentIndex;
    detailed["initialPeerType"] = experiment.initialPeerType();

    ConcreteLoggerProtocol::writeAggregateFile(bootstrap.loggerOutput, aggregate);
    ConcreteLoggerProtocol::writeAggregateFile(bootstrap.loggerDetailsOutput, detailed);

#ifdef _WIN32
    closesocket(server_fd);
#else
    close(server_fd);
#endif

    return 0;
}