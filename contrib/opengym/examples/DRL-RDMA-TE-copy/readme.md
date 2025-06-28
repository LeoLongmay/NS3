# DRL代码环境构建
## 代码
- 首先是 Ptr<OpenGymSpace> GetObservationSpace(); 定义 observation 空间
    ```c
    Ptr<OpenGymSpace> MyGetObservationSpace(void){
        uint32_t nodeNum = 5;
        float low = 0.0;
        float high = 10.0;
        std::vector<uint32_t> shape = {nodeNum,};
        std::string dtype = TypeNameGet<uint32_t> ();
        Ptr<OpenGymBoxSpace> space = CreateObject<OpenGymBoxSpace> (low, high, shape, dtype);
        NS_LOG_UNCOND ("MyGetObservationSpace: " << space);
        return space;
    }
   ```
-  Ptr<OpenGymSpace> GetActionSpace();; 定义 Action 空间
   ```c
    Ptr<OpenGymSpace> MyGetActionSpace(void){
        uint32_t nodeNum = 5;

        Ptr<OpenGymDiscreteSpace> space = CreateObject<OpenGymDiscreteSpace> (nodeNum);
        NS_LOG_UNCOND ("MyGetActionSpace: " << space);
        return space;
    }
   ```
- Ptr<OpenGymDataContainer> GetObservation(); 收集 Observation
    ```c
   Ptr<OpenGymDataContainer> MyGetObservation(void){
        uint32_t nodeNum = 5;
        uint32_t low = 0.0;
        uint32_t high = 10.0;
        Ptr<UniformRandomVariable> rngInt = CreateObject<UniformRandomVariable> ();

        std::vector<uint32_t> shape = {nodeNum,};
        Ptr<OpenGymBoxContainer<uint32_t> > box = CreateObject<OpenGymBoxContainer<uint32_t> >(shape);

        // generate random data
        for (uint32_t i = 0; i<nodeNum; i++){
            uint32_t value = rngInt->GetInteger(low, high);
            box->AddValue(value);
        }

        NS_LOG_UNCOND ("MyGetObservation: " << box);
        return box;
    }
   ```
- float GetReward();  定义Reward
    ```c
   float MyGetReward(void){
        static float reward = 0.0;
        reward += 1;
        return reward;
    }
   ```
- bool GetGameOver(); 是否结束的条件
    ```c
   bool MyGetGameOver(void){
        bool isGameOver = false;
        bool test = false;
        static float stepCounter = 0.0;
        stepCounter += 1;
        if (stepCounter == 10 && test) {
            isGameOver = true;
        }
        NS_LOG_UNCOND ("MyGetGameOver: " << isGameOver);
        return isGameOver;
    }
   ```
- std::string GetExtraInfo();  定义额外的info 
    ```c
   std::string MyGetExtraInfo(void){
        std::string myInfo = "testInfo";
        myInfo += "|123";
        NS_LOG_UNCOND("MyGetExtraInfo: " << myInfo);
        return myInfo;
    }
   ```
- bool ExecuteActions(Ptr<OpenGymDataContainer> action); 执行接收到的动作
    ```c
   bool MyExecuteActions(Ptr<OpenGymDataContainer> action){
        Ptr<OpenGymDiscreteContainer> discrete = DynamicCast<OpenGymDiscreteContainer>(action);
        NS_LOG_UNCOND ("MyExecuteActions: " << action);
        return true;
    }
   ```
-  ScheduleNextStateRead 调度下一个状态读取
    ```c
    void ScheduleNextStateRead(double envStepTime, Ptr<OpenGymInterface> openGym){
        Simulator::Schedule (Seconds(envStepTime), &ScheduleNextStateRead, envStepTime, openGym);
        openGym->NotifyCurrentState();
    }
    ```
