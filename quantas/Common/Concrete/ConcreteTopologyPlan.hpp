#ifndef CONCRETE_TOPOLOGY_PLAN_HPP
#define CONCRETE_TOPOLOGY_PLAN_HPP

#include <algorithm>
#include <map>
#include <random>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include "../Json.hpp"
#include "../Packet.hpp"

namespace quantas {

struct ConcreteTopologyPlan {
    std::vector<interfaceId> publicIdsBySlot;
    std::map<interfaceId, std::set<interfaceId>> adjacency;

    static ConcreteTopologyPlan fromTopology(const json& topology) {
        ConcreteTopologyPlan plan;

        const int initialPeers = topology.value("initialPeers", 0);
        if (initialPeers <= 0) {
            throw std::runtime_error("Concrete topology requires initialPeers > 0.");
        }

        plan.publicIdsBySlot.reserve(static_cast<size_t>(initialPeers));
        for (int index = 0; index < initialPeers; ++index) {
            plan.publicIdsBySlot.push_back(static_cast<interfaceId>(index));
        }

        if (topology.value("identifiers", std::string()) == "random") {
            std::mt19937 rng(std::random_device{}());
            std::shuffle(plan.publicIdsBySlot.begin(), plan.publicIdsBySlot.end(), rng);
        }

        for (interfaceId publicId : plan.publicIdsBySlot) {
            plan.adjacency[publicId];
        }

        const auto addDirectedEdge = [&](int fromSlot, int toSlot) {
            if (fromSlot < 0 || toSlot < 0 || fromSlot >= initialPeers || toSlot >= initialPeers) {
                return;
            }
            interfaceId fromId = plan.publicIdsBySlot[static_cast<size_t>(fromSlot)];
            interfaceId toId = plan.publicIdsBySlot[static_cast<size_t>(toSlot)];
            plan.adjacency[fromId].insert(toId);
        };

        const auto addUndirectedEdge = [&](int lhsSlot, int rhsSlot) {
            addDirectedEdge(lhsSlot, rhsSlot);
            addDirectedEdge(rhsSlot, lhsSlot);
        };

        const std::string topologyType = topology.value("type", std::string());
        if (topologyType == "complete") {
            for (int left = 0; left < initialPeers; ++left) {
                for (int right = left + 1; right < initialPeers; ++right) {
                    addUndirectedEdge(left, right);
                }
            }
        } else if (topologyType == "star") {
            for (int index = 1; index < initialPeers; ++index) {
                addUndirectedEdge(0, index);
            }
        } else if (topologyType == "grid") {
            const int height = topology.value("height", 1);
            const int width = topology.value("width", 1);
            if (height * width != initialPeers) {
                throw std::runtime_error("Concrete grid topology requires height * width == initialPeers.");
            }
            for (int row = 0; row < height; ++row) {
                for (int col = 0; col < width; ++col) {
                    const int index = row * width + col;
                    if (col + 1 < width) addUndirectedEdge(index, index + 1);
                    if (row + 1 < height) addUndirectedEdge(index, index + width);
                }
            }
        } else if (topologyType == "torus") {
            const int height = topology.value("height", 1);
            const int width = topology.value("width", 1);
            if (height * width != initialPeers) {
                throw std::runtime_error("Concrete torus topology requires height * width == initialPeers.");
            }
            for (int row = 0; row < height; ++row) {
                for (int col = 0; col < width; ++col) {
                    const int index = row * width + col;
                    addUndirectedEdge(index, row * width + ((col + 1) % width));
                    addUndirectedEdge(index, ((row + 1) % height) * width + col);
                }
            }
        } else if (topologyType == "chain") {
            for (int index = 0; index + 1 < initialPeers; ++index) {
                addUndirectedEdge(index, index + 1);
            }
        } else if (topologyType == "ring") {
            for (int index = 0; index + 1 < initialPeers; ++index) {
                addUndirectedEdge(index, index + 1);
            }
            if (initialPeers > 1) {
                addUndirectedEdge(initialPeers - 1, 0);
            }
        } else if (topologyType == "unidirectionalRing") {
            for (int index = 0; index + 1 < initialPeers; ++index) {
                addDirectedEdge(index, index + 1);
            }
            if (initialPeers > 1) {
                addDirectedEdge(initialPeers - 1, 0);
            }
        } else if (topologyType == "userList") {
            if (!topology.contains("list") || !topology["list"].is_object()) {
                throw std::runtime_error("Concrete userList topology requires a list object.");
            }
            const json& list = topology["list"];
            for (int index = 0; index < initialPeers; ++index) {
                const std::string key = std::to_string(index);
                if (!list.contains(key) || !list[key].is_array()) {
                    continue;
                }
                for (const auto& value : list[key]) {
                    if (value.is_number_integer()) {
                        addDirectedEdge(index, value.get<int>());
                    }
                }
            }
        } else {
            throw std::runtime_error("Unsupported concrete topology type: " + topologyType);
        }

        return plan;
    }

    static ConcreteTopologyPlan fromJson(const json& encodedPlan) {
        ConcreteTopologyPlan plan;
        if (encodedPlan.contains("publicIds") && encodedPlan["publicIds"].is_array()) {
            for (const auto& value : encodedPlan["publicIds"]) {
                if (value.is_number_integer()) {
                    plan.publicIdsBySlot.push_back(value.get<interfaceId>());
                }
            }
        }

        if (encodedPlan.contains("neighbors") && encodedPlan["neighbors"].is_object()) {
            for (auto it = encodedPlan["neighbors"].begin(); it != encodedPlan["neighbors"].end(); ++it) {
                interfaceId publicId = static_cast<interfaceId>(std::stol(it.key()));
                std::set<interfaceId> neighbors;
                if (it.value().is_array()) {
                    for (const auto& neighbor : it.value()) {
                        if (neighbor.is_number_integer()) {
                            neighbors.insert(neighbor.get<interfaceId>());
                        }
                    }
                }
                plan.adjacency[publicId] = std::move(neighbors);
            }
        }

        return plan;
    }

    json toJson() const {
        json encoded;
        encoded["publicIds"] = publicIdsBySlot;
        for (const auto& entry : adjacency) {
            encoded["neighbors"][std::to_string(entry.first)] = json::array();
            for (interfaceId neighbor : entry.second) {
                encoded["neighbors"][std::to_string(entry.first)].push_back(neighbor);
            }
        }
        return encoded;
    }

    size_t peerCount() const {
        return publicIdsBySlot.size();
    }

    interfaceId publicIdForSlot(size_t slot) const {
        return publicIdsBySlot.at(slot);
    }

    std::set<interfaceId> neighborsFor(interfaceId publicId) const {
        auto found = adjacency.find(publicId);
        if (found == adjacency.end()) {
            return {};
        }
        return found->second;
    }
};

} // namespace quantas

#endif // CONCRETE_TOPOLOGY_PLAN_HPP