/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation;
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
#undef PGO_TRAINING
#define PATH_TO_PGO_CONFIG "path_to_pgo_config"

#include <iostream>
#include <fstream>
#include <algorithm>
#include <unordered_map>
#include <time.h> 
#include "ns3/core-module.h"
#include "ns3/qbb-helper.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/global-route-manager.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/packet.h"
#include "ns3/error-model.h"
#include "ns3/tcp-socket-factory.h"
#include <ns3/rdma.h>
#include <ns3/rdma-client.h>
#include <ns3/rdma-client-helper.h>
#include <ns3/rdma-driver.h>
#include <ns3/switch-node.h>
#include <ns3/sim-setting.h>

using namespace ns3;
using namespace std;

NS_LOG_COMPONENT_DEFINE("GENERIC_SIMULATION");

namespace {
constexpr uint32_t TRANSPORT_MODE_RDMA = 0;
constexpr uint32_t TRANSPORT_MODE_TCP_BBR = 1;
}

uint32_t cc_mode = 1;
uint32_t transport_mode = TRANSPORT_MODE_RDMA;
bool enable_qcn = true;
uint32_t packet_payload_size = 1000, l2_chunk_size = 0, l2_ack_interval = 0;
double pause_time = 5, simulator_stop_time = 3.01;
std::string data_rate, link_delay, topology_file, flow_file, trace_file, trace_output_file;
std::string fct_output_file = "fct.txt";
std::string pfc_output_file = "pfc.txt";

double alpha_resume_interval = 55, rp_timer, ewma_gain = 1 / 16;
double rate_decrease_interval = 4;
uint32_t fast_recovery_times = 5;
std::string rate_ai, rate_hai, min_rate = "100Mb/s";
std::string dctcp_rate_ai = "1000Mb/s";
uint64_t gemini_delay_thresh_ns = 5000000;
double gemini_beta = 0.2;
double gemini_h = 1.2e-7;
uint32_t gemini_dcn_delay_cutoff_us = 100;
uint32_t flow_control_mode = 0;
uint32_t bifrost_timeslot_us = 10;
uint32_t bifrost_k = 1;
uint32_t bifrost_longhaul_delay_cutoff_us = 100;
uint32_t bifrost_h_margin_slots = 3;
uint32_t bbr_rtt_win_ms = 200;
uint32_t bbr_probe_rtt_ms = 50;
uint32_t bbr_init_cwnd = 100;
uint32_t bbr_snd_buf_mb = 64;
uint32_t bbr_rcv_buf_mb = 64;
double   bicc_dst_bdp_factor = 0.1;
uint32_t bicc_longhaul_cutoff_us = 100;
uint32_t bicc_ns_fb_interval_us = 75;
uint64_t bicc_blend_t_ns = 2000000;
uint32_t themis_cnp_interval_us = 50;
uint32_t themis_trp_alpha_init = 5;
uint32_t themis_trp_beta_us = 500;
uint32_t themis_trp_loop_delay_ns = 1000;
uint32_t themis_trp_max_loops = 64;
uint32_t themis_longhaul_cutoff_us = 100;

bool clamp_target_rate = false, l2_back_to_zero = false;
double error_rate_per_link = 0.0;
uint32_t has_win = 1;
uint32_t global_t = 1;
uint32_t mi_thresh = 5;
bool var_win = false, fast_react = true;
bool multi_rate = true;
bool sample_feedback = false;
double pint_log_base = 1.05;
double pint_prob = 1.0;
double u_target = 0.95;
uint32_t int_multi = 1;
bool rate_bound = true;

uint32_t ack_high_prio = 0;
uint64_t link_down_time = 0;
uint32_t link_down_A = 0, link_down_B = 0;

uint32_t enable_trace = 1;

uint32_t buffer_size = 16;

uint32_t qlen_dump_interval = 100000000, qlen_mon_interval = 100;
uint64_t qlen_mon_start = 2000000000, qlen_mon_end = 2100000000;
string qlen_mon_file;

unordered_map<uint64_t, uint32_t> rate2kmax, rate2kmin;
unordered_map<uint64_t, double> rate2pmax;
unordered_map<uint64_t, uint32_t> rate2geminiK;

/************************************************
 * Runtime varibles
 ***********************************************/
std::ifstream topof, flowf, tracef;

NodeContainer n;
NetDeviceContainer switchToSwitchInterfaces;
std::map< uint32_t,std::map< uint32_t,std::vector<Ptr<QbbNetDevice>> > > switchToSwitch;


std::map<uint32_t,uint32_t> switchNumToId;
std::map<uint32_t,uint32_t> switchIdToNum;
std::map<uint32_t,NetDeviceContainer> switchUp;
//NetDeviceContainer switchUp[switch_num];
std::map<uint32_t,NetDeviceContainer> sourceNodes;

NodeContainer servers;
NodeContainer tors;

uint64_t nic_rate;

uint64_t maxRtt, maxBdp;

struct Interface{
	uint32_t idx;
	bool up;
	uint64_t delay;
	uint64_t bw;

	Interface() : idx(0), up(false){}
};
map<Ptr<Node>, map<Ptr<Node>, Interface> > nbr2if;
// Mapping destination to next hop for each node: <node, <dest, <nexthop0, ...> > >
map<Ptr<Node>, map<Ptr<Node>, vector<Ptr<Node> > > > nextHop;
map<Ptr<Node>, map<Ptr<Node>, uint64_t> > pairDelay;
map<Ptr<Node>, map<Ptr<Node>, uint64_t> > pairTxDelay;
map<uint32_t, map<uint32_t, uint64_t> > pairBw;
map<Ptr<Node>, map<Ptr<Node>, uint64_t> > pairBdp;
map<uint32_t, map<uint32_t, uint64_t> > pairRtt;

std::vector<Ipv4Address> serverAddress;

// maintain port number for each host pair
std::unordered_map<uint32_t, unordered_map<uint32_t, uint16_t> > portNumder;

struct FlowInput{
	uint64_t src, dst, pg, maxPacketCount, port, dport;
	double start_time;
	double stop_time;  // per-flow stop time in seconds; <=0 means run to sim end
	uint32_t idx;
};
FlowInput flow_input = {0};

// Fairness-experiment-only: per-source stop flag. When a flow's stop_time fires,
// we set this to skip that source in PrintResultsFlow. Without this, some CC modes
// (HPCC) keep filling the NIC tx counter via rate-paced retransmissions or window
// drainage, so naturally relying on qp_finish->flow_idx_complete++ doesn't trigger.
// Keyed by source node ID (matches the Src container keying).
std::map<uint32_t, bool> g_src_stopped;
uint32_t flow_num;
uint64_t tcp_flow_id = 0;
FILE *g_fct_output = nullptr;

Ipv4Address node_id_to_ip(uint32_t id);

