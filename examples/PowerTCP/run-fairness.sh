#!/bin/bash
set -euo pipefail  # Enable strict mode: exit on error, undefined variable error, pipe failure error

# ===================== Configuration (Modify paths as needed) =====================
# Script/file paths (keep default if in current directory; use absolute path otherwise)
SIMULATOR_SCRIPT="./script-fairness.sh"
PARSE_SCRIPT="./results-fairness.sh"
PLOT_PY="./plot-fairness.py"

# ===================== Utility Functions =====================
# Error message and exit
error_exit() {
    echo -e "\033[31m[ERROR] $1\033[0m"  # Red text for errors
    exit 1
}

# Info message
info_msg() {
    echo -e "\033[32m[INFO] $1\033[0m"   # Green text for info
}

# ===================== Pre-Checks =====================
info_msg "Starting pre-checks..."

# Check if simulator.sh exists
if [ ! -f "$SIMULATOR_SCRIPT" ]; then
    error_exit "Script not found: $SIMULATOR_SCRIPT. Please verify the path is correct."
fi

# Check if simulator.sh is executable
if [ ! -x "$SIMULATOR_SCRIPT" ]; then
    info_msg "$SIMULATOR_SCRIPT lacks execute permission. Attempting to add it automatically..."
    chmod +x "$SIMULATOR_SCRIPT" || error_exit "Failed to add execute permission. Run manually: chmod +x $SIMULATOR_SCRIPT"
fi

# Check if parse-results.sh exists
if [ ! -f "$PARSE_SCRIPT" ]; then
    error_exit "Script not found: $PARSE_SCRIPT. Please verify the path is correct."
fi

# Check if parse-results.sh is executable
if [ ! -x "$PARSE_SCRIPT" ]; then
    info_msg "$PARSE_SCRIPT lacks execute permission. Attempting to add it automatically..."
    chmod +x "$PARSE_SCRIPT" || error_exit "Failed to add execute permission. Run manually: chmod +x $PARSE_SCRIPT"
fi

# Check if plot.py exists
if [ ! -f "$PLOT_PY" ]; then
    error_exit "File not found: $PLOT_PY. Please verify the path is correct."
fi

# Check if python3 is installed
if ! command -v python3 &> /dev/null; then
    error_exit "python3 is not installed. Install it first with: sudo apt update && sudo apt install -y python3"
fi

info_msg "Pre-checks passed!"

# ===================== Execution Flow =====================
# 1. Run simulator.sh
info_msg "Starting execution: $SIMULATOR_SCRIPT"
"$SIMULATOR_SCRIPT" || error_exit "$SIMULATOR_SCRIPT execution failed!"
info_msg "$SIMULATOR_SCRIPT execution completed!"

# 2. Run parse-results.sh
info_msg "Starting execution: $PARSE_SCRIPT"
"$PARSE_SCRIPT" || error_exit "$PARSE_SCRIPT execution failed!"
info_msg "$PARSE_SCRIPT execution completed!"

# 3. Run plot.py
info_msg "Starting execution: python3 $PLOT_PY"
python3 "$PLOT_PY" || error_exit "plot.py execution failed!"
info_msg "$PLOT_PY execution completed!"

# ===================== Completion =====================
info_msg "All scripts/programs executed successfully!"