- 主函数
    ```c
    int main (int argc, char *argv[]){
        // Parameters of the scenario
        uint32_t simSeed = 1;
        double simulationTime = 1; //seconds
        double envStepTime = 0.1; //seconds, ns3gym env step time interval
        uint32_t openGymPort = 5555;
        uint32_t testArg = 0;

        CommandLine cmd;
        // required parameters for OpenGym interface
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
        NS_LOG_UNCOND("--testArg: " << testArg);

        RngSeedManager::SetSeed (1);
        RngSeedManager::SetRun (simSeed);

        // OpenGym Env 下面定义的各个空间函数属性不能改，这个应该是定义在如下的接口回调函数里了，要实现什么功能需要在对应的函数功能内部进行改动
        Ptr<OpenGymInterface> openGym = CreateObject<OpenGymInterface> (openGymPort);
        openGym->SetGetActionSpaceCb( MakeCallback (&MyGetActionSpace) );
        openGym->SetGetObservationSpaceCb( MakeCallback (&MyGetObservationSpace) );
        openGym->SetGetGameOverCb( MakeCallback (&MyGetGameOver) );
        openGym->SetGetObservationCb( MakeCallback (&MyGetObservation) );
        openGym->SetGetRewardCb( MakeCallback (&MyGetReward) );
        openGym->SetGetExtraInfoCb( MakeCallback (&MyGetExtraInfo) );
        openGym->SetExecuteActionsCb( MakeCallback (&MyExecuteActions) );
        Simulator::Schedule (Seconds(0.0), &ScheduleNextStateRead, envStepTime, openGym);

        // 使用了OpenGymInetrface进行替换
        Ptr<OpenGymInterface> openGymInterface = CreateObject<OpenGymInterface> (openGymPort);
        Ptr<MyGymEnv> myGymEnv = CreateObject<MyGymEnv> (Seconds(envStepTime));
        myGymEnv->SetOpenGymInterface(openGymInterface);

        NS_LOG_UNCOND ("Simulation start");
        Simulator::Stop (Seconds (simulationTime));
        Simulator::Run ();
        NS_LOG_UNCOND ("Simulation stop");

        openGym->NotifySimulationEnd();
        Simulator::Destroy ();
    }
    ```
- 环境新建了文件的情况下，主函数内的gym函数构建，同时头文件导入 mygym.h
    ```c
    // OpenGym Env
    Ptr<OpenGymInterface> openGymInterface = CreateObject<OpenGymInterface> (openGymPort);
    Ptr<MyGymEnv> myGymEnv = CreateObject<MyGymEnv> (Seconds(envStepTime));
    myGymEnv->SetOpenGymInterface(openGymInterface);

    ```

## linear-mesh 文件代码概述
### 代码实现：
作为网络负载函数的控制信道访问概率值，在NS3中创建了一个由五个节点组成的线性拓扑，并设置了从最左侧到最右侧节点的饱和UDP数据流
- observation: 节点队列长度 queue lengths of each node
- actions:  频道接入概率
- reward：目的节点收到的数据包数量
- gameover: 仿真时间结束

### 代码内容
在mygym.cc中,具有以下关键步骤：
- 1.收集状态GetObservation：collect每个节点的队列长度存入box类型的状态空间
    ``` c
    Ptr<OpenGymDataContainer>
    MyGymEnv::GetObservation()
    {
    NS_LOG_FUNCTION (this);
    uint32_t nodeNum = NodeList::GetNNodes ();
    std::vector<uint32_t> shape = {nodeNum,};
    Ptr<OpenGymBoxContainer<uint32_t> > box = CreateObject<OpenGymBoxContainer<uint32_t> >(shape);

    for (NodeList::Iterator i = NodeList::Begin (); i != NodeList::End (); ++i) {
        Ptr<Node> node = *i;
        Ptr<WifiMacQueue> queue = GetQueue (node);
        uint32_t value = queue->GetNPackets();
        box->AddValue(value);
    }

    NS_LOG_UNCOND ("MyGetObservation: " << box);
    return box;
    }    
    ```
    - 收集节点队列长度的函数GetQueue
        ``` c
        Ptr<WifiMacQueue>
        MyGymEnv::GetQueue(Ptr<Node> node)
        {
        Ptr<NetDevice> dev = node->GetDevice (0);
        Ptr<WifiNetDevice> wifi_dev = DynamicCast<WifiNetDevice> (dev);
        Ptr<WifiMac> wifi_mac = wifi_dev->GetMac ();
        Ptr<RegularWifiMac> rmac = DynamicCast<RegularWifiMac> (wifi_mac);
        PointerValue ptr;
        rmac->GetAttribute ("Txop", ptr);
        Ptr<Txop> txop = ptr.Get<Txop> ();
        Ptr<WifiMacQueue> queue = txop->GetWifiMacQueue ();
        return queue;
        }
        ```
