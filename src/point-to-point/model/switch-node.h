#ifndef SWITCH_NODE_H
#define SWITCH_NODE_H

#include <unordered_map>
#include <deque>
#include <ns3/node.h>
#include "qbb-net-device.h"
#include "switch-mmu.h"
#include "rdma-flow-table.h"
#include "pint.h"
#include "ns3/nstime.h"

namespace ns3 {

class Packet;

class SwitchNode : public Node{
	static const uint32_t pCnt = 257;	// Number of ports used
	static const uint32_t qCnt = 8;	// Number of queues/priorities used
	uint32_t m_ecmpSeed;
	std::unordered_map<uint32_t, std::vector<int> > m_rtTable; // map from ip address (u32) to possible ECMP port (index of dev)

	// monitor of PFC
	uint32_t m_bytes[pCnt][pCnt][qCnt]; // m_bytes[inDev][outDev][qidx] is the bytes from inDev enqueued for outDev at qidx
	
	uint64_t m_txBytes[pCnt]; // counter of tx bytes


	uint32_t m_lastPktSize[pCnt];
	uint64_t m_lastPktTs[pCnt]; // ns
	double m_u[pCnt];

	struct BifrostState {
		bool enabled;
		uint64_t deltaBytes;
		uint64_t reservedBytesH;
		Time slotTime;
		uint64_t slotBytes;
		uint32_t k;
		uint64_t fBytes;
		uint64_t lastRxBytes;
		uint64_t tickCount;
		EventId tickEvent;

		BifrostState()
			: enabled(false),
			  deltaBytes(0),
			  reservedBytesH(0),
			  slotTime(Time(0)),
			  slotBytes(0),
			  k(1),
			  fBytes(0),
			  lastRxBytes(0),
			  tickCount(0),
			  tickEvent() {}
	};

protected:
	bool m_ecnEnabled;
	uint32_t m_ccMode;
	uint64_t m_maxRtt;

	uint32_t m_ackHighPrio; // set high priority for ACK/NACK

	// vamsi
	bool PowerEnabled;
	uint32_t m_epsilon; // lpcc epsilon
	uint32_t m_fcnpMinIntervalUs; // lpcc FCNP min send interval per egress queue
	uint32_t m_lpccPerFlowFcnpCooldownUs; // lpcc per-flow fCNP cooldown window
	uint32_t m_lpccFcnpTopK; // lpcc FCNP low-queue fanout top-k flows per congested egress queue
	uint32_t m_lpccFcnpTopKHigh; // lpcc FCNP high-queue fanout top-k flows per congested egress queue
	uint32_t m_lpccFcnpKHighThreshBytes; // switch to high-K when queue exceeds this threshold
	uint32_t m_flowControlMode;
	uint32_t m_transportMode;
	uint32_t m_bifrostTimeSlotUs;
	uint32_t m_bifrostK;
	uint32_t m_bifrostLonghaulDelayCutoffUs;
	uint32_t m_bifrostHMarginSlots;
	bool m_biccEnableEcnClear;
	uint32_t m_biccLonghaulDelayCutoffUs;
	uint32_t m_biccNsFeedbackMinIntervalUs;
	double m_biccDstBdpFactor;
	uint32_t m_biccSoftVoqMaxPkts;
	uint64_t m_biccNsFeedbackCount;
	uint64_t m_biccEcnClearCount;
	uint64_t m_biccSoftVoqEnqueueCount;
	uint64_t m_biccSoftVoqDequeueCount;

private:
	int GetOutDev(Ptr<const Packet>, CustomHeader &ch);
	void SendToDev(Ptr<Packet>p, CustomHeader &ch);
	static uint32_t EcmpHash(const uint8_t* key, size_t len, uint32_t seed);
	void CheckAndSendPfc(uint32_t inDev, uint32_t qIndex);
	void CheckAndSendResume(uint32_t inDev, uint32_t qIndex);
	void ScheduleBifrostTick(uint32_t inDev, uint32_t qIndex);
	void RunBifrostTick(uint32_t inDev, uint32_t qIndex);
	bool IsLpccWanNode() const;

