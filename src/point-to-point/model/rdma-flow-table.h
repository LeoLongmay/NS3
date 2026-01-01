#ifndef NS3_POINT_TO_POINT_RDMA_FLOW_TABLE_H
#define NS3_POINT_TO_POINT_RDMA_FLOW_TABLE_H

#include "ns3/object.h"
#include "ns3/ipv4-address.h"
#include "ns3/simulator.h"

#include <functional>
#include <unordered_map>

namespace std {
template<>
struct hash<ns3::Ipv4Address> {
    size_t operator()(const ns3::Ipv4Address& addr) const noexcept {
        return hash<uint32_t>()(addr.Get());
    }
};
} // namespace std

namespace ns3 {

// Flow Unique Identifier (Primary key: sip+dip+sport+dport)
struct RDMAFlowKey {
    Ipv4Address sip;
    Ipv4Address dip;
    uint16_t sport;
    uint16_t dport;

    bool operator==(const RDMAFlowKey& other) const {
        return sip == other.sip && dip == other.dip &&
               sport == other.sport && dport == other.dport;
    }
};

// Flow Table Entry
struct RDMAFlowEntry {
    RDMAFlowKey key;
    uint64_t last_ts; // The timestamp of the last received packet in the flow
};

class RDMAFlowTable : public Object {
public:
    static TypeId GetTypeId(void);

    RDMAFlowTable();
    RDMAFlowTable(uint64_t inactiveThreshold);
    virtual ~RDMAFlowTable() override = default;

    void SetInactiveThreshold(uint64_t threshold) {
        m_inactiveThreshold = threshold;
    }

    void InsertOrUpdateFlow(Ipv4Address sip, Ipv4Address dip, uint16_t sport, uint16_t dport);

    uint16_t GetFlowCountByDip(Ipv4Address dip) const;

    uint16_t GetFlowCountBySip(Ipv4Address sip) const;

    void CleanInactiveFlows();

    size_t GetTotalFlowCount() const { return m_flowMap.size(); }

private:
    RDMAFlowTable(const RDMAFlowTable&) = delete;
    RDMAFlowTable& operator=(const RDMAFlowTable&) = delete;

    struct FlowKeyHash {
        size_t operator()(const RDMAFlowKey& key) const noexcept {
            std::hash<uint32_t> addrHash;
            std::hash<uint16_t> portHash;

            size_t h1 = addrHash(key.sip.Get());
            size_t h2 = addrHash(key.dip.Get());
            size_t h3 = portHash(key.sport);
            size_t h4 = portHash(key.dport);

            size_t seed = h1;
            seed ^= h2 + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            seed ^= h3 + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            seed ^= h4 + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            return seed;
        }
    };

    std::unordered_map<RDMAFlowKey, RDMAFlowEntry, FlowKeyHash> m_flowMap{};
    std::unordered_map<Ipv4Address, uint32_t> m_dipFlowCount{};
    std::unordered_map<Ipv4Address, uint32_t> m_sipFlowCount{};
    uint64_t m_inactiveThreshold{0};
};

} // namespace ns3

#endif // NS3_POINT_TO_POINT_RDMA_FLOW_TABLE_H