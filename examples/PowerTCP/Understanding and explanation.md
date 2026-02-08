### 系统环境：Ubuntu 22.04

1.  使用`./waf --enable-examples configure`时报警告`-bash: ./waf: Permission denied`

    添加waf执行权限：`chmod +x waf`

2.  

3.  





### NS3在VSCode中的配置（以NS3.19为示例）

1.  打开linux终端，输入`snap install code`

2.  进入超级用户，进入到ns3相应目录（比如`ns-allinone-3.19/ns-3.19/`），输入命令`code . --user-data-dir=/root/.vscode-root`

3.  然后，在VS Code界面中，按F1（Ctrl+Shift+P）调出命令面板，配置相关设置`c/c++: Edit configurations(json)`，会在.vscode目录下生成一个`c_cpp_properties.json`文件，`includePath`中更改相应目录，如下

    ```json
    {
        "configurations": [
            {
                "name": "Linux",
                "includePath": [
                    "${workspaceFolder}/build/**"
                ],
                "defines": [],
                "compilerPath": "/usr/bin/gcc",
                "cStandard": "c17",
                "cppStandard": "gnu++14",
                "intelliSenseMode": "linux-gcc-x64"
            }
        ],
        "version": 4
    }
    ```

    配置完成后，可以进行代码提升

4.  配置build，点击终端->配置默认生成任务，会生成一个`task.json`文件，参考的配置如下：

    ```json
    {
        "version": "2.0.0",
        "tasks": [
            {
                "type": "cppbuild",
                "label": "NS3: waf 生成活动文件",
                "command": "python2.7 /root/temp/ns-allinone-3.19/ns-3.19/waf",
                "args": [],
                "options": {
                    "cwd": "/root/temp/ns-allinone-3.19/ns-3.19"
                },
                "problemMatcher": [
                    "$gcc"
                ],
                "group": {
                    "kind": "build",
                    "isDefault": false
                },
                "detail": "编译器: /usr/bin/gcc"
            },
        ]
    }
    ```

    NS3.19默认使用python2的版本进行编译，使用更高版本的NS3时，`command`下去掉python2.7字样（更高版本的NS3采用ns3命令直接进行编译，好像从3.3x某个版本开始）。`command`和`options`中的`cwd`都更改为waf所在路径，label可更改，但注意需与后续的launch.json中的`preLaunchTask`保持一致。`task.json`配置完成后，按ctrl+shift+b可以使用waf编译

5.  按F5，选择`C++(GDB/LLDB)`，之后选择`NS3: waf 生成活动文件`（task.json中的label），会提示找不到`launch.json`文件，点击配置：

    ```json
    {
        "version": "0.2.0",
        "configurations": [
            {
                "name": "waf - Build and debug active file",
                "type": "cppdbg",
                "request": "launch",
                "program": "/root/temp/ns-allinone-3.19/ns-3.19/build/scratch/${fileBasenameNoExtension}",
                "args": [],
                "stopAtEntry": false,
                "cwd": "/root/temp/ns-allinone-3.19/ns-3.19",
                "environment": [],
                "MIMode": "gdb",
                "setupCommands": [
                    {
                        "description": "Enable pretty-printing for gdb",
                        "text": "-enable-pretty-printing",
                        "ignoreFailures": true
                    }
                ],
                "preLaunchTask": "NS3: waf 生成活动文件",
                "miDebuggerPath": "/usr/bin/gdb",
            }
        ]
    }
    ```

    配置完成后，可以进行脚本的调试

6.  使用F5进行调试时，终端可能报错找不到so库，在ns3目录下输入一下命令：

    ```
    root@NS3:~/temp/ns-allinone-3.19/ns-3.19# cp build/lib/* /lib/
    ```

    再次按F5调试脚本！

**参考文章**：https://www.bilibili.com/opus/494071708926899481

**参考代码仓**：https://github.com/osedu/ns3-introduction-video-tutorial-2025.git



### NS3.19 拥塞控制流程解释

**主要逻辑**：NS3.19中没有实现单独的CNP检测、发送以及接收，而是复用了PFC的检测、发送和接收。

**qbb-net-device.cc**：端口模拟类

**switch-node.cc**：交换机节点类

**rdma-hw.cc**：RDMA硬件（网卡）类

1.  `switch-node.cc` 文件的函数`CheckAndSendPfc`：检测队列长度（NS3.19中默认实现一个交换机节点128个端口，每个端口8个队列），并对超过特定长度的队列发送PFC（调用switch节点中的网络设备抽象类`qbb-net-device.cc`中的函数`SendPfc`）

    ![image-20251218224134054](examples/PowerTCP/explaination_image/image-20251218224134054.png)

2.  `SendPfc`中创建数据包，并封装相应的IPv4头（其中IPv4投中填充了上层协议的协议号，PFC对应0xFE，CNP对应0xFF）和PFC头，并添加了定制头`CustomHeader ch`，该定制头似乎可以自动从缓存中获得CNP或fCNP的控制信息（通过`p->PeekHeader`调用实现）

    ![image-20251218224247754](/home/leo/PowerTCP-RAW/ns-3.39/examples/PowerTCP/explaination_image/image-20251218224247754.png)

3.  完成上述操作后，调用`SwitchSend`函数，对于PFC、CNP和fCNP等高优先级报文将直接发出，而不进入队列（实际上进入了队列，但是立马发出去了，即先调用了`m_queue->Enqueue(packet, qIndex)`再立即调用`DequeueAndTransmit()`

    ![image-20251218224308481](/home/leo/PowerTCP-RAW/ns-3.39/examples/PowerTCP/explaination_image/image-20251218224308481.png)

4.  PFC或CNP发送完成后，将在网络中传输，直到被下一个交换机节点或者主机节点接收到

