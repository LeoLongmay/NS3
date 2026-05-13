#!/usr/bin/env bash

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/config.sh"
configFile=$NS3/examples/PowerTCP/config-burst.txt
RES_DUMP=$NS3/examples/PowerTCP/dump_burst

mkdir -p "$RES_DUMP"

# 9 algorithms: DCQCN, timely, DCTCP, HPCC, PowerTCP, LPCC, GEMINI, BBR, BICC
# algs=(0 1 2 3 4 5 6 7 8)
# algNames=("dcqcn" "timely" "dctcp" "hpcc" "powertcp" "lpcc" "gemini" "bbr" "bicc")
# CCMODE=(1 7 8 3 3 9 11 0 12)
# FLOWCTL=(0 0 0 0 0 0 0 0 0)
# TRANSPORT=(0 0 0 0 0 0 0 1 0)

# algs=(0)
# algNames=("gemini")
# CCMODE=(11)
# FLOWCTL=(0)
# TRANSPORT=(0)

# algs=(0)
# algNames=("bbr")
# CCMODE=(0)
# FLOWCTL=(0)
# TRANSPORT=(1)

# algs=(0)
# algNames=("bicc")
# CCMODE=(12)
# FLOWCTL=(0)
# TRANSPORT=(0)

# algs=(0 1 2 3)
# algNames=("dcqcn" "gemini" "bbr" "bicc")
# CCMODE=(1 11 0 12)
# FLOWCTL=(0 0 0 0)
# TRANSPORT=(0 0 1 0)

algs=(0)
algNames=("lpcc")
CCMODE=(9)
FLOWCTL=(0)
TRANSPORT=(0)

# algs=(0)
# algNames=("dcqcn")
# CCMODE=(1)
# FLOWCTL=(0)
# TRANSPORT=(0)

wien=false
delay=false

cd "$NS3"

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
	while [[ $(ps aux | grep "ns3.39-powertcp-evaluation-burst-debug" | grep -v grep | wc -l) -gt 10 ]]; do
		echo "Waiting for cpu cores.... $N-th experiment"
		sleep 60
	done

	echo "evaluation-${algNames[$algorithm]}.out $N"
	N=$((N + 1))
	RESULT_FILE="$RES_DUMP/evaluation-${algNames[$algorithm]}.out"

	CMD=(
		./build/examples/PowerTCP/ns3.39-powertcp-evaluation-burst-debug
		--conf=$configFile
		--algorithm=${CCMODE[$algorithm]}
		--transportMode=${TRANSPORT[$algorithm]}
		--flowControlMode=${FLOWCTL[$algorithm]}
		--wien=$wien
		--delayWien=$delay
		--windowCheck=$window
	)

	# Pin LPCC to tuned parameters; keep other algorithms unchanged.
	# Each --lpcc* knob, when supplied, overrides the matching Attribute default
	# (see src/point-to-point/model/rdma-hw.cc Attribute table). The C++ Attribute
	# defaults are aligned to these values, so this block is an explicit contract
	# rather than a true override.
		if [[ ${algNames[$algorithm]} == "lpcc" ]]; then
			CMD+=(
				# --lpccEpsilon=1000000
				# --lpccThetaUs=3500
				# --lpccFcnpMinIntervalUs=2000
				# --lpccPerFlowFcnpCooldownUs=2000
				# --lpccFcnpTopK=3
				# --lpccFcnpTopKHigh=6
				# --lpccFcnpKHighThreshBytes=4000000
				# --lpccIncreaseIntervalUs=66
				# --lpccIncreaseFactor=0.2
				# --lpccWr=4.0
				# --lpccKr=0.12

				# --lpccEpsilon=1000000
				# --lpccThetaUs=2700
				# --lpccFcnpMinIntervalUs=1000
				# --lpccPerFlowFcnpCooldownUs=1000
				# --lpccFcnpTopK=6
				# --lpccFcnpTopKHigh=10
				# --lpccFcnpKHighThreshBytes=2000000
				# --lpccIncreaseIntervalUs=70
				# --lpccIncreaseFactor=0.18
				# --lpccWr=4.0
				# --lpccKr=0.12

				# v6 (saved):
				# --lpccEpsilon=1500000
				# --lpccThetaUs=1500
				# --lpccFcnpMinIntervalUs=500
				# --lpccPerFlowFcnpCooldownUs=800
				# --lpccFcnpTopK=3
				# --lpccFcnpTopKHigh=10
				# --lpccFcnpKHighThreshBytes=4000000
				# --lpccIncreaseIntervalUs=65
				# --lpccIncreaseFactor=0.10
				# --lpccWr=1.0
				# --lpccKr=0.12

				# v7 (saved):
				# --lpccEpsilon=2000000
				# --lpccThetaUs=3000
				# --lpccFcnpMinIntervalUs=600
				# --lpccPerFlowFcnpCooldownUs=1500
				# --lpccFcnpTopK=3
				# --lpccFcnpTopKHigh=10
				# --lpccFcnpKHighThreshBytes=10000000
				# --lpccIncreaseIntervalUs=65
				# --lpccIncreaseFactor=0.10
				# --lpccWr=2.0
				# --lpccKr=0.12

				# v8 (saved):
				# --lpccEpsilon=2000000
				# --lpccThetaUs=2000
				# --lpccFcnpMinIntervalUs=400
				# --lpccPerFlowFcnpCooldownUs=1200
				# --lpccFcnpTopK=4
				# --lpccFcnpTopKHigh=13
				# --lpccFcnpKHighThreshBytes=5000000
				# --lpccIncreaseIntervalUs=65
				# --lpccIncreaseFactor=0.10
				# --lpccWr=2.0
				# --lpccKr=0.12

				# v9: AI freeze at qRatio>=5 (code-level) + v7 steady params + v8 emergency
				--lpccEpsilon=2000000
				--lpccThetaUs=3000                # back to v7's 3ms (smooth steady state)
				--lpccFcnpMinIntervalUs=500
				--lpccPerFlowFcnpCooldownUs=1500
				--lpccFcnpTopK=3                  # gentle steady-state touch
				--lpccFcnpTopKHigh=13             # keep v8: emergency hits all flows
				--lpccFcnpKHighThreshBytes=5000000  # keep v8: emergency engages early
				--lpccIncreaseIntervalUs=65
				--lpccIncreaseFactor=0.10
				--lpccWr=2.0
				--lpccKr=0.12

			)
		fi

	time "${CMD[@]}" > "$RESULT_FILE" 2>&1 &
done

while [[ $(ps aux | grep "ns3.39-powertcp-evaluation-burst-debug" | grep -v grep | wc -l) -gt 0 ]]; do
	echo "Waiting for cpu cores.... $N-th experiment"
	sleep 5
done

echo "##################################"
echo "#      FINISHED EXPERIMENTS      #"
echo "##################################"
