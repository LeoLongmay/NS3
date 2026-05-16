#include <ns3/simulator.h>
#include <ns3/seq-ts-header.h>
#include <ns3/udp-header.h>
#include <ns3/ipv4-header.h>
#include "ns3/ppp-header.h"
#include "ns3/boolean.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"
#include "ns3/data-rate.h"
#include "ns3/pointer.h"
#include "rdma-hw.h"
#include "ppp-header.h"
#include "qbb-header.h"
#include "cn-header.h"
#include "ns3/unsched-tag.h"
#include <algorithm>
#include <cmath>

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED(RdmaHw);

TypeId RdmaHw::GetTypeId (void)
{
	static TypeId tid = TypeId ("ns3::RdmaHw")
	                    .SetParent<Object> ()
	                    .AddAttribute("MinRate",
	                                  "Minimum rate of a throttled flow",
	                                  DataRateValue(DataRate("100Mb/s")),
	                                  MakeDataRateAccessor(&RdmaHw::m_minRate),
	                                  MakeDataRateChecker())
	                    .AddAttribute("Mtu",
	                                  "Mtu.",
	                                  UintegerValue(1000),
	                                  MakeUintegerAccessor(&RdmaHw::m_mtu),
	                                  MakeUintegerChecker<uint32_t>())
	                    .AddAttribute ("CcMode",
	                                   "which mode of DCQCN is running",
	                                   UintegerValue(0),
	                                   MakeUintegerAccessor(&RdmaHw::m_cc_mode),
	                                   MakeUintegerChecker<uint32_t>())
	                    .AddAttribute("NACKGenerationInterval",
	                                  "The NACK Generation interval",
	                                  DoubleValue(500.0),
	                                  MakeDoubleAccessor(&RdmaHw::m_nack_interval),
	                                  MakeDoubleChecker<double>())
	                    .AddAttribute("L2ChunkSize",
	                                  "Layer 2 chunk size. Disable chunk mode if equals to 0.",
	                                  UintegerValue(0),
	                                  MakeUintegerAccessor(&RdmaHw::m_chunk),
	                                  MakeUintegerChecker<uint32_t>())
	                    .AddAttribute("L2AckInterval",
	                                  "Layer 2 Ack intervals. Disable ack if equals to 0.",
	                                  UintegerValue(0),
	                                  MakeUintegerAccessor(&RdmaHw::m_ack_interval),
	                                  MakeUintegerChecker<uint32_t>())
	                    .AddAttribute("L2BackToZero",
	                                  "Layer 2 go back to zero transmission.",
	                                  BooleanValue(false),
	                                  MakeBooleanAccessor(&RdmaHw::m_backto0),
	                                  MakeBooleanChecker())
	                    .AddAttribute("EwmaGain",
	                                  "Control gain parameter which determines the level of rate decrease",
	                                  DoubleValue(1.0 / 16),
	                                  MakeDoubleAccessor(&RdmaHw::m_g),
	                                  MakeDoubleChecker<double>())
	                    .AddAttribute ("RateOnFirstCnp",
	                                   "the fraction of rate on first CNP",
	                                   DoubleValue(1.0),
	                                   MakeDoubleAccessor(&RdmaHw::m_rateOnFirstCNP),
	                                   MakeDoubleChecker<double> ())
	                    .AddAttribute("ClampTargetRate",
	                                  "Clamp target rate.",
	                                  BooleanValue(false),
	                                  MakeBooleanAccessor(&RdmaHw::m_EcnClampTgtRate),
	                                  MakeBooleanChecker())
	                    .AddAttribute("RPTimer",
	                                  "The rate increase timer at RP in microseconds",
	                                  DoubleValue(1500.0),
	                                  MakeDoubleAccessor(&RdmaHw::m_rpgTimeReset),
	                                  MakeDoubleChecker<double>())
	                    .AddAttribute("RateDecreaseInterval",
	                                  "The interval of rate decrease check",
	                                  DoubleValue(4.0),
	                                  MakeDoubleAccessor(&RdmaHw::m_rateDecreaseInterval),
	                                  MakeDoubleChecker<double>())
	                    .AddAttribute("FastRecoveryTimes",
	                                  "The rate increase timer at RP",
	                                  UintegerValue(5),
	                                  MakeUintegerAccessor(&RdmaHw::m_rpgThreshold),
	                                  MakeUintegerChecker<uint32_t>())
	                    .AddAttribute("AlphaResumInterval",
	                                  "The interval of resuming alpha",
	                                  DoubleValue(55.0),
	                                  MakeDoubleAccessor(&RdmaHw::m_alpha_resume_interval),
	                                  MakeDoubleChecker<double>())
	                    .AddAttribute("RateAI",
	                                  "Rate increment unit in AI period",
	                                  DataRateValue(DataRate("5Mb/s")),
	                                  MakeDataRateAccessor(&RdmaHw::m_rai),
	                                  MakeDataRateChecker())
	                    .AddAttribute("RateHAI",
	                                  "Rate increment unit in hyperactive AI period",
	                                  DataRateValue(DataRate("50Mb/s")),
	                                  MakeDataRateAccessor(&RdmaHw::m_rhai),
	                                  MakeDataRateChecker())
	                    .AddAttribute("VarWin",
	                                  "Use variable window size or not",
	                                  BooleanValue(false),
	                                  MakeBooleanAccessor(&RdmaHw::m_var_win),
	                                  MakeBooleanChecker())
	                    .AddAttribute("FastReact",
	                                  "Fast React to congestion feedback",
	                                  BooleanValue(true),
	                                  MakeBooleanAccessor(&RdmaHw::m_fast_react),
	                                  MakeBooleanChecker())
	                    .AddAttribute("MiThresh",
	                                  "Threshold of number of consecutive AI before MI",
	                                  UintegerValue(5),
	                                  MakeUintegerAccessor(&RdmaHw::m_miThresh),
	                                  MakeUintegerChecker<uint32_t>())
	                    .AddAttribute("TargetUtil",
	                                  "The Target Utilization of the bottleneck bandwidth, by default 95%",
	                                  DoubleValue(0.95),
	                                  MakeDoubleAccessor(&RdmaHw::m_targetUtil),
	                                  MakeDoubleChecker<double>())
	                    .AddAttribute("UtilHigh",
	                                  "The upper bound of Target Utilization of the bottleneck bandwidth, by default 98%",
	                                  DoubleValue(0.98),
	                                  MakeDoubleAccessor(&RdmaHw::m_utilHigh),
	                                  MakeDoubleChecker<double>())
	                    .AddAttribute("RateBound",
	                                  "Bound packet sending by rate, for test only",
	                                  BooleanValue(true),
	                                  MakeBooleanAccessor(&RdmaHw::m_rateBound),
	                                  MakeBooleanChecker())
	                    .AddAttribute("MultiRate",
	                                  "Maintain multiple rates in HPCC",
	                                  BooleanValue(true),
	                                  MakeBooleanAccessor(&RdmaHw::m_multipleRate),
	                                  MakeBooleanChecker())
	                    .AddAttribute("SampleFeedback",
	                                  "Whether sample feedback or not",
	                                  BooleanValue(false),
	                                  MakeBooleanAccessor(&RdmaHw::m_sampleFeedback),
	                                  MakeBooleanChecker())
	                    .AddAttribute("TimelyAlpha",
	                                  "Alpha of TIMELY",
	                                  DoubleValue(0.875),
	                                  MakeDoubleAccessor(&RdmaHw::m_tmly_alpha),
	                                  MakeDoubleChecker<double>())
	                    .AddAttribute("TimelyBeta",
	                                  "Beta of TIMELY",
	                                  DoubleValue(0.8),
	                                  MakeDoubleAccessor(&RdmaHw::m_tmly_beta),
	                                  MakeDoubleChecker<double>())
	                    .AddAttribute("TimelyTLow",
	                                  "TLow of TIMELY (ns)",
	                                  UintegerValue(50000),
	                                  MakeUintegerAccessor(&RdmaHw::m_tmly_TLow),
	                                  MakeUintegerChecker<uint64_t>())
	                    .AddAttribute("TimelyTHigh",
	                                  "THigh of TIMELY (ns)",
	                                  UintegerValue(500000),
	                                  MakeUintegerAccessor(&RdmaHw::m_tmly_THigh),
	                                  MakeUintegerChecker<uint64_t>())
	                    .AddAttribute("TimelyMinRtt",
	                                  "MinRtt of TIMELY (ns)",
	                                  UintegerValue(20000),
	                                  MakeUintegerAccessor(&RdmaHw::m_tmly_minRtt),
	                                  MakeUintegerChecker<uint64_t>())
		                    .AddAttribute("DctcpRateAI",
		                                  "DCTCP's Rate increment unit in AI period",
		                                  DataRateValue(DataRate("1000Mb/s")),
		                                  MakeDataRateAccessor(&RdmaHw::m_dctcp_rai),
		                                  MakeDataRateChecker())
		                    .AddAttribute("GeminiDelayThreshNs",
		                                  "GEMINI WAN delay threshold in ns",
		                                  UintegerValue(5000000),
		                                  MakeUintegerAccessor(&RdmaHw::m_geminiDelayThreshNs),
		                                  MakeUintegerChecker<uint64_t>())
		                    .AddAttribute("GeminiWanBeta",
		                                  "GEMINI WAN multiplicative decrease factor",
		                                  DoubleValue(0.2),
		                                  MakeDoubleAccessor(&RdmaHw::m_geminiWanBeta),
		                                  MakeDoubleChecker<double>())
		                    .AddAttribute("GeminiH",
		                                  "GEMINI additive increase factor",
		                                  DoubleValue(1.2e-7),
		                                  MakeDoubleAccessor(&RdmaHw::m_geminiH),
		                                  MakeDoubleChecker<double>())
		                    .AddAttribute("GeminiKBytes",
		                                  "GEMINI ECN marking threshold in bytes",
		                                  UintegerValue(64000),
		                                  MakeUintegerAccessor(&RdmaHw::m_geminiKBytes),
		                                  MakeUintegerChecker<uint32_t>())
		                    .AddAttribute("GeminiDcnPortDelayCutoff",
		                                  "Propagation delay cutoff used to identify DCN links",
		                                  TimeValue(MicroSeconds(100)),
		                                  MakeTimeAccessor(&RdmaHw::m_geminiDcnPortDelayCutoff),
		                                  MakeTimeChecker(Time(0), Time::Max()))
		                    .AddAttribute("PintSmplThresh",
		                                  "PINT's sampling threshold in rand()%65536",
		                                  UintegerValue(65536),
	                                  MakeUintegerAccessor(&RdmaHw::pint_smpl_thresh),
	                                  MakeUintegerChecker<uint32_t>())
	                    .AddAttribute("PowerTCPEnabled", "to enable PowerTCP", BooleanValue(false), MakeBooleanAccessor(&RdmaHw::PowerTCPEnabled), MakeBooleanChecker())
	                    .AddAttribute("PowerTCPdelay", "to enable PowerTCP in delaymode", BooleanValue(false), MakeBooleanAccessor(&RdmaHw::PowerTCPdelay), MakeBooleanChecker())
						.AddAttribute("LpccEpsilon", "Buffer queue length threshold", UintegerValue(1000000), MakeUintegerAccessor(&RdmaHw::m_epsilon), MakeUintegerChecker<uint32_t>())
            			.AddAttribute("LpccTheta", "Fcnp aggregate time window", DoubleValue(5000),
                        			MakeDoubleAccessor(&RdmaHw::m_theta), MakeDoubleChecker<double>())
						.AddAttribute("LpccIncreaseInterval", "Rate increase interval", UintegerValue(67),
									MakeUintegerAccessor(&RdmaHw::m_increaseInterval), MakeUintegerChecker<uint64_t>())
						.AddAttribute("LpccIncreaseFactor", "Rate increase factor", DoubleValue(0.19),
									MakeDoubleAccessor(&RdmaHw::m_beta), MakeDoubleChecker<double>())
            			.AddAttribute("LpccTau", "RTT detection time window", UintegerValue(20000),
                          			MakeUintegerAccessor(&RdmaHw::m_tau), MakeUintegerChecker<uint32_t>())
						.AddAttribute("LpccFcnpInterval", "Minimum interval between two consecutive FCNPs", TimeValue(MicroSeconds(100)),
						  			MakeTimeAccessor(&RdmaHw::m_fcnpInvokeInterval), MakeTimeChecker(Time(0), Time::Max()))
            			.AddAttribute("Lpcc_m_wr", "lpcc min rate adjustment fraction", DoubleValue(3.0),
                          			MakeDoubleAccessor(&RdmaHw::m_wr), MakeDoubleChecker<double>())
            			.AddAttribute("Lpcc_m_kr", "lpcc min rate regulation faction", DoubleValue(0.12),
                          			MakeDoubleAccessor(&RdmaHw::m_kr), MakeDoubleChecker<double>())
	            			.AddAttribute("Lpcc_ClampTargetRate", "lpcc clamp target rate.", BooleanValue(false),
	                          			MakeBooleanAccessor(&RdmaHw::m_EcnClampTgtRateLpcc), MakeBooleanChecker())
						.AddAttribute("LpccQueueTargetBytes",
						              "LPCC steady-state queue target in bytes. 0 = derive at runtime from epsilon * LpccQueueTargetRatio.",
						              UintegerValue(0),
						              MakeUintegerAccessor(&RdmaHw::m_lpccQueueTargetBytesCfg),
						              MakeUintegerChecker<uint32_t>())
						.AddAttribute("LpccQueueTargetRatio",
						              "Queue target relative to epsilon, used when LpccQueueTargetBytes == 0.",
						              DoubleValue(0.375),
						              MakeDoubleAccessor(&RdmaHw::m_lpccQueueTargetRatio),
						              MakeDoubleChecker<double>(0.0))
						.AddAttribute("LpccDropCapLow",
						              "Single-step rate-drop cap when qlen <= queue target.",
						              DoubleValue(0.2),
						              MakeDoubleAccessor(&RdmaHw::m_lpccDropCapLow),
						              MakeDoubleChecker<double>(0.0, 1.0))
						.AddAttribute("LpccDropCapHigh",
						              "Single-step rate-drop cap when qlen / queue target >= LpccDropCapHighRatio.",
						              DoubleValue(0.5),
						              MakeDoubleAccessor(&RdmaHw::m_lpccDropCapHigh),
						              MakeDoubleChecker<double>(0.0, 1.0))
						.AddAttribute("LpccDropCapHighRatio",
						              "qlen / queue target threshold above which the high drop cap takes effect.",
						              DoubleValue(2.0),
						              MakeDoubleAccessor(&RdmaHw::m_lpccDropCapHighRatio),
						              MakeDoubleChecker<double>(1.0))
						.AddAttribute("LpccAiSuppressMultiplier",
						              "AI suppression window in units of increaseInterval (after a processed FCNP).",
						              UintegerValue(15),
						              MakeUintegerAccessor(&RdmaHw::m_lpccAiSuppressMultiplier),
						              MakeUintegerChecker<uint32_t>(1))
							.AddAttribute("BiccBlendBaseRttNs",
							              "BiCC blend time constant in ns. 0 means per-flow base RTT.",
							              UintegerValue(0),
							              MakeUintegerAccessor(&RdmaHw::m_biccBlendBaseRttNs),
							              MakeUintegerChecker<uint64_t>())
							;
	return tid;
}

