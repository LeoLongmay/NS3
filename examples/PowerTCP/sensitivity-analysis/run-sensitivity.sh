#!/usr/bin/env bash
#
# LPCC 参数敏感性分析 (IncreaseFactor x IncreaseInterval, 3x3 九宫格)
#
# 仅扫描两个参数: --lpccIncreaseFactor 与 --lpccIncreaseIntervalUs
# 其余 CLI 参数逐字照搬用户提供的"较好"基准命令, 不改任何代码/配置。
#
#   行 (FACTORS)   : 0.0005 / 0.001 / 0.002   (对数对称 x2, 中心 0.001)
#   列 (INTERVALS) : 40     / 80    / 160      (对数对称 x2, 中心 80)
#   中心格 = (factor=0.001, interval=80) = cell-1-1
#
# 用法 (建议挂后台过夜):
#   cd /home/master01/CC_Exp
#   nohup bash examples/PowerTCP/sensitivity-analysis/run-sensitivity.sh \
#       > examples/PowerTCP/sensitivity-analysis/run.log 2>&1 &
#
# 次日看图: examples/PowerTCP/sensitivity-analysis/plots/  (cell-R-C__*.png/.pdf)
#
set -u

# ----------------------------------------------------------------------------
# 路径与常量
# ----------------------------------------------------------------------------
NS3=/home/master01/CC_Exp
cd "$NS3" || { echo "FATAL: 无法 cd 到 $NS3"; exit 1; }

BIN=./build/examples/PowerTCP/ns3.39-powertcp-evaluation-burst-optimized
CONF=examples/PowerTCP/config-burst-wan.txt
PLOTPY="$NS3/examples/PowerTCP/plot-burst.py"

RAW=examples/PowerTCP/dump_burst                              # 原始 .out (已被 .gitignore 覆盖)
SENS=examples/PowerTCP/sensitivity-analysis
DATA="$SENS/data"                                            # 过滤后 .burst
PLOTS="$SENS/plots"                                          # 九宫格各格图片
WORK="$SENS/.plotwork"                                       # 临时绘图 CWD (绕开 plot-burst.py 硬编码路径)

# 扫描网格 (行=factor, 列=interval); 中心 = FACTORS[1] x INTERVALS[1]
FACTORS=(0.0005 0.001 0.002)
INTERVALS=(40 80 160)

# 阶段 A 仿真并发上限: 1=最稳(串行) / 3=默认 / 9=最快(需足够内存)
MAX_PARALLEL=${MAX_PARALLEL:-3}

mkdir -p "$RAW" "$DATA" "$PLOTS" "$WORK/results_burst" "$WORK/plot_burst"

# 生成 tag: cell-<行>-<列>__f<factor>_i<interval>
tag_of() { printf 'cell-%d-%d__f%s_i%s' "$1" "$2" "$3" "$4"; }

# ----------------------------------------------------------------------------
# 阶段 0: 预检
# ----------------------------------------------------------------------------
echo "=================================================================="
echo "  阶段 0: 预检"
echo "=================================================================="
fail=0
[ -x "$BIN" ] || { echo "  [X] 找不到可执行二进制: $BIN"; fail=1; }
[ -f "$CONF" ] || { echo "  [X] 找不到配置文件: $CONF"; fail=1; }
[ -f "$PLOTPY" ] || { echo "  [X] 找不到绘图脚本: $PLOTPY"; fail=1; }
python3 -c "import pandas, matplotlib, numpy, requests" 2>/dev/null \
    || { echo "  [X] 缺少 python 依赖 (需要 pandas/matplotlib/numpy/requests)"; fail=1; }
if [ "$fail" -ne 0 ]; then
    echo "  预检失败, 终止。"; exit 1
fi
echo "  [OK] 二进制 / 配置 / 绘图脚本 / python 依赖 均就绪"
echo "  [OK] 并发上限 MAX_PARALLEL=$MAX_PARALLEL"

# ----------------------------------------------------------------------------
# 阶段 A: 跑 9 组仿真 (后台并行, 限流)
# ----------------------------------------------------------------------------
echo ""
echo "=================================================================="
echo "  阶段 A: 运行 9 组仿真"
echo "=================================================================="

run_one() {
    local factor=$1 interval=$2 out=$3
    "$BIN" \
        --conf="$CONF" \
        --algorithm=9 \
        --monitorSwitchId=74 \
        --lpccEpsilon=2000000 \
        --lpccThetaUs=1000 \
        --lpccFcnpMinIntervalUs=200 \
        --lpccPerFlowFcnpCooldownUs=400 \
        --lpccFcnpTopK=8 \
        --lpccFcnpTopKHigh=24 \
        --lpccFcnpKHighThreshBytes=8000000 \
        --lpccIncreaseIntervalUs="$interval" \
        --lpccIncreaseFactor="$factor" \
        --lpccWr=4.5 \
        --lpccKr=0.08 \
        --lpccQueueTargetRatio=2.0 \
        --lpccDropCapHigh=0.5 \
        --lpccAiSuppressMultiplier=5 \
        > "$out" 2>&1
}

for r in 0 1 2; do
    for c in 0 1 2; do
        factor=${FACTORS[$r]}
        interval=${INTERVALS[$c]}
        tag=$(tag_of "$r" "$c" "$factor" "$interval")
        out="$RAW/evaluation-lpcc-sens-${tag}.out"

        # 限流: 后台作业达到上限时, 等任意一个结束
        while [ "$(jobs -rp | wc -l)" -ge "$MAX_PARALLEL" ]; do
            wait -n 2>/dev/null || break
        done

        echo "  [启动] $tag  (factor=$factor interval=$interval)"
        run_one "$factor" "$interval" "$out" &
    done
