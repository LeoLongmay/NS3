source config.sh
RES_DUMP=$NS3/examples/PowerTCP/dump_burst
RES_RESULTS=$NS3/examples/PowerTCP/results_burst

mkdir -p "$RES_DUMP"
mkdir -p "$RES_RESULTS"

# algs=(0 1 2 3 4 5 6 7 8)
# algNames=("dcqcn" "timely" "dctcp" "hpcc" "powertcp" "lpcc" "gemini" "bbr" "bicc")

# algs=(0)
# algNames=("gemini")

# algs=(0)
# algNames=("bbr")

# algs=(0)
# algNames=("bicc")

# algs=(0 1 2 3)
# algNames=("dcqcn" "gemini" "bbr" "bicc")

algs=(0)
algNames=("lpcc")

# algs=(0)
# algNames=("dcqcn")

cd "$NS3"

N=1
for algorithm in "${algs[@]}"; do
	echo "evaluation-${algNames[$algorithm]}.out $N"
	N=$((N + 1))
	RESULT_FILE="$RES_DUMP/evaluation-${algNames[$algorithm]}.out"
	monitor_tor=""
	monitor_port=""
	monitor_scope=""
	monitor_line=$(grep -m1 '^MONITOR_TARGET ' "$RESULT_FILE")
	if [[ -n "$monitor_line" ]]; then
		monitor_tor=$(echo "$monitor_line" | awk '{for(i=1;i<=NF;i++) if($i=="ToRIdx"){print $(i+1)}}')
		monitor_port=$(echo "$monitor_line" | awk '{for(i=1;i<=NF;i++) if($i=="MonitorPort"){print $(i+1)}}')
		monitor_scope=$(echo "$monitor_line" | awk '{for(i=1;i<=NF;i++) if($i=="Scope"){print $(i+1)}}')
	fi
	if [[ -n "$monitor_tor" && "$monitor_tor" != "-1" && "$monitor_port" == "-1" ]]; then
		grep "ToR ${monitor_tor} PortAgg -1 " "$RESULT_FILE" > "$RES_RESULTS/result-${algNames[$algorithm]}.burst"
	elif [[ -n "$monitor_tor" && -n "$monitor_port" && "$monitor_tor" != "-1" && "$monitor_port" != "-1" ]]; then
		grep "ToR ${monitor_tor} Port ${monitor_port} " "$RESULT_FILE" > "$RES_RESULTS/result-${algNames[$algorithm]}.burst"
	else
		echo "Warning: MONITOR_TARGET not found or invalid in $RESULT_FILE, fallback to ToR 0 Port 0" >&2
		grep 'ToR 0 Port 0' "$RESULT_FILE" > "$RES_RESULTS/result-${algNames[$algorithm]}.burst"
	fi
done

echo "##################################"
echo "#      FINISHED PARSING          #"
echo "##################################"
