#ifndef NETWORK_INTERFACE_CONCRETE_HPP
#define NETWORK_INTERFACE_CONCRETE_HPP

#include <memory>
#include <map>
#include <atomic>
#include <set>
#include <deque>
#include <vector>
#include <string>
#include <climits>
#include <algorithm>
#include <mutex>
#include <iostream>
#include <fstream>
#include "../LogWriter.hpp"
#include "../Packet.hpp"
#include "../NetworkInterface.hpp"
#include "../RandomUtil.hpp"
#include "ConcreteBootstrap.hpp"
#include "ConcreteExperiment.hpp"
#include "ConcreteLoggerClient.hpp"
#include "ConcreteTopologyPlan.hpp"
#include "ipUtil.hpp"
#include "../Json.hpp"
#include "../BS_thread_pool.hpp"

namespace quantas {

class NeighborInfo {
public:
    NeighborInfo() {};
    NeighborInfo(interfaceId Nid, std::string Nip, int Nport) : id(Nid), ip(Nip), port(Nport) {};
    interfaceId id{NO_PEER_ID};
    std::string ip;
    int port{-1};
    json jsonify() const {
        return {
            {"id", id},
            {"ip", ip},
            {"port", port}
        };
    }
};

class NetworkInterfaceConcrete : public NetworkInterface {
private:
    struct ConcreteDistributionProperties {
        double dropProbability = 0.0;
        double reorderProbability = 0.0;
        double duplicateProbability = 0.0;
        int maxMsgsRec = 1;
        int size = INT_MAX;
        int avgDelay = 1;
        int minDelay = 1;
        int maxDelay = 1;
        std::string type = "UNIFORM";

        void setParameters(const json& params) {
            dropProbability = params.value("dropProbability", 0.0);
            reorderProbability = params.value("reorderProbability", 0.0);
            duplicateProbability = params.value("duplicateProbability", 0.0);
            maxMsgsRec = params.value("maxMsgsRec", 1);
            size = params.value("size", INT_MAX);
            avgDelay = params.value("avgDelay", 1);
            minDelay = params.value("minDelay", 1);
            maxDelay = params.value("maxDelay", 1);
            type = params.value("type", std::string("UNIFORM"));
        }

        int computeDelay() const {
            if (type == "POISSON") {
                int delay = poissonInt(avgDelay);
                return std::clamp(delay, minDelay, maxDelay);
            }
            if (type == "ONE") {
                return 1;
            }
            return uniformInt(minDelay, maxDelay);
        }
    };

    int my_port = -1;
    int total_peers = -1;
    std::atomic<bool> shutdown_condition = false;
    int server_fd = -1;
    std::string my_ip;
    ConcreteBootstrapConfig bootstrap;
    ConcreteTopologyPlan topologyPlan;

    std::mutex concrete_mtx;
    std::thread listener; // the thread for receiving messages
    BS::thread_pool pool{1}; // currently only 1 thread is allowed to exist for sending messages

    std::map<interfaceId, NeighborInfo> all_peers;
    ConcreteDistributionProperties distribution;
    int throughput_left = INT_MAX;
    std::deque<Packet> channel_queue;
    std::mutex channel_mtx;

    void configure_distribution(const json& distributionParams) {
        distribution.setParameters(distributionParams);
        int roundsLeft = static_cast<int>(RoundManager::lastRound() - RoundManager::currentRound());
        if (roundsLeft < 1) {
            roundsLeft = 1;
        }
        throughput_left = distribution.maxMsgsRec * roundsLeft;
    }

    bool can_queue_packet_locked() const {
        return throughput_left != 0 && static_cast<int>(channel_queue.size()) < distribution.size;
    }

    void consume_throughput_locked() {
        if (throughput_left > 0) {
            --throughput_left;
        }
    }

    void enqueue_received_message(interfaceId sender, const json& body) {
        std::lock_guard<std::mutex> channelLock(channel_mtx);
        if (trueWithProbability(distribution.dropProbability)) {
            return;
        }

        bool duplicate = false;
        do {
            duplicate = false;
            if (!can_queue_packet_locked()) {
                return;
            }

            consume_throughput_locked();

            Packet packet(_publicId, sender, body);
            const int delay = distribution.computeDelay();
            packet.setDelay(delay, delay);
            channel_queue.push_back(std::move(packet));

            duplicate = trueWithProbability(distribution.duplicateProbability);
        } while (duplicate);
    }

