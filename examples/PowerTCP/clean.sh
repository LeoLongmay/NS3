#!/bin/bash
set -euo pipefail  # Enable strict mode: exit on error, undefined variable, or pipe failure

# ===================== Configuration (MODIFY THESE PATHS!) =====================
# Define the two directories to clear (USE ABSOLUTE PATHS FOR SAFETY)
DIR1="/root/PowerTCP-RAW/simulator/ns-3.39/examples/PowerTCP/plot_fairness"  # e.g., /home/user/simulator_results
DIR2="/root/PowerTCP-RAW/simulator/ns-3.39/examples/PowerTCP/results_fairness" # e.g., /home/user/plot_outputs

# ===================== Utility Functions =====================
# Print error message and exit
error_exit() {
    echo -e "\033[31m[ERROR] $1\033[0m"  # Red text for errors
    exit 1
}

# Print info message
info_msg() {
    echo -e "\033[32m[INFO] $1\033[0m"   # Green text for info
}

# Print warning message
warn_msg() {
    echo -e "\033[33m[WARNING] $1\033[0m" # Yellow text for warnings
}

# ===================== Safety Checks =====================
# Validate directory paths (prevent accidental system directory deletion)
validate_directory() {
    local dir="$1"
    
    # Check if directory exists
    if [ ! -d "$dir" ]; then
        error_exit "Directory does not exist: $dir"
    fi

    # Block critical system directories (add more if needed)
    critical_dirs=("/" "/root" "/home" "/usr" "/var" "/etc")
    for critical in "${critical_dirs[@]}"; do
        if [ "$dir" = "$critical" ] || [[ "$dir" == "$critical/"* ]]; then
            error_exit "Operation blocked: $dir is a critical system directory (risk of data loss)"
        fi
    done
}

# ===================== Main Execution =====================
# Step 1: Validate both directories
info_msg "Starting directory validation..."
validate_directory "$DIR1"
validate_directory "$DIR2"
info_msg "All directories passed safety checks"

# Step 2: Confirmation prompt (prevent accidental execution)
warn_msg "WARNING: This will DELETE ALL FILES AND SUBDIRECTORIES in:"
echo "  - $DIR1"
echo "  - $DIR2"
read -p "Type 'CONFIRM' to proceed (any other input will abort): " confirm_input

if [ "$confirm_input" != "CONFIRM" ]; then
    info_msg "Operation aborted by user"
    exit 0
fi

# Step 3: Clear directories (delete all content but keep the directories themselves)
info_msg "Clearing content from: $DIR1"
rm -rf "$DIR1"/* "$DIR1"/.[!.]* "$DIR1"/..?* 2>/dev/null || true
# Ignore "no such file or directory" errors (if directory is already empty)

info_msg "Clearing content from: $DIR2"
rm -rf "$DIR2"/* "$DIR2"/.[!.]* "$DIR2"/..?* 2>/dev/null || true

# Step 4: Verify cleanup (optional)
info_msg "Verifying cleanup completion..."
if [ -z "$(ls -A "$DIR1" 2>/dev/null)" ] && [ -z "$(ls -A "$DIR2" 2>/dev/null)" ]; then
    info_msg "SUCCESS: Both directories have been cleared completely"
else
    warn_msg "Some files may remain (e.g., hidden system files or read-only files)"
fi