#ifndef CONCRETE_LOGGER_PROTOCOL_HPP
#define CONCRETE_LOGGER_PROTOCOL_HPP

#include <algorithm>
#include <cmath>
#include <fstream>
#include <map>
#include <numeric>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

#include "../Json.hpp"
#include "../Packet.hpp"

namespace quantas {

using nlohmann::json;

class ConcreteLoggerProtocol {
public:
    enum class ReducerKind {
        SumScalar,
        MaxScalar,
        SumNumericArray,
        ResampledMedianArray,
        CollectValues
    };

    struct ReducerSpec {
        ReducerKind kind = ReducerKind::CollectValues;
        size_t targetLength = 0;
        bool trimLeadingZeros = false;
    };

    static json makeDetailedReportBundle(const std::vector<json>& reports) {
        json details;
        details["peerReports"] = json::array();

        std::vector<json> sortedReports = reports;
        std::sort(sortedReports.begin(), sortedReports.end(), [](const json& lhs, const json& rhs) {
            return lhs.value("peerId", NO_PEER_ID) < rhs.value("peerId", NO_PEER_ID);
        });

        for (const json& report : sortedReports) {
            details["peerReports"].push_back(report);
        }

        return details;
    }

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

    static ReducerSpec reducerForMetric(const std::string& peerType,
                                        const std::string& metricKey,
                                        const json& sampleValue,
                                        size_t rounds) {
        if (peerType == "KademliaPeer") {
            if (metricKey == "kademliaAverageHops" || metricKey == "kademliaAverageLatency") {
                return ReducerSpec{ReducerKind::ResampledMedianArray, rounds, true};
            }
            if (metricKey == "kademliaRequestsSatisfied") {
                return ReducerSpec{ReducerKind::ResampledMedianArray, rounds, false};
            }
        }

        if (sampleValue.is_array()) {
            bool numeric = true;
            for (const auto& entry : sampleValue) {
                if (!entry.is_number()) {
                    numeric = false;
                    break;
                }
            }
            return ReducerSpec{numeric ? ReducerKind::SumNumericArray : ReducerKind::CollectValues, 0, false};
        }

        if (sampleValue.is_number()) {
            return ReducerSpec{ReducerKind::SumScalar, 0, false};
        }

        return ReducerSpec{};
    }

    static double median(std::vector<double> values) {
        if (values.empty()) {
            return 0.0;
        }

        std::sort(values.begin(), values.end());
        const size_t middle = values.size() / 2;
        if ((values.size() % 2) == 0) {
            return (values[middle - 1] + values[middle]) / 2.0;
        }
        return values[middle];
    }

    static std::vector<double> resampleNumericArray(const json& value,
                                                    size_t targetLength,
                                                    bool trimLeadingZeros) {
        std::vector<double> source;
        source.reserve(value.size());
        for (const auto& entry : value) {
            source.push_back(entry.get<double>());
        }

        if (source.empty()) {
            return {};
        }

        if (targetLength == 0 || source.size() <= targetLength) {
            if (trimLeadingZeros) {
                auto firstNonZero = std::find_if(source.begin(), source.end(), [](double current) {
                    return std::abs(current) > 1e-12;
                });
                if (firstNonZero != source.end()) {
                    source.erase(source.begin(), firstNonZero);
                }
            }
            return source;
        }

        std::vector<double> result;
        result.reserve(targetLength);
        const size_t sourceSize = source.size();
        for (size_t bucketIndex = 0; bucketIndex < targetLength; ++bucketIndex) {
            size_t start = (bucketIndex * sourceSize) / targetLength;
            size_t end = ((bucketIndex + 1) * sourceSize) / targetLength;
            if (end <= start) {
                end = std::min(sourceSize, start + 1);
            }

            std::vector<double> bucket;
            bucket.reserve(end - start);
            for (size_t sourceIndex = start; sourceIndex < end; ++sourceIndex) {
                bucket.push_back(source[sourceIndex]);
            }
            result.push_back(median(std::move(bucket)));
        }

        if (trimLeadingZeros) {
            auto firstNonZero = std::find_if(result.begin(), result.end(), [](double current) {
                return std::abs(current) > 1e-12;
            });
            if (firstNonZero != result.end()) {
                result.erase(result.begin(), firstNonZero);
            }
        }

        return result;
    }