	    Ptr<RDMAFlowTable> m_flowTable; // flow table
	    EventId m_cleanFlowEvent;
	    uint64_t m_flowTableInactiveThresholdNs{50000}; // default 50us; overridable from config
	    uint64_t m_flowTableCleanIntervalNs{50000};     // default 50us; overridable from config
	    bool m_flowTableMaintenance{true};              // false skips Insert*/Clean* for A/B no-op tests
		uint64_t m_lastFcnpSentTs[pCnt][qCnt];
		uint64_t m_lastBiccNsSentTs[pCnt][qCnt];
		BifrostState m_bifrost[pCnt][qCnt];
		struct BiccBufferedPkt {
			Ptr<Packet> packet;
			CustomHeader header;
			uint32_t size;
		};
		struct BiccDstState {
			uint64_t inflightBytes;
			uint64_t queuedBytes;
			uint32_t outDev;
			std::deque<BiccBufferedPkt> queue;
			BiccDstState() : inflightBytes(0), queuedBytes(0), outDev(0), queue() {}
		};
		struct BiccAckState {
			uint32_t lastSeq;
			bool initialized;
			BiccAckState() : lastSeq(0), initialized(false) {}
		};
		struct BiccAckFlowKey {
			uint32_t sip;
			uint32_t dip;
			uint16_t sport;
			uint16_t dport;
			uint16_t pg;
			bool operator==(const BiccAckFlowKey& other) const {
				return sip == other.sip && dip == other.dip && sport == other.sport &&
				       dport == other.dport && pg == other.pg;
			}
		};
		struct BiccAckFlowKeyHasher {
			size_t operator()(const BiccAckFlowKey& key) const {
				size_t h = std::hash<uint32_t>{}(key.sip);
				h = h * 1315423911u + std::hash<uint32_t>{}(key.dip);
				h = h * 1315423911u + std::hash<uint16_t>{}(key.sport);
				h = h * 1315423911u + std::hash<uint16_t>{}(key.dport);
				h = h * 1315423911u + std::hash<uint16_t>{}(key.pg);
				return h;
			}
		};
			std::unordered_map<uint32_t, BiccDstState> m_biccDstState;
			std::unordered_map<BiccAckFlowKey, BiccAckState, BiccAckFlowKeyHasher> m_biccAckState;
			struct LpccFeedbackFlowKey {
				uint32_t sip;
				uint32_t dip;
				uint16_t sport;
				uint16_t dport;
				uint16_t pg;
				bool operator==(const LpccFeedbackFlowKey& other) const {
					return sip == other.sip && dip == other.dip &&
					       sport == other.sport && dport == other.dport &&
					       pg == other.pg;
				}
			};
			struct LpccFeedbackFlowKeyHasher {
				size_t operator()(const LpccFeedbackFlowKey& key) const {
					size_t h = std::hash<uint32_t>{}(key.sip);
					h = h * 1315423911u + std::hash<uint32_t>{}(key.dip);
					h = h * 1315423911u + std::hash<uint16_t>{}(key.sport);
					h = h * 1315423911u + std::hash<uint16_t>{}(key.dport);
					h = h * 1315423911u + std::hash<uint16_t>{}(key.pg);
					return h;
				}
			};
			std::unordered_map<LpccFeedbackFlowKey, uint64_t, LpccFeedbackFlowKeyHasher> m_lpccFlowLastFcnpTs;

    // callback for scheduled flow table cleanup
    void ScheduleCleanFlowTable();
	bool IsLonghaulPort(uint32_t portId) const;
	bool IsSenderSideDciPath(uint32_t inDev, uint32_t outDev) const;
	bool IsReceiverSideDciPath(uint32_t inDev, uint32_t outDev) const;
	uint64_t EstimateBiccDstBudgetBytes(uint32_t outDev) const;
	void MaybeGenerateBiccNearSourceFeedback(uint32_t ifIndex, uint32_t qIndex, uint32_t inDev, Ptr<Packet> p);
	void MaybeApplyBiccEcnClear(uint32_t inDev, uint32_t outDev, Ptr<Packet> p);
	bool MaybeHandleBiccNearDestinationIngress(uint32_t inDev, uint32_t outDev, Ptr<Packet> packet, CustomHeader& ch);
	void MaybeHandleBiccAckRelease(uint32_t inDev, uint32_t outDev, const CustomHeader& ch);
	void DrainBiccDstQueue(uint32_t dstIp);
	BiccAckFlowKey GetBiccAckFlowKey(const CustomHeader& ch) const;
public:
	Ptr<SwitchMmu> m_mmu;

	static TypeId GetTypeId (void);
	SwitchNode();
	~SwitchNode();
	void SetEcmpSeed(uint32_t seed);
	void AddTableEntry(Ipv4Address &dstAddr, uint32_t intf_idx);
	void ClearTable();
	bool SwitchReceiveFromDevice(Ptr<NetDevice> device, Ptr<Packet> packet, CustomHeader &ch);
	void SwitchNotifyDequeue(uint32_t ifIndex, uint32_t qIndex, Ptr<Packet> p);
	Ptr<RDMAFlowTable> GetFlowTable() const { return m_flowTable; }
	void SetFlowTableInactiveThresholdNs(uint64_t ns);
	void SetFlowTableCleanIntervalNs(uint64_t ns) { m_flowTableCleanIntervalNs = ns; }
	void SetFlowTableMaintenance(bool on) { m_flowTableMaintenance = on; }
	void SetEpsilon(uint16_t epsilon) {m_epsilon = epsilon;}
	void ConfigureBifrostPort(uint32_t inPort, uint64_t bdpBytes, uint64_t reservedBytesH, Time slot, uint32_t k);
	void SetBifrostPortEnabled(uint32_t inPort, bool enabled);
	// static uint32_t cnp_count;
	// static uint32_t fcnp_count;

	// for approximate calc in PINT
	int logres_shift(int b, int l);
	int log2apprx(int x, int b, int m, int l); // given x of at most b bits, use most significant m bits of x, calc the result in l bits
};

} /* namespace ns3 */

#endif /* SWITCH_NODE_H */