void ReadFlowInput(){
	if (flow_input.idx < flow_num){
		flowf >> flow_input.src >> flow_input.dst >> flow_input.pg >> flow_input.dport >> flow_input.maxPacketCount >> flow_input.start_time >> flow_input.stop_time;
		std::cout << "Flow "<< flow_input.src << " " << flow_input.dst << " " << flow_input.pg << " " << flow_input.dport << " " << flow_input.maxPacketCount << " start=" << flow_input.start_time << " stop=" << flow_input.stop_time << " now=" << Simulator::Now().GetSeconds() << std::endl;
		NS_ASSERT(n.Get(flow_input.src)->GetNodeType() == 0 && n.Get(flow_input.dst)->GetNodeType() == 0);
	}
}
void ScheduleFlowInputs(){
	while (flow_input.idx < flow_num && Seconds(flow_input.start_time) <= Simulator::Now()){
		uint32_t port = portNumder[flow_input.src][flow_input.dst]++; // get a new port number 
		if (transport_mode == TRANSPORT_MODE_TCP_BBR) {
			Ptr<BulkSendApplication> sender = CreateObject<BulkSendApplication>();
			sender->SetAttribute("Protocol", TypeIdValue(TcpSocketFactory::GetTypeId()));
			sender->SetAttribute("SendSize", UintegerValue(packet_payload_size));
			sender->SetAttribute("MaxBytes", UintegerValue(flow_input.maxPacketCount));
			sender->SetAttribute("FlowId", UintegerValue(++tcp_flow_id));
			sender->SetAttribute("priorityCustom", UintegerValue(1));
			sender->SetAttribute("priority", UintegerValue(1));
			sender->SetAttribute("Remote", AddressValue(InetSocketAddress(serverAddress[flow_input.dst], flow_input.dport)));
			sender->SetAttribute("Local", AddressValue(InetSocketAddress(serverAddress[flow_input.src], port)));
			n.Get(flow_input.src)->AddApplication(sender);
			Time appStart = Simulator::Now() + NanoSeconds(1);
			sender->SetStartTime(appStart);
			sender->SetStopTime(Seconds(simulator_stop_time));

			PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), flow_input.dport));
			ApplicationContainer sinkApp = sink.Install(n.Get(flow_input.dst));
			sinkApp.Get(0)->SetAttribute("TotalQueryBytes", UintegerValue(flow_input.maxPacketCount));
			sinkApp.Get(0)->SetAttribute("recvAt", TimeValue(Seconds(flow_input.start_time)));
			sinkApp.Get(0)->SetAttribute("priority", UintegerValue(1));
			sinkApp.Get(0)->SetAttribute("priorityCustom", UintegerValue(1));
			sinkApp.Get(0)->SetAttribute("senderPriority", UintegerValue(1));
			sinkApp.Get(0)->SetAttribute("flowId", UintegerValue(tcp_flow_id));
			sinkApp.Get(0)->TraceConnectWithoutContext(
				"FlowFinish",
				MakeBoundCallback(
					+[](FILE *fout, uint32_t sid, uint32_t did, uint16_t sport, uint16_t dport, double totalSize, double start, bool, uint32_t) {
						uint64_t flowSize = static_cast<uint64_t>(totalSize);
						uint64_t baseRtt = pairRtt[sid][did];
						uint64_t bw = pairBw[sid][did];
						uint64_t standaloneFct = baseRtt + flowSize * 8000000000ULL / bw;
						uint64_t startNs = static_cast<uint64_t>(start);
						uint64_t fct = Simulator::Now().GetNanoSeconds() - startNs;
						fprintf(fout, "%08x %08x %u %u %lu %lu %lu %lu\n",
							node_id_to_ip(sid).Get(), node_id_to_ip(did).Get(), sport, dport, flowSize, startNs, fct, standaloneFct);
						fflush(fout);
					},
					g_fct_output, static_cast<uint32_t>(flow_input.src), static_cast<uint32_t>(flow_input.dst), static_cast<uint16_t>(port), static_cast<uint16_t>(flow_input.dport)));
			sinkApp.Start(appStart);
			sinkApp.Stop(Seconds(simulator_stop_time));
		} else {
			RdmaClientHelper clientHelper(flow_input.pg, serverAddress[flow_input.src], serverAddress[flow_input.dst], port, flow_input.dport, flow_input.maxPacketCount, has_win?(global_t==1?maxBdp:pairBdp[n.Get(flow_input.src)][n.Get(flow_input.dst)]):0, global_t==1?maxRtt:pairRtt[flow_input.src][flow_input.dst], pairBw[flow_input.src][flow_input.dst], Seconds(simulator_stop_time));
			ApplicationContainer appCon = clientHelper.Install(n.Get(flow_input.src));
			appCon.Start(Seconds(0)); // setting the correct time here conflicts with Sim time since there is already a schedule event that triggered this function at desired time.

			// Fairness-experiment-only: per-flow forced stop time via Simulator::Schedule.
			// Truncates qp->m_size when stop_time fires; only affects this experiment.
			if (flow_input.stop_time > 0.0) {
				Ptr<Node> srcNode = n.Get(flow_input.src);
				uint32_t srcId = flow_input.src;
				uint32_t dipVal = serverAddress[flow_input.dst].Get();
				uint16_t portVal = static_cast<uint16_t>(port);
				uint16_t pgVal = static_cast<uint16_t>(flow_input.pg);
				Time delay = Seconds(flow_input.stop_time) - Simulator::Now();
				Simulator::Schedule(delay, [srcNode, srcId, dipVal, portVal, pgVal]() {
					Ptr<RdmaDriver> rdma = srcNode->GetObject<RdmaDriver>();
					if (!rdma) return;
					Ptr<RdmaQueuePair> qp = rdma->m_rdma->GetQp(dipVal, portVal, pgVal);
					if (qp && !qp->IsFinished()) {
						qp->m_size = qp->snd_nxt;
						std::cout << "FLOW_STOP t=" << Simulator::Now().GetSeconds()
						          << " sport=" << portVal << " bytes=" << qp->snd_nxt << std::endl;
					}
					// Explicitly mark this source as stopped so the dumper skips it.
					// Some CC modes (HPCC) don't naturally drain to IsFinished() after
					// m_size truncation; without this flag the dump file keeps growing.
					g_src_stopped[srcId] = true;
				});
			}
		}
		// get the next flow input
		flow_input.idx++;
		ReadFlowInput();
	}

	// schedule the next time to run this function
	if (flow_input.idx < flow_num){
		Simulator::Schedule(Seconds(flow_input.start_time)-Simulator::Now(), ScheduleFlowInputs);
	}else { // no more flows, close the file
		flowf.close();
	}
}

Ipv4Address node_id_to_ip(uint32_t id){
	return Ipv4Address(0x0b000001 + ((id / 256) * 0x00010000) + ((id % 256) * 0x00000100));
}

uint32_t ip_to_node_id(Ipv4Address ip){
	return (ip.Get() >> 8) & 0xffff;
}

uint32_t flow_idx_complete = 0;

void qp_finish(FILE* fout, Ptr<RdmaQueuePair> q){
	uint32_t sid = ip_to_node_id(q->sip), did = ip_to_node_id(q->dip);
	uint64_t base_rtt = pairRtt[sid][did], b = pairBw[sid][did];
	uint32_t total_bytes = q->m_size + ((q->m_size-1) / packet_payload_size + 1) * (CustomHeader::GetStaticWholeHeaderSize() - IntHeader::GetStaticSize()); // translate to the minimum bytes required (with header but no INT)
	uint64_t standalone_fct = base_rtt + total_bytes * 8000000000lu / b;
	// sip, dip, sport, dport, size (B), start_time, fct (ns), standalone_fct (ns)
	fprintf(fout, "%08x %08x %u %u %lu %lu %lu %lu\n", q->sip.Get(), q->dip.Get(), q->sport, q->dport, q->m_size, q->startTime.GetTimeStep(), (Simulator::Now() - q->startTime).GetTimeStep(), standalone_fct);
	fflush(fout);

	// remove rxQp from the receiver
	Ptr<Node> dstNode = n.Get(did);
	Ptr<RdmaDriver> rdma = dstNode->GetObject<RdmaDriver> ();
	rdma->m_rdma->DeleteRxQp(q->sip.Get(), q->m_pg, q->sport);
	flow_idx_complete++;
}

void get_pfc(FILE* fout, Ptr<QbbNetDevice> dev, uint32_t type){
	fprintf(fout, "%lu %u %u %u %u\n", Simulator::Now().GetTimeStep(), dev->GetNode()->GetId(), dev->GetNode()->GetNodeType(), dev->GetIfIndex(), type);
}

struct QlenDistribution{
	vector<uint32_t> cnt; // cnt[i] is the number of times that the queue len is i KB

	void add(uint32_t qlen){
		uint32_t kb = qlen / 1000;
		if (cnt.size() < kb+1)
			cnt.resize(kb+1);
		cnt[kb]++;
	}
};
map<uint32_t, map<uint32_t, QlenDistribution> > queue_result;
void monitor_buffer(FILE* qlen_output, NodeContainer *n){
	for (uint32_t i = 0; i < n->GetN(); i++){
		if (n->Get(i)->GetNodeType() == 1){ // is switch
			Ptr<SwitchNode> sw = DynamicCast<SwitchNode>(n->Get(i));
			if (queue_result.find(i) == queue_result.end())
				queue_result[i];
			for (uint32_t j = 1; j < sw->GetNDevices(); j++){
				uint32_t size = 0;
				for (uint32_t k = 0; k < SwitchMmu::qCnt; k++)
					size += sw->m_mmu->egress_bytes[j][k];
				queue_result[i][j].add(size);
			}
		}
	}
	if (Simulator::Now().GetTimeStep() % qlen_dump_interval == 0){
		fprintf(qlen_output, "time: %lu\n", Simulator::Now().GetTimeStep());
		for (auto &it0 : queue_result)
			for (auto &it1 : it0.second){
				fprintf(qlen_output, "%u %u", it0.first, it1.first);
				auto &dist = it1.second.cnt;
				for (uint32_t i = 0; i < dist.size(); i++)
					fprintf(qlen_output, " %u", dist[i]);
				fprintf(qlen_output, "\n");
			}
		fflush(qlen_output);
	}
	if (Simulator::Now().GetTimeStep() < (int64_t)qlen_mon_end)
		Simulator::Schedule(NanoSeconds(qlen_mon_interval), &monitor_buffer, qlen_output, n);
}

void CalculateRoute(Ptr<Node> host){
	// queue for the BFS.
	vector<Ptr<Node> > q;
	// Distance from the host to each node.
	map<Ptr<Node>, int> dis;
	map<Ptr<Node>, uint64_t> delay;
	map<Ptr<Node>, uint64_t> txDelay;
	map<Ptr<Node>, uint64_t> bw;
	// init BFS.
	q.push_back(host);
	dis[host] = 0;
	delay[host] = 0;
	txDelay[host] = 0;
	bw[host] = 0xfffffffffffffffflu;
	// BFS.
	for (int i = 0; i < (int)q.size(); i++){
		Ptr<Node> now = q[i];
		int d = dis[now];
		for (auto it = nbr2if[now].begin(); it != nbr2if[now].end(); it++){
			// skip down link
			if (!it->second.up)
				continue;
			Ptr<Node> next = it->first;
			// If 'next' have not been visited.
			if (dis.find(next) == dis.end()){
				dis[next] = d + 1;
				delay[next] = delay[now] + it->second.delay;
				txDelay[next] = txDelay[now] + packet_payload_size * 1000000000lu * 8 / it->second.bw;
				bw[next] = std::min(bw[now], it->second.bw);
				// we only enqueue switch, because we do not want packets to go through host as middle point
				if (next->GetNodeType() == 1)
					q.push_back(next);
			}
			// if 'now' is on the shortest path from 'next' to 'host'.
			if (d + 1 == dis[next]){
				nextHop[next][host].push_back(now);
			}
		}
	}
	for (auto it : delay)
		pairDelay[it.first][host] = it.second;
	for (auto it : txDelay)
		pairTxDelay[it.first][host] = it.second;
	for (auto it : bw)
		pairBw[it.first->GetId()][host->GetId()] = it.second;
}

void CalculateRoutes(NodeContainer &n){
	for (int i = 0; i < (int)n.GetN(); i++){
		Ptr<Node> node = n.Get(i);
		if (node->GetNodeType() == 0)
			CalculateRoute(node);
	}
}

void SetRoutingEntries(){
	// For each node.
	for (auto i = nextHop.begin(); i != nextHop.end(); i++){
		Ptr<Node> node = i->first;
		auto &table = i->second;
		for (auto j = table.begin(); j != table.end(); j++){
			// The destination node.
			Ptr<Node> dst = j->first;
			// The IP address of the dst.
			Ipv4Address dstAddr = dst->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal();
			// The next hops towards the dst.
			vector<Ptr<Node> > nexts = j->second;
			for (int k = 0; k < (int)nexts.size(); k++){
				Ptr<Node> next = nexts[k];
				uint32_t interface = nbr2if[node][next].idx;
				if (node->GetNodeType() == 1)
					DynamicCast<SwitchNode>(node)->AddTableEntry(dstAddr, interface);
				else{
					node->GetObject<RdmaDriver>()->m_rdma->AddTableEntry(dstAddr, interface);
				}
			}
		}
	}
}

