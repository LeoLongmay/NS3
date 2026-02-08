#!/usr/bin/env python3
# -*- coding: utf-8 -*-
import os
import matplotlib.pyplot as plt
import pylab
import numpy as np
from matplotlib.lines import Line2D

# 全局字体设置（与参考代码完全对齐，先18后更新为20）
plt.rcParams.update({'font.size': 24})

plots_dir = "./plot_workload/"

# 颜色配置（复用参考代码中的colorsBurst，保证颜色一致性）
colorsBurst = list(["#1979a9", "red", "#478fb5", "tab:brown", "tab:gray"])
# 算法a：#1979a9（蓝）、算法b：red（红）、比值折线：#478fb5（浅蓝）

# ---------------------- 步骤1：构造模拟数据（可替换为真实数据） ----------------------
# 4组数据的组标签
groups = ["20%", "40%", "60%", "80%"]
n_groups = len(groups)

# 算法a和算法b的报文数量（模拟合理数据，可替换为真实CSV数据）
alg_a_counts = np.array([2.82, 3.54, 3.88, 4.47])  # 算法a报文数量
alg_b_counts = np.array([0.75, 0.98, 1.13, 1.42])   # 算法b报文数量
# 计算比值：算法a / 算法b（折线数据）
ratio_a_b = alg_a_counts / alg_b_counts

# ---------------------- 步骤2：设置柱状图位置参数 ----------------------
bar_width = 0.35  # 柱子宽度
x = np.arange(n_groups)  # 每组的基准x坐标

# ---------------------- 步骤3：创建图表和双轴（柱子用左轴，折线用右轴） ----------------------
fig, ax = plt.subplots(1, 1, figsize=(6,4))
# 调整子图边距（与参考代码保持一致）
plt.subplots_adjust(left=0.12, right=0.9, top=0.9, bottom=0.12)

# 开启网格（与参考代码一致：虚线、显示x/y轴网格）
ax.xaxis.grid(True, ls='--')
ax.yaxis.grid(True, ls='--')

# 创建双轴：右侧y轴用于绘制比值折线
ax2 = ax.twinx()

# ---------------------- 步骤4：绘制柱状图（算法a + 算法b） ----------------------
# 算法a柱状图（颜色#1979a9，与参考代码一致）
bar_a = ax.bar(x - bar_width/2, alg_a_counts, bar_width, 
               color=colorsBurst[0], label="fCNP-LPCC")

# 算法b柱状图（颜色red，与参考代码一致）
bar_b = ax.bar(x + bar_width/2, alg_b_counts, bar_width, 
               color=colorsBurst[1], label="CNP-DCQCN")

# ---------------------- 步骤5：绘制比值折线（算法a/算法b） ----------------------
# 折线图（颜色#478fb5，线宽2，与参考代码绘图格式一致）
line_ratio = ax2.plot(x, ratio_a_b, color="xkcd:orange", lw=2, linestyle="--", marker="*", markersize=16, label="Ratio (fCNP/CNP)")

# ---------------------- 步骤6：设置坐标轴标签和刻度 ----------------------
# 左轴（报文数量）设置
ax.set_ylabel("Ops per KByte")
ax.set_xlabel("Network load")
ax.set_xticks(x)
ax.set_xticklabels(groups)
ax.set_ylim(0, 6)
# ax.set_yticklabels(["0", "2k", "4k", "6k"])
# 右轴（比值）设置
ax2.set_ylabel("Ratio")
ax2.set_ylim(0, np.max(ratio_a_b) * 1.2)  # 预留一定顶部空间

# ---------------------- 步骤7：设置图例（与参考代码格式完全对齐） ----------------------
# 构造图例元素（复用参考代码的Line2D方式，保证样式统一）
legend_elements = [
    # 算法a图例（蓝色，实心块）
    Line2D([0], [0], color=colorsBurst[0], lw=0, marker='s', markersize=15, 
           label="LPCC"),
    # 算法b图例（红色，实心块）
    Line2D([0], [0], color=colorsBurst[1], lw=0, marker='s', markersize=15, 
           label="DCQCN"),
    # 比值折线图例（浅蓝，线宽3）
    Line2D([0], [0], color="xkcd:orange", lw=3, linestyle="--", marker="*", markersize=16,
           label="LPCC:DCQCN")
]

# 绘制图例（与参考代码一致：无背景、字体18、右上位置）
# ax.legend(handles=legend_elements, loc='upper left', ncol=1, 
#           framealpha=0, fontsize=18)

# ---------------------- 步骤8：保存图表（与参考代码格式完全对齐） ----------------------
fig.tight_layout()  # 自动调整布局，避免标签重叠
# 保存PDF格式（去除白边）
fig.savefig(plots_dir + "ope-per-byte.pdf", 
            bbox_inches='tight', pad_inches=0)
# 保存PNG格式（高分辨率300dpi，去除白边）
# fig.savefig(plots_dir + "alg_a_b_ratio_bar_line.png", 
            # bbox_inches='tight', pad_inches=0, dpi=300)