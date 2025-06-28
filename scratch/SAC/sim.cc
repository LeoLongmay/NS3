#include "ns3/rdma.h"
#include "ns3/rdma-hw.h"
#include "ns3/sim-setting.h"
#include "ns3/switch-node.h"
#include "ns3/rdma-client.h"
#include "ns3/rdma-driver.h"
#include "ns3/rdma-client-helper.h"

#include <time.h>
#include <limits>
#include <random>
#include <algorithm>
#include "cstdio"
#include "fstream"
#include "iostream"
#include "unordered_map"

#include "ns3/assert.h"
#include "ns3/packet.h"
#include "ns3/settings.h"
#include "ns3/qbb-helper.h"
#include "ns3/core-module.h"
#include "ns3/error-model.h"
#include "ns3/conweave-voq.h"
#include "ns3/broadcom-node.h"
#include "ns3/conga-routing.h"
#include "ns3/qbb-net-device.h"
#include "ns3/letflow-routing.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/global-route-manager.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/ipv4-static-routing-helper.h"

#include "ns3/node-list.h"
#include "ns3/opengym-module.h"

using namespace ns3;
using namespace std;

NS_LOG_COMPONENT_DEFINE("DMRL_LOAD_BALANCE_SIMULATION");

/*----------------load balancing parameters----------------*/
/**
 * 定义负载均衡模型参数：load balance mode:
 * 0：flow ECMP ；2：DRILL ；3：Conga；6：Letflow ；9：Conweave
 */
uint32_t lb_mode = 0;

/*----------------simulation variables----------------*/
uint64_t one_hop_delay = 1000;

uint32_t cc_model = 1; // 拥塞控制模型: 1：DCQCN
double pause_time = 5; // PFC 暂停帧时间，5us 流量控制过程中暂停的数据传输时间
bool enable_qcn = true, enable_pfc = true, use_dynamic_pfc_threshold = true;
uint32_t packet_payload_size = 1000, l2_chunk_size = 0, l2_ack_interval = 0;
double flowgen_start_time = 2.0, flowgen_stop_time = 2.5, simulator_extra_time = 0.1;
uint64_t qlen_mon_start, qlen_mon_end;
uint64_t cnp_mon_start, irn_mon_start;
uint32_t switch_mon_interval = 10000;
uint64_t cnp_monitor_bucket = 100000;
uint64_t irn_monitor_bucket = 100000;

// 定义文件指针，用于输出不同类型的仿真数据 voq:虚拟输出队列；voq_detail:详细的虚拟输出队列；
FILE *pfc_file = NULL;
FILE *fct_output = NULL;
FILE *path_output = NULL;
FILE *est_error_output = NULL;
FILE *flow_input_stream = NULL;

string fct_output_file = "fct.txt";
string pfc_output_file = "pfc.txt";
string flow_input_file = "flow.txt";
string path_output_file = "path.txt";
string data_rate, link_delay, topology_file, flow_file;

string dctcp_rate_ai = "1000Mb/s";
string rate_ai, rate_hai, min_rate = "100Mb/s";
uint32_t fast_recovery_times = 1;
double rate_decrease_interval = 4;
double alpha_resume_interval = 55, rp_timer = 300, ewma_gain = 1 / 16;

bool var_win = false;
bool rate_bound = true;
bool fast_react = true;
bool multi_rate = true;
bool sample_feedback = false;
bool l2_back_to_zero = false;
bool clamp_target_rate = false;

uint32_t has_win = 1;
uint32_t global_t = 1;
uint32_t mi_thresh = 5;
uint32_t int_multi = 1;
double u_target = 0.95;
double error_rate_per_link = 0.0;

unordered_map<uint64_t, double> rate2pmax;
unordered_map<uint32_t, Ptr<SwitchNode>> idxNodeToR;
unordered_map<uint64_t, uint32_t> rate2kmax, rate2kmin;

// 配置链路故障场景、ACK优先级和缓冲区配置
uint32_t buffer_size = 0; // 0 to set buffer size automatically 缓冲区大小
uint64_t link_down_time = 0;
uint32_t link_down_A = 0, link_down_B = 0; // 链路故障状态，0不可用，1可用

// Added from Here
double load = 10.0;
int enable_irn = 0;
int random_seed = 1;

uint64_t maxRtt, maxBdp;

// 网络接口结构体：idx-索引；up-是否启用；delay-延迟；bw-带宽
struct Interface
{
    uint32_t idx;

    /* data */
    bool up;
    uint64_t bw;
    uint64_t delay;

    Interface() : idx(0), up(false) {}
};

// nbr2if：每个节点与其邻居节点之间的接口映射，链路信息
map<Ptr<Node>, map<Ptr<Node>, Interface>> nbr2if;
// Mapping destination to next hop for each node: <node, <dest, <nexthop0, ...> > >
map<Ptr<Node>, map<Ptr<Node>, vector<Ptr<Node>>>> nextHop;
map<Ptr<Node>, map<Ptr<Node>, uint64_t>> pairBw;
map<Ptr<Node>, map<Ptr<Node>, uint64_t>> pairBdp;
map<Ptr<Node>, map<Ptr<Node>, uint64_t>> pairRtt;
map<Ptr<Node>, map<Ptr<Node>, uint64_t>> pairDelay;
map<Ptr<Node>, map<Ptr<Node>, uint64_t>> pairTxDelay;

// 在TOR交换机 监控 上行/下行链路 （负载均衡性能）TOR交换机ID到上行/下行链路接口的映射
map<uint32_t, vector<uint32_t>> torId2UplinkIf;
map<uint32_t, vector<uint32_t>> torId2DownlinkIf;

// 输入文件 节点容器 服务器地址
ifstream topof;
ifstream flowf;
NodeContainer n;
vector<Ipv4Address> serverAddress;

// 读取拓扑 节点数量 链路数量   链路对:src, dst
uint32_t node_num, switch_num, link_num;
vector<pair<uint32_t, uint32_t>> link_pairs;

unordered_map<uint32_t, uint16_t> portNumber;
unordered_map<uint32_t, uint16_t> dportNumber;

// 从flow.txt文件中 调度输入流  流量信息
struct FlowInput
{
    uint32_t idx;
    double start_time;
    uint32_t src, dst, pg, maxPacketCount, port;
};

uint32_t flow_num, flow_idx;
FlowInput flow_input = {0};
vector<FlowInput> allFlows;

// 路径信息
struct PathInfo
{
    vector<Ptr<Node>> path;
    uint64_t delay;
    uint64_t txDelay;
    uint64_t bw;
    uint32_t hops;
    uint32_t pathCount;
    PathInfo() : delay(0), txDelay(0), bw(0xfffffffffffffffflu), hops(0), pathCount(0) {}
};

uint32_t maxPathCount = 0, maxPacketSize = 0;
// 成功和失败流数量
uint32_t successFlowCount = 0, failedFlowCount = 0, abortedFlowCount = 0;
// 存储每对节点之间的所有路径及其详细信息
map<Ptr<Node>, map<Ptr<Node>, vector<PathInfo>>> allPaths;
// 带宽矩阵
vector<vector<Time>> nextAvailableTime;
// 当前动作
vector<float> currentAction;
// 当前流
FlowInput curflow;
// 所有reward
float totalRew;
// 总时间
double totalTime;

// 归一化常量
const double SPACE_LOW = 0.0, SPACE_HIGH = 1.0;
const double MAX_CHANNEL_BANDWIDTH = 100000000000.0;

/*----------------  DRL environment  ----------------*/
/**
 * ObservationSpace包含的信息：
 * 1.网络拓扑：主机节点、交换机节点、链路数量  即链路信息
 * 2.链路信息：每条链路的传输时延、链路带宽、负载、链路连接状态信息（源节点、目的节点）nbr2if）
 *   路径信息：源节点、目的节点、path_count、跳数、传播时延、传输时延、带宽通过链路信息直接获取任意两个节点之间的路径信息：
 * 3.流量信息：每条流量的源节点、目的节点、流量大小、流开始时间信息
 *   定义流信息与路径信息空间：取最大路径数进行构建：源节点、目的节点、flow.id、流大小、path_count、hops、delay、txdelay、bw
 */
