source config.sh
RES_DUMP=$NS3/examples/PowerTCP/dump_fairness
RES_RESULTS=$NS3/examples/PowerTCP/results_fairness

mkdir -p "$RES_DUMP"
mkdir -p "$RES_RESULTS"

algs=(0 1 2 3 4 5 6 7 8)
algNames=("dcqcn" "timely" "dctcp" "hpcc" "powertcp" "lpcc" "gemini" "bbr" "bicc")

cd "$NS3"

N=1
for algorithm in "${algs[@]}"; do
	echo "evaluation-${algNames[$algorithm]}.out $N"
	N=$((N + 1))
	RESULT_FILE="$RES_DUMP/evaluation-${algNames[$algorithm]}.out"
	grep 'Src 0 Total' "$RESULT_FILE" > "$RES_RESULTS/result-${algNames[$algorithm]}.1"
	grep 'Src 1 Total' "$RESULT_FILE" > "$RES_RESULTS/result-${algNames[$algorithm]}.2"
	grep 'Src 2 Total' "$RESULT_FILE" > "$RES_RESULTS/result-${algNames[$algorithm]}.3"
	grep 'Src 3 Total' "$RESULT_FILE" > "$RES_RESULTS/result-${algNames[$algorithm]}.4"
done

echo "##################################"
echo "#      FINISHED PARSING          #"
echo "##################################"