    void apply_topology_neighbors() {
        _neighbors.clear();
        for (interfaceId neighbor : topologyPlan.neighborsFor(_publicId)) {
            addNeighbor(neighbor);
        }
    }

    // Use a thread pool or message dispatch queue so you don’t spawn hundreds of threads.
    void send_json(const std::string& ip, int port, const json& jmsg, bool async = true) {
        auto task = [=]() {
            if (shutdown_condition) return;
            int sock = socket(AF_INET, SOCK_STREAM, 0);
            sockaddr_in addr{};
            addr.sin_family = AF_INET;
            addr.sin_port = htons(port);
            inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);

            #ifdef __APPLE__
                // macOS: disable SIGPIPE per-socket
                int set = 1;
                setsockopt(sock, SOL_SOCKET, SO_NOSIGPIPE, &set, sizeof(set));
            #endif
    
            while (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0 && !shutdown_condition) {
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
                // std::cerr << "send to " << ip << " " << port  << " failed, retrying." << std::endl;
            }
    
            std::string msg = jmsg.dump();
            send(sock, msg.c_str(), static_cast<int>(msg.size()), 0);
    
            #ifdef _WIN32
                closesocket(sock);
            #else
                close(sock);
            #endif
        };
    
        if (async) {
            pool.push_task(task);
        } else {
            task();
        }
    }

    void send_registration() {
        ConcreteLoggerClient::sendRegistration(bootstrap, my_ip, my_port);
    }

    void wait_for_peer_list() {
        while (true) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            std::lock_guard<std::mutex> lock(concrete_mtx);
            if (all_peers.size() == static_cast<size_t>(total_peers) && _publicId != NO_PEER_ID) break;
        }
    }

    void start_listener() {
        server_fd = socket(AF_INET, SOCK_STREAM, 0);

        if (server_fd < 0) {
            perror("socket creation failed");
            return;
        }
        int opt = 1;

        #ifdef _WIN32
            // On Windows, SO_REUSEADDR allows rebinding if no active listener exists.
            // SO_EXCLUSIVEADDRUSE ensures only one process can bind (strong safety)
            setsockopt(server_fd, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (const char*)&opt, sizeof(opt));
        #else
            // On Linux/macOS, SO_REUSEADDR allows rebinding during TIME_WAIT
            setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        #endif

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(my_port);

        bind(server_fd, (struct sockaddr*)&addr, sizeof(addr));
        listen(server_fd, 8);

        // std::cout << _publicId << " listening on ip " << my_ip << " port " << my_port << std::endl;

        while (!shutdown_condition) {
            sockaddr_in client_addr{};
            socklen_t addrlen = sizeof(client_addr);
            int new_socket = accept(server_fd, (struct sockaddr*)&client_addr, &addrlen);
            if (shutdown_condition) break;  // clean exit
            if (new_socket < 0) {
                perror("accept failed");
                #ifdef _WIN32
                    closesocket(new_socket);
                #else
                    close(new_socket);
                #endif
                continue;  // skip this loop iteration
            }

            char buffer[2048] = {0};

            #ifdef _WIN32
                int bytes = recv(new_socket, buffer, sizeof(buffer), 0);
            #else
                int bytes = read(new_socket, buffer, sizeof(buffer));
            #endif

            if (bytes <= 0) {
                if (bytes == 0) {
                    std::cerr << "[RECV] Connection closed by peer.\n";
                } else {
                    perror("read/recv failed");
                }
                #ifdef _WIN32
                    closesocket(new_socket);
                #else
                    close(new_socket);
                #endif
                continue;
            }
            std::string raw_msg(buffer, bytes);

            try {
                json msg = json::parse(raw_msg);
                std::string type = msg.value("type", "");                

                if (type == "peer_assignment") {
                    std::lock_guard<std::mutex> lock(concrete_mtx);
                    topologyPlan = ConcreteTopologyPlan::fromJson(msg.value("topologyPlan", json::object()));
                    all_peers.clear();
                    for (const auto& item : msg["peers"]) {
                        NeighborInfo newNeighbor(item["id"], item["ip"], item["port"]);
                        all_peers.insert({item["id"], newNeighbor});
                        if (item["ip"] == my_ip && item["port"] == my_port) {
                            _publicId = item["id"];
                        }
                    }
                    apply_topology_neighbors();
                } else if (type == "message") {
                    interfaceId sender = msg.value("from_id", -1);
                    if (sender == -1) continue;
                    enqueue_received_message(sender, msg["body"]);
                }
            } catch (...) {}
            #ifdef _WIN32
                closesocket(new_socket);
            #else
                close(new_socket);
            #endif
        }

        // std::cout << _publicId << " stopped listening on ip " << my_ip << " port " << my_port << std::endl;

        #ifdef _WIN32
            closesocket(server_fd);
        #else
            close(server_fd);
        #endif
    }

    void start() {
        #ifdef _WIN32
            WSADATA wsaData;
            WSAStartup(MAKEWORD(2,2), &wsaData);
        #endif

        install_socket_safety();

        listener = std::thread(&NetworkInterfaceConcrete::start_listener, this);

        send_registration();
        wait_for_peer_list();

        LogWriter::setLogFile("peer_" + std::to_string(_publicId) + ".log");
        LogWriter::setTest(0);
    }

