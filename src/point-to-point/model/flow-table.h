#include "ns3/ipv4-address.h"
#include "ns3/simulator.h"

#include <unordered_map>
#include <functional>

namespace ns3 {

// Flow Unique Identifier (Primary key: sip+dip+sport+dport)
struct FlowKey {
    Ipv4Address sip;
    Ipv4Address dip;
    uint16_t sport;
    uint16_t dport;

    bool operator==(const FlowKey& other) const {
        return sip == other.sip && dip == other.dip &&
               sport == other.sport && dport == other.dport;
    }
};

// Flow Table Entry
struct FlowEntry {
    FlowKey key;
    uint64_t last_ts; // The timestamp of the last received packet in the flow
};

class FlowTable {
public:
    /**
     * @brief Constructor
     * @param inactiveThreshold
     */
    FlowTable(uint64_t inactiveThreshold);

    void InsertOrUpdateFlow(Ipv4Address sip, Ipv4Address dip, uint16_t sport, uint16_t dport);

    uint32_t GetFlowCountByDip(Ipv4Address dip) const;

    uint32_t GetFlowCountBySip(Ipv4Address sip) const;

    void CleanInactiveFlows();

    size_t GetTotalFlowCount() const { return m_flowMap.size(); }

private:
    std::unordered_map<FlowKey, FlowEntry> m_flowMap;
    std::unordered_map<Ipv4Address, uint32_t> m_dipFlowCount;
    std::unordered_map<Ipv4Address, uint32_t> m_sipFlowCount;
    uint64_t m_inactiveThreshold;
};

} // namespace ns3

namespace std {

template<>
struct hash<ns3::Ipv4Address> {
    size_t operator()(const ns3::Ipv4Address& addr) const {
        return hash<uint32_t>()(addr.Get());
    }
};

// Hash for FlowKey
template<>
struct hash<ns3::FlowKey> {
    size_t operator()(const ns3::FlowKey& key) const {
        size_t h1 = std::hash<uint32_t>()(key.sip.Get());
        size_t h2 = std::hash<uint32_t>()(key.dip.Get());
        size_t h3 = std::hash<uint16_t>()(key.sport);
        size_t h4 = std::hash<uint16_t>()(key.dport);
        
        // 优化哈希组合（避免简单异或的碰撞问题，可选但推荐）
        size_t result = h1;
        result ^= h2 + 0x9e3779b9 + (result << 6) + (result >> 2);
        result ^= h3 + 0x9e3779b9 + (result << 6) + (result >> 2);
        result ^= h4 + 0x9e3779b9 + (result << 6) + (result >> 2);
        return result;
    }
};

} // namespace std