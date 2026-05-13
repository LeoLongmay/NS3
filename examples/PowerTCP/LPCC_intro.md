# LPCC 控制机制说明（当前实现）

## 目录
1. [机制范围与代码入口](#1-机制范围与代码入口)
2. [角色划分：拥塞点与响应点](#2-角色划分拥塞点与响应点)
3. [数据结构与状态变量](#3-数据结构与状态变量)
4. [拥塞点逻辑（交换机）](#4-拥塞点逻辑交换机)
5. [响应点逻辑（发送端）](#5-响应点逻辑发送端)
6. [增速与降速的切换时序](#6-增速与降速的切换时序)
7. [参数说明（按作用域）](#7-参数说明按作用域)
8. [门控机制清单与相互作用](#8-门控机制清单与相互作用)
9. [当前 `script-burst.sh` 参数解读](#9-当前-script-burstsh-参数解读)
10. [重要实现注记（当前版本）](#10-重要实现注记当前版本)

## 1. 机制范围与代码入口
本说明对应当前仓库中的 LPCC 实现（`cc_mode == 9`），主要分布在以下文件：

- 交换机侧拥塞检测与 fCNP 生成：`src/point-to-point/model/switch-node.cc`
- 队列占用计数（`egress_bytes`）与 ECN基础逻辑：`src/point-to-point/model/switch-mmu.cc`、`switch-mmu.h`
- 发送端 fCNP 接收、降速、增速、ACK补充调节：`src/point-to-point/model/rdma-hw.cc`、`rdma-hw.h`
- 每流 LPCC 状态变量定义与初始化：`src/point-to-point/model/rdma-queue-pair.h`、`rdma-queue-pair.cc`
- 参数注入入口：`examples/PowerTCP/powertcp-evaluation-burst.cc`
- 试验参数设置：`examples/PowerTCP/script-burst.sh`

时间单位说明：
- 交换机与发送端内部时间戳多用 `Simulator::Now().GetTimeStep()`（ns级时间步）。
- `*Us` 参数是微秒，代码内通常乘 `1000` 转为 ns 比较。

## 2. 角色划分：拥塞点与响应点
### 2.1 拥塞点（Congestion Point）
拥塞点是交换机某个 egress 队列（`ifIndex, qIndex`）：

- 交换机在 `SwitchNotifyDequeue()` 时检查该 egress 队列当前占用 `m_mmu->egress_bytes[ifIndex][qIndex]`。
- 当 `egress_bytes > m_epsilon` 时判定为 LPCC 拥塞触发条件，进入 fCNP 发送流程。

### 2.2 响应点（Reaction Point）
响应点是发送端某条 RDMA 流（QP）：

- 收到 fCNP 后按门控条件触发 LPCC 降速。
- 降速后通过定时器周期性增速，且增速受最近 fCNP 信息抑制。
- ACK 路径在特定静默窗口外可执行轻量 RTT 膨胀降速。

## 3. 数据结构与状态变量
### 3.1 交换机侧关键状态
- `m_epsilon`：LPCC 拥塞阈值（队列字节）
- `m_fcnpMinIntervalUs`：同一 egress 队列两次 fCNP 发送最小间隔
- `m_lpccPerFlowFcnpCooldownUs`：同一流在交换机侧 fCNP 冷却窗口
- `m_lpccFcnpTopK` / `m_lpccFcnpTopKHigh`：低/高队列下的 Top-K 反馈扇出
- `m_lpccFcnpKHighThreshBytes`：高队列阈值，超过后使用 `TopKHigh`
- `m_lastFcnpSentTs[ifIndex][qIndex]`：每队列最近一次发 fCNP 时间
- `m_lpccFlowLastFcnpTs[flowKey]`：每流最近一次发 fCNP 时间

### 3.2 发送端每流（QP）关键状态
定义于 `rdma-queue-pair.h` 的 `lpcc` 子结构：

- `m_lastDecreaseRate`：上次实际降速时间
- `m_lastFcnpTs`：上次收到 fCNP 时间（注意：即使后续被去抖忽略也会更新）
- `m_lastProcessedFcnpTs`：上次“被处理并可触发控制动作”的 fCNP 时间
- `m_lastFcnpQlen`：最近一次 fCNP 携带的队列长度
- `m_lastCongRateBps`：最近一次 fCNP 携带的拥塞链路速率
- `m_flowCount`：最近一次 fCNP 携带的队列流数
- `m_lastRtt` / `m_minRtt`：ACK 路径 RTT 状态
- `m_rpTimer`：LPCC 增速周期定时器
- `m_first_cnp`：首个 fCNP 标记

## 4. 拥塞点逻辑（交换机）
### 4.1 队列占用来源
`egress_bytes[port][qIndex]` 在 MMU 中维护：

- 入队增加：`UpdateEgressAdmission()` 中 `egress_bytes += psize`
- 出队减少：`RemoveFromEgressAdmission()` 中 `egress_bytes -= psize`

因此 LPCC 的拥塞观测量是“该交换机该端口该队列的当前 egress 占用字节”。

### 4.2 LPCC 拥塞触发条件
在 `SwitchNotifyDequeue(ifIndex, qIndex, p)` 中：

1. `m_ccMode == 9`
2. `qIndex != 0`
3. `m_mmu->egress_bytes[ifIndex][qIndex] > m_epsilon`
4. 满足 per-queue 最小发送间隔门限：
   - `nowTs - m_lastFcnpSentTs[ifIndex][qIndex] >= fcnpMinInterval`

满足后进入 fCNP 选流与发送。

### 4.3 fCNP 选流与发送
交换机侧步骤：

1. 计算是否高队列：`qlen >= m_lpccFcnpKHighThreshBytes`
2. 选择 `topK`：
   - 低队列用 `m_lpccFcnpTopK`
   - 高队列用 `m_lpccFcnpTopKHigh`
3. 从流表按该 egress 队列选 Top-K 高频流
4. 对每条候选流做 per-flow 冷却检查：
   - 若 `nowTs - flowLastFcnpTs < m_lpccPerFlowFcnpCooldownUs` 则跳过
5. 对通过检查的流构造并发送 fCNP（协议号 `0xF9`）
6. 若 Top-K 为空，回退为对当前出队包对应流发送 fCNP

fCNP 关键载荷字段：

- `timestamp`：交换机发送时刻
- `pg`、`dport`：用于源端查找对应 QP
- `qlen`：当前 `egress_bytes`
- `m_flowCount`：该 egress 队列流数
- `linkRateBps`：该端口线速

### 4.4 ECN 回退标记
同一轮中如果 egress 被 `ShouldSendCN()` 判定拥塞且本轮未发 fCNP，则仍保留 ECN 标记路径（回退机制）：

- `markEcn = (m_ccMode != 9) || !lpccFcnpSent`

## 5. 响应点逻辑（发送端）
### 5.1 fCNP 接收与去抖门控
在 `RdmaHw::Receive()` 中处理 `l3Prot == 0xF9`：

1. 根据 fCNP 的 `sip + dport + pg` 找到 QP。
2. 无条件刷新"最新观测"两项（不论是否通过去抖）：
   - `m_lastCongRateBps`
   - `m_lastFcnpQlen`
3. 去抖 cooldown（debounce）：
   - `cooldownUs = max(2*increaseIntervalUs, max(200, thetaUs/4))`
   - 若距离 `m_lastProcessedFcnpTs` 小于 `cooldownUs`，本次直接返回（不做降速，**不更新** `m_lastFcnpTs / m_lastProcessedFcnpTs`）。
4. 若首个 fCNP：立即降速，并同时把 `m_lastFcnpTs` 与 `m_lastProcessedFcnpTs` 设为 `nowTs`。
5. 非首个 fCNP：仅当距离上次降速 `>= theta` 时才执行降速；同上更新两个时间戳。
6. 每次实际降速后都重置增速定时器（取消旧 `m_rpTimer`，重新 schedule）。

**重要语义变更**：`m_lastFcnpTs` 与 `m_lastProcessedFcnpTs` 现在语义一致，都代表"曾经真正触发过降速的 FCNP 时刻"——被去抖丢弃的 FCNP 不再更新它们。这是为了让 AI 的快恢复门控（见 5.4）不被密集而无效的 FCNP 永久压制。

### 5.2 fCNP 触发的主降速
`UpdateRateLpcc()` 核心计算：

1. 计算队列-阈值项
   - `m_k = (qlen / epsilon) - 1`（**已移除** 原 `rttlEff/m_rtts` 比例项——它使用 FCNP 反向单程时间，方向与语义都不正确；去掉后首次降速强度与稳态一致）
   - 限幅到 `[0, 1.2]`
2. 计算流强度项
   - `rateRatio = currentRate / congRate`
   - `flowFactor = min(sqrt(flowCount), 3.0)`
   - `m_p = clip(rateRatio * flowFactor, [0, 4])`
3. 队列惩罚项
   - 队列超过 `queueTarget`（参数化，见 7.2）时，`qPenalty` 随超量提高
4. 降速信号
   - `decreaseSignal = clip(m_k * m_p * qPenalty, [0, m_wr])`
5. 原始新速率
   - `newRate = oldRate / (1 + decreaseSignal)`
6. 单步降幅上限（dropCap，**参数化**）
   - 设 `r = qlen / queueTarget`、`R = LpccDropCapHighRatio`
   - `r <= 1`：`dropCap = LpccDropCapLow`（默认 0.2）
   - `1 < r < R`：`dropCap` 在 `LpccDropCapLow → LpccDropCapHigh` 之间线性插值
   - `r >= R`（默认 R=2.0）：`dropCap = LpccDropCapHigh`（默认 0.5，**从原 0.75 收紧**）
   - 若 `dropFrac > dropCap`，按 `dropCap` 裁剪
7. 最小速率保护
   - `newRate >= 10 * minRate`

### 5.3 ACK 路径补充降速（轻量校正）
`UpdateRateLpccOnAck()` 仅在以下条件都满足时执行：

1. 首次 RTT 初始化完成
2. 有最近拥塞链路速率 `m_lastCongRateBps`
3. 距离上次降速超过 `theta`
4. 距离上次 fCNP 超过 `4*theta`（静默窗口）

执行时按 RTT 膨胀比例做轻量减速：

- `rttInflation = (lastRtt - minRtt)/minRtt`
- `ackKr = max(0.01, 0.20 * m_kr)`
- `newRate = oldRate * (1 - min(rttInflation * m_p, ackKr))`

### 5.4 定时增速循环
定时器 `m_rpTimer` 周期触发 `RateIncEventTimerLpcc()`：

1. 重新 schedule 下一次（周期 `m_increaseInterval`）
2. 调用 `RateIncEventLpcc()`

`RateIncEventLpcc()` 中增速门控逻辑：

1. 定义窗口（**已参数化**）：
   - `thetaTs = thetaUs * 1000`
   - `incTs   = increaseIntervalUs * 1000`
   - `fastRecoverQuietTs = LpccAiSuppressMultiplier * incTs`（默认倍率 2）
2. `effectiveQlen` 处理（当前版本）：
   - 无线性衰减
   - 若 `ageSinceProcessed >= theta`，将 `effectiveQlen` 直接清零
3. 最近 fCNP 抑制（**改用"已处理 FCNP"时间戳**）：
   - 若 `ageSinceProcessed < fastRecoverQuietTs`，本次增速直接返回
   - 关键：被去抖丢弃的 FCNP **不会**刷新 `m_lastProcessedFcnpTs`，因此不再压制 AI——拥塞期间 rpTimer 不再被密集而无效的 FCNP 永久冻结。
4. 按 `effectiveQlen / queueTarget` 计算 `qRatio`，动态调整 `adaptiveBeta`（queueTarget 已参数化）
5. 队列高于目标时进一步压低 `adaptiveBeta`
6. 计算 `baseIncRatio` 与 `aiFloor`，并取 `incRatio=max(baseIncRatio, aiFloor)`
7. `rate *= (1 + incRatio)` 完成增速

## 6. 增速与降速的切换时序
对单条流的主要时序如下：

1. 正常状态：增速定时器周期运行。
2. 收到 fCNP：
   - 先无条件刷新 `m_lastCongRateBps / m_lastFcnpQlen`（最新观测）；
   - 通过去抖检查；若被去抖丢弃，**不**更新 `m_lastFcnpTs / m_lastProcessedFcnpTs`，不影响 AI；
   - 若通过去抖、且满足 `theta` 门限则降速，同时把两个时间戳设为 `nowTs`；
   - 降速后重置增速定时器。
3. 降速后恢复：
   - 每 `increaseInterval` 到点触发一次增速尝试；
   - 若距上一次**已处理**的 FCNP 不足 `fastRecoverQuietTs = LpccAiSuppressMultiplier * incInterval`，跳过本次增速；
   - 当满足静默窗口且队列记忆清空后，增速强度才会变大。
4. ACK 补充降速：
   - 仅在较长 fCNP 静默后参与，作为慢速纠偏。

## 7. 参数说明（按作用域）
### 7.1 交换机侧参数（Attribute 与脚本默认已对齐，见 9）
- `lpccEpsilon`：拥塞触发阈值（`egress_bytes > epsilon`）
- `lpccFcnpMinIntervalUs`：同一 egress 队列发 fCNP 的最小间隔
- `lpccPerFlowFcnpCooldownUs`：同一流在交换机侧的 fCNP 冷却
- `lpccFcnpTopK`：低队列时 fCNP 扇出流数（**实际请求 2*topK 候选回填**，避免冷却命中导致空发）
- `lpccFcnpTopKHigh`：高队列时 fCNP 扇出流数
- `lpccFcnpKHighThreshBytes`：切换 `TopKHigh` 阈值

### 7.2 发送端参数
- `lpccThetaUs`：两次可执行降速的最小间隔（主要降速节拍）
- `lpccIncreaseIntervalUs`：增速定时器周期
- `lpccIncreaseFactor` (`beta`)：增速上限因子
- `lpccWr`：主降速强度上限参数（`decreaseSignal` 限幅上界）
- `lpccKr`：ACK 补充降速强度参数
- `LpccQueueTargetBytes`：稳态队列目标（绝对值）。`0` 表示运行时按 `epsilon * LpccQueueTargetRatio` 派生。
- `LpccQueueTargetRatio`：派生比例（默认 `0.375`，与 epsilon=2MB 联动得 `0.75MB`）。
- `LpccDropCapLow`：`qlen <= queueTarget` 时的单步降幅上限（默认 `0.2`）。
- `LpccDropCapHigh`：`qlen / queueTarget >= LpccDropCapHighRatio` 时的单步降幅上限（默认 `0.5`，**从原 0.75 收紧**）。
- `LpccDropCapHighRatio`：到达 `LpccDropCapHigh` 的 qlen 倍率（默认 `2.0`）。
- `LpccAiSuppressMultiplier`：AI 静默窗口 = 该值 × `increaseInterval`（默认 `2`）。

### 7.3 参数注入链路
1. `script-burst.sh` 通过命令行传 `--lpcc*`（**仅在显式 `>0` 时生效**）。
2. `powertcp-evaluation-burst.cc` 不再有 `if (cc_mode == 9)` 硬编码预设；只在 `--lpcc*` 显式提供时 SetAttribute 覆盖。
3. 未显式覆盖的参数全部走 Attribute 默认（`rdma-hw.cc` / `switch-node.cc`）。三处默认值已对齐。

## 8. 门控机制清单与相互作用
### 8.1 交换机门控
- `egress_bytes > epsilon` 才触发 fCNP
- per-queue 最小间隔门控 `fcnpMinInterval`
- per-flow 冷却门控 `perFlowCooldown`
- 高低队列动态 Top-K
- **Top-K 候选回填**：实际请求 `2*topK` 候选，被 per-flow 冷却跳过的不占用 fanout 名额，由次高速流顶上

### 8.2 发送端门控
- fCNP 去抖门控：`max(2*inc, max(200us, theta/4))`；**被去抖丢弃的 FCNP 不刷新时间戳**
- 降速节拍门控：`>= theta` 才允许再次降速
- 增速静默门控：距上一次**已处理** FCNP `< LpccAiSuppressMultiplier * inc` 时跳过本次增速
- 队列记忆清零门控：`ageSinceProcessed >= theta` 才清 `effectiveQlen`
- ACK 纠偏门控：距上次降速 < `theta` 或距上次 fCNP < `4*theta` 时不执行 ACK 降速

### 8.3 典型耦合风险
- `theta` 过大：降速响应稀疏，队列回落慢；同时 ACK 纠偏也更晚放开。
- `perFlowCooldown` 太小 + `fcnpMinInterval` 太小：fCNP 到达过密——本版本中这不再压制 AI，但仍可能放大降速节拍下的批量降速幅度。
- `dropCapHigh` 过大：单步降速过深，吞吐恢复更难（已默认收紧到 0.5）。
- `LpccQueueTargetBytes / Ratio` 与 `epsilon` 失配：`queueTarget << epsilon` 会让 dropCap 永远落在 `High` 档（原硬编码 0.75MB ↔ epsilon=2MB 即是此症）。

## 9. 当前 `script-burst.sh` 参数解读
脚本中 LPCC 参数（与 Attribute 默认对齐）：

- `epsilon=2000000`
- `thetaUs=5000`
- `fcnpMinIntervalUs=2000`
- `perFlowFcnpCooldownUs=2000`
- `topK=5`, `topKHigh=10`, `kHighThresh=4000000`
- `increaseIntervalUs=66`
- `increaseFactor=0.19`
- `wr=3.0`
- `kr=0.12`

新参数默认值（未在脚本中显式注入，由 Attribute 提供）：

- `LpccQueueTargetBytes=0`（运行时派生）
- `LpccQueueTargetRatio=0.375` → 派生 `queueTarget = 0.75MB`
- `LpccDropCapLow=0.2`、`LpccDropCapHigh=0.5`、`LpccDropCapHighRatio=2.0`
- `LpccAiSuppressMultiplier=2`

简要含义：

- 拥塞反馈频率偏低（交换机和发送端两侧都较保守）。
- 降速频率由 `theta=5ms` 限制，属于低频降速。
- 增速周期较快（80us），但仍受“最近 fCNP 抑制窗口 `2*inc=160us`”限制。

## 10. 重要实现注记（当前版本）
1. 有效路径：`Receive(fCNP) -> UpdateRateLpcc + 重置rpTimer`。历史死代码 `fcnp_received_lpcc / CheckRateDecreaseLpcc / ScheduleDecreaseRateLpcc / lpcc.m_eventDecreaseRate` 已**删除**。`RateIncEventLpcc` 内的旧 `m_targetRate += 500Mb/s` 代码段也已删除。
2. 增速分支取消"线性衰减到 0"逻辑：`ageSinceProcessed >= theta` 时 `effectiveQlen` 直接清零。
3. **AI 静默窗口语义变更**：现以 `m_lastProcessedFcnpTs` 为锚点，被去抖丢弃的 FCNP **不再**刷新该时间戳，AI 不会被密集的无效 FCNP 永久压制。
4. **dropCap 参数化**：分段阈值与默认值改为 `LpccDropCapLow=0.2` / `LpccDropCapHigh=0.5` / `LpccDropCapHighRatio=2.0`，从原硬编码 `0.75MB` 与 `0.2 / 0.4–0.5 / 0.75` 收紧。
5. **queueTarget 参数化**：原 `kLpccQueueTargetBytes = 0.75MB` 硬编码改为 Attribute `LpccQueueTargetBytes`（默认 0，运行时 = `epsilon * LpccQueueTargetRatio=0.375`），自动与 `epsilon` 联动。
6. **m_k 公式简化**：`UpdateRateLpcc` 去掉了原 `rttlEff / m_rtts` 比例项（m_rtts 是 FCNP 反向单程时间，方向/语义都不对），改为 `m_k = qlen/epsilon - 1`，与原 LPCC 论文一致。首次降速强度因此不再因 fallback 而被错误压低。
7. **Top-K 候选回填**：交换机端按 `2*topK` 请求候选；被 per-flow 冷却跳过的不占用 fanout 名额，由次高速流顶上，避免"全部命中冷却 → 实际发出 0 个 FCNP"。
8. **配置漂移已收敛**：Attribute 默认（`rdma-hw.cc` / `switch-node.cc`）与 `script-burst.sh` 已对齐。`powertcp-evaluation-burst.cc` 不再有 `if (cc_mode == 9)` 硬编码预设；仅在 `--lpcc*` 显式提供时 SetAttribute 覆盖。

