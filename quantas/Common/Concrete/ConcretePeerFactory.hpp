#ifndef CONCRETE_PEER_FACTORY_HPP
#define CONCRETE_PEER_FACTORY_HPP

#include <stdexcept>
#include <string>

#include "NullNetworkInterface.hpp"
#include "NetworkInterfaceConcrete.hpp"

#ifdef QUANTAS_CONCRETE_HAS_ALTBIT_PEER
#include "../../AltBitPeer/AltBitPeer.hpp"
#endif
#ifdef QUANTAS_CONCRETE_HAS_BITCOIN_PEER
#include "../../BitcoinPeer/BitcoinPeer.hpp"
#endif
#ifdef QUANTAS_CONCRETE_HAS_ETHEREUM_PEER
#include "../../EthereumPeer/EthereumPeer.hpp"
#endif
#ifdef QUANTAS_CONCRETE_HAS_EXAMPLE_PEER
#include "../../ExamplePeer/ExamplePeer.hpp"
#endif
#ifdef QUANTAS_CONCRETE_HAS_EXAMPLE_PEER2
#include "../../ExamplePeer/ExamplePeer2.hpp"
#endif
#ifdef QUANTAS_CONCRETE_HAS_KADEMLIA_PEER
#include "../../KademliaPeer/KademliaPeer.hpp"
#endif
#ifdef QUANTAS_CONCRETE_HAS_LINEARCHORD_PEER
#include "../../LinearChordPeer/LinearChordPeer.hpp"
#endif
#ifdef QUANTAS_CONCRETE_HAS_PBFT_PEER
#include "../../PBFTPeer/PBFTPeer.hpp"
#endif
#ifdef QUANTAS_CONCRETE_HAS_RAFT_PEER
#include "../../RaftPeer/RaftPeer.hpp"
#endif
#ifdef QUANTAS_CONCRETE_HAS_STABLEDATALINK_PEER
#include "../../StableDataLinkPeer/StableDataLinkPeer.hpp"
#endif

namespace quantas {

class ConcretePeerFactory {
public:
    static Peer* createActivePeer(const std::string& peerType) {
        return create(peerType, new NetworkInterfaceConcrete());
    }

    static Peer* createShadowPeer(const std::string& peerType, interfaceId publicId) {
        return create(peerType, new NullNetworkInterface(publicId));
    }

private:
    static Peer* create(const std::string& peerType, NetworkInterface* networkInterface) {
#ifdef QUANTAS_CONCRETE_HAS_ALTBIT_PEER
        if (peerType == "AltBitPeer") return new AltBitPeer(networkInterface);
#endif
#ifdef QUANTAS_CONCRETE_HAS_BITCOIN_PEER
        if (peerType == "BitcoinPeer") return new BitcoinPeer(networkInterface);
#endif
#ifdef QUANTAS_CONCRETE_HAS_ETHEREUM_PEER
        if (peerType == "EthereumPeer") return new EthereumPeer(networkInterface);
#endif
#ifdef QUANTAS_CONCRETE_HAS_EXAMPLE_PEER
        if (peerType == "ExamplePeer") return new ExamplePeer(networkInterface);
#endif
#ifdef QUANTAS_CONCRETE_HAS_EXAMPLE_PEER2
        if (peerType == "ExamplePeer2") return new ExamplePeer2(networkInterface);
#endif
#ifdef QUANTAS_CONCRETE_HAS_KADEMLIA_PEER
        if (peerType == "KademliaPeer") return new KademliaPeer(networkInterface);
#endif
#ifdef QUANTAS_CONCRETE_HAS_LINEARCHORD_PEER
        if (peerType == "LinearChordPeer") return new LinearChordPeer(networkInterface);
#endif
#ifdef QUANTAS_CONCRETE_HAS_PBFT_PEER
        if (peerType == "PBFTPeer") return new PBFTPeer(networkInterface);
#endif
#ifdef QUANTAS_CONCRETE_HAS_RAFT_PEER
        if (peerType == "RaftPeer") return new RaftPeer(networkInterface);
#endif
#ifdef QUANTAS_CONCRETE_HAS_STABLEDATALINK_PEER
        if (peerType == "StableDataLinkPeer") return new StableDataLinkPeer(networkInterface);
#endif

        delete networkInterface;
        throw std::runtime_error("Unsupported concrete peer type: " + peerType);
    }
};

} // namespace quantas

#endif // CONCRETE_PEER_FACTORY_HPP