#ifndef NULL_NETWORK_INTERFACE_HPP
#define NULL_NETWORK_INTERFACE_HPP

#include "../NetworkInterface.hpp"

namespace quantas {

class NullNetworkInterface : public NetworkInterface {
public:
    NullNetworkInterface() = default;
    explicit NullNetworkInterface(interfaceId publicId)
        : NetworkInterface(publicId, publicId) {}

    void unicastTo(json, const interfaceId&) override {}
    void receive() override {}
};

} // namespace quantas

#endif // NULL_NETWORK_INTERFACE_HPP