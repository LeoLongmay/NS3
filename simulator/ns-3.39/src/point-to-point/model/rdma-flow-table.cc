#include "rdma-flow-table.h"
#include "switch-node.h"
#include "ns3/simulator.h"

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
    RDMAFlowKey key{ sip, dip, sport, dport };
    auto it = m_flowMap.find(key);

    if (it != m_flowMap.end()) {
        it->second.last_ts = static_cast<uint64_t>(Simulator::Now().GetTimeStep());
    } else {
        RDMAFlowEntry entry{ key, static_cast<uint64_t>(Simulator::Now().GetTimeStep()) };
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
                                               uint32_t qIndex) {
    RDMAEgressFlowKey key{{sip, dip, sport, dport}, port, qIndex};
    auto it = m_egressFlowMap.find(key);
    uint64_t now = static_cast<uint64_t>(Simulator::Now().GetTimeStep());

    if (it != m_egressFlowMap.end()) {
        it->second.last_ts = now;
        return;
    }

    RDMAFlowEntry entry{key.flow, now};
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

void RDMAFlowTable::CleanInactiveFlows() {
    uint64_t now = static_cast<uint64_t>(Simulator::Now().GetTimeStep());
    std::vector<RDMAFlowKey> toDelete;
    std::vector<RDMAEgressFlowKey> toDeleteEgress;

    for (const auto& pair : m_flowMap) {
        const RDMAFlowEntry& entry = pair.second;
        if (now - entry.last_ts > m_inactiveThreshold) {
            toDelete.push_back(pair.first);
        }
    }

    for (const auto& pair : m_egressFlowMap) {
        const RDMAFlowEntry& entry = pair.second;
        if (now - entry.last_ts > m_inactiveThreshold) {
            toDeleteEgress.push_back(pair.first);
        }
    }

    for (const RDMAFlowKey& key : toDelete) {
        m_flowMap.erase(key);

        auto dipIt = m_dipFlowCount.find(key.dip);
        if (dipIt != m_dipFlowCount.end()) {
            if (dipIt->second > 1) {
                dipIt->second--;
            } else {
                m_dipFlowCount.erase(dipIt);
            }
        }

        auto sipIt = m_sipFlowCount.find(key.sip);
        if (sipIt != m_sipFlowCount.end()) {
            if (sipIt->second > 1) {
                sipIt->second--;
            } else {
                m_sipFlowCount.erase(sipIt);
            }
        }
    }

    for (const RDMAEgressFlowKey& key : toDeleteEgress) {
        m_egressFlowMap.erase(key);

        RDMAPortQueueKey portQueue{key.port, key.qIndex};
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

    return totalMem;
}

} // namespace ns3