- 2.奖励：根据收包数确定
    ``` c
    float
    MyGymEnv::GetReward()
    {
    NS_LOG_FUNCTION (this);
    static float lastValue = 0.0;
    float reward = m_rxPktNum - lastValue;
    lastValue = m_rxPktNum;
    NS_LOG_UNCOND ("MyGetReward: " << reward);
    return reward;
    }
    ```
    - 收包数的统计
        ``` c
        void
        MyGymEnv::NotifyPktRxEvent(Ptr<MyGymEnv> entity, Ptr<Node> node, Ptr<const Packet> packet)
        {
        NS_LOG_DEBUG ("Client received a packet of " << packet->GetSize () << " bytes");
        entity->m_currentNode = node;
        entity->m_rxPktNum++;

        NS_LOG_UNCOND ("Node with ID " << entity->m_currentNode->GetId() << " received " << entity->m_rxPktNum << " packets");

        entity->Notify();
        }

        void
        MyGymEnv::CountRxPkts(Ptr<MyGymEnv> entity, Ptr<Node> node, Ptr<const Packet> packet)
        {
        NS_LOG_DEBUG ("Client received a packet of " << packet->GetSize () << " bytes");
        entity->m_currentNode = node;
        entity->m_rxPktNum++;
        }
        ```
- 3.动作：调整CWmin和CWmax，改变随机接入概率
    ``` c
    bool
    MyGymEnv::ExecuteActions(Ptr<OpenGymDataContainer> action)
    {
    NS_LOG_FUNCTION (this);
    NS_LOG_UNCOND ("MyExecuteActions: " << action);
    Ptr<OpenGymBoxContainer<uint32_t> > box = DynamicCast<OpenGymBoxContainer<uint32_t> >(action);
    std::vector<uint32_t> actionVector = box->GetData();

    uint32_t nodeNum = NodeList::GetNNodes ();
    for (uint32_t i=0; i<nodeNum; i++)
    {
        Ptr<Node> node = NodeList::GetNode(i);
        uint32_t cwSize = actionVector.at(i);
        SetCw(node, cwSize, cwSize);
    }

    return true;
    }
    ```
    - 调整Cw的函数SetCw：
        ```c
        bool
        MyGymEnv::SetCw(Ptr<Node> node, uint32_t cwMinValue, uint32_t cwMaxValue)
        {
        Ptr<NetDevice> dev = node->GetDevice (0);
        Ptr<WifiNetDevice> wifi_dev = DynamicCast<WifiNetDevice> (dev);
        Ptr<WifiMac> wifi_mac = wifi_dev->GetMac ();
        Ptr<RegularWifiMac> rmac = DynamicCast<RegularWifiMac> (wifi_mac);
        PointerValue ptr;
        rmac->GetAttribute ("Txop", ptr);
        Ptr<Txop> txop = ptr.Get<Txop> ();

        // if both set to the same value then we have uniform backoff?
        if (cwMinValue != 0) {
            NS_LOG_DEBUG ("Set CW min: " << cwMinValue);
            txop->SetMinCw(cwMinValue);
        }

        if (cwMaxValue != 0) {
            NS_LOG_DEBUG ("Set CW max: " << cwMaxValue);
            txop->SetMaxCw(cwMaxValue);
        }
        return true;
        }
        ```
- 4.路由环境与ns3-gym接口耦合，上述过程代码均在 mygym.cc中实现，接口在 sim.cc 中实现，

在sim.cc 文件中，需要先单独配置好正常的网络环境，然后写入接口，主要是Env的初始化、统计收包数、目的函数回调、
- 代码如下：
    ```c
    // OpenGym Env
    Ptr<OpenGymInterface> openGymInterface = CreateObject<OpenGymInterface> (openGymPort);
    Ptr<MyGymEnv> myGymEnv;
    if (eventBasedEnv)
    {
        myGymEnv = CreateObject<MyGymEnv> ();
    } else {
        myGymEnv = CreateObject<MyGymEnv> (Seconds(envStepTime));
    }
    myGymEnv->SetOpenGymInterface(openGymInterface);

    // connect OpenGym entity to event source
    Ptr<UdpServer> udpServer = DynamicCast<UdpServer>(sinkApps.Get(0));
    if (eventBasedEnv)
    {
        udpServer->TraceConnectWithoutContext ("Rx", MakeBoundCallback (&MyGymEnv::NotifyPktRxEvent, myGymEnv, dstNode));
    } else {
        udpServer->TraceConnectWithoutContext ("Rx", MakeBoundCallback (&MyGymEnv::CountRxPkts, myGymEnv, dstNode));
    }
    ```
