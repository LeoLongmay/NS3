#include "ns3/assert.h"
#include "ns3/rdma-client-helper.h"
#include "ns3/rdma-client.h"
#include "ns3/rdma-driver.h"
#include "ns3/rdma.h"
#include "ns3/sim-setting.h"
#include "ns3/switch-node.h"
#include <time.h>

#include "fstream"
#include "iostream"
#include "unordered_map"
#include "cstdio"
#include <limits>

#include "ns3/applications-module.h"
#include "ns3/broadcom-node.h"
#include "ns3/conga-routing.h"
#include "ns3/conweave-voq.h"
#include "ns3/core-module.h"
#include "ns3/error-model.h"
#include "ns3/global-route-manager.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/letflow-routing.h"
#include "ns3/packet.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/qbb-helper.h"
#include "ns3/qbb-net-device.h"
#include "ns3/rdma-hw.h"
#include "ns3/settings.h"


#include "ns3/opengym-module.h"
#include "ns3/node-list.h"

using namespace ns3;
using namespace std;
NS_LOG_COMPONENT_DEFINE("GENERATED_SIMULATION");

/*----------------load balancing parameters----------------*/
/**
 * 定义负载均衡模型参数：load balance mode:
 * 0：flow ECMP ；2：DRILL ；3：Conga；6：Letflow ；9：Conweave
 */
uint32_t lb_mode = 0;

// Conga parameters (based on paper recommendation)
Time conga_flowletTimeout = MicroSeconds(100);   
Time conga_dreTime = MicroSeconds(50);
Time conga_agingTime = MicroSeconds(500);
uint32_t conga_quantizeBit = 3;
double conga_alpha = 0.2;

// Letflow parameters
Time letflow_flowletTimeout = MicroSeconds(100);
Time letflow_agingTime = MilliSeconds(2);    

// Conweave parameters
Time conweave_extraReplyDeadline = MicroSeconds(4);       
Time conweave_pathPauseTime = MicroSeconds(8);            
Time conweave_txExpiryTime = MicroSeconds(100);           
Time conweave_extraVOQFlushTime = MicroSeconds(32);       
Time conweave_defaultVOQWaitingTime = MicroSeconds(500);  
bool conweave_pathAwareRerouting = true;                  


/*----------------simulation variables----------------*/
uint64_t one_hop_delay = 1000;  
//拥塞控制模型: 1：DCQCN
uint32_t cc_model = 1;           
bool enable_qcn = true, enable_pfc = true, use_dynamic_pfc_threshold = true;
uint32_t packet_payload_size = 1000, l2_chunk_size = 0, l2_ack_interval = 0;
double pause_time = 5;    //PFC 暂停帧时间，5us 流量控制过程中暂停的数据传输时间
double flowgen_start_time = 2.0, flowgen_stop_time = 2.5, simulator_extra_time = 0.1; 
// uint32_t qlen_dump_interval = 100000000, qlen_mon_interval = 1000; //ns
uint64_t qlen_mon_start;       
uint64_t qlen_mon_end;
uint32_t switch_mon_interval = 10000;   
uint64_t cnp_mon_start;                 
uint64_t cnp_monitor_bucket = 100000;   
uint64_t irn_mon_start;                
uint64_t irn_monitor_bucket = 100000;

// 定义文件指针，用于输出不同类型的仿真数据 voq:虚拟输出队列；voq_detail:详细的虚拟输出队列；
FILE *pfc_file = NULL;
FILE *fct_output = NULL;
FILE *flow_input_stream = NULL;    
FILE *cnp_output = NULL;           
FILE *est_error_output = NULL;     
FILE *voq_output = NULL;           
FILE *voq_detail_output = NULL;
FILE *uplink_output = NULL;
FILE *conn_output = NULL;           
FILE *path_output = NULL;

string data_rate, link_delay, topology_file, flow_file;
string flow_input_file = "flow.txt";
string fct_output_file = "fct.txt";
string pfc_output_file = "pfc.txt";
string cnp_output_file = "cnp.txt";
string qlen_mon_file = "qlen.txt";
string voq_mon_file = "voq.txt";
string voq_mon_detail_file = "voq_detail.txt";
string uplink_mon_file = "uplink.txt";
string conn_mon_file = "conn.txt";
string est_error_output_file = "est_error.txt";
string path_output_file = "path.txt";
    
double alpha_resume_interval = 55, rp_timer = 300, ewma_gain = 1/16;
double rate_decrease_interval = 4;     
uint32_t fast_recovery_times = 1;      
string rate_ai, rate_hai, min_rate = "100Mb/s";    
string dctcp_rate_ai = "1000Mb/s";

bool clamp_target_rate = false, l2_back_to_zero = false;
double error_rate_per_link = 0.0;
uint32_t has_win = 1;       
uint32_t global_t = 1;      
uint32_t mi_thresh = 5;     
bool var_win = false, fast_react = true;    
bool multi_rate = true;    
bool sample_feedback = false;   
double u_target = 0.95;         
uint32_t int_multi = 1;         
bool rate_bound = true;         
unordered_map<uint64_t, uint32_t> rate2kmax, rate2kmin;   
unordered_map<uint64_t, double> rate2pmax;               
// 映射:将节点索引（Id）映射到交换机节点指针（Ptr<SwitchNode>）
unordered_map<uint32_t, Ptr<SwitchNode>> idxNodeToR;     

// 配置链路故障场景、ACK优先级和缓冲区配置
uint64_t link_down_time = 0;                  
uint32_t link_down_A = 0, link_down_B = 0;    //链路故障状态，0不可用，1可用
uint32_t buffer_size = 0;  // 0 to set buffer size automatically 缓冲区大小

// Added from Here
double load = 10.0;
int enable_irn = 0;
int random_seed = 1;    

uint64_t maxRtt, maxBdp;

// 网络接口结构体：idx-索引；up-是否启用；delay-延迟；bw-带宽
struct Interface {
    uint32_t idx;
    /* data */
    bool up;
    uint64_t delay;
    uint64_t bw;

    Interface(): idx(0), up(false) {}
};

// nbr2if：每个节点与其邻居节点之间的接口映射，链路信息
map<Ptr<Node>, map<Ptr<Node>, Interface>> nbr2if;
// Mapping destination to next hop for each node: <node, <dest, <nexthop0, ...> > >
map<Ptr<Node>,map<Ptr<Node>,vector<Ptr<Node>>>> nextHop;    
map<Ptr<Node>,map<Ptr<Node>,uint64_t>> pairDelay;           
map<Ptr<Node>, map<Ptr<Node>, uint64_t>> pairTxDelay;       
map<Ptr<Node>,map<Ptr<Node>,uint64_t>> pairBw;
map<Ptr<Node>,map<Ptr<Node>,uint64_t>> pairBdp;
map<Ptr<Node>,map<Ptr<Node>,uint64_t>> pairRtt;
// map<Ptr<Node>, Ptr<Node>> prev;

// 在TOR交换机 监控 上行/下行链路 （负载均衡性能）TOR交换机ID到上行/下行链路接口的映射
map<uint32_t, vector<uint32_t>> torId2UplinkIf;  
map<uint32_t, vector<uint32_t>> torId2DownlinkIf;

// 输入文件 节点容器 服务器地址
ifstream topof, flowf;
NodeContainer n;                        
vector<Ipv4Address> serverAddress;  

// 读取拓扑 节点数量 链路数量   链路对:src, dst
uint32_t node_num, switch_num, link_num;
vector<pair<uint32_t, uint32_t>> link_pairs;   

unordered_map<uint32_t, uint16_t> portNumber;    
unordered_map<uint32_t, uint16_t> dportNumber;  
uint16_t *port_per_host;    


// 从flow.txt文件中 调度输入流  流量信息 
struct FlowInput {
    //pg:priority group ——— 优先级组
    uint32_t src, dst, pg, maxPacketCount, port;
    double start_time;
    uint32_t idx;
};
// src、dst、maxPacketCount（flow_size）、start_time
FlowInput flow_input = {0};  // 全局变量
uint32_t flow_num;
uint32_t flow_id = 0;
vector<FlowInput> allFlows;

// 路径信息读取
typedef struct {
    uint32_t src;
    uint32_t dst;
    uint32_t pathCount;
    uint32_t hops;
    uint64_t delay;
    uint64_t txDelay;
    uint64_t bw;  
    vector<Ptr<Node>> path; 
} PathFile;

// 路径信息
struct PathInfo {
    vector<Ptr<Node>> path;     
    uint64_t delay;            
    uint64_t txDelay;           
    uint64_t bw;  
    uint32_t hops;
    uint32_t pathCount;          
    PathInfo() : delay(0), txDelay(0), bw(0xfffffffffffffffflu), hops(0), pathCount(0) {} 
};


// 存储每对节点之间的所有路径及其详细信息
map<Ptr<Node>, map<Ptr<Node>, vector<PathInfo>>> allPaths;
uint32_t maxPathCount = 14;
// 存储流分配路径的Action
map<uint32_t, vector<float>> flowAllocations;

Ptr<OpenGymDataContainer> lastValidData = nullptr;

/*----------------  DRL environment  ----------------*/ 
/**
 * ObservationSpace包含的信息：
 * 1.网络拓扑：主机节点、交换机节点、链路数量  即链路信息
 * 2.链路信息：每条链路的传输时延、链路带宽、负载、链路连接状态信息（源节点、目的节点）nbr2if）
 *   路径信息：源节点、目的节点、path_count、跳数、传播时延、传输时延、带宽通过链路信息直接获取任意两个节点之间的路径信息：
 * 3.流量信息：每条流量的源节点、目的节点、流量大小、流开始时间信息
 *   定义流信息与路径信息空间：取最大路径数进行构建：源节点、目的节点、flow.id、流大小、path_count、hops、delay、txdelay、bw
 */
Ptr<OpenGymSpace> MyGetObservationSpace(void){
    // 创建流量 路径信息空间
    vector<uint32_t> shape = {maxPathCount * 9};
    Ptr<OpenGymBoxSpace> space = CreateObject<OpenGymBoxSpace>(0.0,10000000000.0,shape,"float");
    cout << "MyGetObservationSpace创建成功" << "space\n" << space << endl;
    NS_LOG_UNCOND("MyGetObservationSpace: " << space);
    return space;
}

// 读取文件
int parse_line(const char *line, PathFile *PathFile) {
    // 使用sscanf从每行中提取字段
    return sscanf(line, 
                   "src: %d, dst: %d, pathCount: %d, hops: %d, delay: %ld, txDelay: %ld, bw: %ld",
                   &PathFile->src, &PathFile->dst, &PathFile->pathCount, &PathFile->hops, 
                   &PathFile->delay, &PathFile->txDelay, &PathFile->bw);
}

// 整合所有流信息
void LoadFlows(const string& flowFilePath)
{
    ifstream flowFile(flowFilePath.c_str());
    if (!flowFile.is_open()) {
        cerr << "Error: Cannot open flow file " << flowFilePath << endl;
        return;
    }

    string flowLine;
    uint32_t flowIndex = 0;
    while (getline(flowFile, flowLine)) {
        istringstream iss(flowLine);
        FlowInput flow;
        // 根据实际的文件格式解析每个字段
        if (!(iss >> flow.src >> flow.dst >> flow.pg >> flow.maxPacketCount >> flow.start_time)) {
            cerr << "Error: Invalid flow file format in line: " << flowLine << endl;
            continue;
        }
        flow.idx = flowIndex++;
        allFlows.push_back(flow);
    }
    flowFile.close();
    cout << "Loaded " << allFlows.size() << " flows." << endl;
}

/**
 * 定义动作空间：MyGetActionSpace
 */
Ptr<OpenGymSpace> MyGetActionSpace(void)
{
    vector<uint32_t> shape = {maxPathCount};
    float low = 0.0;
    float high = 1.0;
    string dtype = TypeNameGet<float>();
    Ptr<OpenGymBoxSpace> actionSpace = CreateObject<OpenGymBoxSpace>(low, high, shape, dtype);
    // // 遍历所有源节点和目的节点
    // for (const auto& srcPair : allPaths) {
    //     cout << "srcPair: " << srcPair.first << endl;
    //     Ptr<Node> srcNode = srcPair.first;
    //     for (const auto& dstPair : srcPair.second) {
    //         Ptr<Node> dstNode = dstPair.first;
    //         uint32_t pathCount = dstPair.second.size();
    
    //         // 为每对源节点和目的节点定义一个路径数量的动作空间
    //         cout << "创建空间pathCount: " << pathCount << endl;
    //         vector<uint32_t> shape = {pathCount};
    //         float low = 0.0;
    //         float high = 1.0;
    //         string dtype = TypeNameGet<float>();
    //         Ptr<OpenGymBoxSpace> pathSpace = CreateObject<OpenGymBoxSpace>(low, high, shape, dtype);
    
    //         // 使用源节点和目的节点的ID作为键，将路径空间添加到动作空间字典中
    //         string key = to_string(srcNode->GetId()) + "-" + to_string(dstNode->GetId());
    //         actionSpace->Add(key, pathSpace);
    //         cout << "key: " << key << " pathSpace: " << pathSpace << endl;
            
    //     }
    // }
    
    NS_LOG_UNCOND("MyGetActionSpace创建成功: " << actionSpace);
    cout << "Action Space: MyGetActionSpace创建成功" << "actionSpace:\n" << actionSpace << endl;
    return actionSpace;
}

/*
Define game over condition
*/
bool MyGetGameOver(void)
{
  bool isGameOver = false;
  NS_LOG_UNCOND ("MyGetGameOver: " << isGameOver);
  if(Simulator::Now().GetSeconds() > flowgen_stop_time + simulator_extra_time) {
    isGameOver = true;
  }
  cout << "MyGetGameOver  " << "isGameOver:" << isGameOver << endl;
  return isGameOver;
}

