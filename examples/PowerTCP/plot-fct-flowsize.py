import numpy as np
import matplotlib.pyplot as plt
import pylab
from matplotlib.lines import Line2D

# 绘图保存目录
plots_dir = "./plot_workload/"
# 确保目录存在（如果不需要自动创建，可注释掉这行）
import os
os.makedirs(plots_dir, exist_ok=True)
font_size = 24

# 设置全局字体大小（基础字号，后续所有未单独设置的元素都会继承此值）
plt.rcParams.update({
    'font.size': font_size,
    'xtick.labelsize': font_size,  # 显式设置x轴刻度字号
    'ytick.labelsize': font_size   # 显式设置y轴刻度字号
})

# ====================== 核心配置（可根据需要调整） ======================
# 算法名称映射
algs = list(["dcqcn", "timely", "beta-lpcc", "lpcc",])
algnames = {}
algnames["dcqcn"] = "DCQCN"
algnames["timely"] = "Timely"
algnames["lpcc"] = "LPCC"
algnames["beta-lpcc"] = r"$\delta$-LPCC"

# 标记样式配置
markers = {}
markers["dcqcn"] = "x"
markers["timely"] = "*"
markers["beta-lpcc"] = "."
markers["lpcc"] = "^"

# 颜色配置
colors = {}
colors["dcqcn"] = 'xkcd:grass green'
colors["timely"] = 'xkcd:purple'
colors["beta-lpcc"] = 'xkcd:brown'
colors["lpcc"] = 'xkcd:orange'

# 横坐标配置（流量大小）
flowStep = [0, 5000, 10000, 20000, 30000, 50000, 75000, 100000, 200000, 400000, 600000, 800000, 1000000, 5000000, 10000000, 30000000]
flowSteps = [5000, 10000, 20000, 30000, 50000, 75000, 100000, 200000, 400000, 600000, 800000, 1000000, 5000000, 10000000, 30000000]
fS = np.arange(len(flowSteps))  # 横坐标索引
flowStep_labels = ["250K", "", "1M", "", "2.5M", "", "5M", "", "20M", "", "40M", "", "250M", "", "1.5G"]  # 横坐标显示标签

# ====================== 手动输入数据区域（请替换为你的实际数据） ======================
# 数据结构说明：fcts99[算法][负载] = [对应每个flowStep的99.9分位FCT慢down值]
fcts99 = {}
# 初始化数据字典
for alg in algs:
    fcts99[alg] = {}

# ---------------------- 请在这里手动输入你的数据 ----------------------
# 示例数据（仅作占位，务必替换为你的实际数据）
# 负载0.2的数据
factor = 1.8
# fcts99["dcqcn"]["0.2"] = [11.2, 9.6, 10.5, 9.8, 8.5, 7.9, 6.4, 7.7, 6.8, 8.2, 9.1, 8.5, 9.4, 8.7, 10.0]
# fcts99["timely"]["0.2"] = [12.1, 10.2, 10.4, 9.6, 8.9, 9.2, 7.6, 8.1, 6.6, 8.1, 9.6, 10.1, 8.9, 9.1, 8.6]
# fcts99["beta-lpcc"]["0.2"] = [5.4, 5.0, 6.3, 6.5, 6.8, 5.9, 6.5, 5.8, 6.6, 6.1, 7.2, 7.0, 7.6, 8.1, 6.9]
# fcts99["lpcc"]["0.2"] = [2.7, 4.2, 5.0, 5.7, 5.4, 5.7, 5.5, 6.1, 5.9, 6.2, 5.7, 5.6, 6.4, 5.9, 5.7]

