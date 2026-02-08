import numpy as np
import matplotlib.pyplot as plt
from scipy.stats import lognorm
from statsmodels.distributions.empirical_distribution import ECDF

plots_dir = "./plot_workload/"

# ====================== 全局样式配置（字体使用默认） ======================
fontsize = 24
linewidth = 5
marksize = 8  # MATLAB中的marksize，此处暂未用到
# 对应MATLAB的颜色列表C
C = ["#84e184", "#0088ff", "#9900cc", "#ff7700", "#00cccc", "#aa4444", "#000000", "#996633", "#888888"]
color1 = "#3498db"
color2 = "#dbcb34"
color3 = "#db3444"

# ====================== 1. 生成模拟数据（复刻MATLAB逻辑） ======================
n = 10000  # 样本量

# MATLAB的lognrnd(mu, sigma) 对应 scipy的lognorm(s=sigma, scale=np.exp(mu))
# 数据中心内部RTT：对数正态分布
mu_in = np.log(0.2)       # 对数均值
sigma_in = np.log(1.5)    # 对数标准差
rtt_in = lognorm.rvs(s=sigma_in, scale=np.exp(mu_in), size=n)

# 跨数据中心RTT（500KM~1000KM）
mu_out0 = np.log(3)       # 对数均值
sigma_out0 = np.log(2)    # 对数标准差
rtt_out0 = lognorm.rvs(s=sigma_out0, scale=np.exp(mu_out0), size=n)

# 跨数据中心RTT（>1000KM）
mu_out = np.log(7)        # 对数均值
sigma_out = np.log(2)     # 对数标准差
rtt_out = lognorm.rvs(s=sigma_out, scale=np.exp(mu_out), size=n)

# ====================== 2. 计算经验累积分布函数(ECDF) ======================
ecdf_in = ECDF(rtt_in)
x_in = ecdf_in.x
f_in = ecdf_in.y

ecdf_out0 = ECDF(rtt_out0)
x_out0 = ecdf_out0.x
f_out0 = ecdf_out0.y

ecdf_out = ECDF(rtt_out)
x_out = ecdf_out.x
f_out = ecdf_out.y

# ====================== 3. 绘制阶梯型CDF图（对应MATLAB的stairs） ======================
plt.figure()

# 绘制三条CDF曲线
plt.step(x_in, f_in, '-', color='#1979a9', linewidth=linewidth,
         label='Intra-DC')
plt.step(x_out0, f_out0, '--', color='r', linewidth=linewidth,
         label='Inter-DC (<1000KM)')
plt.step(x_out, f_out, '--', color='xkcd:orange', linewidth=linewidth,
         label='Inter-DC (>1000KM)')

# ====================== 4. 美化图形 + 调整图例大小 ======================
plt.xlabel('RTT (ms)', fontsize=fontsize)
plt.ylabel('CDF', fontsize=fontsize)

# 调整图例大小：降低字体大小，优化布局参数让图例更紧凑
# 关键修改：fontsize减小到16，handlelength缩短线条长度，handletextpad减小间距
# legend = plt.legend(
#     loc='lower right',
#     fontsize=16,          # 原22→16，显著缩小字体
#     handlelength=1.5,     # 图例中线条手柄长度（默认2.0）
#     handletextpad=0.5,    # 线条与文字的间距（默认0.8）
#     frameon=False,         # 显示图例边框（可选）
#     borderaxespad=0.2     # 图例与坐标轴的间距
# )

# 网格线与坐标轴设置
# plt.grid(True, which='major', alpha=0.3)
plt.grid(True, ls='--')
# ax.yaxis.grid(True, ls='--')
plt.minorticks_on()
plt.xlim([0, 30])
plt.ylim([0, 1])
plt.gca().tick_params(axis='both', labelsize=fontsize)

# ====================== 5. 单独保存图例为图片 ======================
# 创建新的空白画布
# fig_legend = plt.figure(figsize=(8, 2))  # 调整画布大小适配图例
# # # 将主图的图例提取并添加到新画布
# fig_legend.legend(
#     legend.legendHandles,  # 图例线条/标记
#     [t.get_text() for t in legend.get_texts()],  # 图例文字
#     loc='center',          # 居中显示
#     ncol=3,                # 横向排列（可选，根据需求调整）
#     fontsize=16,
#     handlelength=1.5,
#     handletextpad=0.5,
#     frameon=False
# )
# # # 保存图例（去除多余空白）
# fig_legend.tight_layout()
# fig_legend.savefig('RTT_Legend.pdf', bbox_inches='tight')
# fig_legend.savefig('RTT_Legend.png', dpi=300, bbox_inches='tight')

# ====================== 6. 保存主图（可选隐藏图例） ======================
# 如果主图不需要显示图例，取消下面两行注释
# legend.remove()  # 移除主图中的图例
plt.tight_layout()
plt.savefig(f"{plots_dir}RTT_CDF.pdf", bbox_inches='tight')
# plt.savefig('RTT_CDF_Comparison.png', dpi=300, bbox_inches='tight')

# 显示图形（可选）
# plt.show()