/**
 * Collect observations：收集网络状态并返回观察空间
 * 链路信息：每条链路的传输时延、链路带宽、负载、链路连接状态信息（源节点、目的节点）
 * 流量信息：每条流量的源节点、目的节点、流量大小等信息
*/
Ptr<OpenGymDataContainer> MyGetObservation(void)
{
    std::vector<uint32_t> shape = {maxPathCount * 9};
    Ptr<OpenGymBoxContainer<float>> data = CreateObject<OpenGymBoxContainer<float>>(shape);
    float src = 0.0, dst = 0.0, flow_size = 0.0,flow_id = 0.0;
    // 设置新数据为0 由前一条数据代替
    bool newDataAvailable = false; 
    cout << "MyGetObservation函数执行   " << "allFlows    " << allFlows.size() << endl;
    cout << "Seconds(flow.start_time) " << Seconds(allFlows[0].start_time) << "   Simulator::Now()   " <<  Simulator::Now()  << endl;
    for(const auto& flow : allFlows){
        if(flow.idx < flow_num && Seconds(flow.start_time) == (Simulator::Now() )){
            newDataAvailable = true;
            src = flow.src;
            dst = flow.dst;
            flow_size = flow.maxPacketCount;
            flow_id = flow.idx;
        }
    }
    if (!newDataAvailable && lastValidData != nullptr) {
        cout << "No new flow data, using last valid data." << endl;
        Ptr<OpenGymBoxContainer<float>> boxData = DynamicCast<OpenGymBoxContainer<float>>(lastValidData);;
        src = boxData->GetValue(0);
        dst = boxData->GetValue(1);
        flow_id = boxData->GetValue(2);
        flow_size = boxData->GetValue(3);
    }
    // cout << "src " << src << " dst " << dst << " flow_size " << flow_size << endl;
    Ptr<Node> node1 = n.Get(src);
    Ptr<Node> node2 = n.Get(dst);
    std::vector<PathInfo> paths;
    auto outerIt = allPaths.find(node1);  
    if (outerIt != allPaths.end()) {
        auto innerIt = outerIt->second.find(node2); 
        if (innerIt != outerIt->second.end()) {  
            // 获取 node1 到 node2 的路径信息
            paths = innerIt->second;
            // uint32_t maxPaths = paths.size();
            // cout << "源节点 " << src << " 目的节点" << dst << " " << maxPaths << endl;
            // for (const auto& path : paths) {
            //     // 处理每条路径，输出路径信息
            //     cout << "Path details: delay=" << path.delay
            //          << ", txDelay=" << path.txDelay 
            //          << ", bw=" << path.bw 
            //          << ", hops=" << path.hops << endl;
            // }
        }else{
            // node1 到 node2 的路径信息不存在
            // cout << "No paths found from " << node1->GetId() << " to " << node2->GetId() << ".\n";
            NS_LOG_UNCOND(" No paths found from");
        }
    } else{
        // node1 在 allPaths 中不存在
        // cout << "Node1: " << src << "Node1 not found in allPaths.\n";
        NS_LOG_UNCOND("Node1 not found in allPaths.");
    }

    // 按行填充数据，每行包含9个元素
    for (uint32_t i = 0; i < maxPathCount; ++i) {
        data->AddValue(src);
        data->AddValue(dst);
        data->AddValue(flow_id);
        data->AddValue(flow_size);
        if (i < paths.size()) {
            const PathInfo & path = paths[i];
            data->AddValue(path.pathCount);
            data->AddValue(path.hops);
            data->AddValue(path.delay);
            data->AddValue(path.txDelay);
            data->AddValue(path.bw);
        } else {
            // 如果没有对应路径，则填充默认值，比如 0
            data->AddValue(0);
            data->AddValue(0); 
            data->AddValue(0); 
            data->AddValue(0); 
            data->AddValue(0); 
        }
    }
    lastValidData = data;
    cout << "MyGetObservation收集状态成功"  << "data:\n" << data << endl;
    NS_LOG_UNCOND("MyGetObservation: " << data);
    return data;
}

/**
 * 定义reward：根据当前分配记录和最新的网络状态计算奖励。
 * 1.带宽利用率最高：带宽利用率 = 分配流量（分配比例 * flowSize）/ 路径带宽，取所有路径利用率的平均值
 * 2.流完成时间最小：流完成时间 = (分配比例 * txDelay) + delay，取所有路径中的最大值；
 * 3.达到避免乱序的效果，根据时延差
 * 4.奖励函数：reward = -α * max_flow_completion_time + β * avg_bandwidth_utilization
 */
float MyGetReward(void)
{
    cout << "MyGetReward函数执行" << endl;
    const double alpha = 1.0;
    const double beta = 1.0;
    double totalReward = 0.0;
    const double penalty_scale = 2; 
    
    for(const auto &pair : flowAllocations){
        uint32_t flowIdx = pair.first;
        vector<float> allocation = pair.second;
        if (flowIdx >= allFlows.size()) continue;

        double maxCompletionTime = 0.0;
        double sumUtilization = 0.0;
        uint32_t AllocatedFlowPath  = 0;
        vector<double> completionTimes;
        double avgUtilization = 0.0;
        double flowPenalty = 0;
        // bool hasValidPath = false;

        const FlowInput &flow = allFlows[flowIdx];
        Ptr<Node> srcNode = n.Get(flow.src);
        Ptr<Node> dstNode = n.Get(flow.dst);
        std::vector<PathInfo> paths;

        // 获取路径信息
        auto outerIt = allPaths.find(srcNode);
        if (outerIt != allPaths.end()){
            auto innerIt = outerIt->second.find(dstNode);
            if (innerIt != outerIt->second.end()){
                paths = innerIt->second;
            }
        }
        // cout << "Flow " << flowIdx << " 获取路径数量: " << paths.size() << endl;
        // cout << "MyGetReward获取路径信息:  " << paths.size() << "  "<< maxPathCount << endl;

        for (size_t i = 0; i < maxPathCount; i++){
            double path_time = std::numeric_limits<double>::infinity();
            double utilization = 0.0;
            // 判断哪些路径分配了流量    包大小为64kB
            double allocatedData = allocation[i] * flow.maxPacketCount * 64 * 1024;  
            if(allocatedData > 0.0){
                AllocatedFlowPath  =  AllocatedFlowPath  + 1;
                // hasValidPath = true;
            }
            // cout << "MyGetRewardpaths:  " << paths.size() << endl;
            if(i < paths.size() && i < allocation.size()){
                const PathInfo &p = paths[i];
                // 转换为比特
                if(allocatedData == 0.0){
                    continue;
                }
                double transmissionTime = (allocatedData * 8) / p.bw;  
                // 时间单位转换为 s
                path_time = transmissionTime + p.delay * 1e-9;;
                double allocatedBandwidth = (allocatedData * 8) / transmissionTime; // bits/s;
                utilization = allocatedBandwidth / p.bw; // 实际带宽占用率
                // utilization = transmissionTime / path_time;
                if (path_time > maxCompletionTime){
                    maxCompletionTime = path_time;
                }    
            }
            completionTimes.push_back(path_time);
            sumUtilization += utilization;

        }


        // 检查路径完成时间顺序,要求时间是递增的
        for (size_t i = 1; i < completionTimes.size(); ++i) {
            // 跳过无穷大的完成时间，不参与顺序检查
            if (completionTimes[i] == std::numeric_limits<double>::infinity() || completionTimes[i-1] == std::numeric_limits<double>::infinity()) {
                continue;
            }
            if (completionTimes[i] <= completionTimes[i-1]) {
                flowPenalty -= 8.0;
            }
        }

        // 平均带宽利用率应该是，哪些路径分配了流量进行计算，不应该是paths.size()
        // cout << "AllocatedFlowPath: " << AllocatedFlowPath  << "sumUtilization: " << sumUtilization << endl;
        avgUtilization = (AllocatedFlowPath > 0) ? (sumUtilization / AllocatedFlowPath) : 0.0;
        double flowReward = -alpha * maxCompletionTime * 100 + beta * avgUtilization * 100 + penalty_scale * flowPenalty;
        totalReward += flowReward;
        cout << "Flow " << flowIdx << " Reward: " << flowReward 
                << " (Time: " << maxCompletionTime * 100 << ", Util: " << avgUtilization * 100
                << ", Penalty: " << penalty_scale * flowPenalty << ")" << endl;

    }
    cout << "Total Reward: " << totalReward << endl;
    return static_cast<float>(totalReward);
}


/*
Define extra info. Optional
*/
// string MyGetExtraInfo(void){
//   string myInfo = "RDMA-DRL";
//   myInfo += "|123";
//   NS_LOG_UNCOND("MyGetExtraInfo: " << myInfo);
//   return myInfo;
// }

bool MyExecuteActions(Ptr<OpenGymDataContainer> actionData)
{
    cout << "MyExecuteActions函数执行" << actionData<< endl;
    NS_LOG_UNCOND("MyExecuteActions: " << actionData);
    flowAllocations.clear();
    Ptr<OpenGymBoxContainer<float> > box = DynamicCast<OpenGymBoxContainer<float> >(actionData);
    if (!box) { 
        NS_LOG_ERROR("动作数据类型错误，预期为OpenGymBoxContainer<float>");
        return false;
    }
    // std::cout << "box: " << box << std::endl;
    std::vector<float> actionVector = box->GetData();
    // for (uint32_t i=0; i< maxPathCount; i++)
    // {
    //     std::cout << "actionVector: " << actionVector.at(i) << std::endl;
    // }
    // 归一化处理：确保所有分量之和为 1
    float sum = 0.0;
    for (float a : actionVector){
        sum += a;
    }
    if (sum <= 0.0f) {
        float defaultVal = 1.0f / actionVector.size();
        std::fill(actionVector.begin(), actionVector.end(), defaultVal);
    } else{
        for (float &a : actionVector){
            a /= sum;
        }
    }
    // if(sum > 0.0){
    //     for (float &a : actionVector){
    //         a /= sum;
    //     }
    // }
    uint32_t flowIdx = 0;
    for(const auto& flow : allFlows){
        if(flow.idx < flow_num && Seconds(flow.start_time) == Simulator::Now() ){
            flowIdx = flow.idx;
            // cout << "src " << flow.src << " dst " << flow.dst << " flow_size " << flow.maxPacketCount << endl;
        }
    }
    // for (uint32_t i=0; i< maxPathCount; i++)
    // {
    //     std::cout << "actionVector: " << actionVector.at(i) << std::endl;
    // }
   
    // 保存该流的分配比例
    flowAllocations[flowIdx] = actionVector;
    // cout << "flowAllocations 内容:" << endl;
    // for (const auto& pair : flowAllocations) {
    //     // cout << "Flow ID: " << pair.first << " -> Allocations: ";
    //     for (float allocation : pair.second) {
    //         // cout << allocation << " ";
    //    }
    //     // cout << endl;
    // }

    // 根据分配比例更新 NS3 中该流的调度参数
    // 例如，根据 flow.idx 找到对应的流，然后更新每条路径上实际分配的流量
    // UpdateFlowAllocation(flowIdx, actionVector);
    // }
    
    NS_LOG_UNCOND("MyExecuteActions: 更新流量分配完成");
    cout << "更新流量分配完成" << endl;
    return true;
}

void ScheduleNextStateRead(double envStepTime, Ptr<OpenGymInterface> openGymInterface)
{
  Simulator::Schedule (Seconds(envStepTime), &ScheduleNextStateRead, envStepTime, openGymInterface);
  openGymInterface->NotifyCurrentState();
}

/*----------------  Simulation function  ----------------*/ 
/**
 * ReadFlowInput：从文件“flowf”中读取流输入
 */
void ReadFlowInput(){
    if (flow_input.idx < flow_num){
        flowf >> flow_input.src >> flow_input.dst >> flow_input.pg >> flow_input.maxPacketCount >> flow_input.start_time;
        // cout << "ReadFlowInput流量信息" << flow_input.src << "  " << flow_input.dst << "  " << flow_input.maxPacketCount << "  " << flow_input.start_time << "  " <<endl;
        // cout << "flow_input.src GetNodeType" << n.Get(flow_input.src)->GetNodeType() << endl;
        // cout << "flow_input.dst GetNodeType" << n.Get(flow_input.dst)->GetNodeType() << endl;
        assert(n.Get(flow_input.src)->GetNodeType() == 0 && n.Get(flow_input.dst)->GetNodeType() == 0);
    }else{
        cout << "*** input flow is over the prefixed number -- flow number : " << flow_num
                  << endl;
        cout << "*** flow_input.idx : " << flow_input.idx << endl;
        cout << "*** THIS IS THE LAST FLOW TO SEND :) " << endl;
    }
}

void test(){
    cout << "test" << endl;
}

/**
 * ScheduleFlowInputs:
 * 用于根据 flow_input 中的流量信息 调度流量输入
 */