    static json reduceMetricValues(const std::vector<json>& values,
                                   const ReducerSpec& spec) {
        switch (spec.kind) {
            case ReducerKind::MaxScalar: {
                double maxValue = 0.0;
                bool hasValue = false;
                for (const auto& value : values) {
                    if (!value.is_number()) {
                        continue;
                    }
                    maxValue = hasValue ? std::max(maxValue, value.get<double>()) : value.get<double>();
                    hasValue = true;
                }
                return hasValue ? json(maxValue) : json();
            }
            case ReducerKind::SumScalar: {
                double total = 0.0;
                for (const auto& value : values) {
                    if (value.is_number()) {
                        total += value.get<double>();
                    }
                }
                return total;
            }
            case ReducerKind::SumNumericArray: {
                json reduced = json::array();
                for (const auto& value : values) {
                    if (!value.is_array()) {
                        continue;
                    }
                    while (reduced.size() < value.size()) {
                        reduced.push_back(0.0);
                    }
                    for (size_t index = 0; index < value.size(); ++index) {
                        reduced[index] = reduced[index].get<double>() + value[index].get<double>();
                    }
                }
                return reduced;
            }
            case ReducerKind::ResampledMedianArray: {
                std::vector<std::vector<double>> normalized;
                normalized.reserve(values.size());
                size_t maxLength = 0;
                for (const auto& value : values) {
                    if (!value.is_array()) {
                        continue;
                    }
                    std::vector<double> current = resampleNumericArray(value, spec.targetLength, spec.trimLeadingZeros);
                    maxLength = std::max(maxLength, current.size());
                    normalized.push_back(std::move(current));
                }

                json reduced = json::array();
                for (size_t index = 0; index < maxLength; ++index) {
                    std::vector<double> bucket;
                    for (const auto& series : normalized) {
                        if (index < series.size()) {
                            bucket.push_back(series[index]);
                        }
                    }
                    if (!bucket.empty()) {
                        reduced.push_back(median(std::move(bucket)));
                    }
                }
                return reduced;
            }
            case ReducerKind::CollectValues:
            default: {
                json reduced = json::array();
                for (const auto& value : values) {
                    reduced.push_back(value);
                }
                return reduced;
            }
        }
    }

    static json aggregateReports(const std::vector<json>& reports,
                                 const std::string& peerType,
                                 size_t rounds) {
        json aggregate;
        aggregate["tests"] = json::array();

        std::vector<json> sortedReports = reports;
        std::sort(sortedReports.begin(), sortedReports.end(), [](const json& lhs, const json& rhs) {
            return lhs.value("peerId", NO_PEER_ID) < rhs.value("peerId", NO_PEER_ID);
        });

        bool hasPeakMemory = false;
        double peakMemory = 0.0;
        bool hasRuntime = false;
        double runtime = 0.0;

        std::map<size_t, std::map<std::string, std::vector<json>>> collectedTestMetrics;

        for (const json& report : sortedReports) {
            const json metrics = report.value("metrics", json::object());

            if (metrics.contains("Peak Memory KB") && metrics["Peak Memory KB"].is_number()) {
                peakMemory = std::max(peakMemory, metrics["Peak Memory KB"].get<double>());
                hasPeakMemory = true;
            }
            if (metrics.contains("RunTime") && metrics["RunTime"].is_number()) {
                runtime = std::max(runtime, metrics["RunTime"].get<double>());
                hasRuntime = true;
            }

            if (!metrics.contains("tests") || !metrics["tests"].is_array()) {
                continue;
            }

            const json& tests = metrics["tests"];
            for (size_t testIndex = 0; testIndex < tests.size(); ++testIndex) {
                while (aggregate["tests"].size() <= testIndex) {
                    aggregate["tests"].push_back(json::object());
                }

                for (auto it = tests[testIndex].begin(); it != tests[testIndex].end(); ++it) {
                    collectedTestMetrics[testIndex][it.key()].push_back(it.value());
                }
            }
        }

        for (const auto& testEntry : collectedTestMetrics) {
            const size_t testIndex = testEntry.first;
            while (aggregate["tests"].size() <= testIndex) {
                aggregate["tests"].push_back(json::object());
            }

            for (const auto& metricEntry : testEntry.second) {
                const ReducerSpec spec = reducerForMetric(peerType,
                                                         metricEntry.first,
                                                         metricEntry.second.front(),
                                                         rounds);
                aggregate["tests"][testIndex][metricEntry.first] = reduceMetricValues(metricEntry.second, spec);
            }
        }

        if (hasPeakMemory) {
            aggregate["Peak Memory KB"] = peakMemory;
        }
        if (hasRuntime) {
            aggregate["RunTime"] = runtime;
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