5.  若是交换机节点接收到，则继续调用`SwitchSend`向下一跳继续转发（调用逻辑为`qbb-net-device`收到该数据包，调用上层函数`SwitchReceiveFromDevice`->`SendToDev`->`SendToDevContinue`->`DoSwitchSend`->`SwitchSend`)

6.  若是主机节点接收到，则触发`rdma-hw.cc`类中的回调函数`m_rdmaReciveCb(packet, ch)`，并调用`Receive`函数

    ![image-20251218225630773](/home/leo/PowerTCP-RAW/ns-3.39/examples/PowerTCP/explaination_image/image-20251218225630773.png)

7.  `Receive`函数将进行判断是哪种数据包，并进行相应的操作

    ![image-20251218225729887](/home/leo/PowerTCP-RAW/ns-3.39/examples/PowerTCP/explaination_image/image-20251218225729887.png)

8.  其中，`ReceiveCnp`函数将快速初始化发送端速率为一个特定值，ReceiveAck函数将判断接收到的数据包的是否携带cnp位，若cnp位为true，则DCQCN等基于cnp的算法将进行速率调整；最后，基于RTT的算法也将调整速率

    ![image-20251218230419648](/home/leo/PowerTCP-RAW/ns-3.39/examples/PowerTCP/explaination_image/image-20251218230419648.png)

疑似组包函数：`qbb-net-device`中的`DequeueAndTransmit`

发送端收到的CNP报文实际上是一个标记了CNP位的ACK报文



#### **调用流程**：

**发送端**

1.  主模拟脚本->`ScheduleFlowInputs`
2.  `rdma-client-helper.cc`->`Install`
3.  `rdma-client.cc`->`StartApplication`
4.  `rdma-driver.cc`->`AddQueuePair`
5.  `rdma-hw.cc`->`AddQueuePair`
6.  `qbb-net-device.cc`->`NewQp`->`DequeueAndTransmit`->`SwitchNotifyDequeue`(当前节点为交换机）->`TransmitStart`
7.  `qbb-channel.cc`->`TransmitStart`



**交换机接收到发送**

1.  `qbb-net-device.cc`->`Receive`
2.  `switch-node.cc`->`SwitchReceiveFromDevice`->`SendToDev`
3.  `qbb-net-device.cc`->`SwitchSend`
4.  `broadcom-egress-queue.cc`->`Enqueue`
5.  `qbb-net-device.cc`->`DequeueAndTransmit`
6.  `switch-node.cc`->`SwitchNotifyDequeue`
7.  `qbb-net-device.cc`->`TransmitStart`

调用`DequeueAndTransmit`中，交换机会轮询`DequeueRR`每个队列查找是否有可发送的数据包

`SwitchNotifyDequeue`中收到一个普通数据包后，会判断出口队列是否发生拥塞，若是检测到拥塞，会标记当前数据包的ECN，并继续转发

**我们要做的**：上述交换机检测到拥塞后，封装一个FCNP报文并发送给源端，调用`SendToDev`进行转发



**接收端**

1.  `qbb-net-device.cc`->`Receive`->`m_rdmaReceiveCb`
2.  `rdma-hw.cc`->`Receive`





接收端收到UDP报文，并根据收到的UDP报文中的ECN标记确认是否标记CNP

`rdma-hw.cc`->`ReceiveUdp`

`qbb-net-device.cc`->`RdmaEnqueueHighPrioQ`->`EnqueueHighPrioQ`

`drop-tail-queue.cc`->`Enqueue`

`qbb-net-device.cc`->`TriggerTransmit`->`DequeueAndTransmit`->`TransmitStart`





qbb-net-device中的Receive函数中收到数据包时是具有PPP头和IP头的，custom-header头也是有数据的，但是fcnp没有数据，所以怀疑是否是发送fcnp时携带的数据方式不对



`qbb-net-device.cc`->`RdmaEgressQueue`->`m_rdmaEQ`

入队函数：`EnqueueHighPrioQ`

出队函数：`DequeueQindex`



**NS3中交换机内存管理**

交换机类`switch-node.h`存在一个内存管理类`switch-mmu.h`，内存管理类负责管理交换机的256个端口（每个端口8队列），每个端口有一个端口模拟类`qbb-net-device.h`，端口类继承自NS3原生的端到端设备类`point-to-point-net-device.h`类，每个端口有一个唯一标识`m_ifindex`和端口内的队列`m_queue`，`m_queue`为端口队列类`broadcom-egress-queue.h`的对象，端口类通过`m_queue`管理端口内部的8个队列





##### qp队列逻辑

1.  主模拟脚本->`ScheduleFlowInputs`
2.  `rdma-client-helper.cc`->`Install`
3.  `rdma-client.cc`->`StartApplication`
4.  `rdma-driver.cc`->`AddQueuePair`
5.  `rdma-hw.cc`->`AddQueuePair`->`GetNicIdxOfQp`->
6.  







`gdb build/examples/PowerTCP/ns3.39-powertcp-evaluation-workload-debug --conf= --algorithm=9 --wien=false --delayWien=false --windowCheck=0 --queryRequestRate=0 --load=load --START_TIME=0.1 --END_TIME=10 --FLOW_LAUNCH_END_TIME=9 --incast=10 --cdfFileName=cdf --request=0`



#### 调试脚本文件

调试前提：首先要有可运行的调试文件（debug后缀，optimized后缀是发布版本，不可调试），进入NS3.39目录，输入一下命令：
`gdb build/examples/PowerTCP/ns3.39-powertcp-evaluation-workload-debug`





`ps aux | grep powertcp-evaluation-fairness | grep -v grep`



**PCN**: proactive congestion notification

**GRR**: global rate regulation