Ptr<OpenGymSpace> MyGetObservationSpace(void)
{
    // 创建流量 路径信息空间
    string dtype = TypeNameGet<float>();
    vector<uint32_t> shape = {maxPathCount * 6};
    Ptr<OpenGymBoxSpace> space = CreateObject<OpenGymBoxSpace>(SPACE_LOW, SPACE_HIGH, shape, dtype);

    cout << "MyGetObservationSpace Created:\n"
         << space << endl;
    return space;
}

/**
 * 定义动作空间：MyGetActionSpace
 */
Ptr<OpenGymSpace> MyGetActionSpace(void)
{
    string dtype = TypeNameGet<float>();
    vector<uint32_t> shape = {maxPathCount};
    Ptr<OpenGymBoxSpace> space = CreateObject<OpenGymBoxSpace>(SPACE_LOW, SPACE_HIGH, shape, dtype);

    cout << "MyGetActionSpace Created:\n"
         << space << endl;
    return space;
}

/**
 * 定义结束条件：MyGetGameOver
 */
bool MyGetGameOver(void)
{
    if (flow_idx >= flow_num)
    {
        cout << "All flows have been processed." << endl;
        cout << "Average utilization: "
             << totalTime / ((curflow.start_time - allFlows[0].start_time) * maxPathCount) << endl;
        cout << "Failed flows: " << failedFlowCount << endl;
        cout << "Aborted flows: " << abortedFlowCount << endl;
        cout << "Success flows: " << successFlowCount << endl;
        cout << "Total reward: " << totalRew << endl;
        cout << "Total time: " << totalTime << endl;
        return true;
    }
    return false;
}

/**
 * Collect observations：收集网络状态并返回观察空间
 */
Ptr<OpenGymDataContainer> MyGetObservation(void)
{
    curflow = allFlows[flow_idx];
    assert(Seconds(curflow.start_time) == Simulator::Now());

    vector<uint32_t> shape = {maxPathCount * 6};
    float src = curflow.src, dst = curflow.dst, flow_size = curflow.maxPacketCount;
    Ptr<OpenGymBoxContainer<float>> data = CreateObject<OpenGymBoxContainer<float>>(shape);

    Ptr<Node> srcNode = n.Get(src);
    Ptr<Node> dstNode = n.Get(dst);
    auto outerIt = allPaths.find(srcNode);
    if (outerIt == allPaths.end())
    {
        cout << "Node1 not found in allPaths." << endl;
        exit(1);
    }
    auto innerIt = outerIt->second.find(dstNode);
    if (innerIt == outerIt->second.end())
    {
        cout << "No paths found from " << srcNode->GetId() << " to " << dstNode->GetId() << ".\n";
        exit(1);
    }
    vector<PathInfo> paths = innerIt->second;

    for (uint32_t i = 0; i < maxPathCount; ++i)
    {
        data->AddValue(src / Settings::host_num);
        data->AddValue(dst / Settings::host_num);
        data->AddValue(flow_size / maxPacketSize);
        data->AddValue(paths.size() / float(maxPathCount));
        data->AddValue((i < paths.size()) ? (paths[i].bw / MAX_CHANNEL_BANDWIDTH) : 0);
        data->AddValue((i < paths.size()) ? (paths[i].pathCount / float(maxPathCount)) : 0);
    }
    flow_idx++;
    return data;
}

/**
 * 智能体执行动作：MyExecuteActions
 */
bool MyExecuteActions(Ptr<OpenGymDataContainer> actionData)
{
    Ptr<OpenGymBoxContainer<float>> box = DynamicCast<OpenGymBoxContainer<float>>(actionData);
    if (!box)
    {
        cout << "action type error, expected: OpenGymBoxContainer<float>" << endl;
        exit(1);
    }
    Ptr<Node> srcNode = n.Get(curflow.src);
    Ptr<Node> dstNode = n.Get(curflow.dst);
    auto outerIt = allPaths.find(srcNode);
    if (outerIt == allPaths.end())
    {
        cout << "srcNode not found in allPaths." << endl;
        exit(1);
    }
    auto innerIt = outerIt->second.find(dstNode);
    if (innerIt == outerIt->second.end())
    {
        cout << "No paths found from " << srcNode->GetId() << " to " << dstNode->GetId() << ".\n";
        exit(1);
    }

    // 获取动作向量并计算总和
    vector<float> actionVector = box->GetData();
    actionVector.resize(allPaths[srcNode][dstNode].size());
    float sum = std::accumulate(actionVector.begin(), actionVector.end(), 0.0f);
    // 如果总和小于等于 0 则将所有动作设置为 0.0
    // std::numeric_limits<float>::epsilon() 返回浮点数运算中能分辨的最小差值
    if (sum <= std::numeric_limits<float>::epsilon())
    {
        std::fill(actionVector.begin(), actionVector.end(), 0.0);
    }
    // 否则归一化处理
    else
    {
        const float invSum = 1.0f / sum;
        std::transform(actionVector.begin(), actionVector.end(),
                       actionVector.begin(),
                       [invSum](float x)
                       { return x * invSum; });
    }
    currentAction = actionVector;
    return true;
}

/**
 * 定义reward：根据当前分配记录和最新的网络状态计算奖励。
 */
float MyGetReward(void)
{
    if (currentAction.size() == 0)
    {
        cout << "No actions taken yet." << endl;
        return 0.0;
    }
    if (std::accumulate(currentAction.begin(), currentAction.end(), 0.0) == 0.0)
    {
        abortedFlowCount++;
        return 0.0;
    }
    Ptr<Node> srcNode = n.Get(curflow.src);
    Ptr<Node> dstNode = n.Get(curflow.dst);
    auto outerIt = allPaths.find(srcNode);
    if (outerIt == allPaths.end())
    {
        cout << "srcNode not found in allPaths." << endl;
        exit(1);
    }
    auto innerIt = outerIt->second.find(dstNode);
    if (innerIt == outerIt->second.end())
    {
        cout << "No paths found from " << srcNode->GetId() << " to " << dstNode->GetId() << ".\n";
        exit(1);
    }
    vector<PathInfo> paths = innerIt->second;

    // 检查是否足够被分配
    for (size_t i = 0; i < paths.size(); i++)
    {
        const PathInfo &p = paths[i];
        for (size_t j = 1; j < p.path.size(); j++)
        {
            uint32_t node1 = p.path[j - 1]->GetId();
            uint32_t node2 = p.path[j]->GetId();
            if (nextAvailableTime[node1][node2] > Simulator::Now())
            {
                failedFlowCount++;
                return -0.1;
            }
        }
    }

    // 更新分配记录
    for (size_t i = 0; i < paths.size(); i++)
    {
        const PathInfo &p = paths[i];
        double allocatedData = currentAction[i] * curflow.maxPacketCount * 128;
        double transmissionTime = (allocatedData * 8) / p.bw;
        for (size_t j = 1; j < p.path.size(); j++)
        {
            uint32_t node1 = p.path[j - 1]->GetId();
            uint32_t node2 = p.path[j]->GetId();
            nextAvailableTime[node1][node2] = Simulator::Now() + Seconds(transmissionTime);
            nextAvailableTime[node2][node1] = nextAvailableTime[node1][node2];
        }
        totalTime += transmissionTime;
    }
    successFlowCount++;
    float ratio = float(curflow.maxPacketCount) / float(maxPacketSize);
    float base_reward = 1.0 / (1.0 + exp(-10 * ratio)) - 0.5;  // sigmoid函数压缩到 0-0.5
    float size_bonus = 0.5 * log2(1 + ratio);  // 额外的大小奖励 0-0.5
    float reward = base_reward + size_bonus;
    totalRew += reward;
    return reward;
}

void ScheduleNextStateRead(double envStepTime, Ptr<OpenGymInterface> openGymInterface)
{
    openGymInterface->NotifyCurrentState();
    Simulator::Schedule(Seconds(allFlows[flow_idx].start_time) - Simulator::Now(),
                        &ScheduleNextStateRead, envStepTime, openGymInterface);
}