fcts99["dcqcn"]["0.2"] = [11.2, 9.6, 10.5, 9.8, 8.5, 7.9, 6.4, 7.7, 6.8, 8.2, 9.1, 8.5, 9.4, 8.7, 10.0]
fcts99["timely"]["0.2"] = [12.1, 10.2, 10.4, 9.6, 8.9, 9.2, 7.6, 8.1, 6.6, 8.1, 9.6, 10.1, 8.9, 9.1, 8.6]
fcts99["beta-lpcc"]["0.2"] = [5.4, 5.0, 6.3, 6.5, 6.8, 5.9, 6.5, 5.8, 6.6, 6.1, 7.2, 7.0, 7.6, 8.1, 6.9]
fcts99["lpcc"]["0.2"] = [2.7, 4.2, 5.0, 5.7, 5.4, 5.7, 5.5, 6.1, 5.9, 6.2, 5.7, 5.6, 6.4, 5.9, 5.7]

for algo in algs:
    fcts99[algo]["0.2"] = [round(ele * factor, 2) for ele in fcts99[algo]["0.2"]]

# 负载0.6的数据（保留注释，按需启用）
# fcts99["dcqcn"]["0.6"] = [2.0, 2.2, 2.5, 2.9, 3.4, 4.0, 4.7, 5.5, 6.4, 7.4, 8.5, 9.7, 11.0, 12.5, 14.0]
# fcts99["timely"]["0.6"] = [1.8, 2.0, 2.3, 2.7, 3.1, 3.6, 4.2, 4.9, 5.7, 6.6, 7.6, 8.7, 9.9, 11.2, 12.6]
# fcts99["lpcc"]["0.6"] = [1.5, 1.7, 1.9, 2.2, 2.5, 2.9, 3.4, 3.9, 4.5, 5.1, 5.8, 6.5, 7.3, 8.2, 9.1]

# ====================== 绘图逻辑（已统一字号为18） ======================
# 创建图例单独保存（调整画布尺寸适配18号字体）
figlegend = pylab.figure(figsize=(12, 1.5))  # 原32.5过宽，适配18号字体缩小
legend_elements = list()
for alg in algs:
    legend_elements.append(
        Line2D([0], [0], color=colors[alg], marker=markers[alg], lw=10, markersize=30, label=algnames[alg]))

# 为每个负载值绘制单独的图表
for load in ["0.2"]:
    fig, ax = plt.subplots(1, 1, figsize=(10, 6))
    # 坐标轴标签（显式指定字号，确保全局设置生效）
    ax.set_ylabel("99.9-pct FCT Slowdown", fontsize=font_size)
    ax.set_xlabel("Flow size (bytes)", fontsize=font_size)
    ax.set_ylim(3, 25)    # 纵轴范围
    ax.set_xticks(fS)     # 横轴刻度位置
    # ax.set_xticklabels(flowStep_labels, rotation=30)  # 横轴标签（旋转30度防重叠）
    ax.set_xticklabels(flowStep_labels)

    # 绘制每个算法的曲线
    for alg in algs:
        # 从手动输入的字典中获取对应数据
        lfct99 = fcts99[alg][load]
        # 绘制折线图
        ax.plot(fS, lfct99, label=algnames[alg], marker=markers[alg],
                lw=2, markersize=14, c=colors[alg])
        # 打印关键数据（可选）
        print(f"负载{load}下，{algnames[alg]}的第10个数据点值：{lfct99[9]}")

    # 图表内图例（字号统一为18）
    ax.legend(loc='upper center', fontsize=font_size, framealpha=0.8, frameon=False, ncol=2)

    # 调整布局并保存图片
    fig.tight_layout()
    fig.savefig(f"{plots_dir}FCT-{load}.pdf")
    fig.savefig(f"{plots_dir}FCT-{load}.png")

# 保存独立图例（字号改为18，ncol适配4个算法）
# figlegend.tight_layout()
# figlegend.legend(handles=legend_elements, loc='center', ncol=4, framealpha=0, fontsize=18)
# figlegend.savefig(f"{plots_dir}fct-legend.pdf")

# 显示图表（如果需要在运行时查看）
# plt.show()