void ScheduleFlowInputs(FILE *infile){
    NS_LOG_DEBUG("ScheduleFlowInputs at" << Simulator::Now());
    // cout << "进入调度 ScheduleFlowInputs" << endl;
    while (flow_input.idx < flow_num && Seconds(flow_input.start_time) == Simulator::Now()){
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
        if(target_len == 0){
            target_len = 1;
        }
        assert(n.Get(src)->GetNodeType() == 0 && n.Get(dst)->GetNodeType() == 0);

        /**
         * Turn on if you want to record all input streams into output file for logging.
         * But, the input stream can be found in config. We do not recommend to do this
         * as it consumes storage resource, redundantly.
         */
        if (0) {  // logging input streams to "XXXX_out_in.txt"
            /************************
             * record flow's 4-tuple
             ************************/
            fprintf(infile, "%u %u %u %u %u %lu\n", src, dst, sport, dport, target_len,
                    (uint64_t)(flow_input.start_time * (uint64_t)1000000000));
            fflush(infile);

            /***********    FCT Tracking    **************/
            UdpServerHelper server0(dport);
            server0.SetAttribute("FlowSize", UintegerValue(target_len));
            server0.SetAttribute("irn", BooleanValue(enable_irn));
            server0.SetAttribute("StatHostSrc", UintegerValue(src));
            server0.SetAttribute("StatHostDst", UintegerValue(dst));
            server0.SetAttribute("StatRxLen", UintegerValue(target_len));
            server0.SetAttribute("StatFlowID", UintegerValue(flow_input.idx));
            server0.SetAttribute("Port", UintegerValue(dport));

            ApplicationContainer apps0s = server0.Install(n.Get(dst));  // DST
            apps0s.Start(Seconds(Time(0)));
            apps0s.Stop(Seconds(100.0));
        }  // end of logging input streams

        // 查找往返时延
        if(pairRtt.find(n.Get(src)) == pairRtt.end() || 
           pairRtt[n.Get(src)].find(n.Get(dst)) == pairRtt[n.Get(src)].end()){
            cerr << "pairRtt src: " << src << "-> dst: " << dst
            << "————> cannot be found from database" << endl;
            assert(false);
        } 

        // RDMA客户端
        RdmaClientHelper clientHelper(
            pg, serverAddress[src],serverAddress[dst], sport, dport, target_len,
            has_win ? (global_t == 1 ? maxBdp : pairBdp[n.Get(src)][n.Get(dst)]) : 0,
            global_t == 1 ? maxRtt : pairRtt[n.Get(src)][n.Get(dst)]
        );
        clientHelper.SetAttribute("StatFlowID", IntegerValue(flow_input.idx));  //设置flow id

        // 应用容器
        ApplicationContainer appCon = clientHelper.Install(n.Get(src));  //安装src
        appCon.Start(Seconds(Time(0)));
        appCon.Stop(Seconds(100.0));

        // 写入文件
        fprintf(infile, "%u %u %u %u %u %u %u\n",pg, src, dst, sport, dport, maxPacketCount, target_len);
        fflush(infile);  

        flow_input.idx++;
        ReadFlowInput();
    }

    // schedule the next time to run this function
    if (flow_input.idx < flow_num){
        // cout << "flow_input.idx   " <<  flow_input.idx  << endl;
        // std::cout << "Scheduling ScheduleFlowInputs at time: " << Simulator::Now().GetSeconds() << std::endl;
        Simulator::Schedule(Seconds(flow_input.start_time) - Simulator::Now(), &ScheduleFlowInputs, infile);
    }else{
        flowf.close();
    }   
}


/**
 * cnp_freq_monitoring:
 * 监控并记录 CNP（Congestion Notification Protocol，拥塞通知协议）的相关统计数据（如通过 ECN、OOO 等）
 */
void cnp_freq_mointoring(FILE *fout, Ptr<RdmaHw> rdmahw){
    // cout << "CNP 写入文件调用" << rdmahw->cnp_total << endl;
    if(rdmahw->cnp_total > 0){
        // CNP数据写入文件   时间戳 节点ID 通过ECN 通过OOO 接收的CNP数 和 CNP总数
        fprintf(fout, "%lu %u %u %u %u\n",Simulator::Now().GetNanoSeconds(),
                rdmahw->m_node->GetId(), rdmahw->cnp_by_ecn, rdmahw->cnp_by_ooo, rdmahw->cnp_total
        );
        fflush(fout);    // 刷新文件缓冲区，确保数据写入文件

        // 初始化 CNP 统计数据
        rdmahw->cnp_by_ecn = 0;
        rdmahw->cnp_by_ooo = 0;
        rdmahw->cnp_total = 0;
    }
    // 回调函数；定期调用cnp_freq_monitoring   设置下一次调用时间为当前时间加上 cnp_monitor_bucket 指定的时间间隔
    Simulator::Schedule(NanoSeconds(cnp_monitor_bucket), &cnp_freq_mointoring, fout, rdmahw);
}


/**
 * periodic_monitoring:
 * 定期监控交换机的状态，包括交换机的 VOQ（Virtual Output Queue，虚拟输出队列）的数量、上行链路的状态、
 * 在RNIC网卡上的 活动连接数量
 */
void periodic_monitoring(FILE *fout_voq, FILE *fout_voq_detail, FILE *fout_uplink, FILE *fout_conn, uint32_t *lb_mode){
    uint32_t lb_mode_val = *lb_mode;
    uint64_t now = Simulator::Now().GetNanoSeconds();   //获取当前时间
    // 遍历每个 TOR 交换机 的 上行链路信息
    for(const auto &tor2If : torId2UplinkIf){
        Ptr<Node> node = n.Get(tor2If.first);        //获取交换机节点
        auto swNode = DynamicCast<SwitchNode>(node); //将节点转换为SwitchNode类型
        assert(swNode->m_isToR == true);          //断言，确保是ToR交换机
        

        if(lb_mode_val == 9){   // 9:conweave 监控VOQ
            // 监控每个交换机的VOQ数量  VOQ数据包总量
            uint32_t nVOQ = swNode->m_mmu->m_conweaveRouting.GetNumVOQ();
            uint32_t nVolumeVOQ = swNode->m_mmu->m_conweaveRouting.GetNumVOQ();
            // 将当前时间、TOR ID、VOQ 数量、VOQ 数据包数量写入到 fout_voq 文件中
            fprintf(fout_voq, "%lu,%u,%u,%u\n", now, tor2If.first, nVOQ, nVolumeVOQ);
        
            // 监控每个目的IP的VOQ <time, dstip, #VOQ, #Pkts>
            unordered_map<uint32_t, pair<uint32_t, uint32_t>> dip_to_nvoq_npkt;
            for(auto voq:swNode->m_mmu->m_conweaveRouting.GetVOQMap()){  //遍历交换机中的VOQ
                auto &nvoq_npkt = dip_to_nvoq_npkt[voq.second.getDIP()];
                nvoq_npkt.first += 1;
                nvoq_npkt.second += voq.second.getQueueSize();

            }

           for (auto x : dip_to_nvoq_npkt){
            // 将当前时间、目的IP、VOQ数量、VOQ数据包数量写入到 fout_voq_detail 文件中
            fprintf(fout_voq_detail,"%lu, %u,%u,%u\n",now, x.first, x.second.first,x.second.second);
           }
        }

        // 监控每个交换机的上行链路的状态 测量负载均衡性能
        for(const auto &iface : tor2If.second){
            uint64_t uplink_txbyte = swNode->GetTxBytesOutDev(iface);      //获取上行链路接口发送字节数
            fprintf(fout_uplink, "%lu,%u,%u,%lu\n",now, tor2If.first, iface, uplink_txbyte);  //<time, ToRId, OutDev, Bytes>  输出 fout_uplink
        }
    }

    // 获取每个服务器的并发连接数（活跃的队列对）
    for(uint32_t i = 0 ; i < Settings::node_num; i++){
        if(n.Get(i)->GetNodeType() == 0){  //类型是服务器
            Ptr<Node> server = n.Get(i);                                   //服务器节点
            Ptr<RdmaDriver> rdmaDriver = server->GetObject<RdmaDriver>();  //RDMA驱动
            Ptr<RdmaHw> rdmaHw = rdmaDriver->m_rdma;                       //RDMA硬件

            // monitor total/active QP number <time, serverId, #ExistingQP, #ActiveQP>
            uint64_t nQP = rdmaHw->m_qpMap.size();
            uint64_t nActiveQP = 0;
            for(auto qp : rdmaHw->m_qpMap){    //遍历QP；计算QP活跃个数    conns with bytes left
                if(qp.second->GetBytesLeft() > 0){
                    nActiveQP++;
                }
            }
            fprintf(fout_conn,"%lu,%u,%lu,%lu\n",now, i, nQP, nActiveQP);   //<time, serverId, #ExistingQP, #ActiveQP>  输出 fout_conn
        }
    }

    if(Simulator::Now() < Seconds(flowgen_stop_time + 0.05)){   
        // 回调函数
        Simulator::Schedule(NanoSeconds(switch_mon_interval), &periodic_monitoring, fout_voq,
                            fout_voq_detail, fout_uplink, fout_conn, lb_mode);  // every 10us
    }

    return; 
}


/**
 * conga_history_print：
 * 记录并打印 timeout 信息
 */
void conga_history_print(){
    cout << "\n -------------CONGA History--------------" << endl;
    cout << "Number of flowlet's timeout:" << LetflowRouting::nFlowletTimeout
              << "\n Conga's timeout:   " << letflow_flowletTimeout << endl;
}


/**
 * letflow_history_print：
 * 记录并打印 timeout 信息
 */
void letflow_history_print(){
    cout << "\n------------Letflow History---------------" << endl;
    cout << "Number of flowlet's timeout:" << LetflowRouting::nFlowletTimeout
              << "\nLetflow's timeout: " << letflow_flowletTimeout << endl;
}

/**
 * conweave_history_print：
 * 记录并打印 conweve 参数信息，和VOQ统计信息
 */
void conweave_history_print(){
    // conweve parameters
    cout << "\n -----------Conweave parameters----------" << endl;
    cout << "parameter - extraReplyDeadline:" << conweave_extraReplyDeadline << endl;  //额外回复截止时间
    cout << "parameter - extraVOQFlushTime:" << conweave_extraVOQFlushTime << endl;    //额外VOQ刷新时间
    cout << "parameter - txExpiryTime:" << conweave_txExpiryTime<< endl;               //输出发送超时
    cout << "parameter - defaultVOQWaitingTime:" << conweave_defaultVOQWaitingTime << endl;   //默认VOQ等待时间
    cout << "parameter - pathPauseTime:" << conweave_pathPauseTime << endl;                   //路径暂停时间
    cout << "parameter - pathAwareRerouting:" << conweave_pathAwareRerouting << endl;         //路径感知重路由

    cout << "\n------------ConWeave History---------------" << endl;
    cout << "Number of INIT's Reply sent (RTT_REPLY): " << ConWeaveRouting::m_nReplyInitSent
              << "\n Number of Timely RTT_REPLY (INIT's Replay): " << ConWeaveRouting::m_nTimelyInitReplied
              << "\n Number of TAIL's Reply Sent (CLEAR): " << ConWeaveRouting::m_nReplyTailSent
              << "\n Number of Timely CLEAR (TAIL's Reply): " << ConWeaveRouting::m_nTimelyTailReplied
              << "\n Number of NOTIFY Sent: " << ConWeaveRouting::m_nNotifySent
              << "\n Number of Rerouting:" << ConWeaveRouting::m_nReRoute
              << "\n Number of OoO enqueued pkts: " << ConWeaveRouting::m_nOutOfOrderPkts
              << "\n Number of VOQ Flush Total: " << ConWeaveRouting::m_nFlushVOQTotal
              << "\n Number of VOQ Flush From History: " << ConWeaveRouting::m_historyVOQSize.size()
              << "\n Number of VOQ Flush by TAIL: " << ConWeaveRouting::m_nFlushVOQByTail
              << endl;
    cout << "----------------------------------------" << endl;


    // VOQ 一致性检查
    for(size_t ToRId = 0; ToRId < Settings::node_num; ToRId++){
        Ptr<Node> node = n.Get(ToRId);
        if(node->GetNodeType() == 1){   //是否为交换机节点
            auto swNode = DynamicCast<SwitchNode>(n.Get(ToRId));
            if(swNode->m_isToR){    //TOR交换机
                uint32_t num_remained_voq = swNode->m_mmu->m_conweaveRouting.GetNumVOQ();
                if(num_remained_voq > 0){
                    // 剩余VOQ数量；交换机ID以及为清空的VOQ数量
                    printf("----------------------------------\n");
                    printf("WARNING - Tor Sw (%lu) - VOQ (num=%u) is not flushed yet !!\n",ToRId,num_remained_voq);
                    printf("Probably the history print is too early so simulation mighrt not be , finished");
                    printf("----------------------------------\n");
                }
            }
        }
    }

}

/**
 * qp_finish：RDMA处理机制
 * 当RDMA完成时，需要处理
 * 1.QP(Queue Pair)
 * 2.RxQP(接收端QP)
 * 3.记录完成的流量信息，将记录信息写入fct.txt文件中
 */
