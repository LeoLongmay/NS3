#!/usr/bin/env bash
# Supplementary sweep: run only the NEW larger flow counts (4096, 16384, 32768).
# SIM_STOP is scaled per flow count to ensure all flows finish.

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

FLOW_COUNTS=(4096 16384 32768)
PER_RUN_TIMEOUT=3600   # 60 minutes per run (larger sims)

mkdir -p "$OUT_ROOT" "$FLOW_DIR"

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
        # Scale SIM_STOP based on flow count:
        #   flows start at 0.13s; each host (64 total) sends N/64 flows of 1MB.
        #   Serialization at 100Gbps: N/64 * 80us. Add 2x headroom.
        if   (( N <= 4096 ));  then SIM_STOP=0.25
        elif (( N <= 16384 )); then SIM_STOP=0.40
        else                        SIM_STOP=0.60
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

        if ! grep -q "^FLOW_TABLE_MON_FILE " "$run_conf"; then
            echo "FLOW_TABLE_MON_FILE $run_dir/flow_table.txt" >> "$run_conf"
            echo "FLOW_TABLE_MON_INTERVAL_NS 100000" >> "$run_conf"
        fi

        echo "[*] alg=$alg cc=$cc N=$N SIM_STOP=$SIM_STOP -> $run_dir (timeout=${PER_RUN_TIMEOUT}s)"
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
echo "#  EXTRA SWEEP FINISHED          #"
echo "##################################"
