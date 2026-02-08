import numpy as np
import matplotlib.pyplot as plt

# ====================== 全局参数配置（对应MATLAB的参数） ======================
# 清除之前的绘图（对应MATLAB的clc/clear）
plt.close('all')


plots_dir = "./plot_workload/"

# 字体和样式参数
fontsize = 24
linewidth = 1.6
barWidth = 0.5  # 分组柱状图的整体宽度
marksize = 8
density = 10

# 颜色列表（与MATLAB的C完全对应，注意Python索引从0开始）
C = ["#84e184", "#0088ff", "#9900cc", "#ff7700", "#00cccc", "#aa4444", "#000000", "#996633", "#888888"]
color1 = "#3498db"
color2 = "#db3444"
color3 = "#dbcb34"

# ====================== 1. 参数定义（复刻MATLAB逻辑） ======================
# 带宽设置（单位：Gbps）
bandwidth = np.array([50, 100, 200])  # 横轴数据
bandwidth_bps = bandwidth * 1e9  # 转换为bps（比特/秒）

# 延迟假设（单位：秒）
delay_in = 0.05e-3  # 数据中心内部延迟（0.05毫秒）
delay_cross = 3e-3  # 跨数据中心延迟（3毫秒）
delay_long = 10e-3  # 长距离跨数据中心延迟（10毫秒）

# ====================== 2. BDP计算（带宽延迟积） ======================
# BDP = 带宽 * 延迟（单位：比特）
bdp_in_bits = bandwidth_bps * delay_in  # 内部通信BDP（比特）
bdp_cross_bits = bandwidth_bps * delay_cross  # 跨数据中心BDP（比特）
bdp_long_bits = bandwidth_bps * delay_long  # 长距离BDP（比特）

# 转换为MB（1 MB = 8*1024*1024 比特）
bdp_in_MB = bdp_in_bits / (8 * 1024 * 1024)
bdp_cross_MB = bdp_cross_bits / (8 * 1024 * 1024)
bdp_long_MB = bdp_long_bits / (8 * 1024 * 1024)

# 组织数据（对应MATLAB的data矩阵）
# 第一组：固定值[10,10,10]，第二组：bdp_cross_MB，第三组：bdp_long_MB
data1 = np.array([10, 10, 10])  # Intra-datacenter
data2 = bdp_cross_MB  # Inter-datacenter (500~1000KM)
data3 = bdp_long_MB  # Inter-datacenter (>1000KM)

# ====================== 3. 绘制分组柱状图（核心：复刻MATLAB的bar） ======================
# 定义分组柱状图的位置和宽度
x = np.arange(len(bandwidth))  # 横轴基础位置（0,1,2）
bar_width = barWidth / 3  # 每个柱子的宽度（分组数=3）

# 创建画布
fig, ax = plt.subplots()  # 对应MATLAB的figure大小

# 绘制三组柱子（分别对应Intra/Inter/Inter-long）
bar1 = ax.bar(x - bar_width, data1, width=bar_width, color='#1979a9', label='Intra-DC')
bar2 = ax.bar(x, data2, width=bar_width, color='r', label='Inter-DC (<1000KM)')
bar3 = ax.bar(x + bar_width, data3, width=bar_width, color='xkcd:orange', label='Inter-DC (>1000KM)')

# ====================== 4. 图表美化（完全复刻MATLAB） ======================
# 设置横轴标签（显示带宽值）
ax.set_xticks(x)
ax.set_xticklabels(bandwidth)

# 坐标轴标签
ax.set_xlabel('Bandwidth (Gbps)', fontsize=fontsize)
ax.set_ylabel('BDP (MB)', fontsize=fontsize)

# 添加图例
# ax.legend(loc='best', fontsize=fontsize - 2)

# 添加网格线（增强可读性，仅主网格，避免过密）
# ax.grid(True, which='major', alpha=0.3)
# ax.grid(True, which='minor', alpha=0.1)  # 如需细网格可取消注释
ax.xaxis.grid(True, ls='--')
ax.yaxis.grid(True, ls='--')

# 设置坐标轴刻度字体大小
ax.tick_params(axis='both', labelsize=fontsize)

# ====================== 关键修改：设置y轴刻度 ======================
# 1. 设置y轴范围为0到250
ax.set_ylim(0, 250)
# 2. 设置y轴刻度：0,50,100,150,200,250（每50一个刻度）
ax.set_yticks(np.arange(0, 251, 50))  # 结束值251确保包含250

# ====================== 5. 添加数值标签（核心复刻MATLAB逻辑） ======================
temp = np.array([0.40, 0.99, 1.68])  # 第一组柱子的自定义标签值


# 定义添加标签的通用函数
# def add_value_labels(bar, values, fontsize_label):
#     for i, rect in enumerate(bar):
#         height = rect.get_height()
#         # 计算标签位置：柱子中心x，顶部y+偏移
#         x_label = rect.get_x() + rect.get_width() / 2
#         y_label = height + 0.02 * 250  # 偏移基于新的y轴最大值250，保持比例
#
#         # 添加文本标签
#         ax.text(x_label, y_label, f'{values[i]:.2f}',
#                 ha='center', va='bottom',  # 水平居中，垂直底部
#                 fontsize=fontsize_label - 2)
#
#
# # 为三组柱子添加标签（第一组用temp，其他用自身数值）
# add_value_labels(bar1, temp, fontsize - 2)
# add_value_labels(bar2, data2, fontsize - 2)
# add_value_labels(bar3, data3, fontsize - 2)

# ====================== 6. 保存图片（对应MATLAB的print） ======================
plt.tight_layout()  # 自动调整布局，避免标签截断
# 保存为PDF（600dpi）和PNG
plt.savefig(f'{plots_dir}BDP.pdf', dpi=600, bbox_inches='tight')
# plt.savefig('test.png', dpi=300, bbox_inches='tight')

# 显示图形
# plt.show()