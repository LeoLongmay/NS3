import os
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.lines import Line2D

# 图片与本脚本保存在同一目录
script_dir = os.path.dirname(os.path.abspath(__file__))

# 全局字体大小（沿用 plot-reaction-delay.py 配置）
plt.rcParams.update({'font.size': 36})
# legend 字体保持不变(36 号生效前的 32 号)
LEGEND_FONTSIZE = 32

# ====================== 数据：overhead.md 第二张表的第三、五行 ======================
# 横轴：并发流数量 N
N = [64, 256, 1024, 4096, 16384, 65536]
N_index = np.arange(len(N))                       # 等间隔类目横轴（沿用参考脚本做法）
# 横轴用 2 的幂次方显示：64=2^6, 256=2^8, 1024=2^10, 4096=2^12, 16384=2^14, 65536=2^16
N_labels = [r"$2^{6}$", r"$2^{8}$", r"$2^{10}$", r"$2^{12}$", r"$2^{14}$", r"$2^{16}$"]

# 左纵轴：Peak flow-table mem，原始单位 KB，换算为 MB
ft_mem_kb = [1.265, 4.765, 19.718, 348.28, 1052.524, 5026.44]
ft_mem_mb = [v / 1024.0 for v in ft_mem_kb]
# 右纵轴：Peak concurrent flows
peak_flows = [9, 37, 397, 2816, 8959, 41087]

# ====================== 样式配置（沿用参考脚本的调色板/标记） ======================
color_mem = 'xkcd:orange'    # 左轴：流表内存
color_flows = 'blue'         # 右轴：并发流数
marker_mem = '^'
marker_flows = '*'

# ====================== 绘图 ======================
fig, ax1 = plt.subplots(1, 1, figsize=(10, 8))  # 压扁图高（36 号竖排标签约 6.6in 是高度下限）
# 收紧上下边距，让坐标区尽量占满纵向，从而在更矮的图里完整容纳竖排纵轴标签
fig.subplots_adjust(top=0.98, bottom=0.13)
ax2 = ax1.twinx()

# 左纵轴：Peak flow-table mem (MB)
ax1.plot(N_index, ft_mem_mb,
         marker=marker_mem, lw=2, markersize=14, c=color_mem)
# 右纵轴：Peak concurrent flows
ax2.plot(N_index, peak_flows,
         marker=marker_flows, lw=2, markersize=18, c=color_flows)

# 横轴：增大刻度标签与轴的间距(tick pad)及 xlabel 间距
ax1.set_xlabel("Number of flows (N)", labelpad=12)
ax1.set_xticks(N_index)
ax1.set_xticklabels(N_labels)
ax1.tick_params(axis='x', pad=12)

# 左纵轴：线性刻度 0/4/8/12；下限略低于 0，使 0 刻度与 x 轴留间隔
ax1.set_yticks([0, 2, 4, 6])
ax1.set_ylim(-0.2, 6.2)
# 右纵轴（并发流）跨 ~4 个数量级，保持 log 刻度
ax2.set_yscale('log')

# 左右纵轴标签/刻度均为黑色；全图统一 28 号字(竖排长标签靠增大图高来容纳)
ax1.set_ylabel("Peak flow-table mem (MB)", color='black')
ax1.tick_params(axis='y', colors='black')
ax2.set_ylabel("Peak concurrent flows", color='black')
ax2.tick_params(axis='y', colors='black')

# 合并图例（两条曲线，沿用参考脚本 legend 风格）
legend_elements = [
    Line2D([0], [0], color=color_mem, marker=marker_mem, lw=2, markersize=14,
           label="Flow-table mem"),
    Line2D([0], [0], color=color_flows, marker=marker_flows, lw=2, markersize=18,
           label="Concurrent flows"),
]
ax1.legend(handles=legend_elements,
           loc='upper left',
           framealpha=0,
           ncol=1,
           frameon=False,
           markerscale=1.2,
           fontsize=LEGEND_FONTSIZE)  # legend 字号保持不变

# 不调用 tight_layout，避免与 bbox_inches='tight' 在大字号竖排标签上冲突导致截断；
# 仅用 bbox_inches='tight' 自动框住所有元素(含溢出的轴标签)，pad_inches 保持最小空白。
# 保存到脚本同目录（pdf + png）
pdf_path = os.path.join(script_dir, "flowtable-mem-vs-flows.pdf")
png_path = os.path.join(script_dir, "flowtable-mem-vs-flows.png")
fig.savefig(pdf_path, bbox_inches='tight', pad_inches=0.05)
fig.savefig(png_path, dpi=300, bbox_inches='tight', pad_inches=0.05)
print("saved:", png_path)
print("saved:", pdf_path)
