#ifndef NS3_POINT_TO_POINT_RDMA_FLOW_TABLE_H
#define NS3_POINT_TO_POINT_RDMA_FLOW_TABLE_H

#include "ns3/object.h"
#include "ns3/ipv4-address.h"
#include "ns3/simulator.h"

#include <functional>
#include <list>
#include <unordered_map>
#include <vector>

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

struct RDMAPortQueueKey {
    uint32_t port;
    uint32_t qIndex;

    bool operator==(const RDMAPortQueueKey& other) const {
        return port == other.port && qIndex == other.qIndex;
    }
};

struct RDMAEgressFlowKey {
    RDMAFlowKey flow;
    uint32_t port;
    uint32_t qIndex;
    uint16_t pg;

    bool operator==(const RDMAEgressFlowKey& other) const {
        return flow == other.flow && port == other.port && qIndex == other.qIndex && pg == other.pg;
    }
};

// Flow Table Entry (ingress). Includes an LRU-list iterator so eviction is O(1) per expired flow.
struct RDMAFlowEntry {
    RDMAFlowKey key;
    uint64_t last_ts; // The timestamp of the last received packet in the flow
    uint64_t last_pkt_bytes; // Last observed packet size on this flow
    double ewma_rate_bps; // Smoothed per-flow rate estimate
    std::list<RDMAFlowKey>::iterator lru_it;
};

// Egress flow table entry — separate type because its LRU list holds RDMAEgressFlowKey.
struct RDMAEgressFlowEntry {
    RDMAFlowKey flow;
    uint64_t last_ts;
    uint64_t last_pkt_bytes;
    double ewma_rate_bps;
    std::list<RDMAEgressFlowKey>::iterator lru_it;
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
    void InsertOrUpdateFlowOnEgress(Ipv4Address sip, Ipv4Address dip, uint16_t sport, uint16_t dport, uint32_t port, uint32_t qIndex, uint16_t pg, uint32_t pktBytes);

    uint16_t GetFlowCountByDip(Ipv4Address dip) const;

    uint16_t GetFlowCountBySip(Ipv4Address sip) const;
    uint16_t GetFlowCountByEgress(uint32_t port, uint32_t qIndex) const;
    uint16_t GetFlowCountByEgressPort(uint32_t port) const;
    std::vector<RDMAEgressFlowKey> GetActiveFlowsByEgressPort(uint32_t port) const;
    bool GetMaxRateFlowByEgressQueue(uint32_t port, uint32_t qIndex, RDMAEgressFlowKey& outFlow) const;
    std::vector<RDMAEgressFlowKey> GetTopRateFlowsByEgressQueue(uint32_t port, uint32_t qIndex, uint32_t k) const;

    void CleanInactiveFlows();

    size_t GetTotalFlowCount() const { return m_flowMap.size(); }
    size_t GetTotalEgressFlowCount() const { return m_egressFlowMap.size(); }

    uint64_t GetTotalMemoryUsage() const;

    uint64_t GetInsertFlowCounter() const { return m_insertFlowCalls; }
    uint64_t GetInsertEgressFlowCounter() const { return m_insertEgressFlowCalls; }
    uint64_t GetCleanInactiveCounter() const { return m_cleanInactiveCalls; }
    uint64_t GetTopRateSelectCounter() const { return m_topRateSelectCalls; }

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

    struct PortQueueKeyHash {
        size_t operator()(const RDMAPortQueueKey& key) const noexcept {
            std::hash<uint32_t> h;
            size_t seed = h(key.port);
            seed ^= h(key.qIndex) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            return seed;
        }
    };

    struct EgressFlowKeyHash {
        size_t operator()(const RDMAEgressFlowKey& key) const noexcept {
            FlowKeyHash flowHash;
            std::hash<uint32_t> h;
            size_t seed = flowHash(key.flow);
            seed ^= h(key.port) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            seed ^= h(key.qIndex) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            return seed;
        }
    };

    std::unordered_map<RDMAFlowKey, RDMAFlowEntry, FlowKeyHash> m_flowMap{};
    std::unordered_map<RDMAEgressFlowKey, RDMAEgressFlowEntry, EgressFlowKeyHash> m_egressFlowMap{};
    // LRU lists keyed by last_ts (oldest at front, freshest at back).
    // Insert: push_back; Update: splice to end; Cleanup: pop_front while expired.
    std::list<RDMAFlowKey> m_flowLru{};
    std::list<RDMAEgressFlowKey> m_egressLru{};
    std::unordered_map<Ipv4Address, uint32_t> m_dipFlowCount{};
    std::unordered_map<Ipv4Address, uint32_t> m_sipFlowCount{};
    std::unordered_map<RDMAPortQueueKey, uint32_t, PortQueueKeyHash> m_egressFlowCount{};
    uint64_t m_inactiveThreshold{0};

    uint64_t m_insertFlowCalls{0};
    uint64_t m_insertEgressFlowCalls{0};
    uint64_t m_cleanInactiveCalls{0};
    uint64_t m_topRateSelectCalls{0};
};

} // namespace ns3

#endif // NS3_POINT_TO_POINT_RDMA_FLOW_TABLE_H