done

echo "  等待全部仿真结束..."
wait
echo "  [OK] 9 组仿真完成"

# ----------------------------------------------------------------------------
# 阶段 B: 过滤 + 绘图 (串行, 避免共享 result-lpcc.burst / plot_burst 竞争)
# ----------------------------------------------------------------------------
echo ""
echo "=================================================================="
echo "  阶段 B: 过滤监控数据 + 绘图"
echo "=================================================================="

# 复用 results-burst.sh 的过滤逻辑: 从 MONITOR_TARGET 取 ToRIdx / MonitorPort
filter_burst() {
    local out=$1 dst=$2
    local mline tor port
    mline=$(grep -m1 '^MONITOR_TARGET ' "$out")
    if [ -z "$mline" ]; then
        echo "    [警告] $out 无 MONITOR_TARGET, 回退 grep 'ToR 0 Port 0'"
        grep 'ToR 0 Port 0' "$out" > "$dst"
        return
    fi
    tor=$(echo "$mline"  | awk '{for(i=1;i<=NF;i++) if($i=="ToRIdx"){print $(i+1)}}')
    port=$(echo "$mline" | awk '{for(i=1;i<=NF;i++) if($i=="MonitorPort"){print $(i+1)}}')
    if [ -n "$tor" ] && [ "$tor" != "-1" ] && [ "$port" = "-1" ]; then
        grep "ToR ${tor} PortAgg -1 " "$out" > "$dst"
    elif [ -n "$tor" ] && [ -n "$port" ] && [ "$tor" != "-1" ] && [ "$port" != "-1" ]; then
        grep "ToR ${tor} Port ${port} " "$out" > "$dst"
    else
        echo "    [警告] MONITOR_TARGET 解析异常 (tor=$tor port=$port), 回退 'ToR 0 Port 0'"
        grep 'ToR 0 Port 0' "$out" > "$dst"
    fi
}

summary=()
for r in 0 1 2; do
    for c in 0 1 2; do
        factor=${FACTORS[$r]}
        interval=${INTERVALS[$c]}
        tag=$(tag_of "$r" "$c" "$factor" "$interval")
        out="$RAW/evaluation-lpcc-sens-${tag}.out"
        burst="$DATA/result-${tag}.burst"

        marker=""
        [ "$r" = "1" ] && [ "$c" = "1" ] && marker="   <-- 中心 (基准)"
        echo "  [$tag]$marker"

        if [ ! -s "$out" ]; then
            echo "    [跳过] 原始输出缺失或为空: $out"
            summary+=("$tag : MISSING .out")
            continue
        fi

        # 1) 过滤监控时间序列
        filter_burst "$out" "$burst"
        nlines=$(wc -l < "$burst" 2>/dev/null || echo 0)
        if [ "$nlines" -eq 0 ]; then
            echo "    [跳过] 过滤后 .burst 为空, 不绘图"
            summary+=("$tag : EMPTY .burst")
            continue
        fi
        echo "    过滤 -> $burst  ($nlines 行)"

        # 2) 绘图: 把本组数据伪装成 plot-burst.py 期望的 result-lpcc.burst,
        #    在独立 CWD 内运行 (不改脚本, 不污染 examples/PowerTCP/{results_burst,plot_burst})
        cp -f "$burst" "$WORK/results_burst/result-lpcc.burst"
        rm -f "$WORK"/plot_burst/lpcc-* 2>/dev/null
        ( cd "$WORK" && python3 "$PLOTPY" ) > "$WORK/plot.log" 2>&1
        if [ $? -ne 0 ]; then
            echo "    [警告] plot-burst.py 失败, 见 $WORK/plot.log"
            summary+=("$tag : PLOT FAILED")
            continue
        fi

        # 3) 搬运并按 tag 改名 (主图 = 全程吞吐+队列)
        [ -f "$WORK/plot_burst/lpcc-new.png" ]       && cp -f "$WORK/plot_burst/lpcc-new.png"       "$PLOTS/${tag}.png"
        [ -f "$WORK/plot_burst/lpcc-burst-new.pdf" ] && cp -f "$WORK/plot_burst/lpcc-burst-new.pdf" "$PLOTS/${tag}.pdf"
        [ -f "$WORK/plot_burst/lpcc-power-new.png" ] && cp -f "$WORK/plot_burst/lpcc-power-new.png" "$PLOTS/${tag}-zoom.png"
        echo "    绘图 -> $PLOTS/${tag}.png / .pdf"
        summary+=("$tag : OK")
    done
done

# ----------------------------------------------------------------------------
# 阶段 C: 收尾
# ----------------------------------------------------------------------------
# 清理临时绘图文件 (保留目录结构)
rm -f "$WORK"/results_burst/result-lpcc.burst "$WORK"/plot_burst/lpcc-* "$WORK"/plot.log 2>/dev/null

echo ""
echo "=================================================================="
echo "  完成。九宫格图片位于: $PLOTS/"
echo "  (按文件名排序即九宫格阅读顺序; 中心 = cell-1-1__f0.001_i80)"
echo "=================================================================="
printf '  %s\n' "${summary[@]}"
echo ""
echo "  布局:"
echo "                int=40         int=80          int=160"
echo "  factor=0.0005  cell-0-0       cell-0-1        cell-0-2"
echo "  factor=0.001   cell-1-0      [cell-1-1 中心]  cell-1-2"
echo "  factor=0.002   cell-2-0       cell-2-1        cell-2-2"