RdmaHw::RdmaHw() {
}

void RdmaHw::SetNode(Ptr<Node> node) {
	m_node = node;
}
void RdmaHw::Setup(QpCompleteCallback cb) {
	for (uint32_t i = 0; i < m_nic.size(); i++) {
		Ptr<QbbNetDevice> dev = m_nic[i].dev;
		if (dev == NULL)
			continue;
		// share data with NIC
		dev->m_rdmaEQ->m_qpGrp = m_nic[i].qpGrp;
		// setup callback
		dev->m_rdmaReceiveCb = MakeCallback(&RdmaHw::Receive, this);
		dev->m_rdmaLinkDownCb = MakeCallback(&RdmaHw::SetLinkDown, this);
		dev->m_rdmaPktSent = MakeCallback(&RdmaHw::PktSent, this);
		// config NIC
		dev->m_rdmaEQ->m_rdmaGetNxtPkt = MakeCallback(&RdmaHw::GetNxtPacket, this);
	}
	// setup qp complete callback
	m_qpCompleteCallback = cb;
}

uint32_t RdmaHw::GetNicIdxOfQp(Ptr<RdmaQueuePair> qp) {
	auto &v = m_rtTable[qp->dip.Get()];
	if (v.size() > 0) {
		return v[qp->GetHash() % v.size()];
	} else {
		NS_ASSERT_MSG(false, "We assume at least one NIC is alive");
	}
}
uint64_t RdmaHw::GetQpKey(uint32_t dip, uint16_t sport, uint16_t pg) {
	return ((uint64_t)dip << 32) | ((uint64_t)sport << 16) | (uint64_t)pg;
}
Ptr<RdmaQueuePair> RdmaHw::GetQp(uint32_t dip, uint16_t sport, uint16_t pg) {
	uint64_t key = GetQpKey(dip, sport, pg);
	auto it = m_qpMap.find(key);
	if (it != m_qpMap.end())
		return it->second;
	return NULL;
}
void RdmaHw::AddQueuePair(uint64_t size, uint16_t pg, Ipv4Address sip, Ipv4Address dip, uint16_t sport, uint16_t dport, uint32_t win, uint64_t baseRtt, uint64_t pathBwBps, Callback<void> notifyAppFinish, Time stopTime) {
	// create qp
	Ptr<RdmaQueuePair> qp = CreateObject<RdmaQueuePair>(pg, sip, dip, sport, dport);
	qp->SetSize(size);
	qp->SetBaseRtt(baseRtt);
	qp->pathBwBps = pathBwBps;
	qp->SetVarWin(m_var_win);
	qp->SetAppNotifyCallback(notifyAppFinish);
	qp->stopTime = stopTime;

	if (stopTime == Simulator::GetMaximumSimulationTime()-MicroSeconds(1)){
		qp->incastFlow = 1;
	}
	else{
		qp->incastFlow = 0;
	}

	// add qp
	uint32_t nic_idx = GetNicIdxOfQp(qp);
	m_nic[nic_idx].qpGrp->AddQp(qp);
	uint64_t key = GetQpKey(dip.Get(), sport, pg);
	m_qpMap[key] = qp;


	qp->powerEnabled = PowerTCPEnabled;

	// set init variables
	if (m_nic[nic_idx].dev == NULL) {
		std::cout << "sip " << sip << " dip " << dip << " sport " << sport  << " dport " << dport << std::endl;
	}
	DataRate m_bps = m_nic[nic_idx].dev->GetDataRate();
	uint32_t initWin = win;
	if (m_cc_mode == 11) {
		// Paper IW = 10 MSS regardless of what caller passed; AIMD ramps up from there
		// to preserve fairness (caller passes maxBdp ≈ 122 MB, which causes monopoly).
		initWin = 10 * m_mtu;
	}
	qp->SetWin(initWin);
	if (win && m_cc_mode != 11)
		qp->SetWin(m_bps.GetBitRate() * 1 * baseRtt * 1e-9 / 8);
	qp->m_rate = m_bps;
	qp->m_max_rate = m_bps;
	if (m_cc_mode == 1) {
		qp->mlx.m_targetRate = m_bps;
	} else if (m_cc_mode == 3) {
		qp->hp.m_curRate = m_bps;
		if (m_multipleRate) {
			for (uint32_t i = 0; i < IntHeader::maxHop; i++)
				qp->hp.hopState[i].Rc = m_bps;
		}
	} else if (m_cc_mode == 7) {
		qp->tmly.m_curRate = m_bps;
		} else if (m_cc_mode == 9) {
			qp->mlx.m_targetRate = m_bps;
	        qp->lpcc.m_targetRate = m_bps;
		} else if (m_cc_mode == 11) {
			qp->useExplicitWin = true;
			qp->explicitWinBytes = qp->m_win;
			qp->gemini.cwndBytes = qp->m_win;
			qp->gemini.rttBaseNs = qp->m_baseRtt;
			qp->gemini.rttMinWindowNs = qp->m_baseRtt;
			qp->gemini.alpha = 1;
			qp->gemini.batchSizePkts = std::max(1u, uint32_t(std::max<uint64_t>(1, qp->m_win / m_mtu)));
		} else if (m_cc_mode == 10) {
			qp->hpccPint.m_curRate = m_bps;
		} else if (m_cc_mode == 12) {
			InitBiCcState(qp);
		}

	// Notify Nic
	m_nic[nic_idx].dev->NewQp(qp);
}

void RdmaHw::DeleteQueuePair(Ptr<RdmaQueuePair> qp) {
	// remove qp from the m_qpMap
	uint64_t key = GetQpKey(qp->dip.Get(), qp->sport, qp->m_pg);
	m_qpMap.erase(key);
}

Ptr<RdmaRxQueuePair> RdmaHw::GetRxQp(uint32_t sip, uint32_t dip, uint16_t sport, uint16_t dport, uint16_t pg, bool create) {
	uint64_t key = ((uint64_t)dip << 32) | ((uint64_t)pg << 16) | (uint64_t)dport;
	auto it = m_rxQpMap.find(key);
	if (it != m_rxQpMap.end())
		return it->second;
	if (create) {
		// create new rx qp
		Ptr<RdmaRxQueuePair> q = CreateObject<RdmaRxQueuePair>();
		// init the qp
		q->sip = sip;
		q->dip = dip;
		q->sport = sport;
		q->dport = dport;
		q->m_ecn_source.qIndex = pg;
		// store in map
		m_rxQpMap[key] = q;
		return q;
	}
	return NULL;
}
uint32_t RdmaHw::GetNicIdxOfRxQp(Ptr<RdmaRxQueuePair> q) {
	auto &v = m_rtTable[q->dip];
	if (v.size() > 0) {
		return v[q->GetHash() % v.size()];
	} else {
		NS_ASSERT_MSG(false, "We assume at least one NIC is alive");
	}
}
void RdmaHw::DeleteRxQp(uint32_t dip, uint16_t pg, uint16_t dport) {
	uint64_t key = ((uint64_t)dip << 32) | ((uint64_t)pg << 16) | (uint64_t)dport;
	m_rxQpMap.erase(key);
}

int RdmaHw::ReceiveUdp(Ptr<Packet> p, CustomHeader &ch) {
	uint8_t ecnbits = ch.GetIpv4EcnBits();

	uint32_t payload_size = p->GetSize() - ch.GetSerializedSize();

	// TODO find corresponding rx queue pair
	Ptr<RdmaRxQueuePair> rxQp = GetRxQp(ch.dip, ch.sip, ch.udp.dport, ch.udp.sport, ch.udp.pg, true);
	if (ecnbits != 0) {
		rxQp->m_ecn_source.ecnbits |= ecnbits;
		rxQp->m_ecn_source.qfb++;
	}
	rxQp->m_ecn_source.total++;
	rxQp->m_milestone_rx = m_ack_interval;

	int x = ReceiverCheckSeq(ch.udp.seq, rxQp, payload_size);
	if (x == 1 || x == 2) { //generate ACK or NACK
		qbbHeader seqh;
		seqh.SetSeq(rxQp->ReceiverNextExpectedSeq);
		seqh.SetPG(ch.udp.pg);
		seqh.SetSport(ch.udp.dport);
		seqh.SetDport(ch.udp.sport);
		seqh.SetIntHeader(ch.udp.ih);
		if (ecnbits) {
			seqh.SetCnp();
		}
			
		Ptr<Packet> newp = Create<Packet>(std::max(60 - 14 - 20 - (int)seqh.GetSerializedSize(), 0));
		newp->AddHeader(seqh);

		Ipv4Header head;	// Prepare IPv4 header
		head.SetDestination(Ipv4Address(ch.sip));
		head.SetSource(Ipv4Address(ch.dip));
		head.SetProtocol(x == 1 ? 0xFC : 0xFD); //ack=0xFC nack=0xFD
		head.SetTtl(64);
		head.SetPayloadSize(newp->GetSize());
		head.SetIdentification(rxQp->m_ipid++);

		newp->AddHeader(head);
		AddHeader(newp, 0x800);	// Attach PPP header
		// send
		uint32_t nic_idx = GetNicIdxOfRxQp(rxQp);
		m_nic[nic_idx].dev->RdmaEnqueueHighPrioQ(newp);
		m_nic[nic_idx].dev->TriggerTransmit();
	}
	return 0;
}

