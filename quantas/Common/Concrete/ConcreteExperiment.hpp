#ifndef CONCRETE_EXPERIMENT_HPP
#define CONCRETE_EXPERIMENT_HPP

#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "../Json.hpp"

namespace quantas {

using nlohmann::json;

struct ConcreteExperiment {
    std::string inputFile;
    size_t experimentIndex = 0;
    json root;
    json selectedExperiment;
    std::vector<std::string> algorithms;

    static ConcreteExperiment load(const std::string& inputFile, size_t experimentIndex = 0) {
        std::ifstream in(inputFile);
        if (!in) {
            throw std::runtime_error("Failed to open concrete experiment file: " + inputFile);
        }

        json parsed;
        in >> parsed;
        if (!parsed.contains("experiments") || !parsed["experiments"].is_array()) {
            throw std::runtime_error("Concrete experiment file must contain an experiments array.");
        }

        const auto& experiments = parsed["experiments"];
        if (experimentIndex >= experiments.size()) {
            throw std::runtime_error("Concrete experiment index out of range.");
        }

        ConcreteExperiment experiment;
        experiment.inputFile = inputFile;
        experiment.experimentIndex = experimentIndex;
        experiment.root = std::move(parsed);
        experiment.selectedExperiment = experiment.root["experiments"][experimentIndex];

        if (experiment.root.contains("algorithms") && experiment.root["algorithms"].is_array()) {
            for (const auto& algorithm : experiment.root["algorithms"]) {
                if (algorithm.is_string()) {
                    experiment.algorithms.push_back(algorithm.get<std::string>());
                }
            }
        }

        if (!experiment.selectedExperiment.contains("topology") || !experiment.selectedExperiment["topology"].is_object()) {
            throw std::runtime_error("Concrete experiment must contain a topology object.");
        }

        return experiment;
    }

    json topology() const {
        return selectedExperiment["topology"];
    }

    json parameters() const {
        if (selectedExperiment.contains("parameters") && selectedExperiment["parameters"].is_object()) {
            return selectedExperiment["parameters"];
        }
        return json::object();
    }

    json distribution() const {
        if (selectedExperiment.contains("distribution") && selectedExperiment["distribution"].is_object()) {
            return selectedExperiment["distribution"];
        }
        return json::object();
    }

    std::string initialPeerType() const {
        return topology().value("initialPeerType", std::string());
    }

    int initialPeers() const {
        return topology().value("initialPeers", 0);
    }

    int rounds() const {
        return selectedExperiment.value("rounds", 0);
    }

    int tests() const {
        return selectedExperiment.value("tests", 1);
    }
};

} // namespace quantas

#endif // CONCRETE_EXPERIMENT_HPP