// take down the link between a and b, and redo the routing
void TakeDownLink(NodeContainer n, Ptr<Node> a, Ptr<Node> b){
	if (!nbr2if[a][b].up)
		return;
	// take down link between a and b
	nbr2if[a][b].up = nbr2if[b][a].up = false;
	nextHop.clear();
	CalculateRoutes(n);
	// clear routing tables
	for (uint32_t i = 0; i < n.GetN(); i++){
		if (n.Get(i)->GetNodeType() == 1)
			DynamicCast<SwitchNode>(n.Get(i))->ClearTable();
		else
			n.Get(i)->GetObject<RdmaDriver>()->m_rdma->ClearTable();
	}
	DynamicCast<QbbNetDevice>(a->GetDevice(nbr2if[a][b].idx))->TakeDown();
	DynamicCast<QbbNetDevice>(b->GetDevice(nbr2if[b][a].idx))->TakeDown();
	// reset routing table
	SetRoutingEntries();

	// redistribute qp on each host
	for (uint32_t i = 0; i < n.GetN(); i++){
		if (n.Get(i)->GetNodeType() == 0)
			n.Get(i)->GetObject<RdmaDriver>()->m_rdma->RedistributeQp();
	}
}

uint64_t get_nic_rate(NodeContainer &n){
	for (uint32_t i = 0; i < n.GetN(); i++)
		if (n.Get(i)->GetNodeType() == 0)
			return DynamicCast<QbbNetDevice>(n.Get(i)->GetDevice(1))->GetDataRate().GetBitRate();
	return 0;
}

void PrintResults(std::map<uint32_t,NetDeviceContainer> ToR,uint32_t numToRs,double delay){
	for (uint32_t i=0; i<numToRs;i++){
		double throughputTotal=0;
		// uint64_t torBuffer;
		for (uint32_t j=0; j< ToR[i].GetN();j++){
			Ptr<QbbNetDevice> nd = DynamicCast<QbbNetDevice>(ToR[i].Get(j));
//			uint64_t txBytes = nd->getTxBytes();
			uint64_t txBytes = nd->GetQueue()->getTxBytes();

			uint64_t qlen = nd->GetQueue()->GetNBytesTotal();
			double throughput = double(txBytes*8)/delay;
			throughputTotal+=throughput;
			std::cout << "ToR " << i << " Port " << j << " throughput "<< throughput << " txBytes " << txBytes << " qlen " << qlen << " time " << Simulator::Now().GetSeconds() << std::endl;
		}
		std::cout << "ToR " << i << " Total " << 0 << " throughput " << throughputTotal <<  " time " << Simulator::Now().GetSeconds() << std::endl;
	}
	Simulator::Schedule(Seconds(delay),PrintResults,ToR,numToRs,delay);
}


void PrintResultsFlow(std::map<uint32_t,NetDeviceContainer> Src,uint32_t numFlows,double delay){
	for (uint32_t i=flow_idx_complete; i<numFlows;i++){
		// Skip sources that have been explicitly stopped by the fairness-experiment
		// stop_time logic. Required because some CC modes don't trigger qp_finish()
		// naturally after m_size truncation, so flow_idx_complete may not advance.
		auto stopIt = g_src_stopped.find(i);
		if (stopIt != g_src_stopped.end() && stopIt->second) continue;

		double throughputTotal=0;

		for (uint32_t j=0; j< Src[i].GetN();j++){
			Ptr<QbbNetDevice> nd = DynamicCast<QbbNetDevice>(Src[i].Get(j));
//			uint64_t txBytes = nd->getTxBytes();
			uint64_t txBytes = nd->getNumTxBytes();

			// uint64_t qlen = nd->GetQueue()->GetNBytesTotal();
			double throughput = double(txBytes*8)/delay;
			throughputTotal+=throughput;
			// std::cout << "Src " << i << " Port " << j << " throughput "<< throughput << " txBytes " << txBytes << " qlen " << qlen << " time " << Simulator::Now().GetSeconds() << std::endl;
		}
		std::cout << "Src " << i << " Total " << 0 << " throughput " << throughputTotal <<  " time " << Simulator::Now().GetSeconds() << std::endl;
	}
	Simulator::Schedule(Seconds(delay),PrintResultsFlow,Src,numFlows,delay);
}




