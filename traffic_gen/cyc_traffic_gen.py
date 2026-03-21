
# 迭代流量轮次
import random
import numpy as np

# 节点数量和交换机数量（对应循环拓扑）
nodeNum = 16
switchNum = 6

# 迭代轮次
episodes = 30

# 初始开始时间
initStartTime = 2.005

# 迭代开始时间（s）
startTime = 2.005

# 脚本模拟时间（对应模拟脚本，s）
simulationTime = 1

# 包大小（字节）
packetSize = 2000

# 流大小（G）
flowSize = 4.8 * 1e9 / packetSize

# 节点发包间隔（s）
interval = 0.005

# 轮次间隔（s）
episodesInterval = 0.02

# 随机小流数量
smallFlowNumber = 200

# 随机小流大小上限 (600M)
smallFlowNumberUp = 600 * 10e6 / packetSize

smallFlowNumberDown = 100 * 10e6 / packetSize

lines = []

ouputFileName = "cyc_traffic_gen.txt"

# 使用Beta分布生成[a, b]区间内的右偏随机数，alpha < beta时呈现右偏特性
def beta_skewed_random(a, b, alpha=0.5, beta=2):

    # 生成Beta分布随机数（范围[0,1]）
    beta_sample = np.random.beta(alpha, beta)
    # 映射到[a, b]区间
    return a + (b - a) * beta_sample

# 产生迭代大流
for e in range(episodes):
    initStartTime += episodesInterval
    startTime = initStartTime
    for i in range(0, 4):
        if i != 3:
            j = i + 1
        else:
            j = 0
        lines.append([i, j, 3, int(flowSize), round(startTime, 3)])
        startTime += interval


# 产生随机小流
for f in range(smallFlowNumber):
    # 随机生成小流大小
    smallFlowSize = int(beta_skewed_random(int(smallFlowNumberDown), int(smallFlowNumberUp)))
    src, dst = np.random.choice(range(0, nodeNum), size=2, replace=False)
    smallFlowStartTime = round(random.uniform(2.000, 2.000 + simulationTime), 3)
    lines.append([src, dst, 3, smallFlowSize, smallFlowStartTime])

# 依据开始时间对所有流进行排序
lines.sort(key=lambda x : x[-1])

# 写入文件
with open(ouputFileName, "w") as f:
    f.writelines(str(len(lines)) + '\n')
    for line in lines:
        f.writelines(' '.join(str(x) for x in line) + '\n')
    
