#!/usr/bin/env bash
#
# LPCC 参数敏感性分析 (第三组: 单参数 Epsilon, 6 个代表值)
#
# 仅扫描一个参数: --lpccEpsilon (字节)
# 其余 CLI 参数逐字照搬前两组基准命令(含已调好的 Wr=4.5 / Theta=1000 /
# IncreaseFactor=0.001 / IncreaseIntervalUs=80 / Kr=0.08 ...), 不改任何代码/配置。
#
#   阶梯 (EPS, 2x 几何, 中心参考点 = 2MB = cell-2):
#     cell-0  500000   (0.5MB, 更紧)
#     cell-1  1000000  (1MB,   偏紧)
#     cell-2  2000000  (2MB,   <-- 当前基准/参考点)
#     cell-3  4000000  (4MB,   偏松)
#     cell-4  8000000  (8MB,   松)
#     cell-5  16000000 (16MB,  很松, 观察 bufferbloat)
#
# 注: --lpccEpsilon 由二进制同时设到主机 CC (rdmaHw) 与交换机 FCNP 阈值 (sw),
#     两处同向, 故仍是严格单参数敏感性 (参数=epsilon, 双重作用为其设计语义)。
#
# 用法 (建议挂后台过夜):
#   cd /home/master01/CC_Exp
#   nohup bash examples/PowerTCP/sensitivity-analysis/run-sensitivity-epsilon.sh \
#       > examples/PowerTCP/sensitivity-analysis/epsilon/run.log 2>&1 &
#
# 次日看图: examples/PowerTCP/sensitivity-analysis/epsilon/plots/  (cell-<i>__eps<bytes>.png/.pdf)
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
SWEEP=examples/PowerTCP/sensitivity-analysis/epsilon         # 本组独立子目录
DATA="$SWEEP/data"
PLOTS="$SWEEP/plots"
WORK="$SWEEP/.plotwork"

# 扫描阶梯 (单参数 epsilon, 字节); 参考点 = EPS[2] = 2MB
EPS=(500000 1000000 2000000 4000000 8000000 16000000)

# 阶段 A 仿真并发上限: 1=最稳(串行) / 3=默认 / 6=最快(需足够内存)
MAX_PARALLEL=${MAX_PARALLEL:-3}

mkdir -p "$RAW" "$DATA" "$PLOTS" "$WORK/results_burst" "$WORK/plot_burst"

# 生成 tag: cell-<序号>__eps<bytes>
tag_of() { printf 'cell-%d__eps%s' "$1" "$2"; }

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
# 阶段 A: 跑 6 组仿真 (后台并行, 限流)
# ----------------------------------------------------------------------------
echo ""
echo "=================================================================="
echo "  阶段 A: 运行 6 组仿真 (仅变 epsilon)"
echo "=================================================================="

run_one() {
    local eps=$1 out=$2
    "$BIN" \
        --conf="$CONF" \
        --algorithm=9 \
        --monitorSwitchId=74 \
        --lpccEpsilon="$eps" \
        --lpccThetaUs=1000 \
        --lpccFcnpMinIntervalUs=200 \
        --lpccPerFlowFcnpCooldownUs=400 \
        --lpccFcnpTopK=8 \
        --lpccFcnpTopKHigh=24 \
        --lpccFcnpKHighThreshBytes=8000000 \
        --lpccIncreaseIntervalUs=80 \
        --lpccIncreaseFactor=0.001 \
        --lpccWr=4.5 \
        --lpccKr=0.08 \
        --lpccQueueTargetRatio=2.0 \
        --lpccDropCapHigh=0.5 \
        --lpccAiSuppressMultiplier=5 \
        > "$out" 2>&1
}

for i in 0 1 2 3 4 5; do
    eps=${EPS[$i]}
    tag=$(tag_of "$i" "$eps")
    out="$RAW/evaluation-lpcc-sens-eps-${tag}.out"

    # 限流: 后台作业达到上限时, 等任意一个结束
    while [ "$(jobs -rp | wc -l)" -ge "$MAX_PARALLEL" ]; do
        wait -n 2>/dev/null || break
    done

    echo "  [启动] $tag  (epsilon=$eps)"
    run_one "$eps" "$out" &
done

echo "  等待全部仿真结束..."
wait
echo "  [OK] 6 组仿真完成"

# ----------------------------------------------------------------------------
# 阶段 B: 过滤(自动选繁忙出口) + 绘图 (串行)
# ----------------------------------------------------------------------------
echo ""
echo "=================================================================="
echo "  阶段 B: 过滤监控数据 + 绘图"
echo "=================================================================="