/**
 * 对allFlows进行随机采样
 * @param sampleSize 需要采样的流数量
 * @return 采样后的流量集合
 */
vector<FlowInput> SampleFlows(size_t sampleSize = 10000) 
{
    if (allFlows.size() <= sampleSize) {
        // 重置最大流大小
        maxPacketSize = 0;
        for (const auto& flow : allFlows) {
            maxPacketSize = std::max(maxPacketSize, flow.maxPacketCount);
        }
        return allFlows;
    }

    vector<FlowInput> sampledFlows;
    sampledFlows.reserve(sampleSize);
    
    // 创建索引数组并初始化
    vector<size_t> indices(allFlows.size());
    for (size_t i = 0; i < allFlows.size(); i++) {
        indices[i] = i;
    }
    
    // 使用随机数引擎进行随机采样
    std::random_device rd;
    std::mt19937 gen(rd());
    
    // 重置最大流大小
    maxPacketSize = 0;
    
    // Fisher-Yates洗牌算法，确保不会重复选择
    for (size_t i = 0; i < sampleSize; ++i) {
        std::uniform_int_distribution<size_t> dis(i, indices.size() - 1);
        size_t j = dis(gen);
        // 交换当前位置和随机位置的元素
        if (i != j) {
            std::swap(indices[i], indices[j]);
        }
        sampledFlows.push_back(allFlows[indices[i]]);
        // 更新最大流大小
        maxPacketSize = std::max(maxPacketSize, allFlows[indices[i]].maxPacketCount);
    }

    // 按开始时间排序，保持时序
    sort(sampledFlows.begin(), sampledFlows.end(), 
         [](const FlowInput& a, const FlowInput& b) {
             return a.start_time < b.start_time;
         });

    return sampledFlows;
}

// 整合所有流信息
void LoadFlows(const string &flowFilePath)
{
    ifstream flowFile(flowFilePath.c_str());
    if (!flowFile.is_open())
    {
        cerr << "Error: Cannot open flow file " << flowFilePath << endl;
        return;
    }

    string flowLine;
    uint32_t flowIndex = 0;
    while (getline(flowFile, flowLine))
    {
        istringstream iss(flowLine);
        FlowInput flow;
        // 根据实际的文件格式解析每个字段
        if (!(iss >> flow.src >> flow.dst >> flow.pg >> flow.maxPacketCount >> flow.start_time))
        {
            cerr << "Error: Invalid flow file format in line: " << flowLine << endl;
            continue;
        }
        if (flowIndex < flow_num && flow.maxPacketCount > maxPacketSize)
        {
            maxPacketSize = flow.maxPacketCount;
        }
        flow.idx = flowIndex++;
        allFlows.push_back(flow);
    }
    flowFile.close();
    allFlows = SampleFlows();
    flow_num = allFlows.size();
    cout << "Loaded " << allFlows.size() << " flows." << endl;
}

/*----------------  Simulation function  ----------------*/
/**
 * get_pfc：
 * PFC（Priority Flow Control，优先级流控制）信息 事件记录
 */
void get_pfc(FILE *fout, Ptr<QbbNetDevice> dev, uint32_t type)
{
    //  time, nodeID, nodeType, Interface's Idx, 0:resume, 1:pause
    fprintf(fout, "%lu %u %u %u %u\n", Simulator::Now().GetTimeStep(), dev->GetNode()->GetId(),
            dev->GetNode()->GetNodeType(), dev->GetIfIndex(), type);
}

/**
 * get_nic_rate：
 * 获取节点容器中所有服务器节点的 NIC（Network Interface Card，网络接口卡）速率
 * 计算的是平均速率
 */
uint64_t get_nic_rate(NodeContainer &n)
{
    uint64_t avg_nic_rate = 0;
    uint64_t n_servers = 0;
    for (uint32_t i = 0; i < n.GetN(); i++)
    {
        if (n.Get(i)->GetNodeType() == 0)
        {
            avg_nic_rate += DynamicCast<QbbNetDevice>(n.Get(i)->GetDevice(1))->GetDataRate().GetBitRate();
            n_servers += 1;
        }
    }
    return avg_nic_rate / n_servers;
}

/**
 * qp_finish：RDMA处理机制
 * 当RDMA完成时，需要处理
 * 1.QP(Queue Pair)
 * 2.RxQP(接收端QP)
 * 3.记录完成的流量信息，将记录信息写入fct.txt文件中
 */
void qp_finish(FILE *fout, Ptr<RdmaQueuePair> q)
{
    uint32_t sid = Settings::ip_to_node_id(q->sip);
    uint32_t did = Settings::ip_to_node_id(q->dip);
    uint64_t base_rtt = pairRtt[n.Get(sid)][n.Get(did)]; // 源节点和目的节点之间的RTT
    uint64_t b = pairBw[n.Get(sid)][n.Get(did)];
    // 传输总字节数；流完成时间：往返时延 + 传输时间
    uint32_t total_bytes = q->m_size + ((q->m_size - 1) / packet_payload_size + 1) * (CustomHeader::GetStaticWholeHeaderSize() - IntHeader::GetStaticSize());
    uint64_t standalone_fct = base_rtt + total_bytes * 800000000lu / b;

    // 从接收端删除RxQP(接收队列)   QP 完成后，它的接收队列不再需要存在。
    Ptr<Node> dstNode = n.Get(did);
    Ptr<RdmaDriver> rdma = dstNode->GetObject<RdmaDriver>();
    rdma->m_rdma->DeleteRxQp(q->sip.Get(), q->sport, q->dport, q->m_pg);

    fprintf(fout, "%lu QP complete\n", Simulator::Now().GetTimeStep());
    fprintf(fout, "%u %u %u %u %lu %lu %lu %lu\n", Settings::ip_to_node_id(q->sip),
            Settings::ip_to_node_id(q->dip), q->sport, q->dport, q->m_size,
            q->startTime.GetTimeStep(), (Simulator::Now() - q->startTime).GetTimeStep(),
            standalone_fct);

    // debugging
    NS_LOG_DEBUG(Settings::ip_to_node_id(q->sip) << "  " << Settings::ip_to_node_id(q->dip) << "    "
                                                 << q->sport << "  " << q->dport << "  " << q->m_size << "  " << q->startTime.GetTimeStep() << "  "
                                                 << (Simulator::Now() - q->startTime).GetTimeStep() << "  " << standalone_fct);

    Settings::cnt_finished_flows++;
    fflush(fout);
}

/**
 * WriteAllPaths：将 allPaths信息写入文件
 */
void WriteAllPaths()
{
    path_output = fopen(path_output_file.c_str(), "w");
    if (path_output == NULL)
    {
        cerr << "Error: Cannot open file " << path_output_file << endl;
        exit(1);
    }

    for (const auto &srcPair : allPaths)
    {
        for (const auto &dstPair : srcPair.second)
        {
            for (const auto &pathInfo : dstPair.second)
            {
                fprintf(path_output, "src: %d, dst: %d, pathCount: %d, hops: %d, delay: %lu, txDelay: %lu, bw: %lu, path: ",
                        srcPair.first->GetId(),
                        dstPair.first->GetId(),
                        pathInfo.pathCount,
                        pathInfo.hops,
                        pathInfo.delay,
                        pathInfo.txDelay,
                        pathInfo.bw);
                for (const auto &node : pathInfo.path)
                {
                    fprintf(path_output, "%d ", node->GetId());
                }
                fprintf(path_output, "\n");
            }
        }
    }
    fclose(path_output);
}

/**
 * DFS查找从源节点到目标节点的所有路径
 */
void BuildPathsFromNextHop(Ptr<Node> curr, Ptr<Node> dst, vector<Ptr<Node>> &currentPath,
                           vector<vector<Ptr<Node>>> &allPossiblePaths)
{
    if (curr == dst)
    {
        allPossiblePaths.push_back(currentPath);
        return;
    }
    for (auto next : nextHop[curr][dst])
    {
        currentPath.push_back(next);
        BuildPathsFromNextHop(next, dst, currentPath, allPossiblePaths);
        currentPath.pop_back();
    }
}