void qp_finish(FILE *fout, Ptr<RdmaQueuePair> q){
    uint32_t sid = Settings::ip_to_node_id(q->sip);
    uint32_t did = Settings::ip_to_node_id(q->dip);
    uint64_t base_rtt = pairRtt[n.Get(sid)][n.Get(did)];    //源节点和目的节点之间的RTT
    uint64_t b = pairBw[n.Get(sid)][n.Get(did)];  
    //传输总字节数；流完成时间：往返时延 + 传输时间          
    uint32_t total_bytes = q->m_size +((q->m_size - 1) / packet_payload_size + 1) * (CustomHeader::GetStaticWholeHeaderSize() - IntHeader::GetStaticSize());
    uint64_t standalone_fct = base_rtt + total_bytes * 800000000lu / b;

    // 从接收端删除RxQP(接收队列)   QP 完成后，它的接收队列不再需要存在。
    Ptr<Node> dstNode = n.Get(did);
    Ptr<RdmaDriver> rdma = dstNode->GetObject<RdmaDriver>();
    rdma->m_rdma->DeleteRxQp(q->sip.Get(), q->sport, q->dport, q->m_pg);

    fprintf(fout, "%lu QP complete\n", Simulator::Now().GetTimeStep());
    fprintf(fout, "%u %u %u %u %lu %lu %lu %lu\n", Settings::ip_to_node_id(q->sip),
            Settings::ip_to_node_id(q->dip), q->sport, q->dport, q->m_size,
            q->startTime.GetTimeStep(),(Simulator::Now()-q->startTime).GetTimeStep(),
            standalone_fct);

    // debugging
    NS_LOG_DEBUG(Settings::ip_to_node_id(q->sip) << "  " << Settings::ip_to_node_id(q->dip) << "    "
                 << q->sport << "  " << q->dport << "  " << q->m_size << "  " << q->startTime.GetTimeStep() << "  "
                 << (Simulator::Now() - q->startTime).GetTimeStep() << "  " << standalone_fct
                );

    Settings::cnt_finished_flows++;
    fflush(fout);
}

/**
 * get_pfc：
 * PFC（Priority Flow Control，优先级流控制）信息 事件记录
 */
void get_pfc(FILE *fout, Ptr<QbbNetDevice> dev, uint32_t type){
    //  time, nodeID, nodeType, Interface's Idx, 0:resume, 1:pause
    fprintf(fout,"%lu %u %u %u %u\n", Simulator::Now().GetTimeStep(),dev->GetNode()->GetId(),
            dev->GetNode()->GetNodeType(), dev->GetIfIndex(), type);
}

/*******************************************************************/
#if (false)

/**
 * @brief Qlen monitoring at switches (output: qlen.txt), I think "periodically"...
 *
 */
struct QlenDistribution {
    vector<uint32_t> cnt;  // cnt[i] is the number of times that the queue len is i KB
    void add(uint32_t qlen) {
        uint32_t kb = qlen / 1000;
        if (cnt.size() < kb + 1) cnt.resize(kb + 1);
        cnt[kb]++;
    }
};

map<uint32_t, map<uint32_t, QlenDistribution>> queue_result;
void monitor_buffer(FILE *qlen_output, NodeContainer *n) {
    /*******************************************************************/
    /************************** UNUSED NOW *****************************/
    /*******************************************************************/
    for (uint32_t i = 0; i < n->GetN(); i++) {
        if (n->Get(i)->GetNodeType() == 1) {  // is switch
            Ptr<SwitchNode> sw = DynamicCast<SwitchNode>(n->Get(i));
            if (queue_result.find(i) == queue_result.end()) queue_result[i];
            for (uint32_t j = 1; j < sw->GetNDevices(); j++) {
                uint32_t size = 0;
                for (uint32_t k = 0; k < SwitchMmu::qCnt; k++)
                    size += sw->m_mmu->egress_bytes[j][k];
                queue_result[i][j].add(size);
            }
        }
    }
    if (Simulator::Now().GetTimeStep() % qlen_dump_interval == 0) {
        fprintf(qlen_output, "time: %lu\n", Simulator::Now().GetTimeStep());
        for (auto &it0 : queue_result) {
            for (auto &it1 : it0.second) {
                fprintf(qlen_output, "%u %u", it0.first, it1.first);
                auto &dist = it1.second.cnt;
                for (uint32_t i = 0; i < dist.size(); i++) fprintf(qlen_output, " %u", dist[i]);
                fprintf(qlen_output, "\n");
            }
        }
        fflush(qlen_output); 
    }
    if (Simulator::Now().GetTimeStep() < qlen_mon_end)
        Simulator::Schedule(NanoSeconds(qlen_mon_interval), &monitor_buffer, qlen_output, n);
}
#endif
/*******************************************************************/


/**
 * stop_simulation_middle：
 * 当所有flows 大部分完成时，提前终止仿真
 * 当所以信息被发送时，这个函数被调用进而快速完成仿真
 */
// void stop_simulation_middle(){
//     uint32_t target_flow_num = flow_num - 0;           //需要完成的目标流数量
//     if(Settings::cnt_finished_flows >= target_flow_num){
//         cout << "\n Simulator is enforced to be finished, finished so far:"
//                   << Settings::cnt_finished_flows << "/ total: " << target_flow_num
//                   << ", Time: " << Simulator::Now() << endl;
//         // 打印历史信息
//         if(lb_mode == 3){
//             conga_history_print();
//         }
//         if(lb_mode == 6){
//             letflow_history_print();
//         }
//         if(lb_mode == 9){
//             conweave_history_print();
//         }
//         Simulator::Stop(NanoSeconds(1));  //停止仿真
//         return;
//     }
//     Simulator::Schedule(MicroSeconds(1000000000), &stop_simulation_middle);
// }

/**
 * FindAllPaths:计算从给定主机节点到目标节点的所有路径，并记录时延、传输时延和带宽
 * 该函数通过深度优先搜索（DFS）遍历从给定主机节点到目标节点的所有路径，
 * 并记录每条路径的时延、传输时延和带宽。
 * host 起始节点
 * target 目标节点
 * path 当前路径
 * totalDelay 当前路径的总延迟
 * totalTxDelay 当前路径的总传输时延
 * minBw 当前路径的最小带宽
 */
void FindAllPaths(Ptr<Node> host, Ptr<Node> target, vector<Ptr<Node>>& path, uint64_t totalDelay, uint64_t totalTxDelay, uint64_t minBw, uint32_t& count, uint32_t src) {
    // 如果当前节点已经在路径中，跳过防止回环
    if (find(path.begin(), path.end(), host) != path.end()) {
        return;
    }
    // 将当前节点加入路径
    path.push_back(host);
    for (auto it = nbr2if[host].begin(); it != nbr2if[host].end(); it++) {
        // 跳过非启用的下行接口
        if (!it->second.up) continue; 

        Ptr<Node> next = it->first;  
        // 计算路径的时延、传输时延、带宽
        uint64_t edgeDelay = it->second.delay;       
        uint64_t edgeTxDelay = packet_payload_size * 1000000000lu * 8 / it->second.bw; 
        uint64_t edgeBw = it->second.bw;              

        // 更新路径的延迟、传输时延和最小带宽
        uint64_t newDelay = totalDelay + edgeDelay;
        uint64_t newTxDelay = totalTxDelay + edgeTxDelay;
        uint64_t newBw = min(minBw, edgeBw);
        // 只有当邻居节点为交换机节点 才进行递归
        if ((next->GetId() >= 128 && next->GetId() < 144) && next != target) {
            // 递归查找路径
            FindAllPaths(next, target, path, newDelay, newTxDelay, newBw,count,src);
        }
        if (next == target) {// 如果到达目标节点，则记录路径和相关信息
            PathInfo pathInfo;
            pathInfo.path = path;
            pathInfo.delay = newDelay;
            pathInfo.txDelay = newTxDelay;
            pathInfo.bw = newBw;
            pathInfo.hops = path.size() + 1;  
            // 更新路径计数
            pathInfo.pathCount = ++count;
            allPaths[n.Get(src)][target].push_back(pathInfo);  // 存储路径和信息

            
            cout << "找到路径：" << src << " 到 " << target->GetId() << "，路径大小：" << path.size() << endl;
            // cout << "src " << src << " dst " << target->GetId() << " pathCount " << pathInfo.pathCount << " hopCount " << path.size() + 1 <<  endl;
            // 打印路径信息
            // cout << "Path: ";
            // for (auto node : path) {
            //     cout << node->GetId() << " ";
            // }
            // cout << target->GetId() << endl; 
            path_output = fopen(path_output_file.c_str(), "a");
            if(path_output == NULL){
                cerr << "Error: Cannot open file " << path_output_file << endl;
                exit(1);
            }
            fprintf(path_output, "src: %d, dst: %d, pathCount: %d, hops: %d, delay: %lu, txDelay: %lu, bw: %lu, path: ",
                src, target->GetId(), pathInfo.pathCount, pathInfo.hops, pathInfo.delay, pathInfo.txDelay, pathInfo.bw);
            for (auto node : path) {
                fprintf(path_output, "%d ", node->GetId());
            }
            fprintf(path_output, "%d\n", target->GetId());
            fclose(path_output);
            return;
        }
    }

    // 回溯时移除当前节点
    path.pop_back();
}

/**
 * CalculateAllPaths：遍历所有节点并计算每对节点之间的所有路径
 * 该函数遍历 `NodeContainer` 中的所有节点，调用 `FindAllPaths` 查找每对节点之间的所有路径，
 * 并记录路径时延、传输时延、带宽等信息。
 */
void CalculateAllPaths(NodeContainer &n) {
    Ptr<Node> node1 = n.Get(43);
    Ptr<Node> node2 = n.Get(12);
    vector<Ptr<Node>> path;
    uint32_t pathCount = 0;
    cout << "开始寻找路径：" << node1->GetId() << " 到 " << node2->GetId() << endl;
    FindAllPaths(node1, node2, path, 0, 0, 0xfffffffffffffffflu, pathCount, node1->GetId());
    Ptr<Node> node3 = n.Get(93);
    Ptr<Node> node4 = n.Get(11);
    vector<Ptr<Node>> path1;
    uint32_t pathCount1 = 0;
    cout << "开始寻找路径：" << node3->GetId() << " 到 " << node4->GetId() << endl;
    FindAllPaths(node3, node4, path1, 0, 0, 0xfffffffffffffffflu, pathCount1, node3->GetId());
    // for (int i = 0; i < (int)n.GetN(); i++) {
    //     Ptr<Node> node1 = n.Get(i);
    //     for (int j = 0 ; j < (int)n.GetN(); j++) {  
    //         Ptr<Node> node2 = n.Get(j);
    //         vector<Ptr<Node>> path;
    //         uint32_t pathCount = 0;
    //         if(node1->GetNodeType() == 0 && node2->GetNodeType() == 0 && j != i){
    //             // cout << "开始寻找路径：" << node1->GetId() << " 到 " << node2->GetId() << endl;
    //             FindAllPaths(node1, node2, path, 0, 0, 0xfffffffffffffffflu, pathCount, node1->GetId());  
    //         }
    //     }
    // }
}

/**
 * FindMaxPathCount:找到最大的路径
 */