public:

    inline NetworkInterfaceConcrete() {};
    inline NetworkInterfaceConcrete(interfaceId pubId) : NetworkInterface(pubId, pubId) {};
    inline NetworkInterfaceConcrete(interfaceId pubId, interfaceId internalId) : NetworkInterface(pubId, internalId) {};
    inline ~NetworkInterfaceConcrete() {};

    bool getShutdownCondition() const {
        return shutdown_condition;
    }

    const ConcreteTopologyPlan& getTopologyPlan() const {
        return topologyPlan;
    }

    const ConcreteBootstrapConfig& getBootstrapConfig() const {
        return bootstrap;
    }

    void shutDown() {
        shutdown_condition = true;
        if (server_fd != -1) {
            #ifdef _WIN32
                closesocket(server_fd);
            #else
                close(server_fd);
            #endif

            // Wake up accept() with a dummy connection
            int dummy = socket(AF_INET, SOCK_STREAM, 0);
            sockaddr_in addr{};
            addr.sin_family = AF_INET;
            addr.sin_port = htons(my_port);
            inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);  // connect to self

            connect(dummy, (sockaddr*)&addr, sizeof(addr));
            #ifdef _WIN32
                closesocket(dummy);
            #else
                close(dummy);
            #endif
        }
    }

    void load_config(const ConcreteExperiment& experiment, const std::string& bootstrapPath, int port = -1) {
        my_ip = get_local_ip();
        bootstrap = ConcreteBootstrapConfig::load(bootstrapPath);
        topologyPlan = ConcreteTopologyPlan::fromTopology(experiment.topology());
        total_peers = experiment.initialPeers();

        if (total_peers <= 0) {
            throw std::runtime_error("Concrete experiment requires topology.initialPeers > 0.");
        }

        if (port == -1) {
            my_port = get_unused_port();
        } else {
            my_port = port;
        }

        if (!can_bind_port(my_port)) {
            throw std::runtime_error("Failed to bind to port " + std::to_string(my_port));
        }
        start();
    }

    // Send messages to to others using this
    inline void unicastTo (json msg, const interfaceId& dest) override;
    
    inline void receive() override {
        std::vector<Packet> delivered;
        {
            std::lock_guard<std::mutex> channelLock(channel_mtx);
            if (channel_queue.size() > 1 && trueWithProbability(distribution.reorderProbability)) {
                std::shuffle(channel_queue.begin(), channel_queue.end(), threadLocalEngine());
            }

            int deliveredCount = 0;
            while (!channel_queue.empty() && channel_queue.front().hasArrived() && deliveredCount < distribution.maxMsgsRec) {
                delivered.push_back(std::move(channel_queue.front()));
                channel_queue.pop_front();
                ++deliveredCount;
            }
        }

        if (!delivered.empty()) {
            std::lock_guard<std::mutex> streamLock(_inStream_mtx);
            for (Packet& packet : delivered) {
                _inStream.push_back(std::move(packet));
            }
        }
    };

    inline void clearAll() override {
        #ifdef _WIN32
            WSACleanup();
        #endif
        if (listener.joinable()) {
            listener.join();
        }
        pool.wait_for_tasks();
        _inStream.clear();
        _neighbors.clear();
        all_peers.clear();
        channel_queue.clear();
    }
};

void NetworkInterfaceConcrete::unicastTo(json msg, const interfaceId& nbr) {
    if (_neighbors.find(nbr) == _neighbors.end()) return;
    json newMsg = {
        {"type", "message"},
        {"from_id", _publicId},
        {"body", msg}
    };
    send_json(all_peers[nbr].ip, all_peers[nbr].port, newMsg);
}

}

#endif /* NETWORK_INTERFACE_CONCRETE_HPP */