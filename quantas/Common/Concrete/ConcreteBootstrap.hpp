#ifndef CONCRETE_BOOTSTRAP_HPP
#define CONCRETE_BOOTSTRAP_HPP

#include <fstream>
#include <stdexcept>
#include <string>

#include "../Json.hpp"

namespace quantas {

using nlohmann::json;

struct ConcreteBootstrapConfig {
    std::string loggerIp;
    int loggerPort = -1;
    std::string loggerOutput = "concrete-aggregate.json";
    std::string loggerDetailsOutput;

    static std::string defaultDetailsOutputPath(const std::string& aggregateOutput) {
        const std::string suffix = ".json";
        if (aggregateOutput.size() >= suffix.size() &&
            aggregateOutput.compare(aggregateOutput.size() - suffix.size(), suffix.size(), suffix) == 0) {
            return aggregateOutput.substr(0, aggregateOutput.size() - suffix.size()) + "-details" + suffix;
        }
        return aggregateOutput + "-details.json";
    }

    static ConcreteBootstrapConfig load(const std::string& bootstrapFile) {
        std::ifstream in(bootstrapFile);
        if (!in) {
            throw std::runtime_error("Failed to open concrete bootstrap file: " + bootstrapFile);
        }

        json parsed;
        in >> parsed;
        ConcreteBootstrapConfig config;
        if (!parsed.contains("logger") || !parsed["logger"].is_object()) {
            throw std::runtime_error("Concrete bootstrap file must contain a logger object.");
        }

        const json& logger = parsed["logger"];
        config.loggerIp = logger.value("ip", std::string());
        config.loggerPort = logger.value("port", -1);
        config.loggerOutput = logger.value("output", config.loggerOutput);
        config.loggerDetailsOutput = logger.value("detailsOutput", defaultDetailsOutputPath(config.loggerOutput));

        if (config.loggerIp.empty() || config.loggerPort < 0) {
            throw std::runtime_error("Concrete bootstrap logger must define ip and port.");
        }

        return config;
    }

    bool hasLogger() const {
        return !loggerIp.empty() && loggerPort >= 0;
    }
};

} // namespace quantas

#endif // CONCRETE_BOOTSTRAP_HPP