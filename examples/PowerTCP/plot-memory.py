#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Process rdma: formatted data and plot dual-axis curve graph
"""
import os
import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import pylab

# 配置路径（可根据你的实际数据文件路径修改）
data_file_path = "/home/leo/PowerTCP-RAW/ns-3.39/examples/PowerTCP/dump_workload/evaluation-lpcc-buffer.out"  # 替换为你的实际数据文件路径
plots_dir = "./plot_workload/"
os.makedirs(plots_dir, exist_ok=True)

# 保持与参考代码一致的字体大小配置
plt.rcParams.update({'font.size': 24})

# ---------------------- 步骤1：数据过滤、处理与排序 ----------------------
processed_data = []
with open(data_file_path, 'r', encoding='utf-8') as f:
    for line in f:
        line = line.strip()
        # 过滤以"rdma:"开头的行
        if line.startswith("rdma:"):
            # 去掉"rdma:"前缀并按空格分割数据
            data_parts = line.replace("rdma:", "", 1).strip().split()
            # 验证数据格式是否正确（包含3个数据项：时间戳、总内存、流表内存）
            if len(data_parts) == 3:
                try:
                    timestamp = float(data_parts[0]) / 1000000
                    total_memory = float(data_parts[1]) / 100
                    flow_table_memory = float(data_parts[2])
                    processed_data.append([timestamp, total_memory, flow_table_memory])
                except ValueError:
                    # 跳过数据类型转换失败的行
                    continue

# 转换为DataFrame方便排序和处理
if not processed_data:
    raise ValueError("未找到有效的'rdma:'格式数据，请检查数据文件")

df = pd.DataFrame(processed_data, columns=["timestamp", "total_memory", "flow_table_memory"])

# 按时间戳从小到大排序
df.sort_values(by="timestamp", inplace=True, ascending=True)
df.reset_index(drop=True, inplace=True)

# ---------------------- 步骤2：绘制双轴曲线图（与参考代码样式一致） ----------------------
fig, ax = plt.subplots(1, 1, figsize=(6, 4))

# 添加网格（与参考代码一致，虚线样式）
ax.xaxis.grid(True, ls='--')
ax.yaxis.grid(True, ls='--')

# 创建右纵轴（双轴核心）
# ax1 = ax.twinx()

# 设置轴标签
ax.set_xlabel("Time (ms)")
ax.set_xticklabels([0, 10, 20, 30, 8])
ax.set_ylabel("Used buffer (MB)")
ax.set_yticklabels([-1, 0, 1, 2, 3])
# ax1.set_ylabel("Flow Table Memory (KB)")

# 绘制左纵轴曲线（交换机总内存）- 沿用参考代码的蓝色#1979a9，线宽lw=2
ax.plot(
    df["timestamp"].dropna().to_numpy(),
    df["total_memory"].dropna().to_numpy(),
    label="Switch Total Memory",
    c='#1979a9',
    lw=2
)

# 绘制右纵轴曲线（流表内存）- 沿用参考代码的红色red，线宽lw=2
ax.plot(
    df["timestamp"].dropna().to_numpy(),
    df["flow_table_memory"].dropna().to_numpy(),
    label="Flow Table Memory",
    c='red',
    lw=2
)
# ax1.plot(
#     df["timestamp"].dropna().to_numpy(),
#     df["flow_table_memory"].dropna().to_numpy(),
#     label="Flow Table Memory",
#     c='red',
#     lw=2
# )

# 构建图例元素（与参考代码图例风格一致）
legend_elements = [
    plt.Line2D([0], [0], color='#1979a9', lw=2, label='Total Egress'),
    plt.Line2D([0], [0], color='red', lw=2, label='Flow Table')
]
ax.legend(handles=legend_elements, loc='upper left', ncol=1, framealpha=0, fontsize=20)

# 自动调整布局（与参考代码一致）
fig.tight_layout()

# 保存图片（与参考代码一致，同时保存pdf和png格式）
fig.savefig(plots_dir + "memory_dual_axis.pdf", bbox_inches='tight', pad_inches=0.02)
# fig.savefig(plots_dir + "memory_dual_axis.png")
print(f"图表已保存至 {plots_dir} 目录下")