# 从 .out 自动挑选繁忙出口并过滤出该口时间序列; 回显选中的端口号
filter_burst_busiest() {
    local out=$1 dst=$2
    local mline tor best bestmean
    mline=$(grep -m1 '^MONITOR_TARGET ' "$out")
    tor=$(echo "$mline" | awk '{for(i=1;i<=NF;i++) if($i=="ToRIdx"){print $(i+1)}}')
    if [ -z "$tor" ] || [ "$tor" = "-1" ]; then tor=0; fi

    # 该 ToR 上出现过的端口号
    local ports
    ports=$(grep -oE "^ToR ${tor} Port [0-9]+" "$out" | awk '{print $4}' | sort -un)
    best=""; bestmean=-1
    for p in $ports; do
        local m
        m=$(grep "^ToR ${tor} Port ${p} " "$out" | awk '$12>0.5{s+=$6;n++} END{if(n>0)print s/n; else print 0}')
        # 用 awk 比较浮点
        if awk "BEGIN{exit !($m > $bestmean)}"; then bestmean=$m; best=$p; fi
    done
    if [ -z "$best" ]; then
        echo "WARN_NO_PORT"
        : > "$dst"
        return
    fi
    grep "^ToR ${tor} Port ${best} " "$out" > "$dst"
    # 回显: ToR/端口/该口稳态吞吐(Gbps)
    awk "BEGIN{printf \"tor=%s port=%s th=%.1fGbps\", $tor, $best, $bestmean/1e9}"
}

summary=()
for i in 0 1 2 3 4 5; do
    eps=${EPS[$i]}
    tag=$(tag_of "$i" "$eps")
    out="$RAW/evaluation-lpcc-sens-eps-${tag}.out"
    burst="$DATA/result-${tag}.burst"

    marker=""
    [ "$i" = "2" ] && marker="   <-- 参考点 (基准 2MB)"
    echo "  [$tag]$marker"

    if [ ! -s "$out" ]; then
        echo "    [跳过] 原始输出缺失或为空: $out"
        summary+=("$tag : MISSING .out")
        continue
    fi

    # 1) 过滤: 自动选繁忙出口
    sel=$(filter_burst_busiest "$out" "$burst")
    nlines=$(wc -l < "$burst" 2>/dev/null || echo 0)
    if [ "$nlines" -eq 0 ]; then
        echo "    [跳过] 过滤后 .burst 为空 ($sel)"
        summary+=("$tag : EMPTY .burst")
        continue
    fi
    echo "    选口 [$sel] -> $burst  ($nlines 行)"

    # 2) 绘图: 伪装成 plot-burst.py 期望的 result-lpcc.burst, 在独立 CWD 内运行(不改脚本)
    cp -f "$burst" "$WORK/results_burst/result-lpcc.burst"
    rm -f "$WORK"/plot_burst/lpcc-* 2>/dev/null
    ( cd "$WORK" && python3 "$PLOTPY" ) > "$WORK/plot.log" 2>&1
    if [ $? -ne 0 ]; then
        echo "    [警告] plot-burst.py 失败, 见 $WORK/plot.log"
        summary+=("$tag : PLOT FAILED")
        continue
    fi

    # 3) 搬运并按 tag 改名
    [ -f "$WORK/plot_burst/lpcc-new.png" ]       && cp -f "$WORK/plot_burst/lpcc-new.png"       "$PLOTS/${tag}.png"
    [ -f "$WORK/plot_burst/lpcc-burst-new.pdf" ] && cp -f "$WORK/plot_burst/lpcc-burst-new.pdf" "$PLOTS/${tag}.pdf"
    [ -f "$WORK/plot_burst/lpcc-power-new.png" ] && cp -f "$WORK/plot_burst/lpcc-power-new.png" "$PLOTS/${tag}-zoom.png"
    echo "    绘图 -> $PLOTS/${tag}.png / .pdf"
    summary+=("$tag : OK")
done

# ----------------------------------------------------------------------------
# 阶段 C: 收尾 + 稳态指标小表
# ----------------------------------------------------------------------------
rm -f "$WORK"/results_burst/result-lpcc.burst "$WORK"/plot_burst/lpcc-* "$WORK"/plot.log 2>/dev/null

echo ""
echo "=================================================================="
echo "  完成。阶梯图片位于: $PLOTS/"
echo "  (按文件名排序即 epsilon 升序; 参考点 = cell-2__eps2000000; 建议 2 行 x 3 列排版)"
echo "=================================================================="
printf '  %s\n' "${summary[@]}"
echo ""
echo "  阶梯:"
echo "    cell-0 = 0.5MB   cell-1 = 1MB   [cell-2 = 2MB 参考]   cell-3 = 4MB   cell-4 = 8MB   cell-5 = 16MB"

echo ""
echo "  稳态指标 (繁忙出口, t>0.5s): 吞吐(Gbps) / qlen均值(MB) / qlen峰值(MB)"
printf "  %-26s %8s %10s %10s\n" "组合" "吞吐" "qlen均值" "qlen峰值"
for i in 0 1 2 3 4 5; do
    eps=${EPS[$i]}
    tag=$(tag_of "$i" "$eps")
    burst="$DATA/result-${tag}.burst"
    [ -s "$burst" ] || continue
    awk -v lbl="eps${eps}" '
      $12>0.5 { th+=$6; q+=$10; n++; if($10>mx)mx=$10 }
      END { if(n>0) printf "  %-26s %8.1f %10.1f %10.1f\n", lbl, th/n/1e9, q/n/1e6, mx/1e6 }' "$burst"
done