/**
 * CalculateAllPaths：遍历所有节点并计算每对节点之间的所有路径
 * 该函数遍历 `NodeContainer` 中的所有节点，调用 `FindAllPaths` 查找每对节点之间的所有路径，
 * 并记录路径时延、传输时延、带宽等信息。
 */
void CalculateAllPaths(NodeContainer &n)
{
    for (uint32_t i = 0; i < Settings::host_num; i++)
    {
        Ptr<Node> src = n.Get(i);
        for (uint32_t j = 0; j < Settings::host_num; j++)
        {
            if (i == j)
                continue;
            Ptr<Node> dst = n.Get(j);
            vector<Ptr<Node>> currentPath;
            vector<vector<Ptr<Node>>> allPossiblePaths;
            currentPath.push_back(src);

            // 使用 nextHop 直接构建路径
            BuildPathsFromNextHop(src, dst, currentPath, allPossiblePaths);

            for (auto path : allPossiblePaths)
            {
                PathInfo pathInfo;
                pathInfo.path = path;
                pathInfo.delay = pairDelay[src][dst];
                pathInfo.txDelay = pairTxDelay[src][dst];
                pathInfo.bw = pairBw[src][dst];
                pathInfo.hops = path.size();
                pathInfo.pathCount = allPaths[src][dst].size() + 1;
                allPaths[src][dst].push_back(pathInfo);
            }
        }
    }
    WriteAllPaths();
}

/**
 * FindMaxPathCount:找到最大的路径
 */
uint32_t FindMaxPathCount()
{
    for (const auto &srcPair : allPaths)
    {
        for (const auto &dstPair : srcPair.second)
        {
            uint32_t pathCount = dstPair.second.size();
            maxPathCount = std::max(maxPathCount, pathCount);
        }
    }
    return maxPathCount;
}

/**
 * CalculateRoute：
 * 计算端到端的延迟，传输延迟(TX delay)和带宽
 * BFS（Breadth First Search，广度优先搜索）算法遍历网络中的所有节点
 */
void CalculateRoute(Ptr<Node> host)
{
    vector<Ptr<Node>> q;     // queue for BFS
    map<Ptr<Node>, int> dis; // distance from host
    map<Ptr<Node>, uint64_t> delay;
    map<Ptr<Node>, uint64_t> txDelay;
    map<Ptr<Node>, uint64_t> bw;

    q.push_back(host);
    dis[host] = 0;
    delay[host] = 0;
    txDelay[host] = 0;
    bw[host] = 0xfffffffffffffffflu;

    for (int i = 0; i < (int)q.size(); i++)
    {
        Ptr<Node> now = q[i];
        int d = dis[now];
        for (auto it = nbr2if[now].begin(); it != nbr2if[now].end(); it++)
        {
            if (!it->second.up)
                continue;
            Ptr<Node> next = it->first;
            if (dis.find(next) == dis.end())
            {
                dis[next] = d + 1;
                delay[next] = delay[now] + it->second.delay;
                // 传输时延 = 数据包大小 * 1000000000 * 8 / 带宽；需要重新计算
                txDelay[next] = txDelay[now] + packet_payload_size * 1000000000lu * 8 / it->second.bw;
                bw[next] = min(bw[now], it->second.bw);

                if (next->GetNodeType() == 1)
                {
                    q.push_back(next);
                }
            }
            // if 'now' is on the shortest path from 'next' to 'host'，更新路径
            if (d + 1 == dis[next])
            {
                nextHop[next][host].push_back(now);
            }
        }
    }
    for (auto it : delay)
    {
        pairDelay[it.first][host] = it.second;
    }
    for (auto it : txDelay)
    {
        pairTxDelay[it.first][host] = it.second;
    }
    for (auto it : bw)
    {
        pairBw[it.first][host] = it.second;
    }
}

/**
 * CalculateRoutes：
 *对每个节点计算路由信息
 */
void CalculateRoutes(NodeContainer &n)
{
    for (uint32_t i = 0; i < Settings::host_num; i++)
    {
        CalculateRoute(n.Get(i));
    }
}

/**
 * SetRoutingEntries：
 * 设置路由条目
 */
void SetRoutingEntries()
{
    for (auto i = nextHop.begin(); i != nextHop.end(); i++)
    { // nextHop 节点到下一跳的映射
        Ptr<Node> node = i->first;
        auto &table = i->second; // table 路由表

        for (auto j = table.begin(); j != table.end(); j++)
        {
            Ptr<Node> dst = j->first; // 目的节点
            Ipv4Address dstAddr = dst->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal();
            vector<Ptr<Node>> nexts = j->second; // 目标节点 下一跳节点
            for (int k = 0; k < (int)nexts.size(); k++)
            {
                Ptr<Node> next = nexts[k];
                uint32_t interface = nbr2if[node][next].idx; // 节点之间的接口索引
                if (node->GetNodeType() == 1)
                {
                    DynamicCast<SwitchNode>(node)->AddTableEntry(dstAddr, interface); // 添加路由表
                }
                else
                {
                    node->GetObject<RdmaDriver>()->m_rdma->AddTableEntry(dstAddr, interface); // 添加路由表
                }
            }
        }
    }
}

/**
 * ReadFlowInput：从文件“flowf”中读取流输入
 */
void ReadFlowInput()
{
    if (flow_input.idx < flow_num)
    {
        flowf >> flow_input.src >> flow_input.dst >> flow_input.pg >> flow_input.maxPacketCount >> flow_input.start_time;
        assert(n.Get(flow_input.src)->GetNodeType() == 0 && n.Get(flow_input.dst)->GetNodeType() == 0);
    }
    else
    {
        cout << "*** input flow is over the prefixed number -- flow number : " << flow_num
             << endl;
        cout << "*** flow_input.idx : " << flow_input.idx << endl;
        cout << "*** THIS IS THE LAST FLOW TO SEND :) " << endl;
    }
}

/**
 * ScheduleFlowInputs:
 * 用于根据 flow_input 中的流量信息 调度流量输入
 */
void ScheduleFlowInputs(FILE *infile)
{
    NS_LOG_DEBUG("ScheduleFlowInputs at" << Simulator::Now());
    while (flow_input.idx < flow_num && Seconds(flow_input.start_time) == Simulator::Now())
    {
        uint32_t pg, src, dst, sport, dport, maxPacketCount, target_len;
        pg = flow_input.pg;
        src = flow_input.src;
        dst = flow_input.dst;
        maxPacketCount = flow_input.maxPacketCount;

        // src 端口:获得 新port number
        sport = portNumber[src];
        portNumber[src] = portNumber[src] + 1;

        // dst 端口
        dport = dportNumber[dst];
        dportNumber[dst] = dportNumber[dst] + 1;

        target_len = flow_input.maxPacketCount;
        if (target_len == 0)
        {
            target_len = 1;
        }
        assert(n.Get(src)->GetNodeType() == 0 && n.Get(dst)->GetNodeType() == 0);

        // 查找往返时延
        if (pairRtt.find(n.Get(src)) == pairRtt.end() ||
            pairRtt[n.Get(src)].find(n.Get(dst)) == pairRtt[n.Get(src)].end())
        {
            cerr << "pairRtt src: " << src << "-> dst: " << dst
                 << "————> cannot be found from database" << endl;
            assert(false);
        }

        // RDMA客户端
        RdmaClientHelper clientHelper(
            pg, serverAddress[src], serverAddress[dst], sport, dport, target_len,
            has_win ? (global_t == 1 ? maxBdp : pairBdp[n.Get(src)][n.Get(dst)]) : 0,
            global_t == 1 ? maxRtt : pairRtt[n.Get(src)][n.Get(dst)]);
        clientHelper.SetAttribute("StatFlowID", IntegerValue(flow_input.idx)); // 设置flow id

        // 应用容器
        ApplicationContainer appCon = clientHelper.Install(n.Get(src)); // 安装src
        appCon.Start(Seconds(Time(0)));
        appCon.Stop(Seconds(100.0));

        // 写入文件
        fprintf(infile, "%u %u %u %u %u %u %u\n", pg, src, dst, sport, dport, maxPacketCount, target_len);
        fflush(infile);

        flow_input.idx++;
        ReadFlowInput();
    }

    // schedule the next time to run this function
    if (flow_input.idx < flow_num)
    {
        Simulator::Schedule(Seconds(flow_input.start_time) - Simulator::Now(), &ScheduleFlowInputs, infile);
    }
    else
    {
        flowf.close();
    }
}

