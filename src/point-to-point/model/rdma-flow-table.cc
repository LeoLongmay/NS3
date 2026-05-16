#include "rdma-flow-table.h"
#include "switch-node.h"
#include "ns3/simulator.h"

#include <algorithm>
#include <limits>
#include <vector>

namespace ns3 {

TypeId
RDMAFlowTable::GetTypeId(void)
{
    static TypeId tid = TypeId("ns3::RDMAFlowTable")
        .SetParent<Object>()
        .SetGroupName("PointToPoint")
        .AddConstructor<RDMAFlowTable>()
        .AddAttribute(
            "InactiveThreshold",
            "Flow inactive threshold (time step)",
            UintegerValue(50000),
            MakeUintegerAccessor(&RDMAFlowTable::m_inactiveThreshold),
            MakeUintegerChecker<uint64_t>()
        );
    return tid;
}

RDMAFlowTable::RDMAFlowTable()
    : Object(){}

RDMAFlowTable::RDMAFlowTable(uint64_t inactiveThreshold)
    : Object(),
      m_inactiveThreshold(inactiveThreshold){}

// RDMAFlowTable::~RDMAFlowTable() = default;

void RDMAFlowTable::InsertOrUpdateFlow(Ipv4Address sip, Ipv4Address dip, uint16_t sport, uint16_t dport) {
    ++m_insertFlowCalls;
    RDMAFlowKey key{ sip, dip, sport, dport };
    auto it = m_flowMap.find(key);
    uint64_t now = static_cast<uint64_t>(Simulator::Now().GetTimeStep());

    if (it != m_flowMap.end()) {
        it->second.last_ts = now;
        // Move to back of LRU list (O(1), iterator stays valid).
        m_flowLru.splice(m_flowLru.end(), m_flowLru, it->second.lru_it);
    } else {
        m_flowLru.push_back(key);
        auto lru_it = std::prev(m_flowLru.end());
        RDMAFlowEntry entry{ key, now, 0, 0.0, lru_it };
        m_flowMap.emplace(key, entry);

        m_dipFlowCount[dip]++;
        m_sipFlowCount[sip]++;
    }
}

void RDMAFlowTable::InsertOrUpdateFlowOnEgress(Ipv4Address sip,
                                               Ipv4Address dip,
                                               uint16_t sport,
                                               uint16_t dport,
                                               uint32_t port,
                                               uint32_t qIndex,
                                               uint16_t pg,
                                               uint32_t pktBytes) {
    ++m_insertEgressFlowCalls;
    RDMAEgressFlowKey key{{sip, dip, sport, dport}, port, qIndex, pg};
    auto it = m_egressFlowMap.find(key);
    uint64_t now = static_cast<uint64_t>(Simulator::Now().GetTimeStep());

    if (it != m_egressFlowMap.end()) {
        uint64_t lastTs = it->second.last_ts;
        if (lastTs != 0 && now > lastTs) {
            // Convert bits/ns to bps by multiplying 1e9 (time step is ns).
            double instRateBps = (static_cast<double>(pktBytes) * 8.0 * 1e9) /
                                 static_cast<double>(now - lastTs);
            if (it->second.ewma_rate_bps <= 0.0) {
                it->second.ewma_rate_bps = instRateBps;
            } else {
                // Moderate smoothing to avoid selecting based on one burst packet.
                it->second.ewma_rate_bps = 0.8 * it->second.ewma_rate_bps + 0.2 * instRateBps;
            }
        }
        it->second.last_pkt_bytes = pktBytes;
        it->second.last_ts = now;
        m_egressLru.splice(m_egressLru.end(), m_egressLru, it->second.lru_it);
        return;
    }

    m_egressLru.push_back(key);
    auto lru_it = std::prev(m_egressLru.end());
    RDMAEgressFlowEntry entry{key.flow, now, pktBytes, 0.0, lru_it};
    m_egressFlowMap.emplace(key, entry);
    m_egressFlowCount[RDMAPortQueueKey{port, qIndex}]++;
}

uint16_t RDMAFlowTable::GetFlowCountByDip(Ipv4Address dip) const {
    auto it = m_dipFlowCount.find(dip);
    return (it != m_dipFlowCount.end()) ? it->second : 0;
}

uint16_t RDMAFlowTable::GetFlowCountBySip(Ipv4Address sip) const {
    auto it = m_sipFlowCount.find(sip);
    return (it != m_sipFlowCount.end()) ? it->second : 0;
}

uint16_t RDMAFlowTable::GetFlowCountByEgress(uint32_t port, uint32_t qIndex) const {
    auto it = m_egressFlowCount.find(RDMAPortQueueKey{port, qIndex});
    return (it != m_egressFlowCount.end()) ? it->second : 0;
}

uint16_t RDMAFlowTable::GetFlowCountByEgressPort(uint32_t port) const {
    uint32_t count = 0;
    for (const auto& pair : m_egressFlowCount) {
        if (pair.first.port == port) {
            count += pair.second;
        }
    }
    return static_cast<uint16_t>(std::min<uint32_t>(count, std::numeric_limits<uint16_t>::max()));
}

std::vector<RDMAEgressFlowKey> RDMAFlowTable::GetActiveFlowsByEgressPort(uint32_t port) const {
    std::vector<RDMAEgressFlowKey> flows;
    flows.reserve(m_egressFlowMap.size());
    for (const auto& pair : m_egressFlowMap) {
        if (pair.first.port == port) {
            flows.push_back(pair.first);
        }
    }
    return flows;
}

bool RDMAFlowTable::GetMaxRateFlowByEgressQueue(uint32_t port,
                                                uint32_t qIndex,
                                                RDMAEgressFlowKey& outFlow) const {
    bool found = false;
    double bestRate = -1.0;

    for (const auto& pair : m_egressFlowMap) {
        const RDMAEgressFlowKey& key = pair.first;
        if (key.port != port || key.qIndex != qIndex) {
            continue;
        }
        const RDMAEgressFlowEntry& entry = pair.second;
        if (!found || entry.ewma_rate_bps > bestRate) {
            found = true;
            bestRate = entry.ewma_rate_bps;
            outFlow = key;
        }
    }
    return found;
}

std::vector<RDMAEgressFlowKey> RDMAFlowTable::GetTopRateFlowsByEgressQueue(uint32_t port,
                                                                            uint32_t qIndex,
                                                                            uint32_t k) const {
    const_cast<RDMAFlowTable*>(this)->m_topRateSelectCalls++;
    std::vector<std::pair<RDMAEgressFlowKey, double>> candidates;
    if (k == 0) {
        return {};
    }
    candidates.reserve(m_egressFlowMap.size());
    for (const auto& pair : m_egressFlowMap) {
        const RDMAEgressFlowKey& key = pair.first;
        if (key.port != port || key.qIndex != qIndex) {
            continue;
        }
        candidates.emplace_back(key, pair.second.ewma_rate_bps);
    }
    if (candidates.empty()) {
        return {};
    }

    std::sort(candidates.begin(), candidates.end(),
              [](const std::pair<RDMAEgressFlowKey, double>& a,
                 const std::pair<RDMAEgressFlowKey, double>& b) {
                  return a.second > b.second;
              });

    uint32_t keep = std::min<uint32_t>(k, static_cast<uint32_t>(candidates.size()));
    std::vector<RDMAEgressFlowKey> out;
    out.reserve(keep);
    for (uint32_t i = 0; i < keep; ++i) {
        out.push_back(candidates[i].first);
    }
    return out;
}

void RDMAFlowTable::CleanInactiveFlows() {
    ++m_cleanInactiveCalls;
    uint64_t now = static_cast<uint64_t>(Simulator::Now().GetTimeStep());

    // Drain expired ingress flows from the front of the LRU. Each updated flow
    // is spliced to the back, so the front is always the oldest. We stop at
    // the first non-expired entry — cost is O(K) where K = #flows expiring now.
    while (!m_flowLru.empty()) {
        const RDMAFlowKey& frontKey = m_flowLru.front();
        auto mapIt = m_flowMap.find(frontKey);
        if (mapIt == m_flowMap.end()) {
            // Defensive: dangling LRU node (shouldn't normally happen).
            m_flowLru.pop_front();
            continue;
        }
        if (now - mapIt->second.last_ts <= m_inactiveThreshold) {
            break;
        }
        Ipv4Address dip = frontKey.dip;
        Ipv4Address sip = frontKey.sip;
        m_flowMap.erase(mapIt);
        m_flowLru.pop_front();

        auto dipIt = m_dipFlowCount.find(dip);
        if (dipIt != m_dipFlowCount.end()) {
            if (dipIt->second > 1) {
                dipIt->second--;
            } else {
                m_dipFlowCount.erase(dipIt);
            }
        }

        auto sipIt = m_sipFlowCount.find(sip);
        if (sipIt != m_sipFlowCount.end()) {
            if (sipIt->second > 1) {
                sipIt->second--;
            } else {
                m_sipFlowCount.erase(sipIt);
            }
        }
    }

    while (!m_egressLru.empty()) {
        const RDMAEgressFlowKey& frontKey = m_egressLru.front();
        auto mapIt = m_egressFlowMap.find(frontKey);
        if (mapIt == m_egressFlowMap.end()) {
            m_egressLru.pop_front();
            continue;
        }
        if (now - mapIt->second.last_ts <= m_inactiveThreshold) {
            break;
        }
        uint32_t port = frontKey.port;
        uint32_t qIndex = frontKey.qIndex;
        m_egressFlowMap.erase(mapIt);
        m_egressLru.pop_front();

        RDMAPortQueueKey portQueue{port, qIndex};
        auto egressIt = m_egressFlowCount.find(portQueue);
        if (egressIt != m_egressFlowCount.end()) {
            if (egressIt->second > 1) {
                egressIt->second--;
            } else {
                m_egressFlowCount.erase(egressIt);
            }
        }
    }
}

uint64_t RDMAFlowTable::GetTotalMemoryUsage() const {
    uint64_t totalMem = 0;

    using FlowMapPair = typename decltype(m_flowMap)::value_type;
    totalMem += m_flowMap.size() * sizeof(FlowMapPair);
    using EgressFlowMapPair = typename decltype(m_egressFlowMap)::value_type;
    totalMem += m_egressFlowMap.size() * sizeof(EgressFlowMapPair);
    using DipPair = typename decltype(m_dipFlowCount)::value_type;
    totalMem += m_dipFlowCount.size() * sizeof(DipPair);
    using SipPair = typename decltype(m_sipFlowCount)::value_type;
    totalMem += m_sipFlowCount.size() * sizeof(SipPair);
    using EgressCountPair = typename decltype(m_egressFlowCount)::value_type;
    totalMem += m_egressFlowCount.size() * sizeof(EgressCountPair);
    // LRU list node overhead: key + two pointers (prev/next) per node.
    totalMem += m_flowLru.size() * (sizeof(RDMAFlowKey) + 2 * sizeof(void*));
    totalMem += m_egressLru.size() * (sizeof(RDMAEgressFlowKey) + 2 * sizeof(void*));

    return totalMem;
}

} // namespace ns3
