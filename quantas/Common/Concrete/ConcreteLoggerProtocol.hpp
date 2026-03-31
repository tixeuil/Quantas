#ifndef CONCRETE_LOGGER_PROTOCOL_HPP
#define CONCRETE_LOGGER_PROTOCOL_HPP

#include <algorithm>
#include <fstream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

#include "../Json.hpp"
#include "../Packet.hpp"

namespace quantas {

using nlohmann::json;

class ConcreteLoggerProtocol {
public:
    static json makePeerRegistration(const std::string& peerIp, int peerPort) {
        return {
            {"type", "peer_registration"},
            {"peerIp", peerIp},
            {"peerPort", peerPort}
        };
    }

    static json makePeerAssignment(const json& peers, const json& topologyPlan) {
        return {
            {"type", "peer_assignment"},
            {"peers", peers},
            {"topologyPlan", topologyPlan}
        };
    }

    static json makePeerReport(interfaceId peerId,
                               const std::string& peerType,
                               const std::string& inputFile,
                               size_t experimentIndex,
                               const json& metrics) {
        return {
            {"type", "peer_report"},
            {"peerId", peerId},
            {"peerType", peerType},
            {"inputFile", inputFile},
            {"experimentIndex", experimentIndex},
            {"metrics", metrics}
        };
    }

    static json aggregateReports(const std::vector<json>& reports) {
        json aggregate;
        aggregate["peerReports"] = json::array();
        aggregate["aggregated"]["tests"] = json::array();

        std::vector<json> sortedReports = reports;
        std::sort(sortedReports.begin(), sortedReports.end(), [](const json& lhs, const json& rhs) {
            return lhs.value("peerId", NO_PEER_ID) < rhs.value("peerId", NO_PEER_ID);
        });

        for (const json& report : sortedReports) {
            aggregate["peerReports"].push_back(report);

            const json metrics = report.value("metrics", json::object());
            if (!metrics.contains("tests") || !metrics["tests"].is_array()) {
                continue;
            }

            const json& tests = metrics["tests"];
            for (size_t testIndex = 0; testIndex < tests.size(); ++testIndex) {
                while (aggregate["aggregated"]["tests"].size() <= testIndex) {
                    aggregate["aggregated"]["tests"].push_back(json::object());
                }

                for (auto it = tests[testIndex].begin(); it != tests[testIndex].end(); ++it) {
                    const std::string key = it.key();
                    const json& value = it.value();
                    json& bucket = aggregate["aggregated"]["tests"][testIndex][key];
                    bucket["perPeer"][std::to_string(report.value("peerId", NO_PEER_ID))] = value;

                    if (value.is_array()) {
                        double total = 0.0;
                        bool numeric = true;
                        for (const auto& entry : value) {
                            if (entry.is_number()) {
                                total += entry.get<double>();
                            } else {
                                numeric = false;
                            }
                        }
                        if (numeric) {
                            bucket["numericArraySum"] = bucket.value("numericArraySum", 0.0) + total;
                        }
                    } else if (value.is_number()) {
                        bucket["numericSum"] = bucket.value("numericSum", 0.0) + value.get<double>();
                    }
                }
            }
        }

        return aggregate;
    }

    static void writeAggregateFile(const std::string& outputPath, const json& aggregate) {
        std::ofstream out(outputPath);
        if (!out) {
            throw std::runtime_error("Failed to open aggregate output file: " + outputPath);
        }
        out << aggregate.dump(4) << std::endl;
    }
};

} // namespace quantas

#endif // CONCRETE_LOGGER_PROTOCOL_HPP