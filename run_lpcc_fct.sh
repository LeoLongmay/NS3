#!/bin/bash
# LPCC FCT (优化版 m_wr=4.5 + epsilon=2MB + TopK=8)
# 复用 mix/{40,80}%load_old/workload/ 的 flow 文件

cd /home/master01/CC_Exp

BINARY=./build/examples/PowerTCP/ns3.39-powertcp-evaluation-fct-optimized

LPCC_ARGS="--algorithm=9 --transportMode=0 --flowControlMode=0 --windowCheck=1 --wien=false --delayWien=false --lpccEpsilon=2000000 --lpccThetaUs=1000 --lpccFcnpMinIntervalUs=200 --lpccPerFlowFcnpCooldownUs=400 --lpccFcnpTopK=8 --lpccFcnpTopKHigh=24 --lpccFcnpKHighThreshBytes=8000000 --lpccIncreaseIntervalUs=80 --lpccIncreaseFactor=0.001 --lpccWr=4.5 --lpccKr=0.08 --lpccQueueTargetRatio=2.0 --lpccDropCapHigh=0.5 --lpccAiSuppressMultiplier=5"

echo "=== 40%load 启动 $(date +%H:%M:%S) ==="
$BINARY \
  --conf=examples/PowerTCP/mix/40%load_old/LPCC/config.txt \
  $LPCC_ARGS \
  --randomSeed=4001 \
  > examples/PowerTCP/mix/40%load_old/LPCC/ns3.log 2>&1

echo "=== 80%load 启动 $(date +%H:%M:%S) ==="
$BINARY \
  --conf=examples/PowerTCP/mix/80%load_old/LPCC/config.txt \
  $LPCC_ARGS \
  --randomSeed=8001 \
  > examples/PowerTCP/mix/80%load_old/LPCC/ns3.log 2>&1

echo "=== 全部完成 $(date +%H:%M:%S) ==="