int RdmaHw::ReceiveCnp(Ptr<Packet> p, CustomHeader &ch) {
	// QCN on NIC
	// This is a Congestion signal
	// Then, extract data from the congestion packet.
	// We assume, without verify, the packet is destinated to me
	uint32_t qIndex = ch.cnp.qIndex;
	if (qIndex == 1) {		//DCTCP
		std::cout << "TCP--ignore\n";
		return 0;
	}
	uint16_t udpport = ch.cnp.fid; // corresponds to the sport
	// uint8_t ecnbits = ch.cnp.ecnBits;
	// uint16_t qfb = ch.cnp.qfb;
	// uint16_t total = ch.cnp.total;

	// uint32_t i;
	// get qp
	Ptr<RdmaQueuePair> qp = GetQp(ch.sip, udpport, qIndex);
	if (qp == NULL)
		std::cout << "ERROR: QCN NIC cannot find the flow\n";
	// get nic
	uint32_t nic_idx = GetNicIdxOfQp(qp);
	Ptr<QbbNetDevice> dev = m_nic[nic_idx].dev;

	if (qp->m_rate == 0)			//lazy initialization
	{
		qp->m_rate = dev->GetDataRate();
		if (m_cc_mode == 1) {
			qp->mlx.m_targetRate = dev->GetDataRate();
		} else if (m_cc_mode == 3) {
			qp->hp.m_curRate = dev->GetDataRate();
			if (m_multipleRate) {
				for (uint32_t i = 0; i < IntHeader::maxHop; i++)
					qp->hp.hopState[i].Rc = dev->GetDataRate();
			}
		} else if (m_cc_mode == 7) {
			qp->tmly.m_curRate = dev->GetDataRate();
		} else if (m_cc_mode == 9) { // lpcc
			qp->lpcc.m_targetRate = dev->GetDataRate();
		} else if (m_cc_mode == 10) {
			qp->hpccPint.m_curRate = dev->GetDataRate();
		} else if (m_cc_mode == 12) {
			InitBiCcState(qp);
		}
	}
	return 0;
}

int RdmaHw::ReceiveAck(Ptr<Packet> p, CustomHeader &ch) {
	uint16_t qIndex = ch.ack.pg;
	uint16_t port = ch.ack.dport;
	uint32_t seq = ch.ack.seq;
	uint8_t cnp = (ch.ack.flags >> qbbHeader::FLAG_CNP) & 1;
	// int i;
	Ptr<RdmaQueuePair> qp = GetQp(ch.sip, port, qIndex);
	if (qp == NULL) {
		std::cout << "ERROR: " << "node:" << m_node->GetId() << ' ' << (ch.l3Prot == 0xFC ? "ACK" : "NACK") << " NIC cannot find the flow\n";
		return 0;
	}
	// std::cout << "RTT:" << Simulator::Now().GetTimeStep() - ch.ack.ih.ts << std::endl;

	uint32_t nic_idx = GetNicIdxOfQp(qp);
	Ptr<QbbNetDevice> dev = m_nic[nic_idx].dev;
	if (m_ack_interval == 0)
		std::cout << "ERROR: shouldn't receive ack\n";
	else {
		if (!m_backto0) {
			qp->Acknowledge(seq);
		} else {
			uint32_t goback_seq = seq / m_chunk * m_chunk;
			qp->Acknowledge(goback_seq);
		}
		if (qp->IsFinished()) {
			QpComplete(qp);
		}
	}
	if (ch.l3Prot == 0xFD) // NACK
		RecoverQueue(qp);

	// handle cnp
	if (cnp) {
		if (m_cc_mode == 1) { // mlx version
			cnp_received_mlx(qp);
		} else if (m_cc_mode == 9) { // lpcc
			// Mutual exclusion: if FCNP was received recently, prefer LPCC loop and suppress CNP-driven DCQCN loop.
			uint64_t nowTs = Simulator::Now().GetTimeStep();
			uint64_t guardTs = static_cast<uint64_t>(m_theta) * 1000ULL;
			bool recentFcnp = qp->lpcc.m_lastFcnpTs != 0 && guardTs > 0 &&
			                  (nowTs - qp->lpcc.m_lastFcnpTs) < guardTs;
			if (!recentFcnp) {
				cnp_received_mlx(qp);
			}
		} else if (m_cc_mode == 12) {
			UpdateBiCcEteLoop(qp, true);
		}
	}

	if (m_cc_mode == 3) {
		HandleAckHp(qp, p, ch);
	} else if (m_cc_mode == 7) {
		HandleAckTimely(qp, p, ch);
		} else if (m_cc_mode == 8) {
			HandleAckDctcp(qp, p, ch);
		} else if (m_cc_mode == 9) { // lpcc
		    HandleAckLpcc(qp, p, ch);
	        // HandleAckTimely(qp, p, ch);	
		} else if (m_cc_mode == 11) {
			HandleAckGemini(qp, p, ch);
		} else if (m_cc_mode == 10) {
			HandleAckHpPint(qp, p, ch);
		} else if (m_cc_mode == 12) {
			if (!cnp) {
				UpdateBiCcEteLoop(qp, false);
			}
			BlendBiCcRate(qp);
		}
	// ACK may advance the on-the-fly window, allowing more packets to send
	dev->TriggerTransmit();
	return 0;
}

int RdmaHw::Receive(Ptr<Packet> p, CustomHeader &ch) {
	if (ch.l3Prot == 0x11) { // UDP
		ReceiveUdp(p, ch);
	} else if (ch.l3Prot == 0xFF) { // CNP
		ReceiveCnp(p, ch); // seemingly never used
	} else if (ch.l3Prot == 0xFD) { // NACK
		ReceiveAck(p, ch);
	} else if (ch.l3Prot == 0xFC) { // ACK
		ReceiveAck(p, ch);
		} else if (ch.l3Prot == 0xF9) { // FCNP
			CustomHeader nch(CustomHeader::L2_Header | CustomHeader::L3_Header | CustomHeader::L4_Header);
			Ipv4Header h;
			Ptr<Packet> packet = p->Copy();
		PppHeader ppp;
		packet->RemoveHeader(ppp);
		packet->RemoveHeader(h);
		packet->PeekHeader(nch);
		uint16_t qIndex = nch.fcnp.pg;
			uint16_t port = nch.fcnp.dport;
			Ptr<RdmaQueuePair> qp = GetQp(nch.sip, port, qIndex);
			if (qp == NULL) {
				return 0;
			}
			if (m_cc_mode == 12 && nch.fcnp.qIndex == 1) {
				UpdateBiCcNsLoop(qp, nch);
				BlendBiCcRate(qp);
				return 0;
			}
			if (m_cc_mode == 12) {
				return 0;
			}
			const uint64_t nowTs = Simulator::Now().GetTimeStep();
			qp->lpcc.m_lastCongRateBps = nch.fcnp.linkRateBps;
			qp->lpcc.m_lastFcnpQlen = nch.fcnp.qlen;

			// Note: m_lastFcnpTs / m_lastProcessedFcnpTs are only updated when a FCNP
			// actually triggers a rate decrease (below). A debounce-dropped FCNP must NOT
			// refresh either timestamp, otherwise the AI suppression gate keyed on these
			// timestamps would freeze rate growth permanently during sustained congestion.

			// FCNP debounce/cooldown:
			// ignore repeated FCNP bursts within a short window to avoid excessive
			// back-to-back decreases from clustered feedback packets.
			const uint64_t cooldownUs = std::max<uint64_t>(
				2ULL * static_cast<uint64_t>(m_increaseInterval),
				std::max<uint64_t>(200ULL, static_cast<uint64_t>(m_theta) / 4ULL));
			const uint64_t cooldownTs = cooldownUs * 1000ULL;
			if (qp->lpcc.m_lastProcessedFcnpTs != 0 &&
			    nowTs > qp->lpcc.m_lastProcessedFcnpTs &&
			    nowTs - qp->lpcc.m_lastProcessedFcnpTs < cooldownTs) {
				return 0;
			}

			if (qp->lpcc.m_first_cnp) {
				qp->lpcc.m_lastDecreaseRate = nowTs;
				qp->lpcc.m_lastProcessedFcnpTs = nowTs;
				qp->lpcc.m_lastFcnpTs = nowTs;
				qp->lpcc.m_first_cnp = false;
				UpdateRateLpcc(qp, nch);

			qp->lpcc.m_rpTimeStage = 0;
        	qp->lpcc.m_decrease_cnp_arrived = false;
        	Simulator::Cancel(qp->lpcc.m_rpTimer);
        	qp->lpcc.m_rpTimer = Simulator::Schedule(MicroSeconds(m_increaseInterval),
                                               &RdmaHw::RateIncEventTimerLpcc, this, qp);
			return 0;
		}

			if (nowTs - qp->lpcc.m_lastDecreaseRate >= m_theta * 1000) {
				qp->lpcc.m_lastDecreaseRate = nowTs;
				qp->lpcc.m_lastProcessedFcnpTs = nowTs;
				qp->lpcc.m_lastFcnpTs = nowTs;
				UpdateRateLpcc(qp, nch);

			qp->lpcc.m_rpTimeStage = 0;
        	qp->lpcc.m_decrease_cnp_arrived = false;
        	Simulator::Cancel(qp->lpcc.m_rpTimer);
        	qp->lpcc.m_rpTimer = Simulator::Schedule(MicroSeconds(m_increaseInterval),
                                               &RdmaHw::RateIncEventTimerLpcc, this, qp);
			return 0;
		}
	}
	return 0;
}

int RdmaHw::ReceiverCheckSeq(uint32_t seq, Ptr<RdmaRxQueuePair> q, uint32_t size) {
	uint32_t expected = q->ReceiverNextExpectedSeq;
	if (seq == expected) {
		q->ReceiverNextExpectedSeq = expected + size;
		if (q->ReceiverNextExpectedSeq >= static_cast<uint32_t>(q->m_milestone_rx)) {
			q->m_milestone_rx += m_ack_interval;
			return 1; //Generate ACK
		} else if (q->ReceiverNextExpectedSeq % m_chunk == 0) {
			return 1;
		} else {
			return 5;
		}
	} else if (seq > expected) {
		// Generate NACK
		if (Simulator::Now() >= q->m_nackTimer || q->m_lastNACK != expected) {
			q->m_nackTimer = Simulator::Now() + MicroSeconds(m_nack_interval);
			q->m_lastNACK = expected;
			if (m_backto0) {
				q->ReceiverNextExpectedSeq = q->ReceiverNextExpectedSeq / m_chunk * m_chunk;
			}
			return 2;
		} else
			return 4;
	} else {
		// Duplicate.
		return 3;
	}
}
void RdmaHw::AddHeader (Ptr<Packet> p, uint16_t protocolNumber) {
	PppHeader ppp;
	ppp.SetProtocol (EtherToPpp (protocolNumber));
	p->AddHeader (ppp);
}
uint16_t RdmaHw::EtherToPpp (uint16_t proto) {
	switch (proto) {
	case 0x0800: return 0x0021;   //IPv4
	case 0x86DD: return 0x0057;   //IPv6
	default: NS_ASSERT_MSG (false, "PPP Protocol number not defined!");
	}
	return 0;
}

void RdmaHw::RecoverQueue(Ptr<RdmaQueuePair> qp) {
	qp->snd_nxt = qp->snd_una;
}

void RdmaHw::QpComplete(Ptr<RdmaQueuePair> qp) {
	NS_ASSERT(!m_qpCompleteCallback.IsNull());
	if (m_cc_mode == 1 || m_cc_mode == 9) {
		Simulator::Cancel(qp->mlx.m_eventUpdateAlpha);
		Simulator::Cancel(qp->mlx.m_eventDecreaseRate);
		Simulator::Cancel(qp->mlx.m_rpTimer);
	}

	// This callback will log info
	// It may also delete the rxQp on the receiver
	m_qpCompleteCallback(qp);

	qp->m_notifyAppFinish();

	// delete the qp
	DeleteQueuePair(qp);
}

