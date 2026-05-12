source config.sh
configFile=$NS3/examples/PowerTCP/config-fairness.txt
RES_DUMP=$NS3/examples/PowerTCP/dump_fairness

mkdir -p "$RES_DUMP"

# 9 algorithms: DCQCN, timely, DCTCP, HPCC, PowerTCP, LPCC, GEMINI, BBR, BICC
algs=(0 1 2 3 4 5 6 7 8)
algNames=("dcqcn" "timely" "dctcp" "hpcc" "powertcp" "lpcc" "gemini" "bbr" "bicc")
CCMODE=(1 7 8 3 3 9 11 0 12)

wien=false
delay=false

cd "$NS3"

N=1
for algorithm in "${algs[@]}"; do
	if [[ ${algNames[$algorithm]} == "powertcp" ]]; then
		wien=true
	else
		wien=false
	fi

	if [[ ${algNames[$algorithm]} == "timely" || ${algNames[$algorithm]} == "dcqcn" || ${algNames[$algorithm]} == "lpcc" || ${algNames[$algorithm]} == "bbr" || ${algNames[$algorithm]} == "bicc" ]]; then
		window=0
	else
		window=1
	fi

	sleep 5
	while [[ $(ps aux | grep "powertcp-evaluation-fairness-optimized" | grep -v grep | wc -l) -gt 10 ]]; do
		echo "Waiting for cpu cores.... $N-th experiment "
		sleep 60
	done

	echo "evaluation-${algNames[$algorithm]}.out $N"
	N=$((N + 1))
	RESULT_FILE="$RES_DUMP/evaluation-${algNames[$algorithm]}.out"
	time ./waf --run "powertcp-evaluation-fairness --conf=$configFile --algorithm=${CCMODE[$algorithm]} --wien=$wien --delayWien=$delay --windowCheck=$window" > "$RESULT_FILE" 2> "$RESULT_FILE" &
done

while [[ $(ps aux | grep "powertcp-evaluation-fairness-optimized" | grep -v grep | wc -l) -gt 0 ]]; do
	echo "Waiting for cpu cores.... $N-th experiment "
	sleep 5
done

echo "##################################"
echo "#      FINISHED EXPERIMENTS      #"
echo "##################################"
