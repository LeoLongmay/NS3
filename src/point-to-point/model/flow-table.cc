#include "switch-node.h"
#include "ns3/simulator.h"
#include "flow-table.h"

namespace ns3 {

FlowTable::FlowTable(uint64_t inactiveThreshold)
    : m_inactiveThreshold(inactiveThreshold) {}

void FlowTable::InsertOrUpdateFlow(Ipv4Address sip, Ipv4Address dip, uint16_t sport, uint16_t dport) {
    FlowKey key{ sip, dip, sport, dport };
    auto it = m_flowMap.find(key);

    if (it != m_flowMap.end()) {
        it->second.last_ts = Simulator::Now().GetTimeStep();
    } else {
        FlowEntry entry{ key, Simulator::Now().GetTimeStep() };
        m_flowMap.emplace(key, entry);

        m_dipFlowCount[dip]++;
        m_sipFlowCount[sip]++;
    }
}

uint32_t FlowTable::GetFlowCountByDip(Ipv4Address dip) const {
    auto it = m_dipFlowCount.find(dip);
    return (it != m_dipFlowCount.end()) ? it->second : 0;
}

uint32_t FlowTable::GetFlowCountBySip(Ipv4Address sip) const {
    auto it = m_sipFlowCount.find(sip);
    return (it != m_sipFlowCount.end()) ? it->second : 0;
}

void FlowTable::CleanInactiveFlows() {
    uint64_t now = Simulator::Now().GetTimeStep();
    std::vector<FlowKey> toDelete;

    for (const auto& pair : m_flowMap) {
        const FlowEntry& entry = pair.second;
        if (now - entry.last_ts > m_inactiveThreshold) {
            toDelete.push_back(pair.first);
        }
    }

    for (const FlowKey& key : toDelete) {
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