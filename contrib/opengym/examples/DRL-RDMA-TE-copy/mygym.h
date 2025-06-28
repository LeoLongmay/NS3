#ifndef MY_GYM_ENTITY_H
#define MY_GYM_ENTITY_H
 
#include "ns3/stats-module.h"
#include "ns3/opengym-module.h"
#include "ns3/rdma-client-helper.h"
#include "ns3/rdma-client.h"
#include "ns3/qbb-net-device.h"
#include "ns3/rdma-driver.h"
#include "ns3/rdma-hw.h"
#include "ns3/rdma-queue-pair.h"

 
namespace ns3 {
 
class Node;
// class WifiMacQueue;

class RdmaCliebtHelper;
class RdmaClient;
class RdmaEgressQueue;
class RdmaDriver;
class RdmaHw;
class RdmaQueuePair;
class RdmaRxQueuePair;
class RdmaQueuePairGroup;
class Packet;
 
class MyGymEnv : public OpenGymEnv
{
public:
  MyGymEnv ();
  MyGymEnv (Time stepTime);
  virtual ~MyGymEnv ();
  static TypeId GetTypeId (void);
  virtual void DoDispose ();
 
  Ptr<OpenGymSpace> GetActionSpace();
  Ptr<OpenGymSpace> GetObservationSpace();
  bool GetGameOver();
  Ptr<OpenGymDataContainer> GetObservation();
  float GetReward();
  std::string GetExtraInfo();
  bool ExecuteActions(Ptr<OpenGymDataContainer> action);
 
  // the function has to be static to work with MakeBoundCallback
  // that is why we pass pointer to MyGymEnv instance to be able to store the context (node, etc)
  static void NotifyPktRxEvent(Ptr<MyGymEnv> entity, Ptr<Node> node, Ptr<const Packet> packet);
  static void CountRxPkts(Ptr<MyGymEnv> entity, Ptr<Node> node, Ptr<const Packet> packet);
 
private:
  void ScheduleNextStateRead();

  Ptr<WifiMacQueue> GetQueue(Ptr<Node> node);
  bool SetCw(Ptr<Node> node, uint32_t cwMinValue=0, uint32_t cwMaxValue=0);
 
  Time m_interval = Seconds(0.1);
  // 定义函数
  
  Ptr<Node> m_currentNode;
  uint64_t m_rxPktNum;
 
};
 
}
 
 
#endif // MY_GYM_ENTITY_H
 