#!/usr/bin/env bash
# Sweep LPCC flow-table overhead vs concurrent flow count, across CC modes.
# Serial execution + per-run timeout to make wall-clock cost predictable.

set -e
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/config.sh"

CONFIG_TEMPLATE="$NS3/examples/PowerTCP/config-burst.txt"
OUT_ROOT="$NS3/examples/PowerTCP/dump_burst_overhead"
FLOW_DIR="$NS3/examples/PowerTCP/sweep_flows"
BIN="$NS3/build/examples/PowerTCP/ns3.39-powertcp-evaluation-burst-debug"

algs=("dcqcn" "hpcc" "lpcc")
CCMODE=(1       3      9)
FLOWCTL=(0      0      0)
TRANSPORT=(0    0      0)

# Reduced N range so wall-clock per run stays under timeout.
FLOW_COUNTS=(64 256 1024)
# Tighter sim time. Flow start = 0.13s (template); sim stop = 0.20s -> 70ms of traffic.
SIM_STOP=0.20
PER_RUN_TIMEOUT=900   # 15 minutes per run

mkdir -p "$OUT_ROOT" "$FLOW_DIR"

# Generate 1MB flow files if missing.
python3 "$SCRIPT_DIR/gen-sweep-flows.py" \
    --out_dir "$FLOW_DIR" \
    --sizes "$(IFS=,; echo "${FLOW_COUNTS[*]}")" \
    --bytes 1000000

cd "$NS3"

for idx in "${!algs[@]}"; do
    alg="${algs[$idx]}"
    cc="${CCMODE[$idx]}"
    fc="${FLOWCTL[$idx]}"
    tp="${TRANSPORT[$idx]}"
    wien=false
    delay=false
    window=1
    if [[ "$alg" == "dcqcn" || "$alg" == "lpcc" ]]; then
        window=0
    fi

    for N in "${FLOW_COUNTS[@]}"; do
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

        if ! grep -q "^FLOW_TABLE_MON_FILE " "$run_conf"; then
            echo "FLOW_TABLE_MON_FILE $run_dir/flow_table.txt" >> "$run_conf"
            echo "FLOW_TABLE_MON_INTERVAL_NS 100000" >> "$run_conf"
        fi

        echo "[*] alg=$alg cc=$cc N=$N -> $run_dir (timeout=${PER_RUN_TIMEOUT}s)"
        START=$(date +%s)
        timeout "$PER_RUN_TIMEOUT" "$BIN" \
            --conf="$run_conf" \
            --algorithm="$cc" \
            --transportMode="$tp" \
            --flowControlMode="$fc" \
            --wien="$wien" \
            --delayWien="$delay" \
            --windowCheck="$window" \
            > "$run_dir/run.log" 2>&1 || echo "    (run timed out or failed)"
        END=$(date +%s)
        echo "    elapsed=$((END - START))s, fct lines=$(wc -l < "$run_dir/fct.txt" 2>/dev/null || echo 0), ft lines=$(wc -l < "$run_dir/flow_table.txt" 2>/dev/null || echo 0)"
    done
done

echo "##################################"
echo "#  OVERHEAD SWEEP FINISHED        #"
echo "##################################"