int main(int argc, char *argv[])
{
	clock_t begint, endt;
	begint = clock();
	std::ifstream conf;
	bool wien = true; // wien enables PowerTCP. 
	bool delayWien = false; // delayWien enables Theta-PowerTCP (delaypowertcp) 

	uint32_t algorithm=3;
	uint32_t windowCheck=1;
	uint32_t transportModeArg = transport_mode;
	uint32_t flowControlModeArg = flow_control_mode;
	// LPCC CLI overrides (mirror powertcp-evaluation-burst.cc). -1/-1.0 = keep built-in default.
	int32_t lpccEpsilonArg = -1;
	int32_t lpccFcnpMinIntervalUsArg = -1;
	int32_t lpccPerFlowFcnpCooldownUsArg = -1;
	int32_t lpccFcnpTopKArg = -1;
	int32_t lpccFcnpTopKHighArg = -1;
	int32_t lpccFcnpKHighThreshBytesArg = -1;
	double lpccThetaUsArg = -1.0;
	int32_t lpccIncreaseIntervalUsArg = -1;
	double lpccIncreaseFactorArg = -1.0;
	double lpccWrArg = -1.0;
	double lpccKrArg = -1.0;
	double lpccQueueTargetRatioArg = -1.0;
	double lpccDropCapHighArg = -1.0;
	int32_t lpccAiSuppressMultiplierArg = -1;
	// std::string confFile = "/home/vamsi/src/phd/codebase/ns3-datacenter/simulator/ns-3.39/examples/PowerTCP/config-fairness.txt";
	std::string confFile = "/home/leo/PowerTCP-RAW/ns-3.39/examples/PowerTCP/config-fairness.txt";

	std::cout << confFile;
	CommandLine cmd;
	cmd.AddValue("conf", "config file path", confFile);
	cmd.AddValue("wien", "enable wien --> wien enables PowerTCP.", wien);
	cmd.AddValue("delayWien", "enable wien delay --> delayWien enables Theta-PowerTCP (delaypowertcp) ", delayWien);

	cmd.AddValue ("algorithm", "specify CC mode. This is added for my convinience since I prefer cmd rather than parsing files.", algorithm);
	cmd.AddValue("windowCheck","windowCheck",windowCheck);
	cmd.AddValue("transportMode","specify transport mode. 0=RDMA, 1=TCP_BBR",transportModeArg);
	cmd.AddValue("flowControlMode","specify flow control mode. 0=PFC (default, including BICC), 1=Bifrost (legacy baseline)",flowControlModeArg);
	cmd.AddValue("lpccEpsilon", "LPCC queue threshold in bytes (<=0 means keep built-in default)", lpccEpsilonArg);
	cmd.AddValue("lpccFcnpMinIntervalUs", "LPCC switch FCNP min interval in microseconds (<=0 means keep built-in default)", lpccFcnpMinIntervalUsArg);
	cmd.AddValue("lpccPerFlowFcnpCooldownUs", "LPCC per-flow fCNP cooldown in microseconds (<=0 means keep built-in default)", lpccPerFlowFcnpCooldownUsArg);
	cmd.AddValue("lpccFcnpTopK", "LPCC switch FCNP fanout top-K flows per congested queue (<=0 means keep built-in default)", lpccFcnpTopKArg);
	cmd.AddValue("lpccFcnpTopKHigh", "LPCC switch FCNP dynamic high-queue fanout top-K (<=0 means keep built-in default)", lpccFcnpTopKHighArg);
	cmd.AddValue("lpccFcnpKHighThreshBytes", "LPCC switch FCNP dynamic-K queue threshold in bytes (<=0 means keep built-in default)", lpccFcnpKHighThreshBytesArg);
	cmd.AddValue("lpccThetaUs", "LPCC decrease-cycle interval in microseconds (<=0 means keep built-in default)", lpccThetaUsArg);
	cmd.AddValue("lpccIncreaseIntervalUs", "LPCC AI timer interval in microseconds (<=0 means keep built-in default)", lpccIncreaseIntervalUsArg);
	cmd.AddValue("lpccIncreaseFactor", "LPCC AI factor beta (<=0 means keep built-in default)", lpccIncreaseFactorArg);
	cmd.AddValue("lpccWr", "LPCC FCNP decrease cap wr (<=0 means keep built-in default)", lpccWrArg);
	cmd.AddValue("lpccKr", "LPCC RTT-inflation decrease cap kr (<=0 means keep built-in default)", lpccKrArg);
	cmd.AddValue("lpccQueueTargetRatio", "LPCC steady-state queue target / epsilon (<=0 means keep built-in default 0.375)", lpccQueueTargetRatioArg);
	cmd.AddValue("lpccDropCapHigh", "LPCC single-step rate-drop cap at high qRatio (<=0 means keep built-in default 0.5)", lpccDropCapHighArg);
	cmd.AddValue("lpccAiSuppressMultiplier", "LPCC AI suppression window in increaseInterval units after FCNP (<=0 means keep built-in default 15)", lpccAiSuppressMultiplierArg);

	cmd.Parse (argc,argv);
	conf.open(confFile.c_str());
	while (!conf.eof())
	{
		std::string key;
		conf >> key;

		if (key.compare("ENABLE_QCN") == 0)
		{
			uint32_t v;
			conf >> v;
			enable_qcn = v;
			if (enable_qcn)
				std::cout << "ENABLE_QCN\t\t\t" << "Yes" << "\n";
			else
				std::cout << "ENABLE_QCN\t\t\t" << "No" << "\n";
		}
		else if (key.compare("CLAMP_TARGET_RATE") == 0)
		{
			uint32_t v;
			conf >> v;
			clamp_target_rate = v;
			if (clamp_target_rate)
				std::cout << "CLAMP_TARGET_RATE\t\t" << "Yes" << "\n";
			else
				std::cout << "CLAMP_TARGET_RATE\t\t" << "No" << "\n";
		}
		else if (key.compare("PAUSE_TIME") == 0)
		{
			double v;
			conf >> v;
			pause_time = v;
			std::cout << "PAUSE_TIME\t\t\t" << pause_time << "\n";
		}
		else if (key.compare("DATA_RATE") == 0)
		{
			std::string v;
			conf >> v;
			data_rate = v;
			std::cout << "DATA_RATE\t\t\t" << data_rate << "\n";
		}
		else if (key.compare("LINK_DELAY") == 0)
		{
			std::string v;
			conf >> v;
			link_delay = v;
			std::cout << "LINK_DELAY\t\t\t" << link_delay << "\n";
		}
		else if (key.compare("PACKET_PAYLOAD_SIZE") == 0)
		{
			uint32_t v;
			conf >> v;
			packet_payload_size = v;
			std::cout << "PACKET_PAYLOAD_SIZE\t\t" << packet_payload_size << "\n";
		}
		else if (key.compare("L2_CHUNK_SIZE") == 0)
		{
			uint32_t v;
			conf >> v;
			l2_chunk_size = v;
			std::cout << "L2_CHUNK_SIZE\t\t\t" << l2_chunk_size << "\n";
		}
		else if (key.compare("L2_ACK_INTERVAL") == 0)
		{
			uint32_t v;
			conf >> v;
			l2_ack_interval = v;
			std::cout << "L2_ACK_INTERVAL\t\t\t" << l2_ack_interval << "\n";
		}
		else if (key.compare("L2_BACK_TO_ZERO") == 0)
		{
			uint32_t v;
			conf >> v;
			l2_back_to_zero = v;
			if (l2_back_to_zero)
				std::cout << "L2_BACK_TO_ZERO\t\t\t" << "Yes" << "\n";
			else
				std::cout << "L2_BACK_TO_ZERO\t\t\t" << "No" << "\n";
		}
		else if (key.compare("TOPOLOGY_FILE") == 0)
		{
			std::string v;
			conf >> v;
			topology_file = v;
			std::cout << "TOPOLOGY_FILE\t\t\t" << topology_file << "\n";
		}
		else if (key.compare("FLOW_FILE") == 0)
		{
			std::string v;
			conf >> v;
			flow_file = v;
			std::cout << "FLOW_FILE\t\t\t" << flow_file << "\n";
		}
		else if (key.compare("TRACE_FILE") == 0)
		{
			std::string v;
			conf >> v;
			trace_file = v;
			std::cout << "TRACE_FILE\t\t\t" << trace_file << "\n";
		}
		else if (key.compare("TRACE_OUTPUT_FILE") == 0)
		{
			std::string v;
			conf >> v;
			trace_output_file = v;
			if (argc > 2)
			{
				trace_output_file = trace_output_file + std::string(argv[2]);
			}
			std::cout << "TRACE_OUTPUT_FILE\t\t" << trace_output_file << "\n";
		}
		else if (key.compare("SIMULATOR_STOP_TIME") == 0)
		{
			double v;
			conf >> v;
			simulator_stop_time = v;
			std::cout << "SIMULATOR_STOP_TIME\t\t" << simulator_stop_time << "\n";
		}
		else if (key.compare("ALPHA_RESUME_INTERVAL") == 0)
		{
			double v;
			conf >> v;
			alpha_resume_interval = v;
			std::cout << "ALPHA_RESUME_INTERVAL\t\t" << alpha_resume_interval << "\n";
		}
		else if (key.compare("RP_TIMER") == 0)
		{
			double v;
			conf >> v;
			rp_timer = v;
			std::cout << "RP_TIMER\t\t\t" << rp_timer << "\n";
		}
		else if (key.compare("EWMA_GAIN") == 0)
		{
			double v;
			conf >> v;
			ewma_gain = v;
			std::cout << "EWMA_GAIN\t\t\t" << ewma_gain << "\n";
		}
		else if (key.compare("FAST_RECOVERY_TIMES") == 0)
		{
			uint32_t v;
			conf >> v;
			fast_recovery_times = v;
			std::cout << "FAST_RECOVERY_TIMES\t\t" << fast_recovery_times << "\n";
		}
		else if (key.compare("RATE_AI") == 0)
		{
			std::string v;
			conf >> v;
			rate_ai = v;
			std::cout << "RATE_AI\t\t\t\t" << rate_ai << "\n";
		}
		else if (key.compare("RATE_HAI") == 0)
		{
			std::string v;
			conf >> v;
			rate_hai = v;
			std::cout << "RATE_HAI\t\t\t" << rate_hai << "\n";
		}
		else if (key.compare("ERROR_RATE_PER_LINK") == 0)
		{
			double v;
			conf >> v;
			error_rate_per_link = v;
			std::cout << "ERROR_RATE_PER_LINK\t\t" << error_rate_per_link << "\n";
		}
		else if (key.compare("CC_MODE") == 0){
			conf >> cc_mode;
			std::cout << "CC_MODE\t\t" << cc_mode << '\n';
		}else if (key.compare("RATE_DECREASE_INTERVAL") == 0){
			double v;
			conf >> v;
			rate_decrease_interval = v;
			std::cout << "RATE_DECREASE_INTERVAL\t\t" << rate_decrease_interval << "\n";
		}else if (key.compare("MIN_RATE") == 0){
			conf >> min_rate;
			std::cout << "MIN_RATE\t\t" << min_rate << "\n";
		}else if (key.compare("FCT_OUTPUT_FILE") == 0){
			conf >> fct_output_file;
			std::cout << "FCT_OUTPUT_FILE\t\t" << fct_output_file << '\n';
		}else if (key.compare("HAS_WIN") == 0){
			conf >> has_win;
			std::cout << "HAS_WIN\t\t" << has_win << "\n";
		}else if (key.compare("GLOBAL_T") == 0){
			conf >> global_t;
			std::cout << "GLOBAL_T\t\t" << global_t << '\n';
		}else if (key.compare("MI_THRESH") == 0){
			conf >> mi_thresh;
			std::cout << "MI_THRESH\t\t" << mi_thresh << '\n';
		}else if (key.compare("VAR_WIN") == 0){
			uint32_t v;
			conf >> v;
			var_win = v;
			std::cout << "VAR_WIN\t\t" << v << '\n';
		}else if (key.compare("FAST_REACT") == 0){
			uint32_t v;
			conf >> v;
			fast_react = v;
			std::cout << "FAST_REACT\t\t" << v << '\n';
		}else if (key.compare("U_TARGET") == 0){
			conf >> u_target;
			std::cout << "U_TARGET\t\t" << u_target << '\n';
		}else if (key.compare("INT_MULTI") == 0){
			conf >> int_multi;
			std::cout << "INT_MULTI\t\t\t\t" << int_multi << '\n';
		}else if (key.compare("RATE_BOUND") == 0){
			uint32_t v;
			conf >> v;
			rate_bound = v;
			std::cout << "RATE_BOUND\t\t" << rate_bound << '\n';
		}else if (key.compare("ACK_HIGH_PRIO") == 0){
			conf >> ack_high_prio;
			std::cout << "ACK_HIGH_PRIO\t\t" << ack_high_prio << '\n';
		}else if (key.compare("DCTCP_RATE_AI") == 0){
			conf >> dctcp_rate_ai;
			std::cout << "DCTCP_RATE_AI\t\t\t\t" << dctcp_rate_ai << "\n";
		}else if (key.compare("PFC_OUTPUT_FILE") == 0){
			conf >> pfc_output_file;
			std::cout << "PFC_OUTPUT_FILE\t\t\t\t" << pfc_output_file << '\n';
		}else if (key.compare("LINK_DOWN") == 0){
			conf >> link_down_time >> link_down_A >> link_down_B;
			std::cout << "LINK_DOWN\t\t\t\t" << link_down_time << ' '<< link_down_A << ' ' << link_down_B << '\n';
		}else if (key.compare("ENABLE_TRACE") == 0){
			conf >> enable_trace;
			std::cout << "ENABLE_TRACE\t\t\t\t" << enable_trace << '\n';
		}else if (key.compare("KMAX_MAP") == 0){
			int n_k ;
			conf >> n_k;
			std::cout << "KMAX_MAP\t\t\t\t";
			for (int i = 0; i < n_k; i++){
				uint64_t rate;
				uint32_t k;
				conf >> rate >> k;
				rate2kmax[rate] = k;
				std::cout << ' ' << rate << ' ' << k;
			}
			std::cout<<'\n';
		}else if (key.compare("KMIN_MAP") == 0){
			int n_k ;
			conf >> n_k;
			std::cout << "KMIN_MAP\t\t\t\t";
			for (int i = 0; i < n_k; i++){
				uint64_t rate;
				uint32_t k;
				conf >> rate >> k;
				rate2kmin[rate] = k;
				std::cout << ' ' << rate << ' ' << k;
			}
			std::cout<<'\n';
		}else if (key.compare("PMAX_MAP") == 0){
			int n_k ;
			conf >> n_k;
			std::cout << "PMAX_MAP\t\t\t\t";
			for (int i = 0; i < n_k; i++){
				uint64_t rate;
				double p;
				conf >> rate >> p;
				rate2pmax[rate] = p;
				std::cout << ' ' << rate << ' ' << p;
			}
			std::cout<<'\n';
		}else if (key.compare("GEMINI_DELAY_THRESH_NS") == 0){
			conf >> gemini_delay_thresh_ns;
			std::cout << "GEMINI_DELAY_THRESH_NS\t\t\t" << gemini_delay_thresh_ns << '\n';
		}else if (key.compare("GEMINI_BETA") == 0){
			conf >> gemini_beta;
			std::cout << "GEMINI_BETA\t\t\t\t" << gemini_beta << '\n';
		}else if (key.compare("GEMINI_H") == 0){
			conf >> gemini_h;
			std::cout << "GEMINI_H\t\t\t\t" << gemini_h << '\n';
		}else if (key.compare("GEMINI_DCN_DELAY_CUTOFF_US") == 0){
			conf >> gemini_dcn_delay_cutoff_us;
			std::cout << "GEMINI_DCN_DELAY_CUTOFF_US\t\t" << gemini_dcn_delay_cutoff_us << '\n';
		}else if (key.compare("FLOW_CONTROL_MODE") == 0){
			conf >> flow_control_mode;
			std::cout << "FLOW_CONTROL_MODE\t\t\t" << flow_control_mode << '\n';
		}else if (key.compare("BIFROST_TIMESLOT_US") == 0){
			conf >> bifrost_timeslot_us;
			std::cout << "BIFROST_TIMESLOT_US\t\t\t" << bifrost_timeslot_us << '\n';
		}else if (key.compare("BIFROST_K") == 0){
			conf >> bifrost_k;
			std::cout << "BIFROST_K\t\t\t\t" << bifrost_k << '\n';
		}else if (key.compare("BIFROST_LONGHAUL_DELAY_CUTOFF_US") == 0){
			conf >> bifrost_longhaul_delay_cutoff_us;
			std::cout << "BIFROST_LONGHAUL_DELAY_CUTOFF_US\t" << bifrost_longhaul_delay_cutoff_us << '\n';
		}else if (key.compare("BIFROST_H_MARGIN_SLOTS") == 0){
			conf >> bifrost_h_margin_slots;
			std::cout << "BIFROST_H_MARGIN_SLOTS\t\t\t" << bifrost_h_margin_slots << '\n';
		}else if (key.compare("BBR_RTT_WIN_MS") == 0){
			conf >> bbr_rtt_win_ms;
			std::cout << "BBR_RTT_WIN_MS\t\t\t\t" << bbr_rtt_win_ms << '\n';
		}else if (key.compare("BBR_PROBE_RTT_MS") == 0){
			conf >> bbr_probe_rtt_ms;
			std::cout << "BBR_PROBE_RTT_MS\t\t\t" << bbr_probe_rtt_ms << '\n';
		}else if (key.compare("BBR_INIT_CWND") == 0){
			conf >> bbr_init_cwnd;
			std::cout << "BBR_INIT_CWND\t\t\t\t" << bbr_init_cwnd << '\n';
		}else if (key.compare("BBR_SND_BUF_MB") == 0){
			conf >> bbr_snd_buf_mb;
			std::cout << "BBR_SND_BUF_MB\t\t\t\t" << bbr_snd_buf_mb << '\n';
		}else if (key.compare("BBR_RCV_BUF_MB") == 0){
			conf >> bbr_rcv_buf_mb;
			std::cout << "BBR_RCV_BUF_MB\t\t\t\t" << bbr_rcv_buf_mb << '\n';
		}else if (key.compare("BICC_DST_BDP_FACTOR") == 0){
			conf >> bicc_dst_bdp_factor;
			std::cout << "BICC_DST_BDP_FACTOR\t\t\t" << bicc_dst_bdp_factor << '\n';
		}else if (key.compare("BICC_LONGHAUL_CUTOFF_US") == 0){
			conf >> bicc_longhaul_cutoff_us;
			std::cout << "BICC_LONGHAUL_CUTOFF_US\t\t\t" << bicc_longhaul_cutoff_us << '\n';
		}else if (key.compare("BICC_NS_FB_INTERVAL_US") == 0){
			conf >> bicc_ns_fb_interval_us;
			std::cout << "BICC_NS_FB_INTERVAL_US\t\t\t" << bicc_ns_fb_interval_us << '\n';
		}else if (key.compare("BICC_BLEND_T_NS") == 0){
			conf >> bicc_blend_t_ns;
			std::cout << "BICC_BLEND_T_NS\t\t\t\t" << bicc_blend_t_ns << '\n';
		}else if (key.compare("THEMIS_CNP_INTERVAL_US") == 0){
			conf >> themis_cnp_interval_us;
			std::cout << "THEMIS_CNP_INTERVAL_US\t\t\t" << themis_cnp_interval_us << '\n';
		}else if (key.compare("THEMIS_TRP_ALPHA_INIT") == 0){
			conf >> themis_trp_alpha_init;
			std::cout << "THEMIS_TRP_ALPHA_INIT\t\t\t" << themis_trp_alpha_init << '\n';
		}else if (key.compare("THEMIS_TRP_BETA_US") == 0){
			conf >> themis_trp_beta_us;
			std::cout << "THEMIS_TRP_BETA_US\t\t\t" << themis_trp_beta_us << '\n';
		}else if (key.compare("THEMIS_TRP_LOOP_DELAY_NS") == 0){
			conf >> themis_trp_loop_delay_ns;
			std::cout << "THEMIS_TRP_LOOP_DELAY_NS\t\t" << themis_trp_loop_delay_ns << '\n';
		}else if (key.compare("THEMIS_TRP_MAX_LOOPS") == 0){
			conf >> themis_trp_max_loops;
			std::cout << "THEMIS_TRP_MAX_LOOPS\t\t\t" << themis_trp_max_loops << '\n';
		}else if (key.compare("THEMIS_LONGHAUL_CUTOFF_US") == 0){
			conf >> themis_longhaul_cutoff_us;
			std::cout << "THEMIS_LONGHAUL_CUTOFF_US\t\t" << themis_longhaul_cutoff_us << '\n';
		}else if (key.compare("TRANSPORT_MODE") == 0){
			conf >> transport_mode;
			std::cout << "TRANSPORT_MODE\t\t\t" << transport_mode << '\n';
		}else if (key.compare("GEMINI_K_MAP") == 0){
			int n_k;
			conf >> n_k;
			std::cout << "GEMINI_K_MAP\t\t\t\t";
			for (int i = 0; i < n_k; i++){
				uint64_t rate;
				uint32_t k;
				conf >> rate >> k;
				rate2geminiK[rate] = k;
				std::cout << ' ' << rate << ' ' << k;
			}
			std::cout<<'\n';
		}else if (key.compare("BUFFER_SIZE") == 0){
			conf >> buffer_size;
			std::cout << "BUFFER_SIZE\t\t\t\t" << buffer_size << '\n';
		}else if (key.compare("QLEN_MON_FILE") == 0){
			conf >> qlen_mon_file;
			std::cout << "QLEN_MON_FILE\t\t\t\t" << qlen_mon_file << '\n';
		}else if (key.compare("QLEN_MON_START") == 0){
			conf >> qlen_mon_start;
			std::cout << "QLEN_MON_START\t\t\t\t" << qlen_mon_start << '\n';
		}else if (key.compare("QLEN_MON_END") == 0){
			conf >> qlen_mon_end;
			std::cout << "QLEN_MON_END\t\t\t\t" << qlen_mon_end << '\n';
		}else if (key.compare("MULTI_RATE") == 0){
			int v;
			conf >> v;
			multi_rate = v;
			std::cout << "MULTI_RATE\t\t\t\t" << multi_rate << '\n';
		}else if (key.compare("SAMPLE_FEEDBACK") == 0){
			int v;
			conf >> v;
			sample_feedback = v;
			std::cout << "SAMPLE_FEEDBACK\t\t\t\t" << sample_feedback << '\n';
		}else if(key.compare("PINT_LOG_BASE") == 0){
			conf >> pint_log_base;
			std::cout << "PINT_LOG_BASE\t\t\t\t" << pint_log_base << '\n';
		}else if (key.compare("PINT_PROB") == 0){
			conf >> pint_prob;
			std::cout << "PINT_PROB\t\t\t\t" << pint_prob << '\n';
		}
		fflush(stdout);
	}
	conf.close();

	// debug for lpcc
	// wien = false;
	// delayWien = false;
	// algorithm = 9;
	// windowCheck = 0;

	// overriding config file. I prefer to use cmd arguments
	cc_mode = algorithm; // overrides configuration file
	transport_mode = transportModeArg;
	has_win=windowCheck; // overrides configuration file
	var_win = windowCheck; // overrides configuration file
	flow_control_mode = flowControlModeArg; // overrides configuration file
	if (transport_mode == TRANSPORT_MODE_TCP_BBR) {
		flow_control_mode = 0;
	}


	Config::SetDefault("ns3::QbbNetDevice::PauseTime", UintegerValue(pause_time));
	Config::SetDefault("ns3::QbbNetDevice::QcnEnabled", BooleanValue(enable_qcn));
	if (transport_mode == TRANSPORT_MODE_TCP_BBR) {
		Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::TcpBbr"));
		Config::SetDefault("ns3::TcpBbr::RttWindowLength", TimeValue(MilliSeconds(bbr_rtt_win_ms)));
		Config::SetDefault("ns3::TcpBbr::ProbeRttDuration", TimeValue(MilliSeconds(bbr_probe_rtt_ms)));
		Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(packet_payload_size));
		Config::SetDefault("ns3::TcpSocket::InitialCwnd", UintegerValue(bbr_init_cwnd));
		Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue(bbr_snd_buf_mb * 1024 * 1024));
		Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue(bbr_rcv_buf_mb * 1024 * 1024));
		// Bug 3 root cause: ns3::TcpSocketState::MaxPacingRate defaults to 4 Gb/s,
		// which BBR's SetPacingRate min()-caps with (tcp-bbr.cc:223). On a 100 Gbps
		// DCI link this hardcodes BBR throughput to ~4 Gbps regardless of cwnd or
		// BtlBw estimate. Lift the cap to 200 Gb/s (well above link rate) so BBR
		// can actually probe up to the bottleneck.
		Config::SetDefault("ns3::TcpSocketState::MaxPacingRate", DataRateValue(DataRate("200Gb/s")));
	}

	// set int_multi
	IntHop::multi = int_multi;
	// IntHeader::mode
	if (transport_mode == TRANSPORT_MODE_TCP_BBR)
		IntHeader::mode = IntHeader::NONE;
	else if (cc_mode == 7 || cc_mode == 9 || cc_mode == 11) // timely, lpcc or gemini, use ts
		IntHeader::mode = IntHeader::TS;
	else if (cc_mode == 3) // hpcc, powertcp, use int
		IntHeader::mode = IntHeader::NORMAL;
	else if (cc_mode == 10) // hpcc-pint
		IntHeader::mode = IntHeader::PINT;
	else // others, no extra header
		IntHeader::mode = IntHeader::NONE;

	// lpcc: epsilon — CLI override wins, else fall back to 4 MB (aligned with v17 burst profile;
	// was 4000 = 4 KB, far too tight for DCI). This value is fed to both sw->SetEpsilon and
	// rdmaHw->SetAttribute("LpccEpsilon", ...) below. If --lpccEpsilon is passed via CLI it takes
	// precedence over both.
	uint32_t epsilon = 0;
	if (cc_mode == 9) {
		epsilon = (lpccEpsilonArg > 0) ? static_cast<uint32_t>(lpccEpsilonArg) : 4000000;
	}

	// Set Pint
	if (cc_mode == 10){
		Pint::set_log_base(pint_log_base);
		IntHeader::pint_bytes = Pint::get_n_bytes();
	}

	topof.open(topology_file.c_str());
	flowf.open(flow_file.c_str());
	uint32_t node_num, switch_num, tors, link_num;
	topof >> node_num >> switch_num >> tors >> link_num; // changed here. The previous order was node, switch, link // tors is not used. switch_num=tors for now.
	tors=switch_num;
	std::cout << node_num << " " << switch_num << " " << tors <<  " " << link_num << std::endl;
	flowf >> flow_num;

	NodeContainer serverNodes;
	NodeContainer torNodes;
	NodeContainer spineNodes;
	NodeContainer switchNodes;
	NodeContainer allNodes;

	std::vector<uint32_t> node_type(node_num, 0);

	std::cout << "switch_num "<< switch_num << std::endl;
	for (uint32_t i=0;i<switch_num;i++){
		uint32_t sid;
		topof >> sid;
		std::cout << "sid " << sid << std::endl;
		switchNumToId[i]=sid;
		switchIdToNum[sid]=i;
		if(i<tors){
			node_type[sid]=1;
		}
		else
			node_type[sid]=2;

	}

	// BiCC SwitchNode attribute overrides (applied before SwitchNode creation).
	Config::SetDefault("ns3::SwitchNode::BiccDstBdpFactor",            DoubleValue(bicc_dst_bdp_factor));
	Config::SetDefault("ns3::SwitchNode::BiccLonghaulDelayCutoffUs",   UintegerValue(bicc_longhaul_cutoff_us));
	Config::SetDefault("ns3::SwitchNode::BiccNsFeedbackMinIntervalUs", UintegerValue(bicc_ns_fb_interval_us));
	Config::SetDefault("ns3::SwitchNode::ThemisCnpIntervalUs",         UintegerValue(themis_cnp_interval_us));
	Config::SetDefault("ns3::SwitchNode::ThemisTrpAlphaInit",          UintegerValue(themis_trp_alpha_init));
	Config::SetDefault("ns3::SwitchNode::ThemisTrpBetaUs",             UintegerValue(themis_trp_beta_us));
	Config::SetDefault("ns3::SwitchNode::ThemisTrpLoopDelayNs",        UintegerValue(themis_trp_loop_delay_ns));
	Config::SetDefault("ns3::SwitchNode::ThemisTrpMaxLoops",           UintegerValue(themis_trp_max_loops));
	Config::SetDefault("ns3::SwitchNode::ThemisLonghaulDelayCutoffUs", UintegerValue(themis_longhaul_cutoff_us));

	for (uint32_t i = 0; i < node_num; i++){
		if (node_type[i] == 0){
			Ptr<Node> node = CreateObject<Node>();
			n.Add(node);
			allNodes.Add(node);
			serverNodes.Add(node);
		}
		else{
			Ptr<SwitchNode> sw = CreateObject<SwitchNode>();
			n.Add(sw);
			switchNodes.Add(sw);
			allNodes.Add(sw);
			sw->SetAttribute("EcnEnabled", BooleanValue(enable_qcn));
			sw->SetEpsilon(epsilon);
			// LPCC switch-side CLI overrides (mirror burst.cc:1536-1549). Apply only when user
			// passed a positive value via CLI; otherwise switch-node.cc attribute defaults apply.
			// Note: lpccEpsilonArg already feeds the local 'epsilon' var above via SetEpsilon.
			if (lpccFcnpMinIntervalUsArg > 0) {
				sw->SetAttribute("FcnpMinIntervalUs", UintegerValue(static_cast<uint32_t>(lpccFcnpMinIntervalUsArg)));
			}
			if (lpccPerFlowFcnpCooldownUsArg > 0) {
				sw->SetAttribute("LpccPerFlowFcnpCooldownUs", UintegerValue(static_cast<uint32_t>(lpccPerFlowFcnpCooldownUsArg)));
			}
			if (lpccFcnpTopKArg > 0) {
				sw->SetAttribute("LpccFcnpTopK", UintegerValue(static_cast<uint32_t>(lpccFcnpTopKArg)));
			}
			if (lpccFcnpTopKHighArg > 0) {
				sw->SetAttribute("LpccFcnpTopKHigh", UintegerValue(static_cast<uint32_t>(lpccFcnpTopKHighArg)));
			}
			if (lpccFcnpKHighThreshBytesArg > 0) {
				sw->SetAttribute("LpccFcnpKHighThreshBytes", UintegerValue(static_cast<uint32_t>(lpccFcnpKHighThreshBytesArg)));
			}
			if (node_type[i]==1){
				torNodes.Add(sw);
				sw->SetNodeType(1);
			}
			else{
				spineNodes.Add(sw);
				sw->SetNodeType(2);
			}

		}
	}


	NS_LOG_INFO("Create nodes.");

	InternetStackHelper internet;
	internet.Install(n);

	//
	// Assign IP to each server
	//
	for (uint32_t i = 0; i < node_num; i++){
		if (n.Get(i)->GetNodeType() == 0){ // is server
			serverAddress.resize(i + 1);
			serverAddress[i] = node_id_to_ip(i);
		}
	}

	NS_LOG_INFO("Create channels.");

	//
	// Explicitly create the channels required by the topology.
	//

	Ptr<RateErrorModel> rem = CreateObject<RateErrorModel>();
	Ptr<UniformRandomVariable> uv = CreateObject<UniformRandomVariable>();
	rem->SetRandomVariable(uv);
	uv->SetStream(50);
	rem->SetAttribute("ErrorRate", DoubleValue(error_rate_per_link));
	rem->SetAttribute("ErrorUnit", StringValue("ERROR_UNIT_PACKET"));

	// FILE *pfc_file = fopen(pfc_output_file.c_str(), "w");

	QbbHelper qbb;
	Ipv4AddressHelper ipv4;
	for (uint32_t i = 0; i < link_num; i++)
	{
		uint32_t src, dst;
		std::string data_rate, link_delay;
		double error_rate;
		topof >> src >> dst >> data_rate >> link_delay >> error_rate;

		std::cout << src << " " << dst << " " << n.GetN()<< std::endl;
		Ptr<Node> snode = n.Get(src), dnode = n.Get(dst);


		qbb.SetDeviceAttribute("DataRate", StringValue(data_rate));
		qbb.SetChannelAttribute("Delay", StringValue(link_delay));

		if (error_rate > 0)
		{
			Ptr<RateErrorModel> rem = CreateObject<RateErrorModel>();
			Ptr<UniformRandomVariable> uv = CreateObject<UniformRandomVariable>();
			rem->SetRandomVariable(uv);
			uv->SetStream(50);
			rem->SetAttribute("ErrorRate", DoubleValue(error_rate));
			rem->SetAttribute("ErrorUnit", StringValue("ERROR_UNIT_PACKET"));
			qbb.SetDeviceAttribute("ReceiveErrorModel", PointerValue(rem));
		}
		else
		{
			qbb.SetDeviceAttribute("ReceiveErrorModel", PointerValue(rem));
		}

		fflush(stdout);

		// Assigne server IP
		// Note: this should be before the automatic assignment below (ipv4.Assign(d)),
		// because we want our IP to be the primary IP (first in the IP address list),
		// so that the global routing is based on our IP
		NetDeviceContainer d = qbb.Install(snode, dnode);
		if (snode->GetNodeType() == 0){
			Ptr<Ipv4> ipv4 = snode->GetObject<Ipv4>();
			ipv4->AddInterface(d.Get(0));
			ipv4->AddAddress(1, Ipv4InterfaceAddress(serverAddress[src], Ipv4Mask(0xff000000)));
		}
		if (dnode->GetNodeType() == 0){
			Ptr<Ipv4> ipv4 = dnode->GetObject<Ipv4>();
			ipv4->AddInterface(d.Get(1));
			ipv4->AddAddress(1, Ipv4InterfaceAddress(serverAddress[dst], Ipv4Mask(0xff000000)));
		}


		if (!snode->GetNodeType()){
			sourceNodes[src].Add(DynamicCast<QbbNetDevice>(d.Get(0)));
		}


		if(snode->GetNodeType()&& dnode->GetNodeType()){
			switchToSwitchInterfaces.Add(d);
			switchUp[switchIdToNum[src]].Add(DynamicCast<QbbNetDevice>(d.Get(0)));
			switchUp[switchIdToNum[dst]].Add(DynamicCast<QbbNetDevice>(d.Get(1)));
			switchToSwitch[src][dst].push_back(DynamicCast<QbbNetDevice>(d.Get(0)));
			switchToSwitch[src][dst].push_back(DynamicCast<QbbNetDevice>(d.Get(1)));
		}

		// used to create a graph of the topology
		nbr2if[snode][dnode].idx = DynamicCast<QbbNetDevice>(d.Get(0))->GetIfIndex();
		nbr2if[snode][dnode].up = true;
		nbr2if[snode][dnode].delay = DynamicCast<QbbChannel>(DynamicCast<QbbNetDevice>(d.Get(0))->GetChannel())->GetDelay().GetTimeStep();
		nbr2if[snode][dnode].bw = DynamicCast<QbbNetDevice>(d.Get(0))->GetDataRate().GetBitRate();
		nbr2if[dnode][snode].idx = DynamicCast<QbbNetDevice>(d.Get(1))->GetIfIndex();
		nbr2if[dnode][snode].up = true;
		nbr2if[dnode][snode].delay = DynamicCast<QbbChannel>(DynamicCast<QbbNetDevice>(d.Get(1))->GetChannel())->GetDelay().GetTimeStep();
		nbr2if[dnode][snode].bw = DynamicCast<QbbNetDevice>(d.Get(1))->GetDataRate().GetBitRate();

		// This is just to set up the connectivity between nodes. The IP addresses are useless
		// char ipstring[16];
		std::stringstream ipstring;
		ipstring << "10."<< i / 254 + 1 << "." << i % 254 + 1 << ".0";
		// sprintf(ipstring, "10.%d.%d.0", i / 254 + 1, i % 254 + 1);
		ipv4.SetBase(ipstring.str().c_str(), "255.255.255.0");
		ipv4.Assign(d);

		// setup PFC trace
		// DynamicCast<QbbNetDevice>(d.Get(0))->TraceConnectWithoutContext("QbbPfc", MakeBoundCallback (&get_pfc, pfc_file, DynamicCast<QbbNetDevice>(d.Get(0))));
		// DynamicCast<QbbNetDevice>(d.Get(1))->TraceConnectWithoutContext("QbbPfc", MakeBoundCallback (&get_pfc, pfc_file, DynamicCast<QbbNetDevice>(d.Get(1))));
	}

	nic_rate = get_nic_rate(n);
	uint32_t geminiKBytes = rate2geminiK.count(nic_rate) ? rate2geminiK[nic_rate] : 64000;

	// config switch
	// The switch mmu runs Dynamic Thresholds (DT) by default.
	for (uint32_t i = 0; i < node_num; i++) {
		if (n.Get(i)->GetNodeType()) { // is switch
			Ptr<SwitchNode> sw = DynamicCast<SwitchNode>(n.Get(i));
			// DCI tune: alpha 1/8 → 1/2 to give PFC threshold ~100MB (was 25MB) on 256MB buffer
			double alpha = 1.0/2;
			sw->m_mmu->SetAlphaIngress(alpha);
			uint64_t totalHeadroom = 0;
			for (uint32_t j = 1; j < sw->GetNDevices(); j++) {
				Ptr<QbbNetDevice> dev = DynamicCast<QbbNetDevice>(sw->GetDevice(j));
				uint64_t rate = dev->GetDataRate().GetBitRate();
				uint64_t delay = DynamicCast<QbbChannel>(dev->GetChannel())->GetDelay().GetTimeStep();
				bool useBifrostPort = flow_control_mode == 1 && delay >= bifrost_longhaul_delay_cutoff_us * 1000ULL;
				if (cc_mode == 11) {
					bool isDcnPort = delay <= gemini_dcn_delay_cutoff_us * 1000ULL;
					sw->m_mmu->SetEcnEnabled(j, isDcnPort);
					if (isDcnPort) {
						uint32_t portGeminiK = rate2geminiK.count(rate) ? rate2geminiK[rate] : geminiKBytes;
						sw->m_mmu->ConfigEcnFixed(j, portGeminiK);
					}
				}
				uint64_t deltaBytes = 2 * rate * delay / 8000000000ULL;
				uint64_t slotBytes = rate * bifrost_timeslot_us * 1000ULL / 8000000000ULL;
				uint64_t mtuOnWire = packet_payload_size + CustomHeader::GetStaticWholeHeaderSize();
				uint64_t extraHeadroom = bifrost_k * mtuOnWire;
				uint64_t hMarginSlots = std::max<uint32_t>(2, bifrost_h_margin_slots);
				uint64_t reservedBytesH = deltaBytes + hMarginSlots * slotBytes;
				uint64_t thresholdBytes = reservedBytesH > extraHeadroom ? reservedBytesH - extraHeadroom : 0;
				if (useBifrostPort) {
					sw->ConfigureBifrostPort(j, deltaBytes, reservedBytesH, MicroSeconds(bifrost_timeslot_us), bifrost_k);
				}
				
				for (uint32_t qu = 0; qu < 8; qu++){
					if (cc_mode != 11) {
						sw->m_mmu->ConfigEcn(j, rate2kmin[rate], rate2kmax[rate], rate2pmax[rate]);
					}
					// set pfc
					uint64_t headroom = rate * delay / 8 / 1000000000 * 3;
					if (useBifrostPort && qu != 0) {
						sw->m_mmu->SetReserved(thresholdBytes, j, qu, "ingress");
						headroom = extraHeadroom;
					}
					
					sw->m_mmu->SetHeadroom(headroom, j, qu);
					totalHeadroom += headroom;
				}

			}
			sw->m_mmu->SetBufferPool(buffer_size * 1024 * 1024);
			sw->m_mmu->SetIngressPool(buffer_size * 1024 * 1024 - totalHeadroom);
			sw->m_mmu->SetEgressLosslessPool(buffer_size * 1024 * 1024);
			sw->m_mmu->node_id = sw->GetId();
		}
	}

	#if ENABLE_QP
	g_fct_output = fopen(fct_output_file.c_str(), "w");
	if (!g_fct_output){
		std::cout << "cannot open fct output file " << fct_output_file << std::endl;
		exit(1);
	}
	//
	// install RDMA driver
	//
	// BiCC RdmaHw attribute override (applied before RdmaHw creation).
	Config::SetDefault("ns3::RdmaHw::BiccBlendBaseRttNs", UintegerValue(bicc_blend_t_ns));

	for (uint32_t i = 0; i < node_num; i++){
		if (n.Get(i)->GetNodeType() == 0){ // is server
			// create RdmaHw
			Ptr<RdmaHw> rdmaHw = CreateObject<RdmaHw>();
			rdmaHw->SetAttribute("ClampTargetRate", BooleanValue(clamp_target_rate));
			rdmaHw->SetAttribute("AlphaResumInterval", DoubleValue(alpha_resume_interval));
			rdmaHw->SetAttribute("RPTimer", DoubleValue(rp_timer));
			rdmaHw->SetAttribute("FastRecoveryTimes", UintegerValue(fast_recovery_times));
			rdmaHw->SetAttribute("EwmaGain", DoubleValue(ewma_gain));
			rdmaHw->SetAttribute("RateAI", DataRateValue(DataRate(rate_ai)));
			rdmaHw->SetAttribute("RateHAI", DataRateValue(DataRate(rate_hai)));
			rdmaHw->SetAttribute("L2BackToZero", BooleanValue(l2_back_to_zero));
			rdmaHw->SetAttribute("L2ChunkSize", UintegerValue(l2_chunk_size));
			rdmaHw->SetAttribute("L2AckInterval", UintegerValue(l2_ack_interval));
			rdmaHw->SetAttribute("CcMode", UintegerValue(cc_mode));
			rdmaHw->SetAttribute("RateDecreaseInterval", DoubleValue(rate_decrease_interval));
			rdmaHw->SetAttribute("MinRate", DataRateValue(DataRate(min_rate)));
			rdmaHw->SetAttribute("Mtu", UintegerValue(packet_payload_size));
			rdmaHw->SetAttribute("MiThresh", UintegerValue(mi_thresh));
			rdmaHw->SetAttribute("VarWin", BooleanValue(var_win));
			rdmaHw->SetAttribute("FastReact", BooleanValue(fast_react));
			rdmaHw->SetAttribute("MultiRate", BooleanValue(multi_rate));
			rdmaHw->SetAttribute("SampleFeedback", BooleanValue(sample_feedback));
			rdmaHw->SetAttribute("TargetUtil", DoubleValue(u_target));
			rdmaHw->SetAttribute("RateBound", BooleanValue(rate_bound));
			rdmaHw->SetAttribute("DctcpRateAI", DataRateValue(DataRate(dctcp_rate_ai)));
			rdmaHw->SetAttribute("PowerTCPEnabled", BooleanValue(wien));
			rdmaHw->SetAttribute("PowerTCPdelay", BooleanValue(delayWien));
			rdmaHw->SetAttribute("LpccEpsilon", UintegerValue(epsilon));
			// LPCC sender-side CLI overrides (mirror burst.cc:1434-1457). Only apply if user passed
			// a positive value via CLI; otherwise the C++ attribute defaults from rdma-hw.cc apply.
			if (lpccThetaUsArg > 0) {
				rdmaHw->SetAttribute("LpccTheta", DoubleValue(lpccThetaUsArg));
			}
			if (lpccIncreaseIntervalUsArg > 0) {
				rdmaHw->SetAttribute("LpccIncreaseInterval", UintegerValue(static_cast<uint32_t>(lpccIncreaseIntervalUsArg)));
			}
			if (lpccIncreaseFactorArg > 0) {
				rdmaHw->SetAttribute("LpccIncreaseFactor", DoubleValue(lpccIncreaseFactorArg));
			}
			if (lpccWrArg > 0) {
				rdmaHw->SetAttribute("Lpcc_m_wr", DoubleValue(lpccWrArg));
			}
			if (lpccKrArg > 0) {
				rdmaHw->SetAttribute("Lpcc_m_kr", DoubleValue(lpccKrArg));
			}
			if (lpccQueueTargetRatioArg > 0) {
				rdmaHw->SetAttribute("LpccQueueTargetRatio", DoubleValue(lpccQueueTargetRatioArg));
			}
			if (lpccDropCapHighArg > 0) {
				rdmaHw->SetAttribute("LpccDropCapHigh", DoubleValue(lpccDropCapHighArg));
			}
			if (lpccAiSuppressMultiplierArg > 0) {
				rdmaHw->SetAttribute("LpccAiSuppressMultiplier", UintegerValue(static_cast<uint32_t>(lpccAiSuppressMultiplierArg)));
			}
			rdmaHw->SetAttribute("GeminiDelayThreshNs", UintegerValue(gemini_delay_thresh_ns));
			rdmaHw->SetAttribute("GeminiWanBeta", DoubleValue(gemini_beta));
			rdmaHw->SetAttribute("GeminiH", DoubleValue(gemini_h));
			rdmaHw->SetAttribute("GeminiKBytes", UintegerValue(geminiKBytes));
			rdmaHw->SetAttribute("GeminiDcnPortDelayCutoff", TimeValue(MicroSeconds(gemini_dcn_delay_cutoff_us)));
			rdmaHw->SetPintSmplThresh(pint_prob);
			// create and install RdmaDriver
			Ptr<RdmaDriver> rdma = CreateObject<RdmaDriver>();
			Ptr<Node> node = n.Get(i);
			rdma->SetNode(node);
			rdma->SetRdmaHw(rdmaHw);

			node->AggregateObject (rdma);
			rdma->Init();
			rdma->TraceConnectWithoutContext("QpComplete", MakeBoundCallback (qp_finish, g_fct_output));
		}
	}

	#endif

	// set ACK priority on hosts
	if (ack_high_prio)
		RdmaEgressQueue::ack_q_idx = 0;
	else
		RdmaEgressQueue::ack_q_idx = 3;

	// setup routing
	CalculateRoutes(n);
	std::cout << "yeah " << 0<<  std::endl;
	SetRoutingEntries();
	std::cout << "yeah " << 1<<  std::endl;

	//
	// get BDP and delay
	//
	maxRtt = maxBdp = 0;
	for (uint32_t i = 0; i < node_num; i++){
		if (n.Get(i)->GetNodeType() != 0)
			continue;
		for (uint32_t j = 0; j < node_num; j++){
			if (n.Get(j)->GetNodeType() != 0)
				continue;
			uint64_t delay = pairDelay[n.Get(i)][n.Get(j)];
			uint64_t txDelay = pairTxDelay[n.Get(i)][n.Get(j)];
			uint64_t rtt = delay * 2 + txDelay;
			uint64_t bw = pairBw[i][j];
			uint64_t bdp = rtt * bw / 1000000000/8; 
			pairBdp[n.Get(i)][n.Get(j)] = bdp;
			pairRtt[i][j] = rtt;
			if (bdp > maxBdp)
				maxBdp = bdp;
			if (rtt > maxRtt)
				maxRtt = rtt;
		}
	}
	printf("maxRtt=%lu maxBdp=%lu\n", maxRtt, maxBdp);

	//
	// setup switch CC
	//
	for (uint32_t i = 0; i < node_num; i++){
		if (n.Get(i)->GetNodeType()){ // switch
			Ptr<SwitchNode> sw = DynamicCast<SwitchNode>(n.Get(i));
			sw->SetAttribute("CcMode", UintegerValue(cc_mode));
			sw->SetAttribute("MaxRtt", UintegerValue(maxRtt));
			sw->SetAttribute("PowerEnabled", BooleanValue(wien));
			sw->SetAttribute("TransportMode", UintegerValue(transport_mode));
			sw->SetAttribute("FlowControlMode", UintegerValue(flow_control_mode));
			sw->SetAttribute("BifrostTimeSlotUs", UintegerValue(bifrost_timeslot_us));
			sw->SetAttribute("BifrostK", UintegerValue(bifrost_k));
			sw->SetAttribute("BifrostLonghaulDelayCutoffUs", UintegerValue(bifrost_longhaul_delay_cutoff_us));
			sw->SetAttribute("BifrostHMarginSlots", UintegerValue(bifrost_h_margin_slots));
			if (flow_control_mode == 1) {
				for (uint32_t j = 1; j < sw->GetNDevices(); j++) {
					Ptr<QbbNetDevice> dev = DynamicCast<QbbNetDevice>(sw->GetDevice(j));
					uint64_t delay = DynamicCast<QbbChannel>(dev->GetChannel())->GetDelay().GetTimeStep();
					if (delay >= bifrost_longhaul_delay_cutoff_us * 1000ULL) {
						sw->SetBifrostPortEnabled(j, true);
					}
				}
			}
		}
	}

	Ipv4GlobalRoutingHelper::PopulateRoutingTables();

	NS_LOG_INFO("Create Applications.");

	Time interPacketInterval = Seconds(0.0000005 / 2);


	// maintain port number for each host
	for (uint32_t i = 0; i < node_num; i++){
		if (n.Get(i)->GetNodeType() == 0)
			for (uint32_t j = 0; j < node_num; j++){
				if (n.Get(j)->GetNodeType() == 0)
					portNumder[i][j] = 10000; // each host pair use port number from 10000
			}
	}


	flow_input.idx = 0;
	if (flow_num > 0){
		if (transport_mode == TRANSPORT_MODE_TCP_BBR) {
			for (uint32_t idx = 0; idx < flow_num; ++idx) {
				// Bug 1 fix: BBR path now reads the 7th column (stop_time) too,
				// matching ReadFlowInput()'s parse format. Without this, a 7-column
				// flow file (used by the fairness experiment) silently misaligns —
				// row N+1's `src` gets row N's `stop_time` token, parsing fails for
				// uint32_t, and BulkSendApplication's bind aborts at sim start.
				flowf >> flow_input.src >> flow_input.dst >> flow_input.pg >> flow_input.dport
				      >> flow_input.maxPacketCount >> flow_input.start_time >> flow_input.stop_time;
				uint32_t port = portNumder[flow_input.src][flow_input.dst]++;
				Ptr<BulkSendApplication> sender = CreateObject<BulkSendApplication>();
				sender->SetAttribute("Protocol", TypeIdValue(TcpSocketFactory::GetTypeId()));
				sender->SetAttribute("SendSize", UintegerValue(packet_payload_size));
				sender->SetAttribute("MaxBytes", UintegerValue(flow_input.maxPacketCount));
				sender->SetAttribute("FlowId", UintegerValue(++tcp_flow_id));
				sender->SetAttribute("priorityCustom", UintegerValue(1));
				sender->SetAttribute("priority", UintegerValue(1));
				sender->SetAttribute("Remote", AddressValue(InetSocketAddress(serverAddress[flow_input.dst], flow_input.dport)));
				sender->SetAttribute("Local", AddressValue(InetSocketAddress(serverAddress[flow_input.src], port)));
				n.Get(flow_input.src)->AddApplication(sender);
				sender->SetStartTime(Seconds(flow_input.start_time));
				// Bug 2 fix: honor per-flow stop_time for BBR too. For TCP/BBR we
				// stop the BulkSendApplication directly (no RDMA QP to truncate),
				// and flag g_src_stopped so PrintResultsFlow skips this source
				// after stop — matches RDMA path's force-stop semantics.
				bool useStopTime = (flow_input.stop_time > 0.0);
				Time stopT = useStopTime ? Seconds(flow_input.stop_time) : Seconds(simulator_stop_time);
				sender->SetStopTime(stopT);
				if (useStopTime) {
					uint32_t srcId = flow_input.src;
					Simulator::Schedule(stopT, [srcId]() {
						g_src_stopped[srcId] = true;
						std::cout << "FLOW_STOP t=" << Simulator::Now().GetSeconds()
						          << " srcId=" << srcId << " (TCP/BBR)" << std::endl;
					});
				}

				PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), flow_input.dport));
				ApplicationContainer sinkApp = sink.Install(n.Get(flow_input.dst));
				sinkApp.Get(0)->SetAttribute("TotalQueryBytes", UintegerValue(flow_input.maxPacketCount));
				sinkApp.Get(0)->SetAttribute("recvAt", TimeValue(Seconds(flow_input.start_time)));
				sinkApp.Get(0)->SetAttribute("priority", UintegerValue(1));
				sinkApp.Get(0)->SetAttribute("priorityCustom", UintegerValue(1));
				sinkApp.Get(0)->SetAttribute("senderPriority", UintegerValue(1));
				sinkApp.Get(0)->SetAttribute("flowId", UintegerValue(tcp_flow_id));
				sinkApp.Get(0)->TraceConnectWithoutContext(
					"FlowFinish",
					MakeBoundCallback(
						+[](FILE *fout, uint32_t sid, uint32_t did, uint16_t sport, uint16_t dport, double totalSize, double start, bool, uint32_t) {
							uint64_t flowSize = static_cast<uint64_t>(totalSize);
							uint64_t baseRtt = pairRtt[sid][did];
							uint64_t bw = pairBw[sid][did];
							uint64_t standaloneFct = baseRtt + flowSize * 8000000000ULL / bw;
							uint64_t startNs = static_cast<uint64_t>(start);
							uint64_t fct = Simulator::Now().GetNanoSeconds() - startNs;
							fprintf(fout, "%08x %08x %u %u %lu %lu %lu %lu\n",
								node_id_to_ip(sid).Get(), node_id_to_ip(did).Get(), sport, dport, flowSize, startNs, fct, standaloneFct);
							fflush(fout);
						},
						g_fct_output, static_cast<uint32_t>(flow_input.src), static_cast<uint32_t>(flow_input.dst), static_cast<uint16_t>(port), static_cast<uint16_t>(flow_input.dport)));
				sinkApp.Start(Seconds(flow_input.start_time));
				sinkApp.Stop(Seconds(simulator_stop_time));
			}
			flowf.close();
		} else {
			ReadFlowInput();
			std::cout << flow_input.start_time << std::endl;
			Simulator::Schedule(Seconds(flow_input.start_time)-Simulator::Now(), ScheduleFlowInputs);
		}
	}

	topof.close();
	tracef.close();
	double delay = 0.5*maxRtt*1e-9; // 10 micro seconds
	Simulator::Schedule(Seconds(delay),PrintResultsFlow,sourceNodes,flow_num,delay);

	std::cout << "Running Simulation.\n";
	NS_LOG_INFO("Run Simulation.");
	Simulator::Stop(Seconds(simulator_stop_time));
	Simulator::Run();
	Simulator::Destroy();
	NS_LOG_INFO("Done.");

	endt = clock();
	std::cout << (double)(endt - begint) / CLOCKS_PER_SEC << "\n";
}