void RdmaHw::SetLinkDown(Ptr<QbbNetDevice> dev) {
	printf("RdmaHw: node:%u a link down\n", m_node->GetId());
}

void RdmaHw::AddTableEntry(Ipv4Address &dstAddr, uint32_t intf_idx) {
	uint32_t dip = dstAddr.Get();
	m_rtTable[dip].push_back(intf_idx);
}

void RdmaHw::ClearTable() {
	m_rtTable.clear();
}

void RdmaHw::RedistributeQp() {
	// clear old qpGrp
	for (uint32_t i = 0; i < m_nic.size(); i++) {
		if (m_nic[i].dev == NULL)
			continue;
		m_nic[i].qpGrp->Clear();
	}

	// redistribute qp
	for (auto &it : m_qpMap) {
		Ptr<RdmaQueuePair> qp = it.second;
		uint32_t nic_idx = GetNicIdxOfQp(qp);
		m_nic[nic_idx].qpGrp->AddQp(qp);
		// Notify Nic
		m_nic[nic_idx].dev->ReassignedQp(qp);
	}
}

Ptr<Packet> RdmaHw::GetNxtPacket(Ptr<RdmaQueuePair> qp) {
	uint32_t payload_size = qp->GetBytesLeft();
	if (m_mtu < payload_size)
		payload_size = m_mtu;
	Ptr<Packet> p = Create<Packet> (payload_size);
	uint32_t sentBytes = qp->m_size - qp->GetBytesLeft();
	uint32_t nic_idx = GetNicIdxOfQp(qp);
	DataRate m_bps = m_nic[nic_idx].dev->GetDataRate();
	double bdp = m_bps.GetBitRate() * 1 * qp->m_baseRtt * 1e-9 / 8;
	UnSchedTag unschedtag;
	if (sentBytes <= bdp){
		unschedtag.SetValue(1);
	}
	else{
		unschedtag.SetValue(0);
	}
	p->AddPacketTag(unschedtag);
	// add SeqTsHeader
	SeqTsHeader seqTs;
	seqTs.SetSeq (qp->snd_nxt);
	seqTs.SetPG (qp->m_pg);
	p->AddHeader (seqTs);
	// add udp header
	UdpHeader udpHeader;
	udpHeader.SetDestinationPort (qp->dport);
	udpHeader.SetSourcePort (qp->sport);
	p->AddHeader (udpHeader);
	// add ipv4 header
	Ipv4Header ipHeader;
	ipHeader.SetSource (qp->sip);
	ipHeader.SetDestination (qp->dip);
	ipHeader.SetProtocol (0x11);
	ipHeader.SetPayloadSize (p->GetSize());
	ipHeader.SetTtl (64);
	ipHeader.SetTos (0);
	ipHeader.SetIdentification (qp->m_ipid);
	p->AddHeader(ipHeader);
	// add ppp header
	PppHeader ppp;
	ppp.SetProtocol (0x0021); // EtherToPpp(0x800), see point-to-point-net-device.cc
	p->AddHeader (ppp);

	// update state
	qp->snd_nxt += payload_size;
	qp->m_ipid++;

	// return
	return p;
}

void RdmaHw::PktSent(Ptr<RdmaQueuePair> qp, Ptr<Packet> pkt, Time interframeGap) {
	qp->lastPktSize = pkt->GetSize();
//	SeqTsHeader seqTs;
//	pkt->PeekHeader(seqTs);
	// uint32_t seq = qp->snd_nxt;
	qp->rates[qp->snd_nxt] = Simulator::Now().GetNanoSeconds();
	UpdateNextAvail(qp, interframeGap, pkt->GetSize());

}

void RdmaHw::UpdateNextAvail(Ptr<RdmaQueuePair> qp, Time interframeGap, uint32_t pkt_size) {
	Time sendingTime;
	DataRate effectiveRate = qp->m_rate.GetBitRate() == 0 ? m_minRate : qp->m_rate;
	if (m_rateBound)
		sendingTime = interframeGap + effectiveRate.CalculateBytesTxTime(pkt_size);
	else
		sendingTime = interframeGap + qp->m_max_rate.CalculateBytesTxTime(pkt_size);
	qp->m_nextAvail = Simulator::Now() + sendingTime;
}

void RdmaHw::ChangeRate(Ptr<RdmaQueuePair> qp, DataRate new_rate) {
#if 1
	DataRate oldRate = qp->m_rate.GetBitRate() == 0 ? m_minRate : qp->m_rate;
	DataRate effectiveNewRate = new_rate.GetBitRate() == 0 ? m_minRate : new_rate;
	Time sendingTime = oldRate.CalculateBytesTxTime(qp->lastPktSize);
	Time new_sendintTime = effectiveNewRate.CalculateBytesTxTime(qp->lastPktSize);
	qp->m_nextAvail = qp->m_nextAvail + new_sendintTime - sendingTime;
	// update nic's next avail event
	uint32_t nic_idx = GetNicIdxOfQp(qp);
	m_nic[nic_idx].dev->UpdateNextAvail(qp->m_nextAvail);
#endif
	// change to new rate
	qp->m_rate = effectiveNewRate;
}

#define PRINT_LOG 0
/******************************
 * Mellanox's version of DCQCN
 *****************************/
void RdmaHw::UpdateAlphaMlx(Ptr<RdmaQueuePair> q) {
#if PRINT_LOG
	//std::cout << Simulator::Now() << " alpha update:" << m_node->GetId() << ' ' << q->mlx.m_alpha << ' ' << (int)q->mlx.m_alpha_cnp_arrived << '\n';
	//printf("%lu alpha update: %08x %08x %u %u %.6lf->", Simulator::Now().GetTimeStep(), q->sip.Get(), q->dip.Get(), q->sport, q->dport, q->mlx.m_alpha);
#endif
	if (q->mlx.m_alpha_cnp_arrived) {
		q->mlx.m_alpha = (1 - m_g) * q->mlx.m_alpha + m_g; 	//binary feedback
	} else {
		q->mlx.m_alpha = (1 - m_g) * q->mlx.m_alpha; 	//binary feedback
	}
#if PRINT_LOG
	//printf("%.6lf\n", q->mlx.m_alpha);
#endif
	q->mlx.m_alpha_cnp_arrived = false; // clear the CNP_arrived bit
	ScheduleUpdateAlphaMlx(q);
}
void RdmaHw::ScheduleUpdateAlphaMlx(Ptr<RdmaQueuePair> q) {
	q->mlx.m_eventUpdateAlpha = Simulator::Schedule(MicroSeconds(m_alpha_resume_interval), &RdmaHw::UpdateAlphaMlx, this, q);
}

void RdmaHw::cnp_received_mlx(Ptr<RdmaQueuePair> q) {
	q->mlx.m_alpha_cnp_arrived = true; // set CNP_arrived bit for alpha update
	q->mlx.m_decrease_cnp_arrived = true; // set CNP_arrived bit for rate decrease
	if (q->mlx.m_first_cnp) {
		// init alpha
		q->mlx.m_alpha = 1;
		q->mlx.m_alpha_cnp_arrived = false;
		// schedule alpha update
		ScheduleUpdateAlphaMlx(q);
		// schedule rate decrease
		ScheduleDecreaseRateMlx(q, 1); // add 1 ns to make sure rate decrease is after alpha update
		// set rate on first CNP
		q->mlx.m_targetRate = q->m_rate = m_rateOnFirstCNP * q->m_rate;
		q->mlx.m_first_cnp = false;
	}
}

void RdmaHw::CheckRateDecreaseMlx(Ptr<RdmaQueuePair> q) {
	ScheduleDecreaseRateMlx(q, 0);
	if (q->mlx.m_decrease_cnp_arrived) {
#if PRINT_LOG
		printf("%lu rate dec: %08x %08x %u %u (%0.3lf %.3lf)->", Simulator::Now().GetTimeStep(), q->sip.Get(), q->dip.Get(), q->sport, q->dport, q->mlx.m_targetRate.GetBitRate() * 1e-9, q->m_rate.GetBitRate() * 1e-9);
#endif
		bool clamp = true;
		if (!m_EcnClampTgtRate) {
			if (q->mlx.m_rpTimeStage == 0)
				clamp = false;
		}
		if (clamp)
			q->mlx.m_targetRate = q->m_rate;
		q->m_rate = std::max(m_minRate, q->m_rate * (1 - q->mlx.m_alpha / 2));
		// reset rate increase related things
		q->mlx.m_rpTimeStage = 0;
		q->mlx.m_decrease_cnp_arrived = false;
		Simulator::Cancel(q->mlx.m_rpTimer);
		q->mlx.m_rpTimer = Simulator::Schedule(MicroSeconds(m_rpgTimeReset), &RdmaHw::RateIncEventTimerMlx, this, q);
#if PRINT_LOG
		printf("(%.3lf %.3lf)\n", q->mlx.m_targetRate.GetBitRate() * 1e-9, q->m_rate.GetBitRate() * 1e-9);
#endif
	}
}
void RdmaHw::ScheduleDecreaseRateMlx(Ptr<RdmaQueuePair> q, uint32_t delta) {
	q->mlx.m_eventDecreaseRate = Simulator::Schedule(MicroSeconds(m_rateDecreaseInterval) + NanoSeconds(delta), &RdmaHw::CheckRateDecreaseMlx, this, q);
}

void RdmaHw::RateIncEventTimerMlx(Ptr<RdmaQueuePair> q) {
	q->mlx.m_rpTimer = Simulator::Schedule(MicroSeconds(m_rpgTimeReset), &RdmaHw::RateIncEventTimerMlx, this, q);
	RateIncEventMlx(q);
	q->mlx.m_rpTimeStage++;
}
void RdmaHw::RateIncEventMlx(Ptr<RdmaQueuePair> q) {
	// check which increase phase: fast recovery, active increase, hyper increase
	if (q->mlx.m_rpTimeStage < m_rpgThreshold) { // fast recovery
		FastRecoveryMlx(q);
	} else if (q->mlx.m_rpTimeStage == m_rpgThreshold) { // active increase
		ActiveIncreaseMlx(q);
	} else { // hyper increase
		HyperIncreaseMlx(q);
	}
}

void RdmaHw::FastRecoveryMlx(Ptr<RdmaQueuePair> q) {
#if PRINT_LOG
	printf("%lu fast recovery: %08x %08x %u %u (%0.3lf %.3lf)->", Simulator::Now().GetTimeStep(), q->sip.Get(), q->dip.Get(), q->sport, q->dport, q->mlx.m_targetRate.GetBitRate() * 1e-9, q->m_rate.GetBitRate() * 1e-9);
#endif
	q->m_rate = (q->m_rate / 2) + (q->mlx.m_targetRate / 2);
#if PRINT_LOG
	printf("(%.3lf %.3lf)\n", q->mlx.m_targetRate.GetBitRate() * 1e-9, q->m_rate.GetBitRate() * 1e-9);
#endif
}
void RdmaHw::ActiveIncreaseMlx(Ptr<RdmaQueuePair> q) {
#if PRINT_LOG
	printf("%lu active inc: %08x %08x %u %u (%0.3lf %.3lf)->", Simulator::Now().GetTimeStep(), q->sip.Get(), q->dip.Get(), q->sport, q->dport, q->mlx.m_targetRate.GetBitRate() * 1e-9, q->m_rate.GetBitRate() * 1e-9);
#endif
	// get NIC
	uint32_t nic_idx = GetNicIdxOfQp(q);
	Ptr<QbbNetDevice> dev = m_nic[nic_idx].dev;
	// increate rate
	q->mlx.m_targetRate += m_rai;
	if (q->mlx.m_targetRate > dev->GetDataRate())
		q->mlx.m_targetRate = dev->GetDataRate();
	q->m_rate = (q->m_rate / 2) + (q->mlx.m_targetRate / 2);
#if PRINT_LOG
	printf("(%.3lf %.3lf)\n", q->mlx.m_targetRate.GetBitRate() * 1e-9, q->m_rate.GetBitRate() * 1e-9);
#endif
}
void RdmaHw::HyperIncreaseMlx(Ptr<RdmaQueuePair> q) {
#if PRINT_LOG
	printf("%lu hyper inc: %08x %08x %u %u (%0.3lf %.3lf)->", Simulator::Now().GetTimeStep(), q->sip.Get(), q->dip.Get(), q->sport, q->dport, q->mlx.m_targetRate.GetBitRate() * 1e-9, q->m_rate.GetBitRate() * 1e-9);
#endif
	// get NIC
	uint32_t nic_idx = GetNicIdxOfQp(q);
	Ptr<QbbNetDevice> dev = m_nic[nic_idx].dev;
	// increate rate
	q->mlx.m_targetRate += m_rhai;
	if (q->mlx.m_targetRate > dev->GetDataRate())
		q->mlx.m_targetRate = dev->GetDataRate();
	q->m_rate = (q->m_rate / 2) + (q->mlx.m_targetRate / 2);
#if PRINT_LOG
	printf("(%.3lf %.3lf)\n", q->mlx.m_targetRate.GetBitRate() * 1e-9, q->m_rate.GetBitRate() * 1e-9);
#endif
}

