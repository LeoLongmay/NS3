source config.sh
configFile=$NS3/examples/PowerTCP/config-burst.txt
RES_DUMP=$NS3/examples/PowerTCP/dump_burst

mkdir -p "$RES_DUMP"

# 9 algorithms: DCQCN, timely, DCTCP, HPCC, PowerTCP, LPCC, GEMINI, BBR, BICC
algs=(0 1 2 3 4 5 6 7 8)
algNames=("dcqcn" "timely" "dctcp" "hpcc" "powertcp" "lpcc" "gemini" "bbr" "bicc")
CCMODE=(1 7 8 3 3 9 11 0 12)

windowall=$1
if [[ $windowall == "yes" ]]; then
	nowindow="no"
else
	if [[ $2 == "yes" ]]; then
		nowindow="yes"
		windowall="no"
	fi
fi
echo "WindowAll=$windowall NoWindowForAll=$nowindow"

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

	if [[ $windowall == "yes" ]]; then
		window=1
	fi

	if [[ $nowindow == "yes" ]]; then
		window=0
	fi

	sleep 5
	while [[ $(ps aux | grep "powertcp-evaluation-burst-optimized" | grep -v grep | wc -l) -gt 10 ]]; do
		echo "Waiting for cpu cores.... $N-th experiment "
		sleep 60
	done

	echo "evaluation-${algNames[$algorithm]}.out $N"
	N=$((N + 1))
	RESULT_FILE="$RES_DUMP/evaluation-${algNames[$algorithm]}.out"
	time ./waf --run "powertcp-evaluation-burst --conf=$configFile --algorithm=${CCMODE[$algorithm]} --wien=$wien --delayWien=$delay --windowCheck=$window" > "$RESULT_FILE" 2> "$RESULT_FILE" &
done

while [[ $(ps aux | grep "powertcp-evaluation-burst-optimized" | grep -v grep | wc -l) -gt 0 ]]; do
	echo "Waiting for cpu cores.... $N-th experiment "
	sleep 5
done

echo "##################################"
echo "#      FINISHED EXPERIMENTS      #"
echo "##################################"