int main(int argc, char *argv[])
{
    uint32_t simSeed = 1;
    uint32_t openGymPort = 5020;
    double simulationTime = 1;
    double envStepTime = 0.000000001;
    string configFile = "config/file_path.txt";

    CommandLine cmd;
    cmd.AddValue("simSeed", "Seed for random generator. Default: 1", simSeed);
    cmd.AddValue("configFile", "Configuration file for network topology", configFile);
    cmd.AddValue("openGymPort", "Port number for OpenGym env. Default: 5555", openGymPort);
    cmd.AddValue("simTime", "Simulation time in seconds. Default: 10s", simulationTime);
    cmd.Parse(argc, argv);

    NS_LOG_UNCOND("Ns3Env parameters:");
    NS_LOG_UNCOND("--seed: " << simSeed);
    NS_LOG_UNCOND("--openGymPort: " << openGymPort);
    NS_LOG_UNCOND("--envStepTime: " << envStepTime);
    NS_LOG_UNCOND("--simulationTime: " << simulationTime);

    // 读取配置文件并初始化网络拓扑
    ifstream conf;
    conf.open(configFile);
    if (!conf.is_open())
    {
        cerr << "Error: Unable to open configuration file: " << configFile << endl;
        return 1;
    }
    cout << "Read configuration file:" << endl;

    /******************* READING CONFIG FILE IS START ***********************/
    /**
     * 读取配置文件，检查文件是否结束，key键值
     * 读取拓扑 | 生成流量 | 生成内容 的文件名称 | 变量设置
     */
    string key;
    while (conf >> key)
    {
        if (key.compare("FLOW_INPUT_FILE") == 0)
        {
            conf >> flow_input_file;
            cerr << "FLOW_INPUT_FILE\t\t\t" << flow_input_file << "\n";
        }
        else if (key.compare("PATH_OUTPUT_FILE") == 0)
        {
            conf >> path_output_file;
            cerr << "PATH_OUTPUT_FILE\t\t" << path_output_file << "\n";
        }
        else if (key.compare("LB_MODE") == 0)
        {
            conf >> lb_mode;
            cerr << "LB_MODE\t\t\t\t" << lb_mode << "\n";
        }
        else if (key.compare("SW_MONITORING_INTERVAL") == 0)
        {
            conf >> switch_mon_interval;
            cerr << "SW_MONITORING_INTERVAL\t\t" << switch_mon_interval << "\n";
        }
        else if (key.compare("PAUSE_TIME") == 0)
        {
            conf >> pause_time;
            cerr << "PAUSE_TIME\t\t\t" << pause_time << "\n";
        }
        else if (key.compare("DATA_RATE") == 0)
        {
            conf >> data_rate;
            cerr << "DATA_RATE\t\t\t" << data_rate << "\n";
        }
        else if (key.compare("LINK_DELAY") == 0)
        {
            conf >> link_delay;
            cerr << "LINK_DELAY\t\t\t" << link_delay << "\n";
        }
        else if (key.compare("PACKET_PAYLOAD_SIZE") == 0)
        {
            conf >> packet_payload_size;
            cerr << "PACKET_PAYLOAD_SIZE\t\t" << packet_payload_size << "\n";
        }
        else if (key.compare("L2_CHUNK_SIZE") == 0)
        {
            conf >> l2_chunk_size;
            cerr << "L2_CHUNK_SIZE\t\t\t" << l2_chunk_size << "\n";
        }
        else if (key.compare("L2_ACK_INTERVAL") == 0)
        {
            conf >> l2_ack_interval;
            cerr << "L2_ACK_INTERVAL\t\t\t" << l2_ack_interval << "\n";
        }
        else if (key.compare("TOPOLOGY_FILE") == 0)
        {
            conf >> topology_file;
            cerr << "TOPOLOGY_FILE\t\t\t" << topology_file << "\n";
        }
        else if (key.compare("FLOW_FILE") == 0)
        {
            conf >> flow_file;
            cerr << "FLOW_FILE\t\t\t" << flow_file << "\n";
        }
        else if (key.compare("RANDOM_SEED") == 0)
        {
            conf >> random_seed;
            cerr << "RANDOM_SEED\t\t\t" << random_seed << "\n";
        }
        else if (key.compare("FLOWGEN_STOP_TIME") == 0)
        {
            conf >> flowgen_stop_time;
            cerr << "FLOWGEN_STOP_TIME\t\t" << flowgen_stop_time << "\n";
        }
        else if (key.compare("ALPHA_RESUME_INTERVAL") == 0)
        {
            conf >> alpha_resume_interval;
            cerr << "ALPHA_RESUME_INTERVAL\t\t" << alpha_resume_interval << "\n";
        }
        else if (key.compare("RP_TIMER") == 0)
        {
            conf >> rp_timer;
            cerr << "RP_TIMER\t\t\t" << rp_timer << "\n";
        }
        else if (key.compare("EWMA_GAIN") == 0)
        {
            conf >> ewma_gain;
            cerr << "EWMA_GAIN\t\t\t" << ewma_gain << "\n";
        }
        else if (key.compare("FAST_RECOVERY_TIMES") == 0)
        {
            conf >> fast_recovery_times;
            cerr << "FAST_RECOVERY_TIMES\t\t" << fast_recovery_times << "\n";
        }
        else if (key.compare("RATE_AI") == 0)
        {
            conf >> rate_ai;
            cerr << "RATE_AI\t\t\t\t" << rate_ai << "\n";
        }
        else if (key.compare("RATE_HAI") == 0)
        {
            conf >> rate_hai;
            cerr << "RATE_HAI\t\t\t" << rate_hai << "\n";
        }
        else if (key.compare("ERROR_RATE_PER_LINK") == 0)
        {
            conf >> error_rate_per_link;
            cerr << "ERROR_RATE_PER_LINK\t\t" << error_rate_per_link << "\n";
        }
        else if (key.compare("CC_MODE") == 0)
        {
            conf >> cc_model;
            cerr << "CC_MODE\t\t\t\t" << cc_model << '\n';
        }
        else if (key.compare("RATE_DECREASE_INTERVAL") == 0)
        {
            conf >> rate_decrease_interval;
            cerr << "RATE_DECREASE_INTERVAL\t\t" << rate_decrease_interval << "\n";
        }
        else if (key.compare("MIN_RATE") == 0)
        {
            conf >> min_rate;
            cerr << "MIN_RATE\t\t\t" << min_rate << "\n";
        }
        else if (key.compare("FCT_OUTPUT_FILE") == 0)
        {
            conf >> fct_output_file;
            cerr << "FCT_OUTPUT_FILE\t\t\t" << fct_output_file << '\n';
        }
        else if (key.compare("HAS_WIN") == 0)
        {
            conf >> has_win;
            cerr << "HAS_WIN\t\t\t\t" << has_win << "\n";
        }
        else if (key.compare("GLOBAL_T") == 0)
        {
            conf >> global_t;
            cerr << "GLOBAL_T\t\t\t" << global_t << '\n';
        }
        else if (key.compare("MI_THRESH") == 0)
        {
            conf >> mi_thresh;
            cerr << "MI_THRESH\t\t\t" << mi_thresh << '\n';
        }
        else if (key.compare("VAR_WIN") == 0)
        {
            conf >> var_win;
            cerr << "VAR_WIN\t\t\t\t" << var_win << '\n';
        }
        else if (key.compare("FAST_REACT") == 0)
        {
            conf >> fast_react;
            cerr << "FAST_REACT\t\t\t" << fast_react << '\n';
        }
        else if (key.compare("U_TARGET") == 0)
        {
            conf >> u_target;
            cerr << "U_TARGET\t\t\t" << u_target << '\n';
        }
        else if (key.compare("INT_MULTI") == 0)
        {
            conf >> int_multi;
            cerr << "INT_MULTI\t\t\t" << int_multi << '\n';
        }
        else if (key.compare("RATE_BOUND") == 0)
        {
            conf >> rate_bound;
            cerr << "RATE_BOUND\t\t\t" << rate_bound << '\n';
        }
        else if (key.compare("DCTCP_RATE_AI") == 0)
        {
            conf >> dctcp_rate_ai;
            cerr << "DCTCP_RATE_AI\t\t\t" << dctcp_rate_ai << "\n";
        }
        else if (key.compare("PFC_OUTPUT_FILE") == 0)
        {
            conf >> pfc_output_file;
            cerr << "PFC_OUTPUT_FILE\t\t\t" << pfc_output_file << '\n';
        }
        else if (key.compare("BUFFER_SIZE") == 0)
        {
            conf >> buffer_size;
            cerr << "BUFFER_SIZE\t\t\t" << buffer_size << '\n';
        }
        else if (key.compare("QLEN_MON_START") == 0)
        {
            conf >> qlen_mon_start;
            cerr << "QLEN_MON_START\t\t\t" << qlen_mon_start << '\n';
        }
        else if (key.compare("QLEN_MON_END") == 0)
        {
            conf >> qlen_mon_end;
            cerr << "QLEN_MON_END\t\t\t" << qlen_mon_end << '\n';
        }
        else if (key.compare("MULTI_RATE") == 0)
        {
            conf >> multi_rate;
            cerr << "MULTI_RATE\t\t\t" << multi_rate << '\n';
        }
        else if (key.compare("SAMPLE_FEEDBACK") == 0)
        {
            conf >> sample_feedback;
            cerr << "SAMPLE_FEEDBACK\t\t\t" << sample_feedback << '\n';
        }
        else if (key.compare("LOAD") == 0)
        {
            conf >> load;
            cerr << "LOAD\t\t\t\t" << load << "\n";
        }
        else if (key.compare("LINK_DOWN") == 0)
        {
            conf >> link_down_time >> link_down_A >> link_down_B;
            cerr << "LINK_DOWN\t\t\t" << link_down_time << ' ' << link_down_A << ' '
                 << link_down_B << '\n';
        }
        else if (key.compare("ENABLE_IRN") == 0)
        {
            conf >> enable_irn;
            cerr << "ENABLE_IRN\t\t\t" << (enable_irn ? "Yes" : "No") << "\n";
        }
        else if (key.compare("ENABLE_PFC") == 0)
        {
            conf >> enable_pfc;
            cerr << "ENABLE_PFC\t\t\t" << (enable_pfc ? "Yes" : "No") << "\n";
        }
        else if (key.compare("ENABLE_QCN") == 0)
        {
            conf >> enable_qcn;
            cerr << "ENABLE_QCN\t\t\t" << (enable_qcn ? "Yes" : "No") << "\n";
        }
        else if (key.compare("L2_BACK_TO_ZERO") == 0)
        {
            conf >> l2_back_to_zero;
            cerr << "L2_BACK_TO_ZERO\t\t\t" << (l2_back_to_zero ? "Yes" : "No") << "\n";
        }
        else if (key.compare("CLAMP_TARGET_RATE") == 0)
        {
            conf >> clamp_target_rate;
            cerr << "CLAMP_TARGET_RATE\t\t" << (clamp_target_rate ? "Yes" : "No") << "\n";
        }
        else if (key.compare("USE_DYNAMIC_PFC_THRESHOLD") == 0)
        {
            conf >> use_dynamic_pfc_threshold;
            cerr << "USE_DYNAMIC_PFC_THRESHOLD\t" << (use_dynamic_pfc_threshold ? "Yes" : "No") << "\n";
        }
        else if (key.compare("FLOWGEN_START_TIME") == 0)
        {
            double v;
            conf >> v;
            qlen_mon_end = v;
            qlen_mon_start = v;
            cnp_mon_start = v;
            irn_mon_start = v;
            flowgen_start_time = v;
            cerr << "FLOWGEN_START_TIME\t\t" << flowgen_start_time << "\n";
        }
        else if (key.compare("KMAX_MAP") == 0)
        {
            int n_k;
            conf >> n_k;
            cerr << "KMAX_MAP\t\t\t";
            for (int i = 0; i < n_k; i++)
            {
                uint64_t rate;
                uint32_t k;
                conf >> rate >> k;
                rate2kmax[rate] = k;
                cerr << ' ' << rate << ' ' << k;
            }
            cerr << '\n';
        }
        else if (key.compare("KMIN_MAP") == 0)
        {
            int n_k;
            conf >> n_k;
            cerr << "KMIN_MAP\t\t\t";
            for (int i = 0; i < n_k; i++)
            {
                uint64_t rate;
                uint32_t k;
                conf >> rate >> k;
                rate2kmin[rate] = k;
                cerr << ' ' << rate << ' ' << k;
            }
            cerr << '\n';
        }
        else if (key.compare("PMAX_MAP") == 0)
        {
            int n_k;
            conf >> n_k;
            cerr << "PMAX_MAP\t\t\t";
            for (int i = 0; i < n_k; i++)
            {
                uint64_t rate;
                double p;
                conf >> rate >> p;
                rate2pmax[rate] = p;
                cerr << ' ' << rate << ' ' << p;
            }
            cerr << '\n';
        }
        fflush(stdout);
    }
    conf.close();
    /******************* READING CONFIG FILE IS END ***********************/

    // 启用NS-3日志
    LogComponentEnable("DMRL_LOAD_BALANCE_SIMULATION", LOG_LEVEL_DEBUG);

    // 设置随机种子,仿真过程产生相同的随机数
    NS_LOG_INFO("Initialize random seed: " << random_seed);
    srand((unsigned)random_seed);
    RngSeedManager::SetSeed(1);
    RngSeedManager::SetRun(simSeed);
    // SeedManager::SetSeed(random_seed);

    /**
     * PFC/QCN设置
     * 配置 NS-3 中 QbbNetDevice（一个用于模拟高带宽低延迟网络设备的类）相关的参数
     * 特别是与 PFC（优先级流量控制）和 QCN（量化拥塞通知）有关的设置
     */
    Config::SetDefault("ns3::QbbNetDevice::PauseTime", UintegerValue(pause_time));
    Config::SetDefault("ns3::QbbNetDevice::QcnEnabled", BooleanValue(enable_qcn));
    Config::SetDefault("ns3::QbbNetDevice::QbbEnabled", BooleanValue(enable_pfc));
    Config::SetDefault("ns3::QbbNetDevice::DynamicThreshold", BooleanValue(use_dynamic_pfc_threshold));

    if (cc_model != 1 && lb_mode == 9)
    {
        cout << "Currently, ConWeave supports only DCQCN congestion control for RDMA. \nIf "
                "you want to extend, the reordering delay at DstTor must be considered."
             << endl;
        exit(1);
    }

    /**
     * INT头部设置
     * In-Network Telemetry 头部处理模型
     */
    IntHop::multi = int_multi;
    if (cc_model == 7)
    {
        IntHeader::mode = 1; // timely use ts
    }
    else if (cc_model == 3)
    {
        IntHeader::mode = 0; // hpcc use int
    }
    else
    {
        IntHeader::mode = 5; // others no extra header
    }

    /**
     * 读取网络拓扑文件，流量文件
     */
    flowf.open(flow_file.c_str());
    topof.open(topology_file.c_str());
    flowf >> flow_num;
    topof >> node_num >> switch_num >> link_num;
    cout << "\nRead topology and flows:\n"
         << "Node num: " << node_num << ", Switch num: " << switch_num << ", Link num: " << link_num << ", Flow num: " << flow_num << endl;

    /**  参数设置  **/
    Settings::lb_mode = lb_mode;
    Settings::node_num = node_num;
    Settings::switch_num = switch_num;
    Settings::host_num = node_num - switch_num;
    Settings::packet_payload = packet_payload_size;

    /******************* CREATE NODES ***********************/
    // 创建节点   根据网络拓扑创建节点   交换机节点：node_type：1，主机节点：node_type:0
    vector<uint32_t> node_type(node_num, 0);
    for (uint32_t i = 0; i < switch_num; i++)
    {
        uint32_t sid;
        topof >> sid;
        node_type[sid] = 1;
    }
    for (uint32_t i = 0; i < node_num; i++)
    {
        if (node_type[i] == 0)
        {
            n.Add(CreateObject<Node>());
        }
        else
        {
            Ptr<SwitchNode> sw = CreateObject<SwitchNode>();
            n.Add(sw);
            sw->SetAttribute("EcnEnabled", BooleanValue(enable_qcn));
        }
    }
    NS_LOG_INFO("Created nodes.");

    /******************* INTERNET & IP ADRESS ***********************/
    // 协议安装   分配IP地址   aggregate ipv4, ipv6, udp, tcp, etc
    InternetStackHelper internet;
    internet.Install(n);

    // 为节点分配IP地址
    serverAddress.resize(Settings::host_num);
    for (uint32_t i = 0; i < Settings::host_num; i++)
    {
        serverAddress[i] = Settings::node_id_to_ip(i);
    }
    NS_LOG_INFO("Configured nodes.");

    QbbHelper qbb;
    Ipv4AddressHelper ipv4;
    pfc_file = fopen(pfc_output_file.c_str(), "w");

    // 显式创建拓扑所需的链路错误模型
    Ptr<RateErrorModel> rem = CreateObject<RateErrorModel>();
    Ptr<UniformRandomVariable> uv = CreateObject<UniformRandomVariable>();
    uv->SetStream(50);
    rem->SetRandomVariable(uv);
    rem->SetAttribute("ErrorRate", DoubleValue(error_rate_per_link));
    rem->SetAttribute("ErrorUnit", StringValue("ERROR_UNIT_PACKET"));

    for (uint32_t i = 0; i < link_num; i++)
    {
        double error_rate;
        uint32_t src, dst;
        string data_rate, link_delay;
        topof >> src >> dst >> data_rate >> link_delay >> error_rate;
        // 断言链路的延迟为假定的单跳延迟
        assert(to_string(one_hop_delay) + "ns" == link_delay);
        // 简化链路错误模型
        assert(error_rate == 0);
        // 将链路的源节点和目标节点加入链路对列表
        Ptr<Node> snode = n.Get(src);
        Ptr<Node> dnode = n.Get(dst);
        link_pairs.push_back(make_pair(src, dst));
        // 设置链路属性
        qbb.SetChannelAttribute("Delay", StringValue(link_delay));
        qbb.SetDeviceAttribute("DataRate", StringValue(data_rate));
        qbb.SetDeviceAttribute("ReceiveErrorModel", PointerValue(rem));

        // 服务器IP地址分配
        NetDeviceContainer d = qbb.Install(snode, dnode);
        if (snode->GetNodeType() == 0)
        {
            Ptr<Ipv4> ipv4 = snode->GetObject<Ipv4>();
            ipv4->AddInterface(d.Get(0));
            ipv4->AddAddress(1, Ipv4InterfaceAddress(serverAddress[src], Ipv4Mask(0xff000000)));
        }
        if (dnode->GetNodeType() == 0)
        {
            Ptr<Ipv4> ipv4 = dnode->GetObject<Ipv4>();
            ipv4->AddInterface(d.Get(1));
            ipv4->AddAddress(1, Ipv4InterfaceAddress(serverAddress[dst], Ipv4Mask(0xff000000)));
        }

        // 通过拓扑创建图 获取节点之间的信息
        // src -> dst
        nbr2if[snode][dnode].up = true;
        nbr2if[snode][dnode].idx = DynamicCast<QbbNetDevice>(d.Get(0))->GetIfIndex();
        nbr2if[snode][dnode].bw = DynamicCast<QbbNetDevice>(d.Get(0))->GetDataRate().GetBitRate();
        nbr2if[snode][dnode].delay =
            DynamicCast<QbbChannel>(DynamicCast<QbbNetDevice>(d.Get(0))->GetChannel())
                ->GetDelay()
                .GetTimeStep();

        // dst -> src
        nbr2if[dnode][snode].up = true;
        nbr2if[dnode][snode].idx = DynamicCast<QbbNetDevice>(d.Get(1))->GetIfIndex();
        nbr2if[dnode][snode].bw = DynamicCast<QbbNetDevice>(d.Get(1))->GetDataRate().GetBitRate();
        nbr2if[dnode][snode].delay =
            DynamicCast<QbbChannel>(DynamicCast<QbbNetDevice>(d.Get(1))->GetChannel())
                ->GetDelay()
                .GetTimeStep();

        // 分配IP地址
        Ipv4Address x;
        char ipstring[16];
        sprintf(ipstring, "10.%d.%d.0", (i / 254) % 254 + 1, i % 254 + 1);
        ipv4.SetBase(ipstring, "255.255.255.0");
        ipv4.Assign(d);

        // 设置PFC跟踪
        DynamicCast<QbbNetDevice>(d.Get(0))->TraceConnectWithoutContext(
            "QbbPfc", MakeBoundCallback(&get_pfc, pfc_file, DynamicCast<QbbNetDevice>(d.Get(0))));
        DynamicCast<QbbNetDevice>(d.Get(1))->TraceConnectWithoutContext(
            "QbbPfc", MakeBoundCallback(&get_pfc, pfc_file, DynamicCast<QbbNetDevice>(d.Get(1))));
    }
    cout << "(AVG) NIC RATE: " << get_nic_rate(n) << endl;

    Ipv4Address empty_ip;
    for (uint32_t i = 0; i < node_num; ++i)
    {
        // 获取 host IP 地址 与 NodeId 的 对应关系
        if (n.Get(i)->GetNodeType() == 0)
        {
            if (serverAddress[i].IsEqual(empty_ip))
            {
                printf("IP Address ERROR %d\n", i);
                printf("size of serverAddress: %lu", serverAddress.size());
                NS_FATAL_ERROR("An end-host belongs to no link");
            }
        }
        Settings::hostId2IpMap[i] = serverAddress[i].Get(); // NodeID -> IP
        Settings::hostIp2IdMap[serverAddress[i].Get()] = i; // IP -> NodeID

        // 配置交换机
        if (n.Get(i)->GetNodeType() == 1)
        {
            Ptr<SwitchNode> sw = DynamicCast<SwitchNode>(n.Get(i));
            for (uint32_t j = 1; j < sw->GetNDevices(); j++)
            {
                Ptr<QbbNetDevice> dev = DynamicCast<QbbNetDevice>(sw->GetDevice(j));
                uint64_t rate = dev->GetDataRate().GetBitRate();
                NS_ASSERT_MSG(rate2kmin.find(rate) != rate2kmin.end(),
                              "must set kmin for each link speed");
                NS_ASSERT_MSG(rate2kmax.find(rate) != rate2kmax.end(),
                              "must set kmax for each link speed");
                NS_ASSERT_MSG(rate2pmax.find(rate) != rate2pmax.end(),
                              "must set pmax for each link speed");
                assert(rate2kmin.find(rate) != rate2kmin.end() &&
                       rate2kmax.find(rate) != rate2kmax.end() &&
                       rate2pmax.find(rate) != rate2pmax.end());
                sw->m_mmu->ConfigEcn(j, rate2kmin[rate], rate2kmax[rate], rate2pmax[rate]); // 设置ECN参数
                // 设置PFC   headroom 处理数据包需要的额外空间
                uint64_t delay = DynamicCast<QbbChannel>(dev->GetChannel())->GetDelay().GetTimeStep();
                uint32_t headroom = rate * delay / 8 / 1000000000 * 2 + 2 * sw->m_mmu->MTU;
                sw->m_mmu->ConfigHdrm(j, headroom);
            }
            // 设置交换机 端口 交换机缓存大小 节点id
            sw->m_mmu->ConfigNPort(sw->GetNDevices() - 1);
            sw->m_mmu->ConfigBufferSize(buffer_size * 1024 * 1024); // default 0, specify in run.py!!
            sw->m_mmu->node_id = sw->GetId();
        }
    }

    // FCT 输出文件
    fct_output = fopen(fct_output_file.c_str(), "w");
    flow_input_stream = fopen(flow_input_file.c_str(), "w");

    // 拓扑到BDP映射
    map<string, uint32_t> topo2bdpMap;
    topo2bdpMap[string("fat_k8_100G_OS2")] = 156000;         // RTT=12480 --> all 100G links
    topo2bdpMap[string("leaf_spine_128_100G_OS2")] = 104000; // RTT=8320

    // 拓扑文件  是否找到匹配的拓扑  irn_bdp 存储
    bool found_topo2bdpMap = false;
    uint32_t irn_bdp_lookup = 0;
    for (auto pair : topo2bdpMap)
    {
        // if topology file string includes the word
        if (topology_file.find(pair.first) != string::npos)
        {
            irn_bdp_lookup = pair.second;
            found_topo2bdpMap = true;
            break;
        }
    }
    if (found_topo2bdpMap == false)
    {
        cout << __FILE__ << "(" << __LINE__ << ")"
             << " ERROR - topo2bdpMap has no matched item with " << topology_file << endl;
        assert(false);
    }

    for (uint32_t i = 0; i < node_num; i++)
    {
        // 为主机节点 创建 RdmaHw 设置属性
        if (n.Get(i)->GetNodeType() == 0)
        {
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
            rdmaHw->SetAttribute("CcMode", UintegerValue(cc_model));
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
            rdmaHw->SetAttribute("IrnEnable", BooleanValue(enable_irn));

            // 设置拓扑相关参数 topo2bdpMap (e.g., longest BDP 25000: 8us * 25Gbps)
            rdmaHw->SetAttribute("IrnRtoHigh", TimeValue(MicroSeconds(320))); // 1930
            rdmaHw->SetAttribute("IrnRtoLow", TimeValue(MicroSeconds(100)));  // 454
            rdmaHw->SetAttribute("IrnBdp", UintegerValue(irn_bdp_lookup));

            // 创建 并 安装 RDNMA驱动 RdmaDriver
            Ptr<RdmaDriver> rdma = CreateObject<RdmaDriver>();
            Ptr<Node> node = n.Get(i);

            rdma->SetNode(node);
            rdma->SetRdmaHw(rdmaHw);
            node->AggregateObject(rdma);

            rdma->Init();
            rdma->TraceConnectWithoutContext("QpComplete", MakeBoundCallback(qp_finish, fct_output));
        }

        /**
         * switch（交换机）的节点设置其 CcMode 和 ACK 优先级属性
         * 检查交换机指针是否为空
         */
        if (n.Get(i)->GetNodeType() == 1)
        {
            Ptr<SwitchNode> sw = DynamicCast<SwitchNode>(n.Get(i));
            sw->SetAttribute("CcMode", UintegerValue(cc_model));
            sw->SetAttribute("AckHighPrio", UintegerValue(1));
        }
    }

    // 计算路由并设置路由表
    CalculateRoutes(n);
    CalculateAllPaths(n);
    FindMaxPathCount();
    SetRoutingEntries();

    /**
     * 主机的配置
     * 获取 BDP 和 时延
     */
    maxRtt = maxBdp = 0;
    for (uint32_t i = 0; i < Settings::host_num; i++)
    {
        for (uint32_t j = i + 1; j < Settings::host_num; j++)
        {
            uint64_t txDelay = pairTxDelay[n.Get(i)][n.Get(j)];
            uint64_t delay = pairDelay[n.Get(i)][n.Get(j)];
            uint64_t bw = pairBw[n.Get(i)][n.Get(j)];
            uint64_t rtt = delay * 2 + txDelay;
            uint64_t bdp = rtt * bw / 1000000000 / 8;
            pairBdp[n.Get(i)][n.Get(j)] = bdp;
            pairBdp[n.Get(j)][n.Get(i)] = bdp;
            pairRtt[n.Get(i)][n.Get(j)] = rtt;
            pairRtt[n.Get(j)][n.Get(i)] = rtt;

            if (bdp > maxBdp)
                maxBdp = bdp;
            if (rtt > maxRtt)
                maxRtt = rtt;
        }
    }
    fprintf(stderr, "maxRtt: %lu, maxBdp: %lu\n", maxRtt, maxBdp);
    // 校验最大 BDP 是否等于预期值
    assert(maxBdp == irn_bdp_lookup);

    /* 配置TOR交换机 */
    printf("link_pairs.size() = %lu\n", link_pairs.size());
    for (auto &pair : link_pairs)
    {
        Ptr<Node> probably_host = n.Get(pair.first);
        Ptr<Node> probably_switch = n.Get(pair.second);
        if (probably_host->GetNodeType() == 0 && probably_switch->GetNodeType() == 1)
        {
            Ptr<SwitchNode> sw = DynamicCast<SwitchNode>(probably_switch);
            sw->m_isToR = true;
            uint32_t hostIP = serverAddress[pair.first].Get();
            sw->m_isToR_hostIP.insert(hostIP);
            if (idxNodeToR.find(sw->GetId()) == idxNodeToR.end())
            {
                idxNodeToR[sw->GetId()] = sw;
            };
        }
    }

    // 填充路由表 (although we use our custom impl in switch_node.cc)
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();
    // maintain port number for each host
    for (uint32_t i = 0; i < Settings::host_num; i++)
    {
        // each host use port number from 10000
        portNumber[i] = 10000;
        dportNumber[i] = 100;
    }

    flow_input.idx = 0;
    // if (flow_num > 0)
    // {
    //     cout << "\n--------------------- Schedule FlowInputs ---------------------" << endl;
    //     std::cout << "Current simulation time: " << Simulator::Now().GetSeconds() << " seconds" << std::endl;
    //     ReadFlowInput();
    //     Simulator::Schedule(Seconds(flow_input.start_time), &ScheduleFlowInputs, flow_input_stream);
    // }
    topof.close();

    // 更新 ToR 的上行和下行链路 端口映射
    for (size_t ToRId = 0; ToRId < Settings::node_num; ToRId++)
    {
        Ptr<Node> node = n.Get(ToRId);
        if (node->GetNodeType() == 1)
        {
            auto swNode = DynamicCast<SwitchNode>(n.Get(ToRId));
            if (swNode->m_isToR)
            {
                for (auto &nextNodeIf : nbr2if[node])
                {
                    if (nextNodeIf.first->GetNodeType() == 1)
                    {
                        auto &vec = torId2UplinkIf[ToRId];
                        vec.push_back(nextNodeIf.second.idx);
                    }
                    else
                    {
                        auto &vec = torId2DownlinkIf[ToRId];
                        vec.push_back(nextNodeIf.second.idx);
                    }
                }
            }
        }
    }

    LoadFlows(flow_file);
    // Opengym Env 环境接口
    Ptr<OpenGymInterface> openGymInterface = CreateObject<OpenGymInterface>(openGymPort);
    openGymInterface->SetGetRewardCb(MakeCallback(&MyGetReward));
    openGymInterface->SetGetGameOverCb(MakeCallback(&MyGetGameOver));
    openGymInterface->SetGetObservationCb(MakeCallback(&MyGetObservation));
    openGymInterface->SetGetActionSpaceCb(MakeCallback(&MyGetActionSpace));
    openGymInterface->SetExecuteActionsCb(MakeCallback(&MyExecuteActions));
    openGymInterface->SetGetObservationSpaceCb(MakeCallback(&MyGetObservationSpace));
    nextAvailableTime.resize(node_num, vector<Time>(node_num, Seconds(allFlows[flow_idx].start_time)));
    Simulator::Schedule(Seconds(allFlows[flow_idx].start_time), &ScheduleNextStateRead, envStepTime, openGymInterface);
    // 开始仿真
    cout << "\n--------------------- Simulator::Schedule ---------------------" << endl;
    cout << "Running Simulation.\n";
    fflush(stdout);
    Simulator::Stop(Seconds(flowgen_stop_time + 10.0));
    Simulator::Run();

    cout << "Run over, Destroy at: " << Simulator::Now().GetNanoSeconds() << "ns" << endl;
    openGymInterface->NotifySimulationEnd();
    Simulator::Destroy();
    cout << "Destroy over at: " << Simulator::Now().GetNanoSeconds() << "ns" << endl;
}
