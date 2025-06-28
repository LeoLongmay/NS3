#!/bin/bash

# 基础日志目录
# 为每次运行创建新的日志目录
BASE_LOG_DIR="logs/sac_$(date +"%Y%m%d_%H%M%S")"
mkdir -p "$BASE_LOG_DIR"
LOG_FILE="$BASE_LOG_DIR/execution.log"

while true; do
    # 为每次运行创建新的日志目录
    current_time=$(date +"%Y%m%d_%H%M%S")
    # 设置日志文件路径
    OUTPUT_FILE="$BASE_LOG_DIR/${current_time}_output.log"
    # 记录开始时间
    echo "开始执行时间: $(date)" > "$LOG_FILE"
    
    # 执行SAC程序并重定向输出
    ./waf --run SAC > "$OUTPUT_FILE" 2>&1
    
    # 检查执行状态
    if [ $? -eq 0 ]; then
        echo "SAC程序执行成功" >> "$LOG_FILE"
    else
        echo "SAC程序执行失败，退出代码: $?" >> "$LOG_FILE"
        exit 1
    fi
done