- 5.Agent 的学习（智能体的学习）以 dqn_agent_v1.py为例，
    - RL代理能够学习将较低的 Cwmin/CWmax 值分配给更接近流目标的节点。因此，它的表现能够优于所有节点都被分配到相同的 CWmin/CWmax的基线
    - 即 智能体可以逐渐学会如何根据节点队列长度来优化 CWmin/CWmax的值，通过分配较低的cw值，离目的地较近的节点能够更频繁地尝试发送数据包，从而减少数据传输的延迟和可能的中断，从而提高网络的整体性能
    - 首先，是定义DQN结构，这里使用的是 tensorflow 架构
        ```python
        class DqnAgent(object):
        """docstring for DqnAgent"""
        def __init__(self, inNum, outNum):
            super(DqnAgent, self).__init__()
            self.model = keras.Sequential()
            self.model.add(keras.layers.Dense(inNum, input_shape=(inNum,), activation='relu'))
            self.model.add(keras.layers.Dense(outNum, activation='softmax'))
            self.model.compile(optimizer=tf.train.AdamOptimizer(0.001),
                            loss='categorical_crossentropy',
                            metrics=['accuracy'])

        def get_action(self, state):
            return np.argmax(self.model.predict(state)[0])

        def predict(self, next_state):
            return self.model.predict(next_state)[0]

        def fit(self, state, target, action):
            target_f = self.model.predict(state)
            target_f[0][action] = target
            self.model.fit(state, target_f, epochs=1, verbose=0)
        ```
    - 其次。定义 agent 如何选择 动作，这里使用的是 贪心策略
        ```python
        # Choose action
        if np.random.rand(1) < epsilon:
            action0 = np.random.randint(cwSize)
            action1 = np.random.randint(cwSize)
            action2 = np.random.randint(cwSize)
            action3 = np.random.randint(cwSize)
        else:
            action0 = agent0.get_action(state[:,0:2])
            action1 = agent1.get_action(state[:,1:3])
            action2 = agent2.get_action(state[:,2:4])
            action3 = agent3.get_action(state[:,3:5])
        ```                                                    
    - 执行动作，并得到反馈
        ```python
        # Step
        actionVec = [action0, action1, action2, action3, 100]
        next_state, reward, done, _ = env.step(actionVec)
        ```
    - 根据reward训练、更新，Q值 根据贝尔曼方程计算
        ```python
        # Train
        target0 = reward
        target1 = reward
        target2 = reward
        target3 = reward

        if not done:
            target0 = reward + 0.95 * np.amax(agent0.predict(next_state[:,0:2]))
            target1 = reward + 0.95 * np.amax(agent1.predict(next_state[:,1:3]))
            target2 = reward + 0.95 * np.amax(agent2.predict(next_state[:,2:4]))
            target3 = reward + 0.95 * np.amax(agent3.predict(next_state[:,3:5]))

        agent0.fit(state[:,0:2], target0, action0)
        agent1.fit(state[:,1:3], target1, action1)
        agent2.fit(state[:,2:4], target2, action2)
        agent3.fit(state[:,3:5], target3, action3)
        ```
    - 更新状态、奖励、贪心因子
        ```python
        state = next_state
        rewardsum += reward
        if epsilon > epsilon_min: epsilon *= epsilon_decay    
        ```
    - 记录数据：
        ```python
        time_history.append(time)
        rew_history.append(rewardsum) 
        ```
    - 训练过程绘图
        ```python
        plt.plot(range(len(time_history)), time_history)
        plt.plot(range(len(rew_history)), rew_history)
        plt.xlabel('Episode')
        plt.ylabel('Time')
        plt.show()
        ```

### 接口的思考
- 1. ns3 -> ns3-gym:
    - 初始化环境 就相当于把所有 的回调函数调用了，执行动作、收集状态最好单独写个函数，注意传参要统一
- 2. ns3-gym -> python:
    - 其实和opengym 、opengym-2 的两个例子差不多，主要是添加了强化学习的核心代码，
    像这里使用的是简单的DQN，没有涉及到经验回放，包含两个全连接层（Dense layers），第一个隐藏层使用 ReLU 激活函数，输出层使用softmax激活函数，
    输入节点队列长度，输出竞争窗口大小    