/**********************
 * BiCC
 *********************/
void RdmaHw::InitBiCcState(Ptr<RdmaQueuePair> qp) {
	if (qp->bicc.initialized) {
		return;
	}
	DataRate init = qp->m_rate.GetBitRate() > 0 ? qp->m_rate : qp->m_max_rate;
	if (init.GetBitRate() == 0) {
		init = m_minRate;
	}
	qp->bicc.nsRate = init;
	qp->bicc.eteRate = init;
	qp->bicc.blendedRate = init;
	qp->bicc.lastBlendTsNs = Simulator::Now().GetTimeStep();
	qp->bicc.lastNsAiTsNs = qp->bicc.lastBlendTsNs;
	qp->bicc.lastEteAiTsNs = qp->bicc.lastBlendTsNs;
	qp->bicc.initialized = true;
}

void RdmaHw::UpdateBiCcNsLoop(Ptr<RdmaQueuePair> qp, const CustomHeader& ch) {
	InitBiCcState(qp);
	uint64_t now = Simulator::Now().GetTimeStep();
	uint64_t linkRateBps = ch.fcnp.linkRateBps > 0 ? ch.fcnp.linkRateBps : qp->m_max_rate.GetBitRate();
	uint64_t refRttNs = m_biccBlendBaseRttNs > 0 ? m_biccBlendBaseRttNs : std::max<uint64_t>(qp->m_baseRtt, 1);
	double budgetBytes = (double(linkRateBps) * double(refRttNs)) / 8e9;
	if (budgetBytes < 1.0) {
		budgetBytes = 1.0;
	}
	double pressure = std::min(1.0, ch.fcnp.qlen / budgetBytes);
	double curBps = std::max<double>(qp->bicc.nsRate.GetBitRate(), m_minRate.GetBitRate());
	double dec = 1.0 - 0.5 * pressure;
	double nextBps = std::max<double>(m_minRate.GetBitRate(), curBps * dec);
	qp->bicc.nsRate = DataRate(uint64_t(nextBps));
	qp->bicc.lastNsAiTsNs = now;
}

void RdmaHw::UpdateBiCcEteLoop(Ptr<RdmaQueuePair> qp, bool cnp) {
	InitBiCcState(qp);
	uint64_t now = Simulator::Now().GetTimeStep();
	double curBps = std::max<double>(qp->bicc.eteRate.GetBitRate(), m_minRate.GetBitRate());
	if (cnp) {
		double nextBps = std::max<double>(m_minRate.GetBitRate(), curBps * 0.8);
		qp->bicc.eteRate = DataRate(uint64_t(nextBps));
		qp->bicc.lastEteAiTsNs = now;
		return;
	}
	uint64_t aiIntervalNs = std::max<uint64_t>(1, uint64_t(m_rpgTimeReset * 1000.0));
	if (now - qp->bicc.lastEteAiTsNs >= aiIntervalNs) {
		uint64_t maxRate = std::max<uint64_t>(qp->m_max_rate.GetBitRate(), m_minRate.GetBitRate());
		uint64_t nextBps = std::min<uint64_t>(maxRate, qp->bicc.eteRate.GetBitRate() + m_rai.GetBitRate());
		qp->bicc.eteRate = DataRate(nextBps);
		qp->bicc.lastEteAiTsNs = now;
	}
}

void RdmaHw::BlendBiCcRate(Ptr<RdmaQueuePair> qp) {
	InitBiCcState(qp);
	uint64_t now = Simulator::Now().GetTimeStep();
	uint64_t T = m_biccBlendBaseRttNs > 0 ? m_biccBlendBaseRttNs : std::max<uint64_t>(qp->m_baseRtt, 1);
	uint64_t tau = now > qp->bicc.lastBlendTsNs ? now - qp->bicc.lastBlendTsNs : 0;
	if (tau > T) {
		tau = T;
	}
	uint64_t rmin = std::min<uint64_t>(qp->bicc.nsRate.GetBitRate(), qp->bicc.eteRate.GetBitRate());
	uint64_t blended = qp->bicc.blendedRate.GetBitRate();
	if (blended == 0) {
		blended = rmin;
	}
	double w = T == 0 ? 1.0 : double(tau) / double(T);
	double nextBps = (1.0 - w) * double(blended) + w * double(rmin);
	uint64_t minBps = m_minRate.GetBitRate();
	uint64_t maxBps = std::max<uint64_t>(qp->m_max_rate.GetBitRate(), minBps);
	uint64_t clipped = std::min<uint64_t>(maxBps, std::max<uint64_t>(minBps, uint64_t(nextBps)));
	qp->bicc.blendedRate = DataRate(clipped);
	qp->m_rate = qp->bicc.blendedRate;
	qp->bicc.lastBlendTsNs = now;
}

/***********************
 * High Precision CC
 ***********************/
void RdmaHw::HandleAckHp(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader &ch) {
	uint32_t ack_seq = ch.ack.seq;
	// update rate
	if (ack_seq > qp->hp.m_lastUpdateSeq) { // if full RTT feedback is ready, do full update
		if (PowerTCPEnabled) {
			UpdateRatePower(qp, p, ch, false);
		}
		else
			UpdateRateHp(qp, p, ch, false);
	} else { // do fast react
		if (PowerTCPEnabled)
			FastReactPower(qp, p, ch);
		else
			FastReactHp(qp, p, ch);
	}
}

void RdmaHw::UpdateRateHp(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader &ch, bool fast_react) {
	uint32_t next_seq = qp->snd_nxt;
	// bool print = !fast_react || true;


	if (qp->hp.m_lastUpdateSeq == 0) { // first RTT

		qp->hp.m_lastUpdateSeq = next_seq;
		// store INT
		IntHeader &ih = ch.ack.ih;
		NS_ASSERT(ih.nhop <= IntHeader::maxHop);
		for (uint32_t i = 0; i < ih.nhop; i++)
			qp->hp.hop[i] = ih.hop[i];
#if PRINT_LOG
		if (print) {
			printf("%lu %s %08x %08x %u %u [%u,%u,%u]", Simulator::Now().GetTimeStep(), fast_react ? "fast" : "update", qp->sip.Get(), qp->dip.Get(), qp->sport, qp->dport, qp->hp.m_lastUpdateSeq, ch.ack.seq, next_seq);
			for (uint32_t i = 0; i < ih.nhop; i++)
				printf(" %u %lu %lu", ih.hop[i].GetQlen(), ih.hop[i].GetBytes(), ih.hop[i].GetTime());
			printf("\n");
		}
#endif
	} else {
		// check packet INT
		IntHeader &ih = ch.ack.ih;
		if (ih.nhop <= IntHeader::maxHop) {
			double max_c = 0;
			// bool inStable = false;
#if PRINT_LOG
			if (print)
				printf("%lu %s %08x %08x %u %u [%u,%u,%u]", Simulator::Now().GetTimeStep(), fast_react ? "fast" : "update", qp->sip.Get(), qp->dip.Get(), qp->sport, qp->dport, qp->hp.m_lastUpdateSeq, ch.ack.seq, next_seq);
#endif
			// check each hop
			double U = 0;
			uint64_t dt = 0;
			bool updated[IntHeader::maxHop] = {false}, updated_any = false;
			NS_ASSERT(ih.nhop <= IntHeader::maxHop);
			for (uint32_t i = 0; i < ih.nhop; i++) {
				if (m_sampleFeedback) {
					if (ih.hop[i].GetQlen() == 0 and fast_react)
						continue;
				}
				updated[i] = updated_any = true;
#if PRINT_LOG
				if (print)
					printf(" %u(%u) %lu(%lu) %lu(%lu)", ih.hop[i].GetQlen(), qp->hp.hop[i].GetQlen(), ih.hop[i].GetBytes(), qp->hp.hop[i].GetBytes(), ih.hop[i].GetTime(), qp->hp.hop[i].GetTime());
#endif
				uint64_t tau = ih.hop[i].GetTimeDelta(qp->hp.hop[i]);
				double duration = tau * 1e-9;
				double txRate = (ih.hop[i].GetBytesDelta(qp->hp.hop[i])) * 8 / duration;

				double u;
				u = txRate / ih.hop[i].GetLineRate() + (double)std::min(ih.hop[i].GetQlen(), qp->hp.hop[i].GetQlen()) * qp->m_max_rate.GetBitRate() / ih.hop[i].GetLineRate() / qp->m_win;


#if PRINT_LOG
				if (print)
					printf(" %.3lf %.3lf", txRate, u);
#endif
				if (!m_multipleRate) {
					// for aggregate (single R)
					if (u > U) {
						U = u;
						dt = tau;
					}
				} else {
					// for per hop (per hop R)
					if (tau > qp->m_baseRtt)
						tau = qp->m_baseRtt;
					qp->hp.hopState[i].u = (qp->hp.hopState[i].u * (qp->m_baseRtt - tau) + u * tau) / double(qp->m_baseRtt);
				}
				qp->hp.hop[i] = ih.hop[i];
			}

			DataRate new_rate;
			int32_t new_incStage;
			DataRate new_rate_per_hop[IntHeader::maxHop];
			int32_t new_incStage_per_hop[IntHeader::maxHop];
			if (!m_multipleRate) {
				// for aggregate (single R)
				if (updated_any) {
					if (dt > 1.0 * qp->m_baseRtt)
						dt = 1.0 * qp->m_baseRtt;


					qp->hp.u = (qp->hp.u * (qp->m_baseRtt - dt) + U * dt) / double(qp->m_baseRtt);
					max_c = qp->hp.u / m_targetUtil;

					if (max_c >= 1 || qp->hp.m_incStage >= m_miThresh) {
						new_rate = qp->hp.m_curRate / max_c + m_rai;
						new_incStage = 0;
					}
					else {
						new_rate = qp->hp.m_curRate + m_rai;
						new_incStage = qp->hp.m_incStage + 1;
					}

					if (new_rate < m_minRate)
						new_rate = m_minRate;
					if (new_rate > qp->m_max_rate)
						new_rate = qp->m_max_rate;
#if PRINT_LOG
					if (print)
						printf(" u=%.6lf U=%.3lf dt=%u max_c=%.3lf", qp->hp.u, U, dt, max_c);
#endif
#if PRINT_LOG
					if (print)
						printf(" rate:%.3lf->%.3lf\n", qp->hp.m_curRate.GetBitRate() * 1e-9, new_rate.GetBitRate() * 1e-9);
#endif
				}
			} else {
				// for per hop (per hop R)
				new_rate = qp->m_max_rate;
				for (uint32_t i = 0; i < ih.nhop; i++) {
					if (updated[i]) {
						double c = qp->hp.hopState[i].u / m_targetUtil;
						if (c >= 1 || qp->hp.hopState[i].incStage >= m_miThresh) {
							new_rate_per_hop[i] = qp->hp.hopState[i].Rc / c + m_rai;
							new_incStage_per_hop[i] = 0;
						} else {
							new_rate_per_hop[i] = qp->hp.hopState[i].Rc + m_rai;
							new_incStage_per_hop[i] = qp->hp.hopState[i].incStage + 1;
						}
						// bound rate
						if (new_rate_per_hop[i] < m_minRate)
							new_rate_per_hop[i] = m_minRate;
						if (new_rate_per_hop[i] > qp->m_max_rate)
							new_rate_per_hop[i] = qp->m_max_rate;
						// find min new_rate
						if (new_rate_per_hop[i] < new_rate)
							new_rate = new_rate_per_hop[i];
#if PRINT_LOG
						if (print)
							printf(" [%u]u=%.6lf c=%.3lf", i, qp->hp.hopState[i].u, c);
#endif
#if PRINT_LOG
						if (print)
							printf(" %.3lf->%.3lf", qp->hp.hopState[i].Rc.GetBitRate() * 1e-9, new_rate.GetBitRate() * 1e-9);
#endif
					} else {
						if (qp->hp.hopState[i].Rc < new_rate)
							new_rate = qp->hp.hopState[i].Rc;
					}
				}
#if PRINT_LOG
				printf("\n");
#endif
			}

			if (updated_any) {
				ChangeRate(qp, new_rate);
			}
			if (!fast_react) {
				if (updated_any) {
					qp->hp.m_curRate = new_rate;
					qp->hp.m_incStage = new_incStage;
				}
				if (m_multipleRate) {
					// for per hop (per hop R)
					for (uint32_t i = 0; i < ih.nhop; i++) {
						if (updated[i]) {
							qp->hp.hopState[i].Rc = new_rate_per_hop[i];
							qp->hp.hopState[i].incStage = new_incStage_per_hop[i];
						}
					}
				}
			}
		}
		if (!fast_react) {
			if (next_seq > qp->hp.m_lastUpdateSeq)
				qp->hp.m_lastUpdateSeq = next_seq; //+ rand() % 2 * m_mtu;
		}
	}
}

