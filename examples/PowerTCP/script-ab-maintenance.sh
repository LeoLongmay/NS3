#!/usr/bin/env bash
# A/B test: isolate flow-table maintenance overhead on the forwarding path.
# Run DCQCN (does NOT read the flow table) twice per N:
#   on:  FLOW_TABLE_MAINTENANCE 1  -> Insert/Clean still execute (pure overhead)
#   off: FLOW_TABLE_MAINTENANCE 0  -> Insert/Clean compiled out at runtime
# Same workload, same CC, same SIM_STOP. Diff = forwarding cost of maintenance.
set -e
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/config.sh"

CONFIG_TEMPLATE="$NS3/examples/PowerTCP/config-burst.txt"
OUT_ROOT="$NS3/examples/PowerTCP/dump_burst_overhead_ab"
FLOW_DIR="$NS3/examples/PowerTCP/sweep_flows_ab"
BIN="$NS3/build/examples/PowerTCP/ns3.39-powertcp-evaluation-burst-debug"
PER_RUN_TIMEOUT=3600

cd "$NS3"

alg=dcqcn; cc=1; fc=0; tp=0; window=0; wien=false; delay=false

for N in 64 256 1024 4096; do
    if   (( N <= 1024 )); then SIM_STOP=0.20
    else                       SIM_STOP=0.25
    fi
    for maint in on off; do
        run_dir="$OUT_ROOT/${maint}/$N"
        mkdir -p "$run_dir"
        run_conf="$run_dir/config.txt"
        if [[ "$maint" == "on" ]]; then maint_val=1; else maint_val=0; fi
        awk -v flow="$FLOW_DIR/flow-burstExp-$N.txt" \
            -v fct="$run_dir/fct.txt" \
            -v pfc="$run_dir/pfc.txt" \
            -v ftmon="$run_dir/flow_table.txt" \
            -v qlen="$run_dir/qlen.txt" \
            -v stop="$SIM_STOP" \
            -v maint_val="$maint_val" '
            $1 == "FLOW_FILE"           { print "FLOW_FILE " flow; next }
            $1 == "FCT_OUTPUT_FILE"     { print "FCT_OUTPUT_FILE " fct; next }
            $1 == "PFC_OUTPUT_FILE"     { print "PFC_OUTPUT_FILE " pfc; next }
            $1 == "FLOW_TABLE_MON_FILE" { print "FLOW_TABLE_MON_FILE " ftmon; next }
            $1 == "QLEN_MON_FILE"       { print "QLEN_MON_FILE " qlen; next }
            $1 == "SIMULATOR_STOP_TIME" { print "SIMULATOR_STOP_TIME " stop; next }
            $1 == "FLOW_TABLE_MAINTENANCE" { print "FLOW_TABLE_MAINTENANCE " maint_val; next }
            { print }
        ' "$CONFIG_TEMPLATE" > "$run_conf"
        # Append maintenance flag if missing in template.
        grep -q "^FLOW_TABLE_MAINTENANCE " "$run_conf" || echo "FLOW_TABLE_MAINTENANCE $maint_val" >> "$run_conf"

        echo "[*] alg=$alg N=$N maint=$maint SIM_STOP=$SIM_STOP -> $run_dir"
        START=$(date +%s)
        timeout "$PER_RUN_TIMEOUT" "$BIN" \
            --conf="$run_conf" --algorithm="$cc" --transportMode="$tp" \
            --flowControlMode="$fc" --wien="$wien" --delayWien="$delay" \
            --windowCheck="$window" > "$run_dir/run.log" 2>&1 || echo "    (run failed/timeout)"
        END=$(date +%s)
        echo "    elapsed=$((END - START))s, fct=$(wc -l < $run_dir/fct.txt 2>/dev/null), ft=$(wc -l < $run_dir/flow_table.txt 2>/dev/null)"
    done
done
echo "## A/B MAINTENANCE TEST FINISHED ##"
