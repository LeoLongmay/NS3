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

				# v9 (saved):
				# --lpccEpsilon=2000000
				# --lpccThetaUs=3000                # back to v7's 3ms (smooth steady state)
				# --lpccFcnpMinIntervalUs=500
				# --lpccPerFlowFcnpCooldownUs=1500
				# --lpccFcnpTopK=3                  # gentle steady-state touch
				# --lpccFcnpTopKHigh=13             # keep v8: emergency hits all flows
				# --lpccFcnpKHighThreshBytes=5000000  # keep v8: emergency engages early
				# --lpccIncreaseIntervalUs=65
				# --lpccIncreaseFactor=0.10
				# --lpccWr=2.0
				# --lpccKr=0.12

				# v10 (saved): nearly hit target state (400G/<10MB) at t=0-11ms & t=70-79ms,
				# but cyclic collapse-recovery because per-FCNP cut (~48%) > AI growth (1.57x)
				# per cycle. Root cause was hidden defaults: QueueTargetRatio=0.375 (target only
				# 1.5MB at eps=4MB), DropCapHigh=0.5, AiSuppressMultiplier=15.
				# --lpccEpsilon=4000000
				# --lpccThetaUs=1000
				# --lpccFcnpMinIntervalUs=200
				# --lpccPerFlowFcnpCooldownUs=400
				# --lpccFcnpTopK=5
				# --lpccFcnpTopKHigh=24
				# --lpccFcnpKHighThreshBytes=8000000
				# --lpccIncreaseIntervalUs=80
				# --lpccIncreaseFactor=0.04
				# --lpccWr=4.0
				# --lpccKr=0.08

				# v11 (saved, FAILED): route B with both qTgtRatio AND dropCapHigh relaxed.
				# Buffer saturated at 134MB (avg 108MB, th=349G in incast window) like v9.
				# Two simultaneous weakenings (dropCap 0.5->0.3, wr 4.0->2.0) made cuts
				# too gentle for the 23-flow incast burst.
				# --lpccEpsilon=4000000
				# --lpccThetaUs=1000
				# --lpccFcnpMinIntervalUs=200
				# --lpccPerFlowFcnpCooldownUs=400
				# --lpccFcnpTopK=5
				# --lpccFcnpTopKHigh=18
				# --lpccFcnpKHighThreshBytes=8000000
				# --lpccIncreaseIntervalUs=80
				# --lpccIncreaseFactor=0.04
				# --lpccWr=2.0
				# --lpccKr=0.08
				# --lpccQueueTargetRatio=1.5
				# --lpccDropCapHigh=0.3
				# --lpccAiSuppressMultiplier=5

				# v12 (saved): incast hit 400G+ with qlen <50MB, settled qlen <1.3MB but
				# throughput stuck at 246G — AI gain too small to recoup gentle steady-state
				# FCNP cuts in the qRatio < 1 interpolation band.
				# --lpccEpsilon=4000000
				# --lpccThetaUs=1000
				# --lpccFcnpMinIntervalUs=200
				# --lpccPerFlowFcnpCooldownUs=400
				# --lpccFcnpTopK=5
				# --lpccFcnpTopKHigh=24
				# --lpccFcnpKHighThreshBytes=8000000
				# --lpccIncreaseIntervalUs=80
				# --lpccIncreaseFactor=0.04
				# --lpccWr=4.0
				# --lpccKr=0.08
				# --lpccQueueTargetRatio=1.5
				# --lpccDropCapHigh=0.5
				# --lpccAiSuppressMultiplier=5

				# v13 (saved, FAILED): incFactor 0.10 was too aggressive — after queue
				# drained at t=15ms, AI explosion refilled buffer to 134MB cap and
				# pinned there for the rest of the run.
				# --lpccIncreaseFactor=0.10  (rest same as v12)

				# v14 (saved): reached dynamic equilibrium (qlen 8-15MB oscillating around
				# 6MB target, th 240-270G stable) but th stuck at 250G regardless of
				# incFactor. Diagnosis: per-flow FCNP firing 2-3x per theta with 44%
				# cut each, AI 2.07x can't keep up.
				# --lpccPerFlowFcnpCooldownUs=400  (rest same)

				# v15 (saved, FAILED): cooldown 1500us -> queue blew to 134MB cap at sw72.
				# --lpccPerFlowFcnpCooldownUs=1500  (rest same as v14)

				# v14 (saved, qlen<10MB target): qlen avg 10MB peak 22MB,
				# Port 4 (72->74) settled 246G.
				# --lpccQueueTargetRatio=1.5         (rest same as v16)

				# v16 (saved, FAILED): qTgtRatio=3.0 -> target 12MB, dropCapHigh
				# threshold at qlen>=24MB came in too late; burst pushed buffer to
				# 50MB+ before high-cap kicked in, then drifted to 134MB cap.
				# --lpccQueueTargetRatio=3.0  (rest same as v17)

				# v17: bisect qTgtRatio between v14 (1.5, qlen ok / th 246G) and
				# v16 (3.0, qlen blew up).
				#   qTgtRatio = 2.0 -> target 8MB, dropCapHigh at qlen>=16MB.
				# Steady-state qlen 8-16MB gets gentle cuts; ANY excursion to
				# 16-20MB triggers dropCapHigh=0.5 -> hard pullback.
				# Expected: Port 4 ~300-350G with qlen mostly <20MB.
				--lpccEpsilon=4000000
				--lpccThetaUs=1000
				--lpccFcnpMinIntervalUs=200
				--lpccPerFlowFcnpCooldownUs=400
				--lpccFcnpTopK=5
				--lpccFcnpTopKHigh=24
				--lpccFcnpKHighThreshBytes=8000000
				--lpccIncreaseIntervalUs=80
				--lpccIncreaseFactor=0.06
				--lpccWr=4.0
				--lpccKr=0.08
				--lpccQueueTargetRatio=2.0
				--lpccDropCapHigh=0.5
				--lpccAiSuppressMultiplier=5

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