void RdmaHw::FastReactHp(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader &ch) {
	if (m_fast_react)
		UpdateRateHp(qp, p, ch, true);
}


/**********************
 * PowerTCP (Int/Delay versions) called from HandleAckHp function at the moment
 *********************/

void RdmaHw::UpdateRatePower(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader &ch, bool fast_react) {
	uint32_t next_seq = qp->snd_nxt;
	// bool print = !fast_react || true;
	double prevRtt = qp->m_baseRtt;
	double prevCompletion = Simulator::Now().GetNanoSeconds();
	std::map<uint32_t, double>::iterator it = qp->rates.find(ch.ack.seq);
	DataRate old ;
	// double rtt;

	if (it != qp->rates.end()) {
		prevRtt = Simulator::Now().GetNanoSeconds() - it->second;
		if (PowerTCPdelay) {
			qp->m_baseRtt = std::min(uint64_t(Simulator::Now().GetNanoSeconds() - it->second), qp->m_baseRtt);
		}
		prevCompletion = Simulator::Now().GetNanoSeconds();
        qp->rates.erase(it);
	}
	if (qp->hp.m_lastUpdateSeq == 0 && !PowerTCPdelay) {
		qp->prevRtt = prevRtt;
		qp->prevCompletion = Simulator::Now().GetNanoSeconds();
		qp->hp.m_lastUpdateSeq = next_seq;
		// store INT
		IntHeader &ih = ch.ack.ih;
		NS_ASSERT(ih.nhop <= IntHeader::maxHop);
		for (uint32_t i = 0; i < ih.nhop; i++)
			qp->hp.hop[i] = ih.hop[i];
	}else {
		// check packet INT
		IntHeader &ih = ch.ack.ih;
		if (ih.nhop <= IntHeader::maxHop) {
			double max_c = 0;
			// bool inStable = false;
			// check each hop
			double U = 0;
			uint64_t dt = 0;
			// bool updated[IntHeader::maxHop] = {false};
			bool updated_any = false;
			NS_ASSERT(ih.nhop <= IntHeader::maxHop);
			for (uint32_t i = 0; i < ih.nhop; i++) {
				if (m_sampleFeedback) {
					if (ih.hop[i].GetQlen() == 0 and fast_react)
						continue;
				}
				// updated[i] = true;
				updated_any = true;

				uint64_t tau = ih.hop[i].GetTimeDelta(qp->hp.hop[i]);
				double duration = tau * 1e-9;
				double rxRate = (ih.hop[i].GetBytesDelta(qp->hp.hop[i])) * 8.0 / duration;

				double u;

				if (!PowerTCPdelay) {
					double A = rxRate;
					// double A = txRate + (double(ih.hop[i].GetQlen() * 8.0) - double(qp->hp.hop[i].GetQlen() * 8.0)) / duration;
					double power = ( A ) * (double(ih.hop[i].GetQlen() * 8.0) + ih.hop[i].GetLineRate() * (qp->m_baseRtt * 1e-9));
					double powerx = (power) / (ih.hop[i].GetLineRate() * (ih.hop[i].GetLineRate() * qp->m_baseRtt * 1e-9) );
					u = powerx; // PowerTCP
				}
				else {
					// delay approach
					double A = ( double(prevRtt - qp->prevRtt) / (prevCompletion - qp->prevCompletion) + 1  );
					if (A < 0.5)
						A = 0.5;
					double power = ( A ) * (prevRtt);
					double powerx = (power) / (1.05 * qp->m_baseRtt);
					u = powerx; // theta-PowerTCP
				}
				if (u > U) {
					U = u;
					if (PowerTCPdelay) {
						dt = prevCompletion - qp->prevCompletion;
					}
					else {
						dt = tau;
					}
				}
				qp->hp.hop[i] = ih.hop[i];
			}

			DataRate new_rate;
			int32_t new_incStage = 0;
			// DataRate new_rate_per_hop[IntHeader::maxHop];
			// int32_t new_incStage_per_hop[IntHeader::maxHop];

			if (updated_any) {
				if (dt > 1.0 * qp->m_baseRtt)
					dt = 1.0 * qp->m_baseRtt;

				if (U < 0) {
					U = qp->hp.u;
				}
				qp->hp.u = (qp->hp.u * (1.0 * qp->m_baseRtt - dt) + U * dt) / double(1.0 * qp->m_baseRtt);
				if (!PowerTCPdelay) {
					max_c = qp->hp.u / m_targetUtil;
					new_rate = (0.9 * ( qp->hp.m_curRate / max_c + DataRate("150Mbps") ) + 0.1 * qp->hp.m_curRate);

				}
				else {
					max_c = qp->hp.u;
					new_rate = (0.7 * ( qp->hp.m_curRate / max_c + DataRate("150Mbps") ) + 0.3 * qp->hp.m_curRate);
				}
				if (new_rate < m_minRate)
					new_rate = m_minRate;
				if (new_rate > qp->m_max_rate)
					new_rate = qp->m_max_rate;
			}
			qp->prevRtt = prevRtt;
			qp->prevCompletion = Simulator::Now().GetNanoSeconds();
			if (updated_any) {
				ChangeRate(qp, new_rate);
			}
			if (!fast_react) {
				if (updated_any) {
					qp->hp.m_curRate = new_rate;
					qp->hp.m_incStage = new_incStage;
				}
			}
		}
		if (!fast_react) {
			if (next_seq > qp->hp.m_lastUpdateSeq)
				qp->hp.m_lastUpdateSeq = next_seq;
		}
	}
}

void RdmaHw::FastReactPower(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader &ch) {
	if (m_fast_react)
		UpdateRatePower(qp, p, ch, true);
}

/**********************
 * TIMELY
 *********************/
void RdmaHw::HandleAckTimely(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader &ch) {
	uint32_t ack_seq = ch.ack.seq;
	// update rate
	if (ack_seq > qp->tmly.m_lastUpdateSeq) { // if full RTT feedback is ready, do full update
		UpdateRateTimely(qp, p, ch, false);
	} else { // do fast react
		FastReactTimely(qp, p, ch);
	}
}
void RdmaHw::UpdateRateTimely(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader &ch, bool us) {
	uint32_t next_seq = qp->snd_nxt;
	uint64_t rtt = Simulator::Now().GetTimeStep() - ch.ack.ih.ts;
	// bool print = !us;
	if (qp->tmly.m_lastUpdateSeq != 0) { // not first RTT
		int64_t new_rtt_diff = (int64_t)rtt - (int64_t)qp->tmly.lastRtt;
		double rtt_diff = (1 - m_tmly_alpha) * qp->tmly.rttDiff + m_tmly_alpha * new_rtt_diff;
		double gradient = rtt_diff / m_tmly_minRtt;
		bool inc = false;
		double c = 0;
#if PRINT_LOG
		if (print)
			printf("%lu node:%u rtt:%lu rttDiff:%.0lf gradient:%.3lf rate:%.3lf", Simulator::Now().GetTimeStep(), m_node->GetId(), rtt, rtt_diff, gradient, qp->tmly.m_curRate.GetBitRate() * 1e-9);
#endif
		if (rtt < m_tmly_TLow) {
			inc = true;
		} else if (rtt > m_tmly_THigh) {
			c = 1 - m_tmly_beta * (1 - (double)m_tmly_THigh / rtt);
			inc = false;
		} else if (gradient <= 0) {
			inc = true;
		} else {
			c = 1 - m_tmly_beta * gradient;
			if (c < 0)
				c = 0;
			inc = false;
		}
		if (inc) {
			if (qp->tmly.m_incStage < 5) {
				qp->m_rate = qp->tmly.m_curRate + m_rai;
			} else {
				qp->m_rate = qp->tmly.m_curRate + m_rhai;
			}
			if (qp->m_rate > qp->m_max_rate)
				qp->m_rate = qp->m_max_rate;
			if (!us) {
				qp->tmly.m_curRate = qp->m_rate;
				qp->tmly.m_incStage++;
				qp->tmly.rttDiff = rtt_diff;
			}
		} else {
			qp->m_rate = std::max(m_minRate, qp->tmly.m_curRate * c);
			if (!us) {
				qp->tmly.m_curRate = qp->m_rate;
				qp->tmly.m_incStage = 0;
				qp->tmly.rttDiff = rtt_diff;
			}
		}
#if PRINT_LOG
		if (print) {
			printf(" %c %.3lf\n", inc ? '^' : 'v', qp->m_rate.GetBitRate() * 1e-9);
		}
#endif
	}
	if (!us && next_seq > qp->tmly.m_lastUpdateSeq) {
		qp->tmly.m_lastUpdateSeq = next_seq;
		// update
		qp->tmly.lastRtt = rtt;
	}
}
void RdmaHw::FastReactTimely(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader &ch) {
}


/**********************
 * DCTCP
 *********************/
