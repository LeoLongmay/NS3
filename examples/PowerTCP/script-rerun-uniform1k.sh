#!/usr/bin/env bash
# LPCC at scale: uniform 1KB flows + random staggered arrival.
# Validates that inactivity-based eviction keeps the flow table small even
# when total flow count N is huge — peak active flows << N.
set -e
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/config.sh"

CONFIG_TEMPLATE="$NS3/examples/PowerTCP/config-burst.txt"
OUT_ROOT="$NS3/examples/PowerTCP/dump_burst_overhead_uniform1k"
FLOW_DIR="$NS3/examples/PowerTCP/sweep_flows_uniform1k"
BIN="$NS3/build/examples/PowerTCP/ns3.39-powertcp-evaluation-burst-debug"
PER_RUN_TIMEOUT=5400  # 90 min

cd "$NS3"

alg=lpcc; cc=9; fc=0; tp=0; window=0; wien=false; delay=false

# Take the flow counts as args; default to all five.
if [[ $# -eq 0 ]]; then
    FLOW_COUNTS=(4096 16384 65536 262144 1048576)
else
    FLOW_COUNTS=("$@")
fi

for N in "${FLOW_COUNTS[@]}"; do
    SIM_STOP=0.25  # 0.13 + 0.05 window + headroom

    run_dir="$OUT_ROOT/$alg/$N"
    mkdir -p "$run_dir"
    run_conf="$run_dir/config.txt"
    awk -v flow="$FLOW_DIR/flow-burstExp-$N.txt" \
        -v fct="$run_dir/fct.txt" \
        -v pfc="$run_dir/pfc.txt" \
        -v ftmon="$run_dir/flow_table.txt" \
        -v qlen="$run_dir/qlen.txt" \
        -v stop="$SIM_STOP" '
        $1 == "FLOW_FILE"           { print "FLOW_FILE " flow; next }
        $1 == "FCT_OUTPUT_FILE"     { print "FCT_OUTPUT_FILE " fct; next }
        $1 == "PFC_OUTPUT_FILE"     { print "PFC_OUTPUT_FILE " pfc; next }
        $1 == "FLOW_TABLE_MON_FILE" { print "FLOW_TABLE_MON_FILE " ftmon; next }
        $1 == "QLEN_MON_FILE"       { print "QLEN_MON_FILE " qlen; next }
        $1 == "SIMULATOR_STOP_TIME" { print "SIMULATOR_STOP_TIME " stop; next }
        { print }
    ' "$CONFIG_TEMPLATE" > "$run_conf"

    echo "[*] alg=$alg N=$N SIM_STOP=$SIM_STOP -> $run_dir"
    START=$(date +%s)
    timeout "$PER_RUN_TIMEOUT" "$BIN" \
        --conf="$run_conf" --algorithm="$cc" --transportMode="$tp" \
        --flowControlMode="$fc" --wien="$wien" --delayWien="$delay" \
        --windowCheck="$window" > "$run_dir/run.log" 2>&1 || echo "    (run failed/timeout)"
    END=$(date +%s)
    echo "    elapsed=$((END - START))s, fct=$(wc -l < $run_dir/fct.txt 2>/dev/null), ft=$(wc -l < $run_dir/flow_table.txt 2>/dev/null)"
done
echo "## UNIFORM-1K RUN FINISHED ##"
