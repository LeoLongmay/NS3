#!/usr/bin/env bash
# Re-run LPCC with the new 100us inactive threshold (memory plot).
set -e
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/config.sh"

CONFIG_TEMPLATE="$NS3/examples/PowerTCP/config-burst.txt"
OUT_ROOT="$NS3/examples/PowerTCP/dump_burst_overhead"
FLOW_DIR="$NS3/examples/PowerTCP/sweep_flows"
BIN="$NS3/build/examples/PowerTCP/ns3.39-powertcp-evaluation-burst-debug"
PER_RUN_TIMEOUT=3600

cd "$NS3"

alg=lpcc; cc=9; fc=0; tp=0; window=0; wien=false; delay=false

for N in 64 256 1024 4096; do
    if   (( N <= 1024 )); then SIM_STOP=0.20
    else                       SIM_STOP=0.25
    fi

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
echo "##  LPCC RERUN FINISHED  ##"
