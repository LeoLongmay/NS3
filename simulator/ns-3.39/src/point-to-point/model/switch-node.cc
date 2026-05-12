#include "ns3/ipv4.h"
#include "ns3/packet.h"
#include "ns3/ipv4-header.h"
#include "ns3/pause-header.h"
#include "ns3/interface-tag.h"
#include "ns3/boolean.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"
#include "switch-node.h"
#include "qbb-net-device.h"
#include "ppp-header.h"
#include "ns3/int-header.h"
#include "ns3/simulator.h"
#include <cmath>
#include <algorithm>
#include "ns3/tcp-header.h"
#include "ns3/udp-header.h"
#include "ns3/custom-priority-tag.h"
#include "ns3/feedback-tag.h"
#include "ns3/unsched-tag.h"

namespace ns3 {
namespace {
constexpr uint32_t FLOW_CONTROL_PFC = 0;
constexpr uint32_t FLOW_CONTROL_BIFROST = 1;
constexpr uint32_t TRANSPORT_MODE_RDMA = 0;
constexpr uint32_t TRANSPORT_MODE_TCP_BBR = 1;
}
// uint32_t SwitchNode::cnp_count = 0;
// uint32_t SwitchNode::fcnp_count = 0;

TypeId SwitchNode::GetTypeId (void)
{
	static TypeId tid = TypeId ("ns3::SwitchNode")
	                    .SetParent<Node> ()
	                    .AddConstructor<SwitchNode> ()
	                    .AddAttribute("EcnEnabled",
	                                  "Enable ECN marking.",
	                                  BooleanValue(false),
	                                  MakeBooleanAccessor(&SwitchNode::m_ecnEnabled),
	                                  MakeBooleanChecker())
	                    .AddAttribute("CcMode",
	                                  "CC mode.",
	                                  UintegerValue(0),
	                                  MakeUintegerAccessor(&SwitchNode::m_ccMode),
	                                  MakeUintegerChecker<uint32_t>())
	                    .AddAttribute("AckHighPrio",
	                                  "Set high priority for ACK/NACK or not",
	                                  UintegerValue(0),
	                                  MakeUintegerAccessor(&SwitchNode::m_ackHighPrio),
	                                  MakeUintegerChecker<uint32_t>())
	                    .AddAttribute("MaxRtt",
	                                  "Max Rtt of the network",
	                                  UintegerValue(9000),
	                                  MakeUintegerAccessor(&SwitchNode::m_maxRtt),
	                                  MakeUintegerChecker<uint32_t>())
	                    .AddAttribute("PowerEnabled",
	                                  "Inserts Rxbytes instead of Txbytes in INT header",
	                                  BooleanValue(false),
	                                  MakeBooleanAccessor(&SwitchNode::PowerEnabled),
	                                  MakeBooleanChecker())
						.AddAttribute("FlowControlMode",
	                                  "Lossless flow control mode. 0=PFC, 1=Bifrost",
	                                  UintegerValue(FLOW_CONTROL_PFC),
	                                  MakeUintegerAccessor(&SwitchNode::m_flowControlMode),
	                                  MakeUintegerChecker<uint32_t>())
						.AddAttribute("TransportMode",
	                                  "Transport mode. 0=RDMA, 1=TCP_BBR",
	                                  UintegerValue(TRANSPORT_MODE_RDMA),
	                                  MakeUintegerAccessor(&SwitchNode::m_transportMode),
	                                  MakeUintegerChecker<uint32_t>())
						.AddAttribute("BifrostTimeSlotUs",
	                                  "Bifrost pause control slot in microseconds.",
	                                  UintegerValue(10),
	                                  MakeUintegerAccessor(&SwitchNode::m_bifrostTimeSlotUs),
	                                  MakeUintegerChecker<uint32_t>())
						.AddAttribute("BifrostK",
	                                  "Bifrost periodic correction interval.",
	                                  UintegerValue(1),
	                                  MakeUintegerAccessor(&SwitchNode::m_bifrostK),
	                                  MakeUintegerChecker<uint32_t>())
						.AddAttribute("BifrostLonghaulDelayCutoffUs",
	                                  "Ports with delay above this cutoff are treated as DCI links for Bifrost.",
	                                  UintegerValue(100),
	                                  MakeUintegerAccessor(&SwitchNode::m_bifrostLonghaulDelayCutoffUs),
	                                  MakeUintegerChecker<uint32_t>())
						.AddAttribute("BifrostHMarginSlots",
	                                  "Extra RsT slots reserved on top of Delta for Bifrost.",
	                                  UintegerValue(3),
	                                  MakeUintegerAccessor(&SwitchNode::m_bifrostHMarginSlots),
	                                  MakeUintegerChecker<uint32_t>())
						.AddAttribute("Epsilon",
	                                  "lpcc epsilon",
	                                  UintegerValue(3000),
	                                  MakeUintegerAccessor(&SwitchNode::m_epsilon),
	                                  MakeUintegerChecker<uint32_t>())
						.AddAttribute("FcnpMinIntervalUs",
	                                  "Minimum FCNP send interval per egress queue (us).",
	                                  UintegerValue(50),
	                                  MakeUintegerAccessor(&SwitchNode::m_fcnpMinIntervalUs),
	                                  MakeUintegerChecker<uint32_t>())
						.AddAttribute("BiccEnableEcnClear",
	                                  "Whether BiCC clears ECN marks on longhaul ingress at receiver-side DCI.",
	                                  BooleanValue(true),
	                                  MakeBooleanAccessor(&SwitchNode::m_biccEnableEcnClear),
	                                  MakeBooleanChecker())
						.AddAttribute("BiccLonghaulDelayCutoffUs",
	                                  "Ports with delay above this cutoff are considered longhaul ports for BiCC.",
	                                  UintegerValue(100),
	                                  MakeUintegerAccessor(&SwitchNode::m_biccLonghaulDelayCutoffUs),
	                                  MakeUintegerChecker<uint32_t>())
						.AddAttribute("BiccNsFeedbackMinIntervalUs",
	                                  "Minimum near-source BiCC feedback interval per egress queue (us).",
	                                  UintegerValue(75),
	                                  MakeUintegerAccessor(&SwitchNode::m_biccNsFeedbackMinIntervalUs),
	                                  MakeUintegerChecker<uint32_t>())
						.AddAttribute("BiccDstBdpFactor",
	                                  "BDP factor for BiCC destination aggregate gating.",
	                                  DoubleValue(1.0),
	                                  MakeDoubleAccessor(&SwitchNode::m_biccDstBdpFactor),
	                                  MakeDoubleChecker<double>())
						.AddAttribute("BiccSoftVoqMaxPkts",
	                                  "Maximum packets queued per destination in BiCC soft VOQ.",
	                                  UintegerValue(1024),
	                                  MakeUintegerAccessor(&SwitchNode::m_biccSoftVoqMaxPkts),
	                                  MakeUintegerChecker<uint32_t>())

	                    ;
	return tid;
}

SwitchNode::SwitchNode() {
	m_ecmpSeed = m_id;
	m_node_type = 1;
	m_mmu = CreateObject<SwitchMmu>();
	for (uint32_t i = 0; i < pCnt; i++)
		for (uint32_t j = 0; j < pCnt; j++)
			for (uint32_t k = 0; k < qCnt; k++)
				m_bytes[i][j][k] = 0;
	for (uint32_t i = 0; i < pCnt; i++)
		m_txBytes[i] = 0;
	for (uint32_t i = 0; i < pCnt; i++)
		m_lastPktSize[i] = m_lastPktTs[i] = 0;
	for (uint32_t i = 0; i < pCnt; i++)
		m_u[i] = 0;
	for (uint32_t i = 0; i < pCnt; i++)
		for (uint32_t q = 0; q < qCnt; q++)
			m_lastFcnpSentTs[i][q] = 0;
	for (uint32_t i = 0; i < pCnt; i++)
		for (uint32_t q = 0; q < qCnt; q++)
			m_lastBiccNsSentTs[i][q] = 0;
	m_biccNsFeedbackCount = 0;
	m_biccEcnClearCount = 0;
	m_biccSoftVoqEnqueueCount = 0;
	m_biccSoftVoqDequeueCount = 0;
	    uint64_t inactiveThreshold = 50000; // 50us in ns time steps
	m_flowTable = CreateObject<RDMAFlowTable>();
	m_flowTable->SetInactiveThreshold(inactiveThreshold);

    ScheduleCleanFlowTable();
}

void SwitchNode::ScheduleCleanFlowTable() {
    m_flowTable->CleanInactiveFlows();

    m_cleanFlowEvent = Simulator::Schedule(MicroSeconds(50),
                                           &SwitchNode::ScheduleCleanFlowTable, this);
}

void SwitchNode::ConfigureBifrostPort(uint32_t inPort, uint64_t bdpBytes, uint64_t reservedBytesH, Time slot, uint32_t k) {
	Ptr<QbbNetDevice> dev = DynamicCast<QbbNetDevice>(m_devices[inPort]);
	uint64_t slotBytes = dev->GetDataRate().GetBitRate() * slot.GetTimeStep() / 8000000000ULL;
	for (uint32_t qIndex = 1; qIndex < qCnt; qIndex++) {
		BifrostState &state = m_bifrost[inPort][qIndex];
		state.deltaBytes = bdpBytes;
		state.reservedBytesH = reservedBytesH;
		state.slotTime = slot;
		state.slotBytes = slotBytes;
		state.k = std::max(1u, k);
		state.fBytes = state.deltaBytes + state.slotBytes;
		state.lastRxBytes = m_mmu->GetIngressRxBytes(inPort, qIndex);
		state.tickCount = 0;
		if (!state.tickEvent.IsExpired()) {
			Simulator::Cancel(state.tickEvent);
		}
		state.tickEvent = EventId();
	}
}

void SwitchNode::SetBifrostPortEnabled(uint32_t inPort, bool enabled) {
	m_mmu->SetBifrostEnabled(inPort, enabled);
	for (uint32_t qIndex = 1; qIndex < qCnt; qIndex++) {
		BifrostState &state = m_bifrost[inPort][qIndex];
		state.enabled = enabled;
		if (!state.tickEvent.IsExpired()) {
			Simulator::Cancel(state.tickEvent);
		}
		state.tickEvent = EventId();
		if (enabled && m_flowControlMode == FLOW_CONTROL_BIFROST && state.slotTime.IsPositive()) {
			state.lastRxBytes = m_mmu->GetIngressRxBytes(inPort, qIndex);
			state.fBytes = state.deltaBytes + state.slotBytes;
			state.tickCount = 0;
			state.tickEvent = Simulator::Schedule(state.slotTime, &SwitchNode::RunBifrostTick, this, inPort, qIndex);
		}
	}
}

void SwitchNode::ScheduleBifrostTick(uint32_t inDev, uint32_t qIndex) {
	BifrostState &state = m_bifrost[inDev][qIndex];
	if (!state.enabled || m_flowControlMode != FLOW_CONTROL_BIFROST || !state.slotTime.IsPositive()) {
		return;
	}
	if (state.tickEvent.IsExpired()) {
		state.tickEvent = Simulator::Schedule(state.slotTime, &SwitchNode::RunBifrostTick, this, inDev, qIndex);
	}
}

void SwitchNode::RunBifrostTick(uint32_t inDev, uint32_t qIndex) {
	BifrostState &state = m_bifrost[inDev][qIndex];
	if (!state.enabled || m_flowControlMode != FLOW_CONTROL_BIFROST) {
		return;
	}

	uint64_t L = m_mmu->GetIngressBytes(inDev, qIndex);
	uint64_t rxNow = m_mmu->GetIngressRxBytes(inDev, qIndex);
	uint64_t r = rxNow >= state.lastRxBytes ? rxNow - state.lastRxBytes : 0;
	state.lastRxBytes = rxNow;

	int64_t grantSpace = static_cast<int64_t>(state.reservedBytesH) - static_cast<int64_t>(L) - static_cast<int64_t>(state.fBytes);
	uint64_t c = std::min<uint64_t>(state.slotBytes, std::max<int64_t>(0, grantSpace));
	uint64_t cAdj = c;
	if (state.k > 0 && state.tickCount % state.k == 0 && L + state.fBytes > state.reservedBytesH) {
		uint64_t excess = L + state.fBytes - state.reservedBytesH;
		cAdj = excess >= c ? 0 : c - excess;
	}

	if (state.slotBytes > 0) {
		double grantRatio = std::min(1.0, double(cAdj) / double(state.slotBytes));
		double pauseUs = state.slotTime.GetMicroSeconds() * (1.0 - grantRatio);
		if (pauseUs > 0) {
			Ptr<QbbNetDevice> dev = DynamicCast<QbbNetDevice>(m_devices[inDev]);
			dev->SendPfc(qIndex, MicroSeconds(static_cast<uint64_t>(std::min<double>(pauseUs, state.slotTime.GetMicroSeconds()))));
		}
	}

	int64_t nextF = static_cast<int64_t>(state.fBytes) - static_cast<int64_t>(r) + static_cast<int64_t>(cAdj);
	state.fBytes = std::min<uint64_t>(state.deltaBytes + state.slotBytes, std::max<int64_t>(0, nextF));
	state.tickCount++;
	state.tickEvent = EventId();
	ScheduleBifrostTick(inDev, qIndex);
}

int SwitchNode::GetOutDev(Ptr<const Packet> p, CustomHeader &ch) {
	// look up entries
	Ptr<Packet> cp = p->Copy();

	PppHeader ph; cp->RemoveHeader(ph);
	Ipv4Header ih; cp->RemoveHeader(ih);
	auto entry = m_rtTable.find(ih.GetDestination().Get());

	// no matching entry
	if (entry == m_rtTable.end())
		return -1;

	// entry found
	auto &nexthops = entry->second;

	// pick one next hop based on hash
	union {
		uint8_t u8[4 + 4 + 2 + 2];
		uint32_t u32[3];
	} buf;
	buf.u32[0] = ih.GetSource().Get();
	buf.u32[1] = ih.GetDestination().Get();
	if (ih.GetProtocol() == 0x6) {
		TcpHeader th; cp->PeekHeader(th);
		buf.u32[2] = th.GetSourcePort() | ((uint32_t)th.GetDestinationPort() << 16);
	}
	else if (ch.l3Prot == 0x11) {
		buf.u32[2] = ch.udp.sport | ((uint32_t)ch.udp.dport << 16);
	}
	else if (ch.l3Prot == 0xFC || ch.l3Prot == 0xFD)
		buf.u32[2] = ch.ack.sport | ((uint32_t)ch.ack.dport << 16);

	uint32_t idx = EcmpHash(buf.u8, 12, m_ecmpSeed) % nexthops.size();
	// if (nexthops.size()>1){ std::cout << "selected " << idx << std::endl; }
	return nexthops[idx];
}

void SwitchNode::CheckAndSendPfc(uint32_t inDev, uint32_t qIndex) {
	if (m_flowControlMode == FLOW_CONTROL_BIFROST && m_bifrost[inDev][qIndex].enabled) {
		return;
	}
	Ptr<QbbNetDevice> device = DynamicCast<QbbNetDevice>(m_devices[inDev]);
	if (m_mmu->CheckShouldPause(inDev, qIndex)) {
		device->SendPfc(qIndex, 0);
		// std::cout << "sending PFC" << std::endl;
		m_mmu->SetPause(inDev, qIndex);
	}
}
void SwitchNode::CheckAndSendResume(uint32_t inDev, uint32_t qIndex) {
	if (m_flowControlMode == FLOW_CONTROL_BIFROST && m_bifrost[inDev][qIndex].enabled) {
		return;
	}
	Ptr<QbbNetDevice> device = DynamicCast<QbbNetDevice>(m_devices[inDev]);
	if (m_mmu->CheckShouldResume(inDev, qIndex)) {
		device->SendPfc(qIndex, 1);
		m_mmu->SetResume(inDev, qIndex);
	}
}

void SwitchNode::SendToDev(Ptr<Packet>p, CustomHeader &ch) {
	int idx = GetOutDev(p, ch);
	if (idx >= 0) {
		NS_ASSERT_MSG(m_devices[idx]->IsLinkUp(), "The routing table look up should return link that is up");

		// determine the qIndex
		uint32_t qIndex=0;
		MyPriorityTag priotag;
		// IMPORTANT: MyPriorityTag should only be attached by lossy traffic. This tag indicates the qIndex but also indicates that it is "lossy". Never attach MyPriorityTag on lossless traffic.
		bool found = p->PeekPacketTag(priotag);
		bool tcpBbrLossy = (m_transportMode == TRANSPORT_MODE_TCP_BBR && ch.l3Prot == 0x06);
		if (tcpBbrLossy) {
			found = true;
		}

		// UnSchedTag is used by ABM. End-hosts explicitly tag packets of the first BDP so that ABM then prioritizes these packets in the buffer allocation.
		uint32_t unsched = 0;
		UnSchedTag tag;
		bool foundunSched = p->PeekPacketTag (tag);
		if (foundunSched) {
			unsched = tag.GetValue();
		}

		if (ch.l3Prot == 0xF9 || ch.l3Prot == 0xFF || ch.l3Prot == 0xFE || (m_ackHighPrio && (ch.l3Prot == 0xFD || ch.l3Prot == 0xFC))) { //QCN or PFC or NACK, go highest priority
			qIndex = 0;
		}
		else if (found) {
			qIndex = priotag.GetPriority();
			// std::cout << "using queue " << qIndex << std::endl;
		}
		else {
			qIndex = (ch.l3Prot == 0x06 ? 1 : ch.udp.pg); // For TCP/IP if the stack did not attach MyPriorityTag, put to queue 1.
		}
		if (tcpBbrLossy) {
			qIndex = 1;
		}

		// admission control
		InterfaceTag t;
		p->PeekPacketTag(t);
		uint32_t inDev = t.GetPortId();
		if (qIndex != 0) { //not highest priority
			// IMPORTANT: MyPriorityTag should only be attached by lossy traffic. This tag indicates the qIndex but also indicates that it is "lossy". Never attach MyPriorityTag on lossless traffic.
			if (m_mmu->CheckIngressAdmission(inDev, qIndex, p->GetSize(), found,unsched) && m_mmu->CheckEgressAdmission(idx, qIndex, p->GetSize(), found,unsched)) {			// Admission control
				m_mmu->UpdateIngressAdmission(inDev, qIndex, p->GetSize(), found, unsched);
				m_mmu->UpdateEgressAdmission(idx, qIndex, p->GetSize(), found);
			} else {
				return; // Drop
			}
			CheckAndSendPfc(inDev, qIndex);
		}
		// std::cout << "inDev: " << inDev << " outDev: " << idx << " qIndex: " << qIndex << std::endl;
		// qIndex %= 8;
		m_bytes[inDev][idx][qIndex] += p->GetSize();
		if (ch.l3Prot == 0x11 && qIndex != 0) {
			m_flowTable->InsertOrUpdateFlowOnEgress(Ipv4Address(ch.sip), Ipv4Address(ch.dip),
			                                        ch.udp.sport, ch.udp.dport, idx, qIndex);
		}
		m_devices[idx]->SwitchSend(qIndex, p, ch);
		DynamicCast<QbbNetDevice>(m_devices[idx])->totalBytesRcvd += p->GetSize(); // Attention: this is the egress port's total received packets. Not the ingress port.
	} else
		std::cout << "outdev not found! Dropped. This should not happen. Debugging required!" << std::endl;
	return; // Drop
}

uint32_t SwitchNode::EcmpHash(const uint8_t* key, size_t len, uint32_t seed) {
	uint32_t h = seed;
	if (len > 3) {
		const uint32_t* key_x4 = (const uint32_t*) key;
		size_t i = len >> 2;
		do {
			uint32_t k = *key_x4++;
			k *= 0xcc9e2d51;
			k = (k << 15) | (k >> 17);
			k *= 0x1b873593;
			h ^= k;
			h = (h << 13) | (h >> 19);
			h += (h << 2) + 0xe6546b64;
		} while (--i);
		key = (const uint8_t*) key_x4;
	}
	if (len & 3) {
		size_t i = len & 3;
		uint32_t k = 0;
		key = &key[i - 1];
		do {
			k <<= 8;
			k |= *key--;
		} while (--i);
		k *= 0xcc9e2d51;
		k = (k << 15) | (k >> 17);
		k *= 0x1b873593;
		h ^= k;
	}
	h ^= len;
	h ^= h >> 16;
	h *= 0x85ebca6b;
	h ^= h >> 13;
	h *= 0xc2b2ae35;
	h ^= h >> 16;
	return h;
}

void SwitchNode::SetEcmpSeed(uint32_t seed) {
	m_ecmpSeed = seed;
}

void SwitchNode::AddTableEntry(Ipv4Address &dstAddr, uint32_t intf_idx) {
	uint32_t dip = dstAddr.Get();
	m_rtTable[dip].push_back(intf_idx);
}

void SwitchNode::ClearTable() {
	m_rtTable.clear();
}

// This function can only be called in switch mode
bool SwitchNode::SwitchReceiveFromDevice(Ptr<NetDevice> device, Ptr<Packet> packet, CustomHeader &ch) {
	InterfaceTag t;
	packet->PeekPacketTag(t);
	uint32_t inDev = t.GetPortId();
	int outDevSigned = GetOutDev(packet, ch);
	uint32_t outDev = outDevSigned >= 0 ? static_cast<uint32_t>(outDevSigned) : 0;

	if (ch.l3Prot == 0x11) {
		PppHeader ppp;
		Ipv4Header h;
		UdpHeader udph;
		Ptr<Packet> p = packet->Copy();
		p->RemoveHeader(ppp);
		p->RemoveHeader(h);
		p->PeekHeader(udph);
		m_flowTable->InsertOrUpdateFlow(h.GetSource(), h.GetDestination(), udph.GetSourcePort(), udph.GetDestinationPort());
	}

	if (m_ccMode == 12 && outDevSigned >= 0) {
		MaybeApplyBiccEcnClear(inDev, outDev, packet);
		if (ch.l3Prot == 0xFC || ch.l3Prot == 0xFD) {
			MaybeHandleBiccAckRelease(inDev, outDev, ch);
		}
		if (MaybeHandleBiccNearDestinationIngress(inDev, outDev, packet, ch)) {
			return true;
		}
	}

	SendToDev(packet, ch);
	return true;
}

void SwitchNode::SwitchNotifyDequeue(uint32_t ifIndex, uint32_t qIndex, Ptr<Packet> p) {
	InterfaceTag t;
	p->PeekPacketTag(t);

	MyPriorityTag priotag;
	bool found = p->PeekPacketTag(priotag);

	if (qIndex != 0) {
		uint32_t inDev = t.GetPortId();
		m_mmu->RemoveFromIngressAdmission(inDev, qIndex, p->GetSize(), found);
		m_mmu->RemoveFromEgressAdmission(ifIndex, qIndex, p->GetSize(), found);
		m_bytes[inDev][ifIndex][qIndex] -= p->GetSize();
			if (m_ccMode == 9 && IsLpccWanNode()) { // lpcc only on WAN core nodes 21-25
				if (m_mmu->egress_bytes[ifIndex][qIndex] > m_epsilon) { // send FCNP
				const uint64_t nowTs = Simulator::Now().GetTimeStep();
				const uint64_t minIntervalTs = static_cast<uint64_t>(m_fcnpMinIntervalUs) * 1000ULL;
				if (minIntervalTs == 0 || nowTs - m_lastFcnpSentTs[ifIndex][qIndex] >= minIntervalTs) {
					// std::cout << "egress_bytes: " << m_mmu->egress_bytes[ifIndex][qIndex] << std::endl;egress_bytes[ifIndex][qIndex]
					// if (Simulator::Now().GetTimeStep() >= 165210000) {
					// 	int debug = 1;
					// 	std::cout << debug << std::endl;
					// }
					PppHeader ppp;
					Ipv4Header h;
					UdpHeader ch;
					Ptr<Packet> packet = p->Copy();
					packet->RemoveHeader(ppp);
					packet->RemoveHeader(h);
					packet->PeekHeader(ch);

					Ipv4Address srcip = h.GetDestination(); // origin pkt's dst ip is fcnp's src ip
					Ipv4Address dstip = h.GetSource(); // origin pkt's src ip is fcnp's dst ip;
					PppHeader nppp = ppp;
					Ipv4Header nh = h;
					nh.SetSource(srcip);
					nh.SetDestination(dstip);
					nh.SetProtocol(0xF9); // fcnp
						
					CustomHeader nch(CustomHeader::L2_Header | CustomHeader::L3_Header | CustomHeader::L4_Header);
					nch.sip = srcip.Get();
					nch.dip = dstip.Get();
					nch.l3Prot = 0xF9; // fcnp
					nch.fcnp.timestamp = Simulator::Now().GetTimeStep();
					nch.fcnp.qIndex = 0;
					nch.fcnp.pg = 3;
					nch.fcnp.dport = ch.GetSourcePort();
					Ptr<QbbNetDevice> dev = DynamicCast<QbbNetDevice>(m_devices[ifIndex]);
					nch.fcnp.qlen = m_mmu->egress_bytes[ifIndex][qIndex];
					// nch.fcnp.qlen = m_mmu->totalUsed;
					nch.fcnp.m_flowCount = m_flowTable->GetFlowCountByEgress(ifIndex, qIndex);
					nch.fcnp.linkRateBps = dev->GetDataRate().GetBitRate();

					Ptr<Packet> fcnp_pkt = Create<Packet>();
					fcnp_pkt->AddHeader(nch);
					fcnp_pkt->AddHeader(nh);
					fcnp_pkt->AddHeader(nppp);
					fcnp_pkt->AddPacketTag(t);
					SendToDev(fcnp_pkt, nch);
					m_lastFcnpSentTs[ifIndex][qIndex] = nowTs;
					// fcnp_count++;
					}
				}
			}
			if (m_ccMode == 12) {
				MaybeGenerateBiccNearSourceFeedback(ifIndex, qIndex, inDev, p);
			}
			if (m_ecnEnabled) {
				bool egressCongested = m_mmu->ShouldSendCN(ifIndex, qIndex);
			if (egressCongested) {
				// In LPCC mode, WAN-core congestion uses FCNP (LPCC). Other nodes use ECN/CNP (DCQCN path).
				bool markEcn = !(m_ccMode == 9 && IsLpccWanNode());
				if (markEcn) {
					PppHeader ppp;
					Ipv4Header h;
					p->RemoveHeader(ppp);
					p->RemoveHeader(h);
					h.SetEcn((Ipv4Header::EcnType)0x03);
					p->AddHeader(h);
					p->AddHeader(ppp);
					// cnp_count++;
				}
			}
		}
		//CheckAndSendPfc(inDev, qIndex);
		CheckAndSendResume(inDev, qIndex);
	}
	if (1) {
		uint8_t* buf = p->GetBuffer();
		if (buf[PppHeader::GetStaticSize() + 9] == 0x11) { // udp packet
			IntHeader *ih = (IntHeader*)&buf[PppHeader::GetStaticSize() + 20 + 8 + 6]; // ppp, ip, udp, SeqTs, INT
			Ptr<QbbNetDevice> dev = DynamicCast<QbbNetDevice>(m_devices[ifIndex]);
			if (m_ccMode == 3) { // HPCC or PowerTCP-INT
				if (!PowerEnabled)
					ih->PushHop(Simulator::Now().GetTimeStep(), m_txBytes[ifIndex], dev->GetQueue()->GetNBytesTotal(), dev->GetDataRate().GetBitRate());
				else
					ih->PushHop(Simulator::Now().GetTimeStep(), dev->GetQueue()->GetNBytesRxTotal(), dev->GetQueue()->GetNBytesTotal(), dev->GetDataRate().GetBitRate());
				// ih->PushHop(Simulator::Now().GetTimeStep(), m_txBytes[ifIndex], dev->GetQueue()->GetNBytesTotal(), dev->GetDataRate().GetBitRate());
			} else if (m_ccMode == 10) { // HPCC-PINT
				uint64_t t = Simulator::Now().GetTimeStep();
				uint64_t dt = t - m_lastPktTs[ifIndex];
				if (dt > m_maxRtt)
					dt = m_maxRtt;
				uint64_t B = dev->GetDataRate().GetBitRate() / 8; //Bps
				uint64_t qlen = dev->GetQueue()->GetNBytesTotal();
				double newU;

				/**************************
				 * approximate calc
				 *************************/
				int b = 20, m = 16, l = 20; // see log2apprx's paremeters
				int sft = logres_shift(b, l);
				double fct = 1 << sft; // (multiplication factor corresponding to sft)
				double log_T = log2(m_maxRtt) * fct; // log2(T)*fct
				double log_B = log2(B) * fct; // log2(B)*fct
				double log_1e9 = log2(1e9) * fct; // log2(1e9)*fct
				double qterm = 0;
				double byteTerm = 0;
				double uTerm = 0;
				if ((qlen >> 8) > 0) {
					int log_dt = log2apprx(dt, b, m, l); // ~log2(dt)*fct
					int log_qlen = log2apprx(qlen >> 8, b, m, l); // ~log2(qlen / 256)*fct
					qterm = pow(2, (
					                log_dt + log_qlen + log_1e9 - log_B - 2 * log_T
					            ) / fct
					           ) * 256;
					// 2^((log2(dt)*fct+log2(qlen/256)*fct+log2(1e9)*fct-log2(B)*fct-2*log2(T)*fct)/fct)*256 ~= dt*qlen*1e9/(B*T^2)
				}
				if (m_lastPktSize[ifIndex] > 0) {
					int byte = m_lastPktSize[ifIndex];
					int log_byte = log2apprx(byte, b, m, l);
					byteTerm = pow(2, (
					                   log_byte + log_1e9 - log_B - log_T
					               ) / fct
					              );
					// 2^((log2(byte)*fct+log2(1e9)*fct-log2(B)*fct-log2(T)*fct)/fct) ~= byte*1e9 / (B*T)
				}
				if (m_maxRtt > dt && m_u[ifIndex] > 0) {
					int log_T_dt = log2apprx(m_maxRtt - dt, b, m, l); // ~log2(T-dt)*fct
					int log_u = log2apprx(int(round(m_u[ifIndex] * 8192)), b, m, l); // ~log2(u*512)*fct
					uTerm = pow(2, (
					                log_T_dt + log_u - log_T
					            ) / fct
					           ) / 8192;
					// 2^((log2(T-dt)*fct+log2(u*512)*fct-log2(T)*fct)/fct)/512 = (T-dt)*u/T
				}
				newU = qterm + byteTerm + uTerm;

#if 0
				/**************************
				 * accurate calc
				 *************************/
				double weight_ewma = double(dt) / m_maxRtt;
				double u;
				if (m_lastPktSize[ifIndex] == 0)
					u = 0;
				else {
					double txRate = m_lastPktSize[ifIndex] / double(dt); // B/ns
					u = (qlen / m_maxRtt + txRate) * 1e9 / B;
				}
				newU = m_u[ifIndex] * (1 - weight_ewma) + u * weight_ewma;
				printf(" %lf\n", newU);
#endif

				/************************
				 * update PINT header
				 ***********************/
				uint16_t power = Pint::encode_u(newU);
				if (power > ih->GetPower())
					ih->SetPower(power);

				m_u[ifIndex] = newU;
			}
		}
		else {
			FeedbackTag Int;
			bool found;
			found = p->PeekPacketTag(Int);
			if (found) {
				Ptr<QbbNetDevice> dev = DynamicCast<QbbNetDevice>(m_devices[ifIndex]);
				Int.setTelemetryQlenDeq(Int.getHopCount(), dev->GetQueue()->GetNBytesTotal()); // queue length at dequeue
				Int.setTelemetryTsDeq(Int.getHopCount(), Simulator::Now().GetNanoSeconds()); // timestamp at dequeue
				Int.setTelemetryBw(Int.getHopCount(), dev->GetDataRate().GetBitRate());
				Int.setTelemetryTxBytes(Int.getHopCount(), m_txBytes[ifIndex]);
				Int.incrementHopCount(); // Incrementing hop count at Dequeue. Don't do this at enqueue.
				p->ReplacePacketTag(Int); // replacing the tag with new values
				// std::cout << "found " << Int.getHopCount() << std::endl;
			}
		}
	}
	m_txBytes[ifIndex] += p->GetSize();
	m_lastPktSize[ifIndex] = p->GetSize();
	m_lastPktTs[ifIndex] = Simulator::Now().GetTimeStep();
}

int SwitchNode::logres_shift(int b, int l) {
	static int data[] = {0, 0, 1, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5};
	return l - data[b];
}

int SwitchNode::log2apprx(int x, int b, int m, int l) {
	int x0 = x;
	int msb = int(log2(x)) + 1;
	if (msb > m) {
		x = (x >> (msb - m) << (msb - m));
#if 0
		x += + (1 << (msb - m - 1));
#else
		int mask = (1 << (msb - m)) - 1;
		if ((x0 & mask) > (rand() & mask))
			x += 1 << (msb - m);
#endif
	}
	return int(log2(x) * (1 << logres_shift(b, l)));
}

bool SwitchNode::IsLpccWanNode() const {
	uint32_t id = GetId();
	return id >= 21 && id <= 25;
}

bool SwitchNode::IsLonghaulPort(uint32_t portId) const {
	if (portId == 0 || portId >= GetNDevices()) {
		return false;
	}
	Ptr<QbbNetDevice> dev = DynamicCast<QbbNetDevice>(m_devices[portId]);
	if (dev == nullptr || dev->GetChannel() == nullptr) {
		return false;
	}
	uint64_t delay = DynamicCast<QbbChannel>(dev->GetChannel())->GetDelay().GetTimeStep();
	return delay >= static_cast<uint64_t>(m_biccLonghaulDelayCutoffUs) * 1000ULL;
}

bool SwitchNode::IsSenderSideDciPath(uint32_t inDev, uint32_t outDev) const {
	return !IsLonghaulPort(inDev) && IsLonghaulPort(outDev);
}

bool SwitchNode::IsReceiverSideDciPath(uint32_t inDev, uint32_t outDev) const {
	return IsLonghaulPort(inDev) && !IsLonghaulPort(outDev);
}

uint64_t SwitchNode::EstimateBiccDstBudgetBytes(uint32_t outDev) const {
	if (outDev == 0 || outDev >= GetNDevices()) {
		return 1;
	}
	Ptr<QbbNetDevice> dev = DynamicCast<QbbNetDevice>(m_devices[outDev]);
	if (dev == nullptr || dev->GetChannel() == nullptr) {
		return 1;
	}
	uint64_t rate = dev->GetDataRate().GetBitRate();
	uint64_t delay = DynamicCast<QbbChannel>(dev->GetChannel())->GetDelay().GetTimeStep();
	double bdp = (double(rate) * double(delay * 2)) / 8e9;
	double factor = std::max(0.1, m_biccDstBdpFactor);
	uint64_t budget = static_cast<uint64_t>(std::max(1.0, bdp * factor));
	return budget;
}

void SwitchNode::MaybeGenerateBiccNearSourceFeedback(uint32_t ifIndex, uint32_t qIndex, uint32_t inDev, Ptr<Packet> p) {
	if (!IsSenderSideDciPath(inDev, ifIndex)) {
		return;
	}
	if (!m_mmu->ShouldSendCN(ifIndex, qIndex)) {
		return;
	}
	const uint64_t nowTs = Simulator::Now().GetTimeStep();
	const uint64_t minIntervalTs = static_cast<uint64_t>(m_biccNsFeedbackMinIntervalUs) * 1000ULL;
	if (minIntervalTs > 0 && nowTs - m_lastBiccNsSentTs[ifIndex][qIndex] < minIntervalTs) {
		return;
	}

	CustomHeader orig(CustomHeader::L2_Header | CustomHeader::L3_Header | CustomHeader::L4_Header);
	orig.getInt = 0;
	Ptr<Packet> parsePkt = p->Copy();
	parsePkt->PeekHeader(orig);
	if (orig.l3Prot != 0x11) {
		return;
	}

	PppHeader ppp;
	Ipv4Header h;
	UdpHeader udp;
	Ptr<Packet> packet = p->Copy();
	packet->RemoveHeader(ppp);
	packet->RemoveHeader(h);
	packet->PeekHeader(udp);

	Ipv4Address srcip = h.GetDestination();
	Ipv4Address dstip = h.GetSource();
	PppHeader nppp = ppp;
	Ipv4Header nh = h;
	nh.SetSource(srcip);
	nh.SetDestination(dstip);
	nh.SetProtocol(0xF9); // reuse FCNP channel for BiCC near-source feedback

	CustomHeader nch(CustomHeader::L2_Header | CustomHeader::L3_Header | CustomHeader::L4_Header);
	nch.sip = srcip.Get();
	nch.dip = dstip.Get();
	nch.l3Prot = 0xF9;
	nch.fcnp.timestamp = nowTs;
	nch.fcnp.qIndex = 1; // marker: BiCC near-source feedback
	nch.fcnp.pg = orig.udp.pg;
	nch.fcnp.dport = orig.udp.sport;
	Ptr<QbbNetDevice> dev = DynamicCast<QbbNetDevice>(m_devices[ifIndex]);
	nch.fcnp.qlen = m_mmu->egress_bytes[ifIndex][qIndex];
	nch.fcnp.m_flowCount = m_flowTable->GetFlowCountByEgress(ifIndex, qIndex);
	nch.fcnp.linkRateBps = dev->GetDataRate().GetBitRate();

	Ptr<Packet> fbPkt = Create<Packet>();
	fbPkt->AddHeader(nch);
	fbPkt->AddHeader(nh);
	fbPkt->AddHeader(nppp);
	InterfaceTag inTag(inDev);
	fbPkt->AddPacketTag(inTag);
	SendToDev(fbPkt, nch);
	m_lastBiccNsSentTs[ifIndex][qIndex] = nowTs;
	m_biccNsFeedbackCount++;
}

void SwitchNode::MaybeApplyBiccEcnClear(uint32_t inDev, uint32_t outDev, Ptr<Packet> p) {
	if (!m_biccEnableEcnClear) {
		return;
	}
	if (!IsReceiverSideDciPath(inDev, outDev)) {
		return;
	}
	Ptr<Packet> packet = p;
	PppHeader ppp;
	Ipv4Header h;
	packet->RemoveHeader(ppp);
	packet->RemoveHeader(h);
	if (h.GetProtocol() == 0x11 || h.GetProtocol() == 0xFC || h.GetProtocol() == 0xFD) {
		if (h.GetEcn() != Ipv4Header::ECN_NotECT) {
			h.SetEcn(Ipv4Header::ECN_NotECT);
			m_biccEcnClearCount++;
		}
	}
	packet->AddHeader(h);
	packet->AddHeader(ppp);
}

bool SwitchNode::MaybeHandleBiccNearDestinationIngress(uint32_t inDev, uint32_t outDev, Ptr<Packet> packet, CustomHeader& ch) {
	if (ch.l3Prot != 0x11) {
		return false;
	}
	if (!IsReceiverSideDciPath(inDev, outDev)) {
		return false;
	}

	uint32_t dstIp = ch.dip;
	auto& st = m_biccDstState[dstIp];
	// Use longhaul ingress port as budget reference for receiver-side DCI gating.
	st.outDev = inDev;
	uint64_t budget = EstimateBiccDstBudgetBytes(st.outDev);
	// Keep inflight accounting in the same unit as ACK seq delta (payload bytes).
	uint32_t pktSize = std::max<uint32_t>(1, ch.udp.payload_size);
	bool needQueue = !st.queue.empty() || st.inflightBytes + pktSize > budget;
	if (!needQueue) {
		st.inflightBytes += pktSize;
		return false;
	}
	if (st.queue.size() >= m_biccSoftVoqMaxPkts) {
		// Do not drop: bypass gating for this packet to avoid transport stall.
		st.inflightBytes += pktSize;
		return false;
	}
	BiccBufferedPkt item{packet->Copy(), ch, pktSize};
	st.queue.push_back(item);
	st.queuedBytes += pktSize;
	m_biccSoftVoqEnqueueCount++;
	return true;
}

SwitchNode::BiccAckFlowKey SwitchNode::GetBiccAckFlowKey(const CustomHeader& ch) const {
	return BiccAckFlowKey{ch.sip, ch.dip, ch.ack.sport, ch.ack.dport, ch.ack.pg};
}

void SwitchNode::MaybeHandleBiccAckRelease(uint32_t inDev, uint32_t outDev, const CustomHeader& ch) {
	if (!IsSenderSideDciPath(inDev, outDev)) {
		return;
	}
	BiccAckFlowKey flowKey = GetBiccAckFlowKey(ch);
	auto& ackState = m_biccAckState[flowKey];
	uint32_t prevSeq = ackState.initialized ? ackState.lastSeq : 0;
	ackState.initialized = true;
	if (ch.ack.seq <= prevSeq) {
		return;
	}
	uint32_t delta = ch.ack.seq - prevSeq;
	ackState.lastSeq = ch.ack.seq;

	uint32_t dstIp = ch.sip; // ACK source is destination host for longhaul->intra data
	auto it = m_biccDstState.find(dstIp);
	if (it == m_biccDstState.end()) {
		return;
	}
	BiccDstState& st = it->second;
	st.inflightBytes = st.inflightBytes > delta ? st.inflightBytes - delta : 0;
	DrainBiccDstQueue(dstIp);
}

void SwitchNode::DrainBiccDstQueue(uint32_t dstIp) {
	auto it = m_biccDstState.find(dstIp);
	if (it == m_biccDstState.end()) {
		return;
	}
	BiccDstState& st = it->second;
	uint64_t budget = EstimateBiccDstBudgetBytes(st.outDev);
	uint32_t sent = 0;
	const uint32_t maxBurst = 128;
	while (!st.queue.empty() && sent < maxBurst) {
		BiccBufferedPkt item = st.queue.front();
		if (st.inflightBytes + item.size > budget) {
			break;
		}
		st.queue.pop_front();
		st.queuedBytes = st.queuedBytes > item.size ? st.queuedBytes - item.size : 0;
		st.inflightBytes += item.size;
		SendToDev(item.packet, item.header);
		m_biccSoftVoqDequeueCount++;
		sent++;
	}
	if (st.queue.empty() && st.inflightBytes == 0) {
		m_biccDstState.erase(it);
	}
}

SwitchNode::~SwitchNode() {
    Simulator::Cancel(m_cleanFlowEvent);
	for (uint32_t i = 0; i < pCnt; i++) {
		for (uint32_t q = 1; q < qCnt; q++) {
			if (!m_bifrost[i][q].tickEvent.IsExpired()) {
				Simulator::Cancel(m_bifrost[i][q].tickEvent);
			}
		}
	}
	if (m_ccMode == 12) {
		std::cout << "BiCCStats node=" << GetId()
		          << " ns_fb=" << m_biccNsFeedbackCount
		          << " ecn_clear=" << m_biccEcnClearCount
		          << " softvoq_enq=" << m_biccSoftVoqEnqueueCount
		          << " softvoq_deq=" << m_biccSoftVoqDequeueCount
		          << std::endl;
	}
}

} /* namespace ns3 */