void RdmaHw::HandleAckDctcp(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader &ch) {
	uint32_t ack_seq = ch.ack.seq;
	uint8_t cnp = (ch.ack.flags >> qbbHeader::FLAG_CNP) & 1;
	bool new_batch = false;

	// update alpha
	qp->dctcp.m_ecnCnt += (cnp > 0);
	if (ack_seq > qp->dctcp.m_lastUpdateSeq) { // if full RTT feedback is ready, do alpha update
#if PRINT_LOG
		printf("%lu %s %08x %08x %u %u [%u,%u,%u] %.3lf->", Simulator::Now().GetTimeStep(), "alpha", qp->sip.Get(), qp->dip.Get(), qp->sport, qp->dport, qp->dctcp.m_lastUpdateSeq, ch.ack.seq, qp->snd_nxt, qp->dctcp.m_alpha);
#endif
		new_batch = true;
		if (qp->dctcp.m_lastUpdateSeq == 0) { // first RTT
			qp->dctcp.m_lastUpdateSeq = qp->snd_nxt;
			qp->dctcp.m_batchSizeOfAlpha = qp->snd_nxt / m_mtu + 1;
		} else {
			double frac = std::min(1.0, double(qp->dctcp.m_ecnCnt) / qp->dctcp.m_batchSizeOfAlpha);
			qp->dctcp.m_alpha = (1 - m_g) * qp->dctcp.m_alpha + m_g * frac;
			qp->dctcp.m_lastUpdateSeq = qp->snd_nxt;
			qp->dctcp.m_ecnCnt = 0;
			qp->dctcp.m_batchSizeOfAlpha = (qp->snd_nxt - ack_seq) / m_mtu + 1;
#if PRINT_LOG
			printf("%.3lf F:%.3lf", qp->dctcp.m_alpha, frac);
#endif
		}
#if PRINT_LOG
		printf("\n");
#endif
	}

	// check cwr exit
	if (qp->dctcp.m_caState == 1) {
		if (ack_seq > qp->dctcp.m_highSeq)
			qp->dctcp.m_caState = 0;
	}

	// check if need to reduce rate: ECN and not in CWR
	if (cnp && qp->dctcp.m_caState == 0) {
#if PRINT_LOG
		printf("%lu %s %08x %08x %u %u %.3lf->", Simulator::Now().GetTimeStep(), "rate", qp->sip.Get(), qp->dip.Get(), qp->sport, qp->dport, qp->m_rate.GetBitRate() * 1e-9);
#endif
		qp->m_rate = std::max(m_minRate, qp->m_rate * (1 - qp->dctcp.m_alpha / 2));
#if PRINT_LOG
		printf("%.3lf\n", qp->m_rate.GetBitRate() * 1e-9);
#endif
		qp->dctcp.m_caState = 1;
		qp->dctcp.m_highSeq = qp->snd_nxt;
	}

	// additive inc
	if (qp->dctcp.m_caState == 0 && new_batch)
		qp->m_rate = std::min(qp->m_max_rate, qp->m_rate + m_dctcp_rai);
}

/**********************
 * GEMINI
 *********************/
void RdmaHw::HandleAckGemini(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader &ch) {
	bool congestedDcn = false;
	bool congestedWan = false;
	if (UpdateStateGeminiOnAck(qp, ch, congestedDcn, congestedWan)) {
		if (congestedDcn || congestedWan) {
			ApplyGeminiWindowReduction(qp, congestedDcn, congestedWan);
		} else {
			ApplyGeminiAi(qp);
		}
		SyncGeminiRateAndWindow(qp);
	}
}

bool RdmaHw::UpdateStateGeminiOnAck(Ptr<RdmaQueuePair> qp, CustomHeader &ch, bool &congestedDcn, bool &congestedWan) {
	uint32_t ackSeq = ch.ack.seq;
	uint8_t cnp = (ch.ack.flags >> qbbHeader::FLAG_CNP) & 1;
	uint64_t rttSample = Simulator::Now().GetTimeStep() - ch.ack.ih.ts;

	// Windowed min RTT (10 RTT window) to allow rttBase to recover
	if (qp->gemini.rttBaseNs == 0) {
		qp->gemini.rttBaseNs = qp->m_baseRtt > 0 ? qp->m_baseRtt : rttSample;
		qp->gemini.rttBaseCandidateNs = rttSample;
		qp->gemini.rttBaseWindowStartNs = Simulator::Now().GetTimeStep();
	}
	qp->gemini.rttBaseNs = std::min(qp->gemini.rttBaseNs, rttSample);
	qp->gemini.rttBaseCandidateNs = std::min(qp->gemini.rttBaseCandidateNs, rttSample);
	uint64_t rttWindowNs = std::max<uint64_t>(qp->gemini.rttBaseNs * 10, 10000000ULL); // 10x base RTT, min 10ms
	uint64_t nowNs = Simulator::Now().GetTimeStep();
	if (nowNs - qp->gemini.rttBaseWindowStartNs >= rttWindowNs) {
		qp->gemini.rttBaseNs = qp->gemini.rttBaseCandidateNs;
		qp->gemini.rttBaseCandidateNs = rttSample;
		qp->gemini.rttBaseWindowStartNs = nowNs;
	}
	if (qp->gemini.rttMinWindowNs == 0) {
		qp->gemini.rttMinWindowNs = rttSample;
	} else {
		qp->gemini.rttMinWindowNs = std::min(qp->gemini.rttMinWindowNs, rttSample);
	}
	qp->gemini.ecnCntPkts += (cnp > 0);

	if (ackSeq <= qp->gemini.m_lastUpdateSeq) {
		return false;
	}

	if (qp->gemini.m_lastUpdateSeq == 0) {
		qp->gemini.m_lastUpdateSeq = qp->snd_nxt;
		qp->gemini.batchSizePkts = std::max(1u, uint32_t(qp->snd_nxt / m_mtu + 1));
		qp->gemini.rttMinWindowNs = rttSample;
		return false;
	}

	double frac = std::min(1.0, double(qp->gemini.ecnCntPkts) / std::max(1u, qp->gemini.batchSizePkts));
	qp->gemini.alpha = (1 - m_g) * qp->gemini.alpha + m_g * frac;
	congestedDcn = frac > 0;
	congestedWan = qp->gemini.rttMinWindowNs > qp->gemini.rttBaseNs + m_geminiDelayThreshNs;

	qp->gemini.m_lastUpdateSeq = qp->snd_nxt;
	qp->gemini.batchSizePkts = std::max(1u, uint32_t((qp->snd_nxt - ackSeq) / m_mtu + 1));
	qp->gemini.ecnCntPkts = 0;
	qp->gemini.rttMinWindowNs = rttSample;
	return true;
}

void RdmaHw::ApplyGeminiWindowReduction(Ptr<RdmaQueuePair> qp, bool congestedDcn, bool congestedWan) {
	uint64_t nowNs = Simulator::Now().GetTimeStep();
	// Paper: "window reduction is performed no more than once per RTT" — use baseRTT,
	// not queueing-inflated min RTT, so dominant and starved flows MD at same cadence.
	uint64_t guardNs = qp->gemini.rttBaseNs > 0 ? qp->gemini.rttBaseNs : 1000000ULL;
	if (qp->gemini.lastReductionTsNs != 0 && nowNs - qp->gemini.lastReductionTsNs <= guardNs) {
		return;
	}

	uint64_t pathBwBps = qp->pathBwBps > 0 ? qp->pathBwBps : qp->m_max_rate.GetBitRate();
	uint64_t bdpBytes = std::max<uint64_t>(2 * m_mtu, pathBwBps * qp->gemini.rttBaseNs / 8000000000ULL);
	double F = 4.0 * m_geminiKBytes / double(bdpBytes + m_geminiKBytes);
	double fDcn = congestedDcn ? qp->gemini.alpha * F : 0.0;
	double fWan = congestedWan ? m_geminiWanBeta : 0.0;
	double reduction = std::min(0.95, std::max(fDcn, fWan));
	uint64_t newCwnd = std::max<uint64_t>(2 * m_mtu, uint64_t(qp->gemini.cwndBytes * (1.0 - reduction)));
	qp->gemini.cwndBytes = newCwnd;
	qp->gemini.lastReductionTsNs = nowNs;
}

void RdmaHw::ApplyGeminiAi(Ptr<RdmaQueuePair> qp) {
	// Paper §III-B: h = H * C * RTT (MSS per RTT). Per-RTT cwnd growth = h * MTU.
	// Called per batch (~1 RTT) from UpdateStateGeminiOnAck, so apply h * MTU directly.
	uint64_t rttNs = qp->gemini.rttBaseNs > 0 ? qp->gemini.rttBaseNs : 1;
	uint64_t pathBwBps = qp->pathBwBps > 0 ? qp->pathBwBps : qp->m_max_rate.GetBitRate();
	double h = m_geminiH * double(pathBwBps) * (double(rttNs) / 1e9);
	// Paper bounds h to [0.1, 5] for 1 Gbps testbed; raise upper bound for high-BDP setups.
	// Higher than ~500 MSS/RTT here causes AI to overshoot fair share past buffer capacity,
	// triggering PFC NACK death cascades. Slow steady ramp is better than fast oscillation.
	h = std::max(0.1, std::min(500.0, h));
	uint64_t aiBytes = uint64_t(h * m_mtu);
	uint64_t maxCwnd = qp->m_max_rate.GetBitRate() * rttNs / 8000000000ULL;
	uint64_t newCwnd = std::min(maxCwnd, qp->gemini.cwndBytes + aiBytes);
	qp->gemini.cwndBytes = std::max<uint64_t>(2 * m_mtu, newCwnd);
}

void RdmaHw::SyncGeminiRateAndWindow(Ptr<RdmaQueuePair> qp) {
	qp->explicitWinBytes = qp->gemini.cwndBytes;
	qp->useExplicitWin = true;
	uint64_t rateBps = qp->gemini.rttBaseNs > 0 ? qp->gemini.cwndBytes * 8ULL * 1000000000ULL / qp->gemini.rttBaseNs : qp->m_max_rate.GetBitRate();
	rateBps = std::max<uint64_t>(m_minRate.GetBitRate(), std::min<uint64_t>(qp->m_max_rate.GetBitRate(), rateBps));
	DataRate newRate(rateBps);
	ChangeRate(qp, newRate);
}

/*********************
 * HPCC-PINT
 ********************/
void RdmaHw::SetPintSmplThresh(double p) {
	pint_smpl_thresh = (uint32_t)(65536 * p);
}
void RdmaHw::HandleAckHpPint(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader &ch) {
	uint32_t ack_seq = ch.ack.seq;
	if (static_cast<uint32_t>(rand() % 65536) >= pint_smpl_thresh)
		return;
	// update rate
	if (ack_seq > qp->hpccPint.m_lastUpdateSeq) { // if full RTT feedback is ready, do full update
		UpdateRateHpPint(qp, p, ch, false);
	} else { // do fast react
		UpdateRateHpPint(qp, p, ch, true);
	}
}

void RdmaHw::UpdateRateHpPint(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader &ch, bool fast_react) {
	uint32_t next_seq = qp->snd_nxt;
	if (qp->hpccPint.m_lastUpdateSeq == 0) { // first RTT
		qp->hpccPint.m_lastUpdateSeq = next_seq;
	} else {
		// check packet INT
		IntHeader &ih = ch.ack.ih;
		double U = Pint::decode_u(ih.GetPower());

		DataRate new_rate;
		int32_t new_incStage;
		double max_c = U / m_targetUtil;

		if (max_c >= 1 || qp->hpccPint.m_incStage >= m_miThresh) {
			new_rate = qp->hpccPint.m_curRate / max_c + m_rai;
			new_incStage = 0;
		} else {
			new_rate = qp->hpccPint.m_curRate + m_rai;
			new_incStage = qp->hpccPint.m_incStage + 1;
		}
		if (new_rate < m_minRate)
			new_rate = m_minRate;
		if (new_rate > qp->m_max_rate)
			new_rate = qp->m_max_rate;
		ChangeRate(qp, new_rate);
		if (!fast_react) {
			qp->hpccPint.m_curRate = new_rate;
			qp->hpccPint.m_incStage = new_incStage;
		}
		if (!fast_react) {
			if (next_seq > qp->hpccPint.m_lastUpdateSeq)
				qp->hpccPint.m_lastUpdateSeq = next_seq; //+ rand() % 2 * m_mtu;
		}
	}
}

/**********************
 * LPCC
 *********************/

void RdmaHw::RateIncEventTimerLpcc(Ptr<RdmaQueuePair> q) {
    q->lpcc.m_rpTimer = Simulator::Schedule(MicroSeconds(m_increaseInterval), &RdmaHw::RateIncEventTimerLpcc, this, q);
    RateIncEventLpcc(q);
    q->lpcc.m_rpTimeStage++;
}

