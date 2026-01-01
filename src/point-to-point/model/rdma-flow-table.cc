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
            UintegerValue(100),
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

uint16_t RDMAFlowTable::GetFlowCountByDip(Ipv4Address dip) const {
    auto it = m_dipFlowCount.find(dip);
    return (it != m_dipFlowCount.end()) ? it->second : 0;
}

uint16_t RDMAFlowTable::GetFlowCountBySip(Ipv4Address sip) const {
    auto it = m_sipFlowCount.find(sip);
    return (it != m_sipFlowCount.end()) ? it->second : 0;
}

void RDMAFlowTable::CleanInactiveFlows() {
    uint64_t now = static_cast<uint64_t>(Simulator::Now().GetTimeStep());
    std::vector<RDMAFlowKey> toDelete;

    for (const auto& pair : m_flowMap) {
        const RDMAFlowEntry& entry = pair.second;
        if (now - entry.last_ts > m_inactiveThreshold) {
            toDelete.push_back(pair.first);
        }
    }

    for (const RDMAFlowKey& key : toDelete) {
        m_flowMap.erase(key);

        if (--m_dipFlowCount[key.dip] == 0) {
            m_dipFlowCount.erase(key.dip);
        }

        if (--m_sipFlowCount[key.sip] == 0) {
            m_sipFlowCount.erase(key.sip);
        }
    }
}

} // namespace ns3