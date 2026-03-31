#include <iostream>
#include <vector>

#include "../Peer.hpp"
#include "ConcreteLoggerClient.hpp"
#include "ConcreteLoggerProtocol.hpp"
#include "ConcretePeerFactory.hpp"
#include "ConcreteExperiment.hpp"
#include "NetworkInterfaceConcrete.hpp"
#include "../RoundManager.hpp"
#include "../memoryUtil.hpp"

using namespace quantas;

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: ./peer <experiment.json> <bootstrap.json> [port] [experiment_index]\n";
        return 1;
    }

    std::chrono::time_point<std::chrono::high_resolution_clock> startTime, endTime; // chrono time points
    std::chrono::duration<double> duration; // chrono time interval
    startTime = std::chrono::high_resolution_clock::now();

    RoundManager::asynchronous();

    int port = -1;
    size_t experimentIndex = 0;
    if (argc >= 4) {
        port = std::stoi(argv[3]);
    }
    if (argc >= 5) {
        experimentIndex = static_cast<size_t>(std::stoul(argv[4]));
    }

    ConcreteExperiment experiment = ConcreteExperiment::load(argv[1], experimentIndex);
    RoundManager::setLastRound(RoundManager::currentRound() + 10000);
    
    Peer* peer = ConcretePeerFactory::createActivePeer(experiment.initialPeerType());
    std::vector<Peer*> peer_vector;
    if (auto networkInterface = dynamic_cast<NetworkInterfaceConcrete*>(peer->getNetworkInterface())) {
        networkInterface->load_config(experiment, argv[2], port);

        const ConcreteTopologyPlan& topologyPlan = networkInterface->getTopologyPlan();
        peer_vector.resize(topologyPlan.peerCount(), nullptr);
        for (size_t slot = 0; slot < topologyPlan.peerCount(); ++slot) {
            interfaceId publicId = topologyPlan.publicIdsBySlot[slot];
            if (publicId == peer->publicId()) {
                peer_vector[slot] = peer;
                continue;
            }

            Peer* shadowPeer = ConcretePeerFactory::createShadowPeer(experiment.initialPeerType(), publicId);
            for (interfaceId neighbor : topologyPlan.neighborsFor(publicId)) {
                shadowPeer->addNeighbor(neighbor);
            }
            peer_vector[slot] = shadowPeer;
        }

        peer->initParameters(peer_vector, experiment.parameters());
        int counter = 9000;
        while(!networkInterface->getShutdownCondition()) {
            if (!peer->isCrashed()) {
                peer->receive();
                peer->tryPerformComputation();
            }
            
            if (int(RoundManager::lastRound()) - int(RoundManager::currentRound()) < counter) {
                std::cout << 10 - (counter / 1000) << " seconds passed." << std::endl;
                counter -= 1000;
            }

            if (RoundManager::lastRound() <= RoundManager::currentRound()) {
                networkInterface->shutDown();
                peer->endOfRound(peer_vector);
            } else {
                peer->endOfRound(peer_vector);
            }
        }
    }

    if (peer_vector.empty()) {
        peer_vector.push_back(peer);
    }

    endTime = std::chrono::high_resolution_clock::now();
   	duration = endTime - startTime;
    LogWriter::setValue("RunTime", double(duration.count()));
    size_t peakMemoryKB = getPeakMemoryKB();
    LogWriter::setValue("Peak Memory KB", peakMemoryKB);

    if (auto networkInterface = dynamic_cast<NetworkInterfaceConcrete*>(peer->getNetworkInterface())) {
        json report = ConcreteLoggerProtocol::makePeerReport(
            peer->publicId(),
            experiment.initialPeerType(),
            experiment.inputFile,
            experiment.experimentIndex,
            LogWriter::snapshot());
        ConcreteLoggerClient::sendReport(networkInterface->getBootstrapConfig(), report);
    }

    LogWriter::print();

    for (Peer* peerPtr : peer_vector) {
        if (peerPtr == nullptr) {
            continue;
        }
        peerPtr->clearInterface();
        delete peerPtr;
    }

    return 0;
}