uint32_t FindMaxPathCount() {
    for (const auto& srcPair : allPaths) {
        for (const auto& dstPair : srcPair.second) {
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
void CalculateRoute(Ptr<Node> host){
    vector<Ptr<Node>> q;       //queue for BFS
    map<Ptr<Node>, int> dis;   //distance from host
    map<Ptr<Node>, uint64_t> delay;
    map<Ptr<Node>, uint64_t> txDelay;
    map<Ptr<Node>, uint64_t> bw;


    q.push_back(host);
    dis[host] = 0;
    delay[host] = 0;
    txDelay[host] = 0;
    bw[host] = 0xfffffffffffffffflu;

    for(int i = 0 ; i < (int)q.size(); i++){   
        Ptr<Node> now = q[i];
        int d = dis[now];
        for(auto it = nbr2if[now].begin(); it != nbr2if[now].end(); it++){   
            if(!it->second.up) continue;
            Ptr<Node> next = it->first;
            if(dis.find(next) == dis.end()){  
                dis[next] = d + 1;
                delay[next] = delay[now] + it->second.delay;
                // 传输时延 = 数据包大小 * 1000000000 * 8 / 带宽；需要重新计算
                txDelay[next] = txDelay[now] + packet_payload_size * 1000000000lu * 8 / it->second.bw;
                bw[next] = min(bw[now], it->second.bw);

                if(next->GetNodeType() == 1){
                    q.push_back(next);
                }
            }
            // if 'now' is on the shortest path from 'next' to 'host'，更新路径
            if(d + 1 == dis[next]){
                nextHop[next][host].push_back(now);
            }
        }
    }
    for(auto it : delay){
        pairDelay[it.first][host] = it.second;
    }
    for(auto it : txDelay){
        pairTxDelay[it.first][host] = it.second;
    }
    for(auto it : bw ){
        pairBw[it.first][host] = it.second;
    }
}


/**
 * CalculateRoutes：
 *对每个节点计算路由信息
 */
void CalculateRoutes(NodeContainer &n){
    for(int i = 0; i < (int)n.GetN(); i++){
        Ptr<Node> node = n.Get(i);
        if(node->GetNodeType() == 0){
            CalculateRoute(node);
        }
    }
}


/**
 * SetRoutingEntries：
 * 设置路由条目
 */
void SetRoutingEntries(){
    for(auto i = nextHop.begin(); i != nextHop.end(); i++){   //nextHop 节点到下一跳的映射
        Ptr<Node> node = i->first;
        auto &table = i->second;  //table 路由表

        for(auto j = table.begin(); j != table.end(); j++){
            Ptr<Node> dst = j->first;   //目的节点
            Ipv4Address dstAddr = dst ->GetObject<Ipv4>()->GetAddress(1,0).GetLocal();
            vector<Ptr<Node>> nexts = j->second;   //目标节点 下一跳节点
            for(int k = 0; k < (int)nexts.size(); k++){
                Ptr<Node> next = nexts[k];
                uint32_t interface = nbr2if[node][next].idx;  //节点之间的接口索引
                if(node->GetNodeType() == 1){
                    DynamicCast<SwitchNode>(node)->AddTableEntry(dstAddr,interface);         //添加路由表
                }else{
                    node->GetObject<RdmaDriver>()->m_rdma->AddTableEntry(dstAddr,interface);   //添加路由表
                }
            }
        }
    }
}


/**
 * TakeDownLink：
 * 中断节点 a 和节点 b 之间的链路，并重新计算路由
 */
void TakeDownLink(NodeContainer n, Ptr<Node> a, Ptr<Node> b){
    if(!nbr2if[a][b].up) return;   //up link是否中断 
    nbr2if[a][b].up = nbr2if[b][a].up = false;
    nextHop.clear();
    CalculateRoutes(n);

    // 清空路由表
    for(uint32_t i = 0; i < n.GetN(); i++){
        if(n.Get(i)->GetNodeType() == 1){
            DynamicCast<SwitchNode>(n.Get(i))->ClearTable();
        }else{
            n.Get(i)->GetObject<RdmaDriver>()->m_rdma->ClearTable();
        }
    } 
    // 断开链路
    DynamicCast<QbbNetDevice>(a->GetDevice(nbr2if[a][b].idx))->TakeDown();
    DynamicCast<QbbNetDevice>(b->GetDevice(nbr2if[b][a].idx))->TakeDown();
    SetRoutingEntries();

    for(uint32_t i = 0; i < n.GetN() ; i++){
        if(n.Get(i)->GetNodeType() == 0){
            n.Get(i)->GetObject<RdmaDriver>()->m_rdma->RedistributeQp();   //重新分配RDMA连接
        }
    }
}


/**
 * get_nic_rate：
 * 获取节点容器中所有服务器节点的 NIC（Network Interface Card，网络接口卡）速率
 * 计算的是平均速率
 */
uint64_t get_nic_rate(NodeContainer &n){
    uint64_t avg_nic_rate = 0;
    uint64_t n_servers = 0;
    for(uint32_t i = 0; i < n.GetN(); i++){
        if(n.Get(i)->GetNodeType() == 0){
            avg_nic_rate += DynamicCast<QbbNetDevice>(n.Get(i)->GetDevice(1))->GetDataRate().GetBitRate();
            n_servers += 1;
        }
    }
    return avg_nic_rate / n_servers;
}



// 主函数
int main(int argc, char *argv[]){
    // parameters of the environement
    uint32_t simSeed = 1;
    double simulationTime = 1; 
    double envStepTime = 0.000000001; 
    uint32_t openGymPort = 6000;
    uint32_t testArg = 0;
    string configFile = "~/ns-allinone-3.29/ns-3.29/mix/output/989705940/config.txt";

    // clock_t begint, endt;
    // begint =  clock();

    // 参数读取
    CommandLine cmd;
    cmd.AddValue ("configFile", "Configuration file for network topology", configFile);
    cmd.AddValue ("openGymPort", "Port number for OpenGym env. Default: 5555", openGymPort);
    cmd.AddValue ("simSeed", "Seed for random generator. Default: 1", simSeed);
    // optional parameters
    cmd.AddValue ("simTime", "Simulation time in seconds. Default: 10s", simulationTime);
    cmd.AddValue ("testArg", "Extra simulation argument. Default: 0", testArg);
    cmd.Parse (argc, argv);

    NS_LOG_UNCOND("Ns3Env parameters:");
    NS_LOG_UNCOND("--simulationTime: " << simulationTime);
    NS_LOG_UNCOND("--openGymPort: " << openGymPort);
    NS_LOG_UNCOND("--envStepTime: " << envStepTime);
    NS_LOG_UNCOND("--seed: " << simSeed);
    // NS_LOG_UNCOND("--distance: " << distance);
    NS_LOG_UNCOND("--testArg: " << testArg);

    // 读取配置文件并初始化网络拓扑
    ifstream conf;
    conf.open(configFile);
    if (!conf.is_open()) {
        cerr << "Error: Unable to open configuration file: " << configFile << endl;
        return 1;  // 如果无法打开文件，退出程序
    }
    cout << "Reading configuration file...读取文件" << endl;
    /******************* READING CONFIG FILE IS DONE ***********************/
    /**
     * 读取配置文件，检查文件是否结束，key键值
     * 读取拓扑 | 生成流量 | 生成内容 的文件名称 | 变量设置
     */
    while (!conf.eof()){    
    string key;
    conf >> key;
    if (key.compare("FLOW_INPUT_FILE") == 0) {
        string v;
        conf >> v;
        flow_input_file = v;
        cerr << "FLOW_INPUT_FILE\t\t\t" << flow_input_file << "\n";
    } else if (key.compare("CNP_OUTPUT_FILE") == 0) {
        string v;
        conf >> v;
        cnp_output_file = v;
        cerr << "CNP_OUTPUT_FILE\t\t\t" << cnp_output_file << "\n";
    } else if(key.compare("PATH_OUTPUT_FILE") == 0){
        string v;
        conf >> v;
        path_output_file = v;
        cerr << "PATH_OUTPUT_FILE\t\t\t" << path_output_file << "\n";
    }else if (key.compare("EST_ERROR_MON_FILE") == 0) {
        string v;
        conf >> v;
        est_error_output_file = v;
        cerr << "EST_ERROR_MON_FILE\t\t\t" << est_error_output_file << "\n";
    } else if (key.compare("LB_MODE") == 0) {
        uint32_t v;
        conf >> v;
        lb_mode = v;
        cerr << "LB_MODE\t\t\t" << lb_mode << "\n";
    } else if (key.compare("SW_MONITORING_INTERVAL") == 0) {
        uint32_t v;
        conf >> v;
        switch_mon_interval = v;
        cerr << "SW_MONITORING_INTERVAL\t\t\t" << switch_mon_interval << "\n";
    } else if (key.compare("CONWEAVE_TX_EXPIRY_TIME") == 0) {
        uint32_t v;
        conf >> v;
        conweave_txExpiryTime = Time(MicroSeconds(v));
        cerr << "CONWEAVE_TX_EXPIRY_TIME\t\t\t" << conweave_txExpiryTime << "\n";
    } else if (key.compare("CONWEAVE_REPLY_TIMEOUT_EXTRA") == 0) {
        uint32_t v;
        conf >> v;
        conweave_extraReplyDeadline = Time(MicroSeconds(v));
        cerr << "CONWEAVE_REPLY_TIMEOUT_EXTRA\t\t\t" << conweave_extraReplyDeadline
                    << "\n";
    } else if (key.compare("CONWEAVE_EXTRA_VOQ_FLUSH_TIME") == 0) {
        uint32_t v;
        conf >> v;
        conweave_extraVOQFlushTime = Time(MicroSeconds(v));
        cerr << "CONWEAVE_EXTRA_VOQ_FLUSH_TIME\t\t\t" << conweave_extraVOQFlushTime
                    << "\n";
    } else if (key.compare("CONWEAVE_PATH_PAUSE_TIME") == 0) {
        uint32_t v;
        conf >> v;
        conweave_pathPauseTime = Time(MicroSeconds(v));
        cerr << "CONWEAVE_PATH_PAUSE_TIME\t\t\t" << conweave_pathPauseTime << "\n";
    } else if (key.compare("CONWEAVE_DEFAULT_VOQ_WAITING_TIME") == 0) {
        uint32_t v;
        conf >> v;
        conweave_defaultVOQWaitingTime = Time(MicroSeconds(v));
        cerr << "CONWEAVE_DEFAULT_VOQ_WAITING_TIME\t\t\t"
                    << conweave_defaultVOQWaitingTime << "\n";
    } else if (key.compare("ENABLE_PFC") == 0) {
        uint32_t v;
        conf >> v;
        enable_pfc = v;
        if (enable_pfc)
            cerr << "ENABLE_PFC\t\t\t"
                        << "Yes"
                        << "\n";
        else
            cerr << "ENABLE_PFC\t\t\t"
                        << "No"
                        << "\n";
    } else if (key.compare("ENABLE_QCN") == 0) {
        uint32_t v;
        conf >> v;
        enable_qcn = v;
        if (enable_qcn)
            cerr << "ENABLE_QCN\t\t\t"
                        << "Yes"
                        << "\n";
        else
            cerr << "ENABLE_QCN\t\t\t"
                        << "No"
                        << "\n";
    } else if (key.compare("USE_DYNAMIC_PFC_THRESHOLD") == 0) {
        uint32_t v;
        conf >> v;
        use_dynamic_pfc_threshold = v;
        if (use_dynamic_pfc_threshold)
            cerr << "USE_DYNAMIC_PFC_THRESHOLD\t"
                        << "Yes"
                        << "\n";
        else
            cerr << "USE_DYNAMIC_PFC_THRESHOLD\t"
                        << "No"
                        << "\n";
    } else if (key.compare("CLAMP_TARGET_RATE") == 0) {
        uint32_t v;
        conf >> v;
        clamp_target_rate = v;
        if (clamp_target_rate)
            cerr << "CLAMP_TARGET_RATE\t\t"
                        << "Yes"
                        << "\n";
        else
            cerr << "CLAMP_TARGET_RATE\t\t"
                        << "No"
                        << "\n";
    } else if (key.compare("PAUSE_TIME") == 0) {
        double v;
        conf >> v;
        pause_time = v;
        cerr << "PAUSE_TIME\t\t\t" << pause_time << "\n";
    } else if (key.compare("DATA_RATE") == 0) {
        string v;
        conf >> v;
        data_rate = v;
        cerr << "DATA_RATE\t\t\t" << data_rate << "\n";
    } else if (key.compare("LINK_DELAY") == 0) {
        string v;
        conf >> v;
        link_delay = v;
        cerr << "LINK_DELAY\t\t\t" << link_delay << "\n";
    } else if (key.compare("PACKET_PAYLOAD_SIZE") == 0) {
        uint32_t v;
        conf >> v;
        packet_payload_size = v;
        cerr << "PACKET_PAYLOAD_SIZE\t\t" << packet_payload_size << "\n";
    } else if (key.compare("L2_CHUNK_SIZE") == 0) {
        uint32_t v;
        conf >> v;
        l2_chunk_size = v;
        cerr << "L2_CHUNK_SIZE\t\t\t" << l2_chunk_size << "\n";
    } else if (key.compare("L2_ACK_INTERVAL") == 0) {
        uint32_t v;
        conf >> v;
        l2_ack_interval = v;
        cerr << "L2_ACK_INTERVAL\t\t\t" << l2_ack_interval << "\n";
    } else if (key.compare("L2_BACK_TO_ZERO") == 0) {
        uint32_t v;
        conf >> v;
        l2_back_to_zero = v;
        if (l2_back_to_zero)
            cerr << "L2_BACK_TO_ZERO\t\t\t"
                        << "Yes"
                        << "\n";
        else
            cerr << "L2_BACK_TO_ZERO\t\t\t"
                        << "No"
                        << "\n";
    } else if (key.compare("TOPOLOGY_FILE") == 0) {
        string v;
        conf >> v;
        topology_file = v;
        cerr << "TOPOLOGY_FILE\t\t\t" << topology_file << "\n";
    } else if (key.compare("FLOW_FILE") == 0) {
        string v;
        conf >> v;
        flow_file = v;
        cerr << "FLOW_FILE\t\t\t" << flow_file << "\n";
    } else if (key.compare("FLOWGEN_START_TIME") == 0) {
        cout << "写入 FLOWGEN_START_TIME" << endl;
        double v;
        conf >> v;
        flowgen_start_time = v;
        qlen_mon_start = v;
        qlen_mon_end = v;
        cnp_mon_start = v;
        irn_mon_start = v;
        cerr << "FLOWGEN_START_TIME\t\t" << flowgen_start_time << "\n";
    } else if (key.compare("FLOWGEN_STOP_TIME") == 0) {
        double v;
        conf >> v;
        flowgen_stop_time = v;
        cerr << "FLOWGEN_STOP_TIME\t\t" << flowgen_stop_time << "\n";
    } else if (key.compare("ALPHA_RESUME_INTERVAL") == 0) {
        double v;
        conf >> v;
        alpha_resume_interval = v;
        cerr << "ALPHA_RESUME_INTERVAL\t\t" << alpha_resume_interval << "\n";
    } else if (key.compare("RP_TIMER") == 0) {
        double v;
        conf >> v;
        rp_timer = v;
        cerr << "RP_TIMER\t\t\t" << rp_timer << "\n";
    } else if (key.compare("EWMA_GAIN") == 0) {
        double v;
        conf >> v;
        ewma_gain = v;
        cerr << "EWMA_GAIN\t\t\t" << ewma_gain << "\n";
    } else if (key.compare("FAST_RECOVERY_TIMES") == 0) {
        uint32_t v;
        conf >> v;
        fast_recovery_times = v;
        cerr << "FAST_RECOVERY_TIMES\t\t" << fast_recovery_times << "\n";
    } else if (key.compare("RATE_AI") == 0) {
        string v;
        conf >> v;
        rate_ai = v;
        cerr << "RATE_AI\t\t\t\t" << rate_ai << "\n";
    } else if (key.compare("RATE_HAI") == 0) {
        string v;
        conf >> v;
        rate_hai = v;
        cerr << "RATE_HAI\t\t\t" << rate_hai << "\n";
    } else if (key.compare("ERROR_RATE_PER_LINK") == 0) {
        double v;
        conf >> v;
        error_rate_per_link = v;
        cerr << "ERROR_RATE_PER_LINK\t\t" << error_rate_per_link << "\n";
    } else if (key.compare("CC_MODE") == 0) {
        conf >> cc_model;
        cerr << "CC_MODE\t\t" << cc_model << '\n';
    } else if (key.compare("RATE_DECREASE_INTERVAL") == 0) {
        double v;
        conf >> v;
        rate_decrease_interval = v;
        cerr << "RATE_DECREASE_INTERVAL\t\t" << rate_decrease_interval << "\n";
    } else if (key.compare("MIN_RATE") == 0) {
        conf >> min_rate;
        cerr << "MIN_RATE\t\t" << min_rate << "\n";
    } else if (key.compare("FCT_OUTPUT_FILE") == 0) {
        conf >> fct_output_file;
        cerr << "FCT_OUTPUT_FILE\t\t" << fct_output_file << '\n';
    } else if (key.compare("HAS_WIN") == 0) {
        conf >> has_win;
        cerr << "HAS_WIN\t\t" << has_win << "\n";
    } else if (key.compare("GLOBAL_T") == 0) {
        conf >> global_t;
        cerr << "GLOBAL_T\t\t" << global_t << '\n';
    } else if (key.compare("MI_THRESH") == 0) {
        conf >> mi_thresh;
        cerr << "MI_THRESH\t\t" << mi_thresh << '\n';
    } else if (key.compare("VAR_WIN") == 0) {
        uint32_t v;
        conf >> v;
        var_win = v;
        cerr << "VAR_WIN\t\t" << v << '\n';
    } else if (key.compare("FAST_REACT") == 0) {
        uint32_t v;
        conf >> v;
        fast_react = v;
        cerr << "FAST_REACT\t\t" << v << '\n';
    } else if (key.compare("U_TARGET") == 0) {
        conf >> u_target;
        cerr << "U_TARGET\t\t" << u_target << '\n';
    } else if (key.compare("INT_MULTI") == 0) {
        conf >> int_multi;
        cerr << "INT_MULTI\t\t\t\t" << int_multi << '\n';
    } else if (key.compare("RATE_BOUND") == 0) {
        uint32_t v;
        conf >> v;
        rate_bound = v;
        cerr << "RATE_BOUND\t\t" << rate_bound << '\n';
    } else if (key.compare("DCTCP_RATE_AI") == 0) {
        conf >> dctcp_rate_ai;
        cerr << "DCTCP_RATE_AI\t\t\t\t" << dctcp_rate_ai << "\n";
    } else if (key.compare("PFC_OUTPUT_FILE") == 0) {
        conf >> pfc_output_file;
        cerr << "PFC_OUTPUT_FILE\t\t\t\t" << pfc_output_file << '\n';
    } else if (key.compare("LINK_DOWN") == 0) {
        conf >> link_down_time >> link_down_A >> link_down_B;
        cerr << "LINK_DOWN\t\t\t\t" << link_down_time << ' ' << link_down_A << ' '
                    << link_down_B << '\n';
    } else if (key.compare("KMAX_MAP") == 0) {
        int n_k;
        conf >> n_k;
        cerr << "KMAX_MAP\t\t\t\t";
        for (int i = 0; i < n_k; i++) {
            uint64_t rate;
            uint32_t k;
            conf >> rate >> k;
            rate2kmax[rate] = k;
            cerr << ' ' << rate << ' ' << k;
        }
        cerr << '\n';
    } else if (key.compare("KMIN_MAP") == 0) {
        int n_k;
        conf >> n_k;
        cerr << "KMIN_MAP\t\t\t\t";
        for (int i = 0; i < n_k; i++) {
            uint64_t rate;
            uint32_t k;
            conf >> rate >> k;
            rate2kmin[rate] = k;
            cerr << ' ' << rate << ' ' << k;
        }
        cerr << '\n';
    } else if (key.compare("PMAX_MAP") == 0) {
        int n_k;
        conf >> n_k;
        cerr << "PMAX_MAP\t\t\t\t";
        for (int i = 0; i < n_k; i++) {
            uint64_t rate;
            double p;
            conf >> rate >> p;
            rate2pmax[rate] = p;
            cerr << ' ' << rate << ' ' << p;
        }
        cerr << '\n';
    } else if (key.compare("BUFFER_SIZE") == 0) {
        conf >> buffer_size;
        cerr << "BUFFER_SIZE\t\t\t\t" << buffer_size << '\n';
    } else if (key.compare("QLEN_MON_FILE") == 0) {
        conf >> qlen_mon_file;
        cerr << "QLEN_MON_FILE\t\t\t\t" << qlen_mon_file << '\n';
    } else if (key.compare("VOQ_MON_FILE") == 0) {
        conf >> voq_mon_file;
        cerr << "VOQ_MON_FILE\t\t\t\t" << voq_mon_file << '\n';
    } else if (key.compare("VOQ_MON_DETAIL_FILE") == 0) {
        conf >> voq_mon_detail_file;
        cerr << "VOQ_MON_DETAIL_FILE\t\t\t\t" << voq_mon_detail_file << '\n';
    } else if (key.compare("UPLINK_MON_FILE") == 0) {
        conf >> uplink_mon_file;
        cerr << "UPLINK_MON_FILE\t\t\t\t" << uplink_mon_file << '\n';
    } else if (key.compare("CONN_MON_FILE") == 0) {
        conf >> conn_mon_file;
        cerr << "CONN_MON_FILE\t\t\t\t" << conn_mon_file << '\n';
    } else if (key.compare("QLEN_MON_START") == 0) {
        conf >> qlen_mon_start;
        cerr << "QLEN_MON_START\t\t\t\t" << qlen_mon_start << '\n';
    } else if (key.compare("QLEN_MON_END") == 0) {
        conf >> qlen_mon_end;
        cerr << "QLEN_MON_END\t\t\t\t" << qlen_mon_end << '\n';
    } else if (key.compare("MULTI_RATE") == 0) {
        int v;
        conf >> v;
        multi_rate = v;
        cerr << "MULTI_RATE\t\t\t\t" << multi_rate << '\n';
    } else if (key.compare("SAMPLE_FEEDBACK") == 0) {
        int v;
        conf >> v;
        sample_feedback = v;
        cerr << "SAMPLE_FEEDBACK\t\t\t\t" << sample_feedback << '\n';
    } else if (key.compare("LOAD") == 0) {
        double v;
        conf >> v;
        load = v;
        cerr << "LOAD\t\t\t" << load << "\n";
    } else if (key.compare("ENABLE_IRN") == 0) {
        bool v;
        conf >> v;
        enable_irn = v;
        cerr << "ENABLE_IRN\t\t" << enable_irn << "\n";
    } else if (key.compare("RANDOM_SEED") == 0) {
        int v;
        conf >> v;
        random_seed = v;
        cerr << "RANDOM_SEED\t\t\t" << random_seed << "\n";
    }
    fflush(stdout);
    }
    conf.close(); 
    /******************* READING CONFIG FILE IS DONE ***********************/

    // 启用NS-3日志
    LogComponentEnable("GENERATED_SIMULATION", LOG_LEVEL_DEBUG);

    // 设置随机种子,仿真过程产生相同的随机数
    NS_LOG_INFO("Initialize random seed: " << random_seed);
    srand((unsigned)random_seed);
    // SeedManager::SetSeed(random_seed);
    RngSeedManager::SetSeed (1);
    RngSeedManager::SetRun (simSeed);

    /**
     * PFC/QCN设置 
     * 配置 NS-3 中 QbbNetDevice（一个用于模拟高带宽低延迟网络设备的类）相关的参数
     * 特别是与 PFC（优先级流量控制）和 QCN（量化拥塞通知）有关的设置
     */
    bool dynamicth = use_dynamic_pfc_threshold;
    Config::SetDefault("ns3::QbbNetDevice::PauseTime", UintegerValue(pause_time));
    Config::SetDefault("ns3::QbbNetDevice::QcnEnabled", BooleanValue(enable_qcn));
    Config::SetDefault("ns3::QbbNetDevice::DynamicThreshold", BooleanValue(dynamicth));
    Config::SetDefault("ns3::QbbNetDevice::QbbEnabled", BooleanValue(enable_pfc));

    if(cc_model !=1 && lb_mode == 9){
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
    if(cc_model == 7){
        IntHeader::mode = 1;    //timely use ts
    }else if(cc_model == 3){
        IntHeader::mode = 0;    //hpcc use int
    }else{
        IntHeader::mode = 5;    //others no extra header
    }   
    

    /**
     * 读取网络拓扑文件，流量文件 
     */
    topof.open(topology_file.c_str());
    flowf.open(flow_file.c_str());
    topof >> node_num >> switch_num >> link_num;
    flowf >> flow_num;
    cout << "读取文件拓扑和流量" << "Node num: " << node_num << ", Switch num: " << switch_num << ", Link num: " << link_num
              << ", Flow num: " << flow_num << endl;


    /**  参数设置  **/
    Settings::node_num = node_num;
    Settings::host_num = node_num - switch_num;
    Settings::switch_num = switch_num;
    Settings::lb_mode = lb_mode;
    Settings::packet_payload = packet_payload_size;
    // Settings::MTU = packet_payload_size + 48;    //最大传输单元


    /******************* CREATE NODES ***********************/
    // 创建节点   根据网络拓扑创建节点  交换机节点：node_type：1，主机节点：node_type:0
    vector<uint32_t> node_type(node_num, 0);
    for(uint32_t i = 0; i < switch_num; i++){
        uint32_t sid;
        topof >> sid;
        node_type[sid] = 1;
        // cout << "创建节点" << node_type[sid] << sid << endl;
    }
    for(uint32_t i = 0; i < node_num; i++){
        if(node_type[i] == 0){
            n.Add(CreateObject<Node>());
            // cout << "创建节点------" << i << n.Get(i) << endl;
        }else{
            Ptr<SwitchNode> sw = CreateObject<SwitchNode>();
            n.Add(sw);
            sw->SetAttribute("EcnEnabled", BooleanValue(enable_qcn));
            // cout << "交换机节点-------" << i << n.Get(i) << endl;   
        }
    }
    NS_LOG_INFO("Create nodes.");


    /******************* INTERNET & IP ADRESS ***********************/
    // 协议安装 分配IP地址   aggregate ipv4, ipv6, udp, tcp, etc
    InternetStackHelper internet;
    internet.Install(n);   

    // 为节点分配IP地址
    for(uint32_t i = 0; i < node_num; i++){
        if(n.Get(i)->GetNodeType() == 0){
            serverAddress.resize(i + 1);
            serverAddress[i] = Settings::node_id_to_ip(i);
            // cout << "分配IP" << serverAddress[i] << endl;
        }
    }
    NS_LOG_INFO("Create channels.");


    // 显式创建拓扑所需的链路错误模型
    Ptr<RateErrorModel> rem = CreateObject<RateErrorModel>();
    Ptr<UniformRandomVariable> uv = CreateObject<UniformRandomVariable>();
    rem->SetRandomVariable(uv);
    uv->SetStream(50);
    // cout << "创建链路错误模型-------" << rem << uv << endl;
    rem->SetAttribute("ErrorRate", DoubleValue(error_rate_per_link));
    rem->SetAttribute("ErrorUnit", StringValue("ERROR_UNIT_PACKET"));

    pfc_file = fopen(pfc_output_file.c_str(), "w");

    QbbHelper qbb;
    Ipv4AddressHelper ipv4;
    // vector<pair<uint32_t, uint32_t>> link_pairs;   //链路对:src, dst
    for(uint32_t i = 0; i < link_num; i++){
        uint32_t src, dst;
        string data_rate, link_delay;
        double error_rate;
        topof >> src >> dst >> data_rate >> link_delay >> error_rate;
        // 断言链路的延迟为假定的单跳延迟
        assert(to_string(one_hop_delay) + "ns" == link_delay);
        // 将链路的源节点和目标节点加入链路对列表
        link_pairs.push_back(make_pair(src, dst));   
        Ptr<Node> snode = n.Get(src);
        Ptr<Node> dnode = n.Get(dst);
        // cout << "创建链路----------" << snode << dnode << endl;
        // 设置链路属性
        qbb.SetDeviceAttribute("DataRate", StringValue(data_rate));
        qbb.SetChannelAttribute("Delay", StringValue(link_delay));

        if(error_rate > 0){
            Ptr<RateErrorModel> rem = CreateObject<RateErrorModel>();
            Ptr<UniformRandomVariable> uv = CreateObject<UniformRandomVariable>();
            rem->SetRandomVariable(uv);
            uv->SetStream(50);
            rem->SetAttribute("ErrorRate", DoubleValue(error_rate));
            rem->SetAttribute("ErrorUnit", StringValue("ERROR_UNIT_PACKET"));
            qbb.SetDeviceAttribute("ReceiveErrorModel", PointerValue(rem));
        }else{
            qbb.SetDeviceAttribute("ReceiveErrorModel", PointerValue(rem));
        }

        fflush(stdout);

        // 服务器IP地址分配
        NetDeviceContainer d = qbb.Install(snode, dnode);
        if(snode->GetNodeType() == 0){
            Ptr<Ipv4> ipv4 = snode->GetObject<Ipv4>();
            ipv4->AddInterface(d.Get(0));
            ipv4->AddAddress(1,Ipv4InterfaceAddress(serverAddress[src], Ipv4Mask(0xff000000)));
        }
        if(dnode->GetNodeType() == 0){
            Ptr<Ipv4> ipv4 = dnode->GetObject<Ipv4>();
            ipv4->AddInterface(d.Get(1));
            ipv4->AddAddress(1,Ipv4InterfaceAddress(serverAddress[dst], Ipv4Mask(0xff000000)));
        }

        // 通过拓扑创建图 获取节点之间的信息 src -> dst
        nbr2if[snode][dnode].idx = DynamicCast<QbbNetDevice>(d.Get(0))->GetIfIndex();
        nbr2if[snode][dnode].up = true;
        nbr2if[snode][dnode].delay =
            DynamicCast<QbbChannel>(DynamicCast<QbbNetDevice>(d.Get(0))->GetChannel())
                ->GetDelay()
                .GetTimeStep();
        nbr2if[snode][dnode].bw = DynamicCast<QbbNetDevice>(d.Get(0))->GetDataRate().GetBitRate();


        // dst -> src
        nbr2if[dnode][snode].idx = DynamicCast<QbbNetDevice>(d.Get(1))->GetIfIndex();
        nbr2if[dnode][snode].up = true;
        nbr2if[dnode][snode].delay =
            DynamicCast<QbbChannel>(DynamicCast<QbbNetDevice>(d.Get(1))->GetChannel())
                ->GetDelay()
                .GetTimeStep();
        nbr2if[dnode][snode].bw = DynamicCast<QbbNetDevice>(d.Get(1))->GetDataRate().GetBitRate();

        // 分配IP地址
        char ipstring[16];
        Ipv4Address x;
        sprintf(ipstring, "10.%d.%d.0", (i / 254) % 254 + 1, i % 254 + 1);
        // sprintf(ipstring, "10.%d.%d.0", i / 254 + 1 > 254 ? 254 : i / 254 + 1, i % 254 + 1 > 254 ? 254 : i % 254 + 1);
        ipv4.SetBase(ipstring, "255.255.255.0");
        ipv4.Assign(d);

        // setup PFC trace  设置PFC跟踪
        // NetDeviceContainer d = qbb.Install(snode, dnode);
        // cout << "拓扑建立链路对" << DynamicCast<QbbNetDevice>(d.Get(0)) << DynamicCast<QbbNetDevice>(d.Get(1)) << endl;  
        DynamicCast<QbbNetDevice>(d.Get(0))->TraceConnectWithoutContext(
            "QbbPfc", MakeBoundCallback(&get_pfc, pfc_file, DynamicCast<QbbNetDevice>(d.Get(0))));
        DynamicCast<QbbNetDevice>(d.Get(1))->TraceConnectWithoutContext(
            "QbbPfc", MakeBoundCallback(&get_pfc, pfc_file, DynamicCast<QbbNetDevice>(d.Get(1))));
    }
    cout << "(AVG) NIC RATE: " << get_nic_rate(n) << endl;
    // 链路对link_pairs信息———— src:pair.first  dst: pair.second   

    /* 获取 IP 地址 与 NodeId 的 对应关系 */
    Ipv4Address empty_ip;
    for (uint32_t i = 0; i < node_num; ++i) {
        if (n.Get(i)->GetNodeType() == 0) {  
            if (serverAddress[i].IsEqual(empty_ip)) { 
                printf("IP Address ERROR %d\n", i);     
                printf("size of serverAddress: %lu", serverAddress.size());
                NS_FATAL_ERROR("An end-host belongs to no link");
            }
        }
        Settings::hostId2IpMap[i] = serverAddress[i].Get();   // NodeID -> IP
        Settings::hostIp2IdMap[serverAddress[i].Get()] = i;   // IP -> NodeID
    }

    // 配置交换机
    for (uint32_t i = 0; i < node_num; i++) {
        if (n.Get(i)->GetNodeType() == 1) {  
            Ptr<SwitchNode> sw = DynamicCast<SwitchNode>(n.Get(i));
            for (uint32_t j = 1; j < sw->GetNDevices(); j++) {
                Ptr<QbbNetDevice> dev = DynamicCast<QbbNetDevice>(sw->GetDevice(j));
                // cout << "交换机配置-------" << dev << endl;
                // 设置ECN
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
                sw->m_mmu->ConfigEcn(j, rate2kmin[rate], rate2kmax[rate], rate2pmax[rate]);   //设置ECN参数
                // 设置PFC   headroom 处理数据包需要的额外空间
                uint64_t delay = DynamicCast<QbbChannel>(dev->GetChannel())->GetDelay().GetTimeStep();
                uint32_t headroom = rate * delay / 8 / 1000000000 * 2 + 2 * sw->m_mmu->MTU;
                sw->m_mmu->ConfigHdrm(j, headroom);
            }
            // 设置交换机 端口 交换机缓存大小 节点id
            sw->m_mmu->ConfigNPort(sw->GetNDevices() - 1);
            sw->m_mmu->ConfigBufferSize(buffer_size * 1024 *1024);  // default 0, specify in run.py!!
            sw->m_mmu->node_id = sw->GetId();
            // 修改代码：
            NS_LOG_INFO("Node " << i 
            << " : Broadcom switch (" 
            << (sw->GetNDevices() - 1) 
            << " ports / " 
            << (sw->m_mmu->GetMmuBufferBytes() / 1000000.) 
            << "MB MMU)");
            // NS_LOG_INFO("Node %u : Broadcom switch (%u ports / %gMB MMU)\n" %
            //             (i, sw->GetNDevices() - 1, sw->m_mmu->GetMmuBufferBytes() / 1000000.));
        }
    }

    // FCT 输出文件
    fct_output = fopen(fct_output_file.c_str(), "w");
    printf("写入文件flow_input_file: %s\n", flow_input_file.c_str());
    flow_input_stream = fopen(flow_input_file.c_str(), "w");   
    if (cc_model == 1) {
        cout << "模式为 1 CNP output file: " << cnp_output_file << endl;
        cnp_output = fopen(cnp_output_file.c_str(), "w");
    }


    // 拓扑到BDP映射
    map<string, uint32_t> topo2bdpMap;
    topo2bdpMap[string("leaf_spine_128_100G_OS2")] = 104000;  // RTT=8320
    topo2bdpMap[string("fat_k8_100G_OS2")] = 156000;      // RTT=12480 --> all 100G links

    // 拓扑文件  是否找到匹配的拓扑  irn_bdp 存储
    bool found_topo2bdpMap = false;
    uint32_t irn_bdp_lookup = 0;
    for (auto pair : topo2bdpMap) {
        if (topology_file.find(pair.first) != string::npos) {  // if topology file string includes the word
            irn_bdp_lookup = pair.second;
            found_topo2bdpMap = true;
            break;
        }
    }
    if (found_topo2bdpMap == false) {
        cout << __FILE__ << "(" << __LINE__ << ")"
                  << " ERROR - topo2bdpMap has no matched item with " << topology_file << endl;
        assert(false);
    }

    // rdmaHw config  每个节点创建并配置RDMA硬件
    for (uint32_t i = 0; i < node_num; i++) {
        if (n.Get(i)->GetNodeType() == 0) {  // is server
            // 为主机节点 创建 RdmaHw  设置属性
            // cout << "Create RdmaHw for node " << i << endl;
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
            rdmaHw->SetAttribute("IrnRtoHigh", TimeValue(MicroSeconds(320)));  // 1930
            rdmaHw->SetAttribute("IrnRtoLow", TimeValue(MicroSeconds(100)));   // 454
            rdmaHw->SetAttribute("IrnBdp", UintegerValue(irn_bdp_lookup));

            // Monitoring CNP Marking frequency of DCQCN
            // if (cc_model == 1) {
            //     cout << "调度事件，时间: " << cnp_mon_start << "ns" << endl;
            //     cout << "当前仿真时间: " << Simulator::Now().GetNanoSeconds() << "ns" << endl;
            //     // 问题
            //     Simulator::Schedule(NanoSeconds(cnp_mon_start), &cnp_freq_mointoring, cnp_output,
            //                         rdmaHw);
            //     // Simulator::Schedule(NanoSeconds(0), &test);
            //     cout << "事件调度已完成。" << Simulator::Now().GetNanoSeconds() << "ns" << endl;
            // }

            // 创建 并 安装 RDNMA驱动 RdmaDriver
            Ptr<RdmaDriver> rdma = CreateObject<RdmaDriver>();
            Ptr<Node> node = n.Get(i);
            rdma->SetNode(node);
            rdma->SetRdmaHw(rdmaHw);

            node->AggregateObject(rdma);
            rdma->Init();
            // cout << "RdmaHw for node----- " << rdma << node << rdmaHw << endl;
            rdma->TraceConnectWithoutContext("QpComplete",
                                             MakeBoundCallback(qp_finish, fct_output));
        }
    }

    /**
     *switch"（交换机）的节点设置其 CcMode 和 ACK 优先级属性
     * 检查交换机指针是否为空
     */
    for (uint32_t i = 0; i < node_num; i++) {
        if (n.Get(i)->GetNodeType() == 1) {  // switch
            Ptr<SwitchNode> sw = DynamicCast<SwitchNode>(n.Get(i));
            sw->SetAttribute("CcMode", UintegerValue(cc_model));
            sw->SetAttribute("AckHighPrio", UintegerValue(1));
            // cout << "交换机设置属性--------" << sw << endl;
        }
    }

    
    // 计算路由并设置路由表
    CalculateRoutes(n);
    CalculateAllPaths(n);
    FindMaxPathCount();
    SetRoutingEntries();

    /**
     * 交换机的配置
     * 获取BDP 和 时延
     */
    maxRtt = maxBdp = 0;
    fprintf(stderr, "node_num=%d\n", node_num);
    for (uint32_t i = 0; i < node_num; i++) {
        if (n.Get(i)->GetNodeType() != 0) continue;
        for (uint32_t j = i + 1; j < node_num; j++) {
            if (n.Get(j)->GetNodeType() != 0) continue;
            uint64_t delay = pairDelay[n.Get(i)][n.Get(j)];
            uint64_t txDelay = pairTxDelay[n.Get(i)][n.Get(j)];
            uint64_t rtt = delay * 2 + txDelay;
            uint64_t bw = pairBw[n.Get(i)][n.Get(j)];
            uint64_t bdp = rtt * bw / 1000000000 / 8;
            pairBdp[n.Get(i)][n.Get(j)] = bdp;
            pairBdp[n.Get(j)][n.Get(i)] = bdp;
            pairRtt[n.Get(i)][n.Get(j)] = rtt;
            pairRtt[n.Get(j)][n.Get(i)] = rtt;

            if (bdp > maxBdp) maxBdp = bdp;
            if (rtt > maxRtt) maxRtt = rtt;
        }
    }
    fprintf(stderr, "maxRtt: %lu, maxBdp: %lu\n", maxRtt, maxBdp);
    assert(maxBdp == irn_bdp_lookup);    // 校验最大 BDP 是否等于预期值
    // cout << "Configuring switches" << endl;

    /* 配置TOR交换机 */
    printf("link_pairs.size() = %lu\n", link_pairs.size());
    for (auto &pair : link_pairs) {
        Ptr<Node> probably_host = n.Get(pair.first);
        Ptr<Node> probably_switch = n.Get(pair.second);
        // cout << "Configuring switches-----" << probably_host << probably_switch << endl;
        if (probably_host->GetNodeType() == 0 && probably_switch->GetNodeType() == 1) {
            Ptr<SwitchNode> sw = DynamicCast<SwitchNode>(probably_switch);
            // cout << "Configuring ToR switch----- " << sw << endl;
            sw->m_isToR = true;
            uint32_t hostIP = serverAddress[pair.first].Get();
            sw->m_isToR_hostIP.insert(hostIP);
            if (idxNodeToR.find(sw->GetId()) == idxNodeToR.end()) {
                idxNodeToR[sw->GetId()] = sw;
            };
        }
    }

    /* 配置 交换机负载均衡 使用 ToR-to-ToR 路由 */
    printf("lb_mode = %d\n", lb_mode);
    if (lb_mode == 3 || lb_mode == 6 || lb_mode == 9) {  
        NS_LOG_INFO("Configuring Load Balancer's Switches");
        for (auto &pair : link_pairs) {
            Ptr<Node> probably_host = n.Get(pair.first);
            Ptr<Node> probably_switch = n.Get(pair.second);
            // cout << "Configuring Load Balancer's Switches-----" << probably_host << probably_switch << endl;
            // 主机——交换机 链接
            if (probably_host->GetNodeType() == 0 && probably_switch->GetNodeType() == 1) {
                Ptr<SwitchNode> sw = DynamicCast<SwitchNode>(probably_switch);
                uint32_t hostIP = serverAddress[pair.first].Get();
                Settings::hostIp2SwitchId[hostIP] = sw->GetId();  // hostIP -> connected switch's ID
            }
        }

        // Conga: m_congaFromLeafTable, m_congaToLeafTable, m_congaRoutingTable
        // Letflow: m_letflowRoutingTable
        // Conweave: m_ConWeaveRoutingTable, m_rxToRId2BaseRTT
        for (auto i = nextHop.begin(); i != nextHop.end(); i++) {  
            if (i->first->GetNodeType() == 1) {                    
                Ptr<Node> nodeSrc = i->first;
                Ptr<SwitchNode> swSrc = DynamicCast<SwitchNode>(nodeSrc);  // switch
                uint32_t swSrcId = swSrc->GetId();
                // cout << "Configuring Load Balancer's Switches-----" << nodeSrc << swSrc << swSrcId << endl;
                if (swSrc->m_isToR) {
                    // printf("--- ToR Switch %d\n", swSrcId);
                    auto table1 = i->second;
                    for (auto j = table1.begin(); j != table1.end(); j++) {
                        Ptr<Node> dst = j->first;  // dst
                        uint32_t dstIP = Settings::hostId2IpMap[dst->GetId()];
                        uint32_t swDstId = Settings::hostIp2SwitchId[dstIP];  // Rx(dst)ToR

                        if (swSrcId == swDstId) {
                            continue;  // if in the same pod, then skip
                        }

                        if (lb_mode == 3) {
                            // initialize `m_congaFromLeafTable` and `m_congaToLeafTable`
                            swSrc->m_mmu->m_congaRouting.m_congaFromLeafTable[swDstId];  // dynamically will be added in
                            swSrc->m_mmu->m_congaRouting.m_congaToLeafTable[swDstId];
                        }

                        // construct paths
                        uint32_t pathId;
                        uint8_t path_ports[4] = {0, 0, 0, 0};  // interface is always large than 0
                        vector<Ptr<Node>> nexts1 = j->second;
                        for (auto next1 : nexts1) {
                            uint32_t outPort1 = nbr2if[nodeSrc][next1].idx;
                            auto nexts2 = nextHop[next1][dst];
                            if (nexts2.size() == 1 && nexts2[0]->GetId() == swDstId) {
                                uint32_t outPort2 = nbr2if[next1][nexts2[0]].idx;
                                // printf("[IntraPod-2hop] %d (%d)-> %d (%d) -> %d -> %d\n",
                                // nodeSrc->GetId(), outPort1, next1->GetId(), outPort2,
                                // nexts2[0]->GetId(), dst->GetId());
                                path_ports[0] = (uint8_t)outPort1;
                                path_ports[1] = (uint8_t)outPort2;
                                pathId = *((uint32_t *)path_ports);
                                if (lb_mode == 3) {
                                    swSrc->m_mmu->m_congaRouting.m_congaRoutingTable[swDstId]
                                        .insert(pathId);
                                }
                                if (lb_mode == 6) {
                                    swSrc->m_mmu->m_letflowRouting.m_letflowRoutingTable[swDstId]
                                        .insert(pathId);
                                }
                                if (lb_mode == 9) {
                                    swSrc->m_mmu->m_conweaveRouting.m_ConWeaveRoutingTable[swDstId]
                                        .insert(pathId);
                                    swSrc->m_mmu->m_conweaveRouting.m_rxToRId2BaseRTT[swDstId] =
                                        one_hop_delay * 4;
                                }
                                continue;
                            }

                            for (auto next2 : nexts2) {
                                uint32_t outPort2 = nbr2if[next1][next2].idx;
                                auto nexts3 = nextHop[next2][dst];
                                if (nexts3.size() == 1 && nexts3[0]->GetId() == swDstId) {
                                    // this destination has 3-hop distance
                                    uint32_t outPort3 = nbr2if[next2][nexts3[0]].idx;
                                    // printf("[IntraPod-3hop] %d (%d)-> %d (%d) -> %d (%d) -> %d ->
                                    // %d\n", nodeSrc->GetId(), outPort1, next1->GetId(), outPort2,
                                    // next2->GetId(), outPort3, nexts3[0]->GetId(), dst->GetId());
                                    path_ports[0] = (uint8_t)outPort1;
                                    path_ports[1] = (uint8_t)outPort2;
                                    path_ports[2] = (uint8_t)outPort3;
                                    pathId = *((uint32_t *)path_ports);
                                    if (lb_mode == 3) {
                                        swSrc->m_mmu->m_congaRouting.m_congaRoutingTable[swDstId]
                                            .insert(pathId);
                                    }
                                    if (lb_mode == 6) {
                                        swSrc->m_mmu->m_letflowRouting
                                            .m_letflowRoutingTable[swDstId]
                                            .insert(pathId);
                                    }
                                    if (lb_mode == 9) {
                                        swSrc->m_mmu->m_conweaveRouting
                                            .m_ConWeaveRoutingTable[swDstId]
                                            .insert(pathId);
                                        swSrc->m_mmu->m_conweaveRouting.m_rxToRId2BaseRTT[swDstId] =
                                            one_hop_delay * 6;
                                    }
                                    continue;
                                }

                                for (auto next3 : nexts3) {
                                    uint32_t outPort3 = nbr2if[next2][next3].idx;
                                    auto nexts4 = nextHop[next3][dst];
                                    if (nexts4.size() == 1 && nexts4[0]->GetId() == swDstId) {
                                        // this destination has 4-hop distance
                                        uint32_t outPort4 = nbr2if[next3][nexts4[0]].idx;
                                        // printf("[IntraPod-4hop] %d (%d)-> %d (%d) -> %d (%d) ->
                                        // %d (%d) -> %d -> %d\n", nodeSrc->GetId(), outPort1,
                                        // next1->GetId(), outPort2, next2->GetId(), outPort3,
                                        // next3->GetId(), outPort4, nexts4[0]->GetId(),
                                        // dst->GetId());
                                        path_ports[0] = (uint8_t)outPort1;
                                        path_ports[1] = (uint8_t)outPort2;
                                        path_ports[2] = (uint8_t)outPort3;
                                        path_ports[3] = (uint8_t)outPort4;
                                        pathId = *((uint32_t *)path_ports);
                                        if (lb_mode == 3) {
                                            swSrc->m_mmu->m_congaRouting
                                                .m_congaRoutingTable[swDstId]
                                                .insert(pathId);
                                        }
                                        if (lb_mode == 6) {
                                            swSrc->m_mmu->m_letflowRouting
                                                .m_letflowRoutingTable[swDstId]
                                                .insert(pathId);
                                        }
                                        if (lb_mode == 9) {
                                            swSrc->m_mmu->m_conweaveRouting
                                                .m_ConWeaveRoutingTable[swDstId]
                                                .insert(pathId);
                                            swSrc->m_mmu->m_conweaveRouting
                                                .m_rxToRId2BaseRTT[swDstId] = one_hop_delay * 8;
                                        }
                                        continue;
                                    } else {
                                        printf("Too large topology?\n");
                                        assert(false);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        // Conga 路由方案中的链路带宽设置
        for (auto i = nextHop.begin(); i != nextHop.end(); i++) { 
            if (i->first->GetNodeType() == 1) {                   
                Ptr<Node> node = i->first;
                Ptr<SwitchNode> sw = DynamicCast<SwitchNode>(node);  // switch
                // 修改代码：注释掉未使用的变量
                // uint32_t swId = sw->GetId();
                auto table = i->second;
                for (auto j = table.begin(); j != table.end(); j++) {
                    Ptr<Node> dst = j->first;  // dst
                    // 修改代码：注释掉未使用的变量
                    // uint32_t dstIP = Settings::hostId2IpMap[dst->GetId()];
                    // 修改代码：注释掉未使用的变量
                    // uint32_t swDstId = Settings::hostIp2SwitchId[dstIP];
                    for (auto next : j->second) {
                        uint32_t outPort = nbr2if[node][next].idx;
                        uint64_t bw = nbr2if[node][next].bw;
                        sw->m_mmu->m_congaRouting.SetLinkCapacity(outPort, bw);
                        // printf("Node: %d, interface: %d, bw: %lu\n", swId, outPort, bw);
                    }
                }
            }
        }

        // 设置常量 和 交换信息
        for (auto i = nextHop.begin(); i != nextHop.end(); i++) {  
            if (i->first->GetNodeType() == 1) {
                Ptr<Node> node = i->first;
                Ptr<SwitchNode> sw = DynamicCast<SwitchNode>(node); 
                // 修改代码
                NS_LOG_INFO("Switch Info - ID:" << sw->GetId() << ", ToR:" << sw->m_isToR);
                // NS_LOG_INFO("Switch Info - ID:%u, ToR:%d\n" % (sw->GetId(), sw->m_isToR));
                if (lb_mode == 3) {
                    sw->m_mmu->m_congaRouting.SetConstants(conga_dreTime, conga_agingTime,
                                                           conga_flowletTimeout, conga_quantizeBit,
                                                           conga_alpha);
                    sw->m_mmu->m_congaRouting.SetSwitchInfo(sw->m_isToR, sw->GetId());
                }
                if (lb_mode == 6) {
                    sw->m_mmu->m_letflowRouting.SetConstants(letflow_agingTime, letflow_flowletTimeout);
                    sw->m_mmu->m_letflowRouting.SetSwitchInfo(sw->m_isToR, sw->GetId());
                }
                if (lb_mode == 9) {
                    sw->m_mmu->m_conweaveRouting.SetConstants(
                        conweave_extraReplyDeadline, conweave_extraVOQFlushTime,
                        conweave_txExpiryTime, conweave_defaultVOQWaitingTime,
                        conweave_pathPauseTime, conweave_pathAwareRerouting);
                    sw->m_mmu->m_conweaveRouting.SetSwitchInfo(sw->m_isToR, sw->GetId());
                }
            }
        }

        // 负载均衡输出
        if (lb_mode == 3) {  
            Simulator::Schedule(Seconds(flowgen_stop_time + simulator_extra_time),
                                conga_history_print);
        }
        if (lb_mode == 6) {  
            Simulator::Schedule(Seconds(flowgen_stop_time + simulator_extra_time),
                                letflow_history_print);
        }
        if (lb_mode == 9) { 
            Simulator::Schedule(Seconds(flowgen_stop_time + simulator_extra_time),
                                conweave_history_print);
        }
    }

    // 填充路由表 (although we use our custom impl in switch_node.cc)
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();
    printf("全局路由配置:Routing tables populated\n");
    // maintain port number for each host
    for (uint32_t i = 0; i < node_num; i++) {
        if (n.Get(i)->GetNodeType() == 0) {
            portNumber[i] = 10000;  // each host use port number from 10000
            dportNumber[i] = 100;
        }
    }

    flow_input.idx = 0;
    port_per_host = new uint16_t[node_num - switch_num];
    if (flow_num > 0) {
        ReadFlowInput();  // read flow input
        cout << "ReadFlowInput---------------------" << endl;
        std::cout << "Current simulation time: " << Simulator::Now().GetSeconds() << " seconds" << std::endl;

        cout << Simulator::Now() << Seconds(0) << "&ScheduleFlowInput: " << reinterpret_cast<void*>(&ScheduleFlowInputs) << " " << flow_input_stream << endl;
        Simulator::Schedule(Seconds(0), &ScheduleFlowInputs, flow_input_stream);
        cout << "ScheduleFlowInputs 结束" << endl;
    }
    topof.close();

     // schedule link down
    if (link_down_time > 0) {
        cout << "link_down_time-----------------" << endl;
        Simulator::Schedule(Seconds(flowgen_start_time) + MicroSeconds(link_down_time),
                            &TakeDownLink, n, n.Get(link_down_A), n.Get(link_down_B));
    }

    if (lb_mode == 9) {
        voq_output = fopen(voq_mon_file.c_str(), "w");                // specific to ConWeave
        voq_detail_output = fopen(voq_mon_detail_file.c_str(), "w");  // specific to ConWeave
    }

    uplink_output = fopen(uplink_mon_file.c_str(), "w");  
    conn_output = fopen(conn_mon_file.c_str(), "w");      

    // 更新 ToR 的上行和下行链路 端口映射
    for (size_t ToRId = 0; ToRId < Settings::node_num; ToRId++) {
        Ptr<Node> node = n.Get(ToRId);
        if (node->GetNodeType() == 1) {  
            auto swNode = DynamicCast<SwitchNode>(n.Get(ToRId));
            if (swNode->m_isToR) {  
                for (auto &nextNodeIf : nbr2if[node]) {
                    if (nextNodeIf.first->GetNodeType() == 1) {  
                        auto &vec = torId2UplinkIf[ToRId];
                        vec.push_back(
                            nextNodeIf.second.idx);  // record this uplink port (outDev index)
                        // printf("Sw %lu - uplink port %u\n", ToRId, nextNodeIf.second.idx);  //
                        // debugging
                    } else {
                        auto &vec = torId2DownlinkIf[ToRId];
                        vec.push_back(
                            nextNodeIf.second.idx);  // record this downlink port (outDev index)
                        // printf("Sw %lu - downlink port %u\n", ToRId, nextNodeIf.second.idx);  //
                        // debugging
                    }
                }
            }
        }
    }

    LoadFlows(flow_file);
    // Opengym Env 环境接口
    Ptr<OpenGymInterface> openGymInterface = CreateObject<OpenGymInterface> (openGymPort);
    openGymInterface->SetGetActionSpaceCb( MakeCallback (&MyGetActionSpace) );
    openGymInterface->SetGetObservationSpaceCb( MakeCallback (&MyGetObservationSpace) );
    openGymInterface->SetGetGameOverCb( MakeCallback (&MyGetGameOver) );
    openGymInterface->SetGetObservationCb( MakeCallback (&MyGetObservation) );
    openGymInterface->SetGetRewardCb( MakeCallback (&MyGetReward) );
    // openGymInterface->SetGetExtraInfoCb( MakeCallback (&MyGetExtraInfo) );
    openGymInterface->SetExecuteActionsCb( MakeCallback (&MyExecuteActions) );

    Simulator::Schedule (Seconds(flowgen_start_time), &ScheduleNextStateRead, envStepTime, openGymInterface);
    cout << "Simulator::Schedule-----------------" << endl;
    Simulator::Schedule(Seconds(flowgen_start_time), &periodic_monitoring, voq_output,
                        voq_detail_output, uplink_output, conn_output, &lb_mode);

    // 开始仿真
    // cout << "------------------------------------------" << endl;
    cout << "Running Simulation.\n";
    fflush(stdout);
    NS_LOG_INFO("Run Simulation.");
    // Simulator::Schedule(Seconds(100000),
    //                     &stop_simulation_middle);  // check every 100us
    Simulator::Stop(Seconds(flowgen_stop_time + 10.0));

    cout << "当前仿真Run时间: " << Simulator::Now().GetNanoSeconds() << "ns" << endl;
    Simulator::Run();
    cout << "当前仿真Run时间: " << Simulator::Now().GetNanoSeconds() << "ns" << endl;
    cout << "当前仿真Destroy前时间: " << Simulator::Now().GetNanoSeconds() << "ns" << endl;
    openGymInterface->NotifySimulationEnd();
    Simulator::Destroy();
    cout << "当前仿真Destroy后时间: " << Simulator::Now().GetNanoSeconds() << "ns" << endl;
}