void RdmaHw::RateIncEventLpcc(Ptr<RdmaQueuePair> q) {
	uint32_t nic_idx = GetNicIdxOfQp(q);
	Ptr<QbbNetDevice> dev = m_nic[nic_idx].dev;

	// AI gating logic:
	//   (1) suppress AI for a configurable number of increaseIntervals after a
	//       *processed* FCNP — debounce-dropped FCNPs no longer hold AI down.
	//   (2) shape AI strength by qRatio = effectiveQlen / queueTarget.
	const double kLpccQueueTargetBytes = GetLpccQueueTargetBytes();
	const double lineRate = std::max<double>(1.0, dev->GetDataRate().GetBitRate());
	const double rateNow = std::max<double>(1.0, q->m_rate.GetBitRate());
	const double gapRatio = std::max(0.0, (lineRate - rateNow) / lineRate);

	uint32_t effectiveQlen = q->lpcc.m_lastFcnpQlen;
	const uint64_t nowTs = Simulator::Now().GetTimeStep();
	const uint64_t thetaTs = std::max<uint64_t>(1ULL, m_theta) * 1000ULL;
	const uint64_t incTs = std::max<uint64_t>(1ULL, m_increaseInterval) * 1000ULL;
	const uint64_t fastRecoverQuietTs =
	    static_cast<uint64_t>(std::max<uint32_t>(1u, m_lpccAiSuppressMultiplier)) * incTs;
	const uint64_t ageSinceProcessedTs =
	    (q->lpcc.m_lastProcessedFcnpTs != 0 && nowTs > q->lpcc.m_lastProcessedFcnpTs)
	        ? (nowTs - q->lpcc.m_lastProcessedFcnpTs)
	        : 0;

	// Keep the last FCNP queue signal until theta timeout, then clear it.
	if (q->lpcc.m_lastProcessedFcnpTs == 0 || q->lpcc.m_lastFcnpQlen == 0) {
		effectiveQlen = 0;
	} else if (ageSinceProcessedTs >= thetaTs) {
		effectiveQlen = 0;
		q->lpcc.m_lastFcnpQlen = 0;
	}

	// Suppress AI only while a recently *processed* FCNP is still within the
	// quiet window. Debounce-dropped FCNPs do not refresh m_lastProcessedFcnpTs
	// any more, so they no longer freeze AI growth.
	if (q->lpcc.m_lastProcessedFcnpTs != 0 && ageSinceProcessedTs < fastRecoverQuietTs) {
		return;
	}

	const double qRatio = std::max(0.0, double(effectiveQlen) / std::max(1.0, kLpccQueueTargetBytes));
	double adaptiveBeta = m_beta;
	if (qRatio >= 5.0) {
		// v9: severely saturated queue (queue >> queueTarget, e.g. PFC-locked at cap)
		//   → freeze AI almost completely. Lets aggregate decay under repeated FCNP
		//   cuts so PFC eventually drains and the queue escapes the cap.
		adaptiveBeta = m_beta * 0.02;
	} else if (qRatio >= 1.5) {
		adaptiveBeta = m_beta * 0.20;
	} else if (qRatio >= 1.0) {
		adaptiveBeta = m_beta * 0.50;
	} else if (qRatio >= 0.5) {
		adaptiveBeta = m_beta * 0.90;
	} else {
		adaptiveBeta = std::min(0.25, m_beta * 1.40);
	}

	// Queue-target guard: above target -> throttle AI but avoid hard freeze.
	if (effectiveQlen > kLpccQueueTargetBytes) {
		double over = std::min(2.0, (double(effectiveQlen) - kLpccQueueTargetBytes) / kLpccQueueTargetBytes);
		adaptiveBeta *= std::max(0.20, 1.0 - 0.60 * over);
	}

	const double baseIncRatio = std::max(0.0, std::min(gapRatio, adaptiveBeta));
	double aiFloor = 0.0;
	if (qRatio < 0.5) {
		// v7: lowered from 0.30 to 0.05 to eliminate AI explosion when queue empties.
		// Prevents the sawtooth bistability where queue under-shoot kicks AI into
		// 28%/tick recovery mode (which then overshoots line rate).
		aiFloor = std::min(gapRatio, std::max(0.005, gapRatio * 0.05));
	} else if (qRatio < 1.0) {
		aiFloor = std::min(gapRatio, std::max(0.003, gapRatio * 0.03));
	} else if (qRatio < 1.2) {
		aiFloor = std::min(gapRatio, 0.001);
	}
	// Fast recovery branch keyed on processed FCNP quiet time.
	const bool queueQuietLongEnough = (effectiveQlen == 0) &&
	                                  (q->lpcc.m_lastProcessedFcnpTs == 0 ||
	                                   (nowTs > q->lpcc.m_lastProcessedFcnpTs &&
	                                    nowTs - q->lpcc.m_lastProcessedFcnpTs >= fastRecoverQuietTs));
	if (queueQuietLongEnough) {
		const double fastFloor = std::min(gapRatio, std::max(0.02, gapRatio * 0.40));
		aiFloor = std::max(aiFloor, fastFloor);
	}

	const double incRatio = std::max(baseIncRatio, aiFloor);
	q->m_rate *= (1.0 + incRatio);
}

void RdmaHw::UpdateRateLpcc(Ptr<RdmaQueuePair> qp, CustomHeader &ch) {
	uint32_t m_qlen = ch.fcnp.qlen;
	if (m_epsilon == 0) {
		return;
	}
	// m_k now directly reflects queue overshoot vs epsilon. The previous
	// rttlEff/m_rtts factor used FCNP's reverse one-way travel time, which
	// has the wrong direction and made the first decrease accidentally
	// weaker than steady-state. Drop it.
	double m_k = (1.0 * m_qlen / m_epsilon - 1.0);
	m_k = std::max(0.0, std::min(m_k, 1.2));

	uint64_t congRateBps = ch.fcnp.linkRateBps;
	if (congRateBps == 0) {
		return;
	}
	const double rateRatio = 1.0 * qp->m_rate.GetBitRate() / congRateBps;
	double flowFactor = std::sqrt(std::max(1.0, static_cast<double>(ch.fcnp.m_flowCount)));
	flowFactor = std::min(flowFactor, 3.0);
	double m_p = rateRatio * flowFactor;
	m_p = std::max(0.0, std::min(m_p, 4.0));
	qp->lpcc.m_flowCount = ch.fcnp.m_flowCount;

	// Queue-target-aware decrease intensity.
	const double kLpccQueueTargetBytes = GetLpccQueueTargetBytes();
	double qPenalty = 1.0;
	if (m_qlen > kLpccQueueTargetBytes) {
		double over = std::min(1.0, (double(m_qlen) - kLpccQueueTargetBytes) / kLpccQueueTargetBytes);
		qPenalty += 0.5 * over;
	}
	const double decreaseSignal = std::max(std::min(m_k * m_p * qPenalty, m_wr), 0.0);
	DataRate new_rate = qp->m_rate * (1.0 / (1 + decreaseSignal));

	// Piecewise single-step drop cap (parameterised):
	//   qlen <= queueTarget       -> dropCapLow
	//   queueTarget < qlen < hi   -> linear lerp from low to high
	//   qlen >= queueTarget * R   -> dropCapHigh
	const double rateBps = std::max(1.0, static_cast<double>(qp->m_rate.GetBitRate()));
	double dropFrac = 1.0 - (static_cast<double>(new_rate.GetBitRate()) / rateBps);
	const double hiRatio = std::max(1.0 + 1e-6, m_lpccDropCapHighRatio);
	const double qOverTarget = (kLpccQueueTargetBytes > 0.0)
	                               ? (static_cast<double>(m_qlen) / kLpccQueueTargetBytes)
	                               : 0.0;
	double dropCap;
	if (qOverTarget <= 1.0) {
		dropCap = m_lpccDropCapLow;
	} else if (qOverTarget < hiRatio) {
		const double t = (qOverTarget - 1.0) / (hiRatio - 1.0);
		dropCap = m_lpccDropCapLow + (m_lpccDropCapHigh - m_lpccDropCapLow) * t;
	} else {
		dropCap = m_lpccDropCapHigh;
	}
	if (dropFrac > dropCap) {
		new_rate = qp->m_rate * (1.0 - dropCap);
	}
	new_rate = std::max(new_rate, m_minRate * 10.0); // 1Gbps floor
	qp->m_rate = new_rate;
}

void RdmaHw::HandleAckLpcc(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader &ch) {
    uint32_t ack_seq = ch.ack.seq;
    if (ack_seq > qp->lpcc.m_lastUpdateSeq) {  // if full RTT feedback is ready, do full update
        UpdateRateLpccOnAck(qp, p, ch);
    }
}
void RdmaHw::UpdateRateLpccOnAck(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader &ch) {
    uint64_t nowTs = Simulator::Now().GetTimeStep();
    if (qp->lpcc.m_lastUpdateSeq != 0) {  // not first RTT
        uint64_t m_rttl = nowTs - ch.ack.ih.ts;
        if (qp->lpcc.m_minRtt == 0) {
            qp->lpcc.m_lastRtt = m_rttl;
            qp->lpcc.m_minRtt = m_rttl;
            qp->lpcc.m_lastUpdateSeq = qp->snd_nxt;
            return;
        }

        qp->lpcc.m_lastRtt = m_rttl;
        qp->lpcc.m_minRtt = std::min(qp->lpcc.m_minRtt, m_rttl);

        if (qp->lpcc.m_lastCongRateBps == 0) {
            qp->lpcc.m_lastUpdateSeq = qp->snd_nxt;
            return;
        }

	        const uint64_t thetaTs = std::max<uint64_t>(1ULL, m_theta) * 1000ULL;
	        const uint64_t noAckReduceWindowTs = thetaTs;
	        if (qp->lpcc.m_lastDecreaseRate != 0 &&
	            noAckReduceWindowTs > 0 &&
	            nowTs - qp->lpcc.m_lastDecreaseRate < noAckReduceWindowTs) {
	            qp->lpcc.m_lastUpdateSeq = qp->snd_nxt;
	            return;
	        }
	        // ACK path should only be a light correction when FCNP has been quiet
	        // for a long enough interval.
	        const uint64_t ackQuietWindowTs = thetaTs * 4ULL;
	        if (qp->lpcc.m_lastFcnpTs != 0 &&
	            nowTs > qp->lpcc.m_lastFcnpTs &&
	            nowTs - qp->lpcc.m_lastFcnpTs < ackQuietWindowTs) {
	            qp->lpcc.m_lastUpdateSeq = qp->snd_nxt;
	            return;
	        }

		const double rateRatio = 1.0 * qp->m_rate.GetBitRate() / qp->lpcc.m_lastCongRateBps;
		double flowFactor = std::sqrt(std::max(1.0, static_cast<double>(qp->lpcc.m_flowCount)));
		flowFactor = std::min(flowFactor, 3.0);
		double m_p = rateRatio * flowFactor;
		m_p = std::max(0.0, std::min(m_p, 4.0));

        double rttInflation = 0.0;
        if (m_rttl > qp->lpcc.m_minRtt && qp->lpcc.m_minRtt > 0) {
            rttInflation = 1.0 * (m_rttl - qp->lpcc.m_minRtt) / qp->lpcc.m_minRtt;
        }
	        const double ackKr = std::max(0.01, 0.20 * m_kr);
	        DataRate new_rate = qp->m_rate * (1 - std::min(rttInflation * m_p, ackKr));
        new_rate = std::max(new_rate, m_minRate * 10.0); // min rate is 1Gbps
		// if (Simulator::Now().GetTimeStep() >= 10000000) {
		// 	std::cout << "--------------------------------------" << std::endl;
        // 	std::cout << "m_p: " << m_p << std::endl;
		// std::cout << "now_time\t" << Simulator::Now().GetTimeStep() << std::endl;
		// std::cout << "minRtt: " << qp->lpcc.m_minRtt << std::endl;
		// std::cout << "m_rttl: " << m_rttl << std::endl;
        // std::cout << "Down_rtt\t" << "before: " << qp->m_rate.GetBitRate() << std::endl;
        // std::cout << "Down_rtt\t" << "after: " << new_rate.GetBitRate() << std::endl;
        // 	std::cout << "--------------------------------------" << std::endl;
		// }
        // ChangeRate(qp, new_rate);
			qp->m_rate = new_rate;
	        // qp->lpcc.m_curRate = new_rate;
            qp->lpcc.m_lastUpdateSeq = qp->snd_nxt;
	    } else {
	        qp->lpcc.m_lastUpdateSeq = qp->snd_nxt;
			qp->lpcc.m_lastRtt = nowTs - ch.ack.ih.ts;
			// last_rtt = Simulator::Now().GetTimeStep() - ch.ack.ih.ts;
	        qp->lpcc.m_minRtt = nowTs - ch.ack.ih.ts;
	    }
}

}
