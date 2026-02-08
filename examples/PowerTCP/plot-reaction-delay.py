import numpy as np
import matplotlib.pyplot as plt
import pylab
from matplotlib.lines import Line2D

# 绘图保存目录
plots_dir = "./plot_workload/"
# 确保目录存在
import os
os.makedirs(plots_dir, exist_ok=True)

# 设置全局字体大小（沿用原配置）
plt.rcParams.update({'font.size': 24})

# ====================== 核心配置（沿用原样式，仅调整算法相关映射） ======================
# 算法名称映射（完全沿用）
algs = list(["dcqcn", "I/E-lpcc", "2p-lpcc", "beta-lpcc", "lpcc"])
algnames = {}
algnames["dcqcn"] = "DCQCN"
algnames["I/E-lpcc"] = "I/E-LPCC"
algnames["2p-lpcc"] = "2P-LPCC"
algnames["lpcc"] = "LPCC"
algnames["beta-lpcc"] = r"$\delta$-LPCC"

# 标记样式配置（完全沿用）
markers = {}
markers["dcqcn"] = "x"
markers["I/E-lpcc"] = "*"
markers["2p-lpcc"] = ">"
markers["beta-lpcc"] = "."
markers["lpcc"] = "^"

# 颜色配置（完全沿用）
colors = {}
colors["dcqcn"] = 'xkcd:grass green'
colors["I/E-lpcc"] = 'blue'
colors["2p-lpcc"] = 'xkcd:purple'
colors["beta-lpcc"] = 'xkcd:brown'
colors["lpcc"] = 'xkcd:orange'

# ====================== 横坐标配置（修改为负载大小） ======================
# 负载值（横轴核心数据）
loads = [10, 25, 40, 65, 80]  # 负载百分比：10%、25%、40%、65%、80%
load_index = np.arange(len(loads))  # 横坐标索引
load_labels = ["10%", "25%", "40%", "65%", "80%"]  # 横轴显示标签

# ====================== 手动输入数据区域（请替换为你的实际数据） ======================
# 数据结构：latency[算法] = [10%负载时延, 25%负载时延, 40%负载时延, 65%负载时延, 80%负载时延]
latency = {}
# 初始化每个算法的时延数据列表
for alg in algs:
    latency[alg] = []

# ---------------------- 请在这里手动输入你的实际数据 ----------------------
# 示例数据（仅作占位，务必替换为你的真实响应时延值，单位：ms）
latency["dcqcn"] = [2.56, 8.43, 17.35, 24.81, 35.79]
latency["I/E-lpcc"] = [2.25, 7.57, 12.14, 20.82, 31.58]   # 10%、25%、40%、65%、80%负载下的时延
latency["2p-lpcc"] = [1.17, 5.35, 8.92, 15.2, 24.8]
latency["beta-lpcc"] = [1.12, 3.54, 4.74, 9.97, 17.25]
latency["lpcc"] = [0.76, 1.58, 3.65, 7.86, 9.84]

# ====================== 绘图逻辑（适配新的横轴和纵轴） ======================
# 创建图例单独保存（样式沿用）
figlegend = pylab.figure(figsize=(32.5, 1.5))
legend_elements = list()
for alg in algs:
    latency[alg] = [ele / 2 for ele in latency[alg]]
    legend_elements.append(
        Line2D([0], [0], color=colors[alg], marker=markers[alg], lw=10, markersize=30, label=algnames[alg]))

# 创建主绘图窗口
fig, ax = plt.subplots(1, 1, figsize=(10, 6))  # 可根据需要调整图表尺寸

# 设置坐标轴标签
ax.set_xlabel("Load (%)")  # 横轴：负载百分比
ax.set_ylabel("Reaction Delay (ms)")  # 纵轴：响应时延（ms）

# ax.yaxis.set_label_coords(-0.1, 0.43)

# 设置横轴刻度和标签
ax.set_xticks(load_index)
ax.set_xticklabels(load_labels)

# 可选：设置纵轴范围（根据你的实际数据调整，示例范围0-10ms）
max_latency = 20
ax.set_ylim(0, max_latency)
# 2. 设置刻度：从0到max_latency，每8ms一个刻度
ax.set_yticks(np.arange(0, max_latency + 1, 5))
# 3. 可选：让刻度标签显示更清晰
ax.set_yticklabels(np.arange(0, max_latency + 1, 5))

# 绘制每个算法的曲线（沿用原样式）
for alg in algs:
    ax.plot(load_index, latency[alg],
            label=algnames[alg],
            marker=markers[alg],
            lw=2,           # 线宽
            markersize=14,  # 标记大小
            c=colors[alg])  # 颜色
ax.legend(
    loc='upper left',        # 图例位置（左上，避免遮挡曲线）
    framealpha=0,            # 图例背景透明度（0=完全透明）
    ncol=1,                  # 列数（1列纵向/4列横向，按需调整）
    frameon=False,           # 隐藏图例边框
    markerscale=1.2          # 图例中标记的大小缩放
)

# 调整布局（防止标签重叠）
fig.tight_layout()

# 保存图片（支持pdf和png格式）
fig.savefig(f"{plots_dir}latency_vs_load.pdf", bbox_inches='tight')
fig.savefig(f"{plots_dir}latency_vs_load.png", dpi=300, bbox_inches='tight')

# 保存图例
figlegend.tight_layout()
figlegend.legend(handles=legend_elements, loc=9, ncol=4, framealpha=0, fontsize=52)
# figlegend.savefig(f"{plots_dir}latency-legend.pdf", bbox_inches='tight')

# 可选：显示图表（运行时查看）
# plt.show()