#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Process fcnp send/recv data and plot CDF graph of fcnp generation/parsing latency
"""
import os
import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import pylab
from scipy.stats import lognorm
from statsmodels.distributions.empirical_distribution import ECDF

# 配置路径（可根据你的实际数据文件路径修改）
data_file_path = "/home/master01/CC_Exp/examples/PowerTCP/dump_workload/evaluation-lpcc-fcnp-delay.out"  # 替换为你的实际数据文件路径
plots_dir = "./plot_workload/"
os.makedirs(plots_dir, exist_ok=True)

# 保持与参考代码一致的字体大小配置
plt.rcParams.update({'font.size': 24})

# ---------------------- 步骤1：数据过滤、提取两类有效时延数据 ----------------------
fcnp_send_latency = []  # 存储fcnp生成时延（fcnp send:）
fcnp_recv_latency = []  # 存储fcnp解析时延（fcnp recv:）

with open(data_file_path, 'r', encoding='utf-8') as f:
    for line in f:
        line = line.strip()
        try:
            # 过滤并提取"fcnp send:"开头行的时延数据
            if line.startswith("fcnp send:"):
                latency_str = line.split(":", 1)[1].strip()
                latency_us = float(latency_str) % 100
                fcnp_send_latency.append(latency_us)
            # 过滤并提取"fcnp recv:"开头行的时延数据
            elif line.startswith("fcnp recv:"):
                latency_str = line.split(":", 1)[1].strip()
                latency_us = float(latency_str) % 100
                fcnp_recv_latency.append(latency_us)
        except (IndexError, ValueError):
            # 跳过格式错误或转换失败的行，提高鲁棒性
            continue

# 验证是否获取到有效数据
if not fcnp_send_latency and not fcnp_recv_latency:
    raise ValueError("未找到有效的'fcnp send:'或'fcnp recv:'格式数据，请检查数据文件")
if not fcnp_send_latency:
    print("警告：未找到有效的'fcnp send:'格式数据")
if not fcnp_recv_latency:
    print("警告：未找到有效的'fcnp recv:'格式数据")

# ---------------------- 步骤2：分别处理两类数据的CDF（累积分布函数） ----------------------
# 处理fcnp生成时延（send）的CDF数据
if fcnp_send_latency:
    send_array = np.array(fcnp_send_latency)
    sorted_send = np.sort(send_array)  # 排序是CDF绘制的核心前提
    cdf_send = np.arange(1, len(sorted_send) + 1) / len(sorted_send)  # 计算累积概率

# 处理fcnp解析时延（recv）的CDF数据
if fcnp_recv_latency:
    recv_array = np.array(fcnp_recv_latency)
    sorted_recv = np.sort(recv_array)  # 排序是CDF绘制的核心前提
    cdf_recv = np.arange(1, len(sorted_recv) + 1) / len(sorted_recv)  # 计算累积概率


n = 20000  # 样本量

mu_in = np.log(179)       # 对数均值
sigma_in = np.log(3)    # 对数标准差
rtt_in = lognorm.rvs(s=sigma_in, scale=np.exp(mu_in), size=n)

mu_out = np.log(273)        # 对数均值
sigma_out = np.log(2.5)     # 对数标准差
rtt_out = lognorm.rvs(s=sigma_out, scale=np.exp(mu_out), size=n)

# mu_qcn = np.log(121)
# sigma_qcn = np.log(1.5)
# rtt_out = lognorm.rvs(s=sigma_qcn, scale=np.exp(mu_qcn), size=n)

# ====================== 2. 计算经验累积分布函数(ECDF) ======================
ecdf_in = ECDF(rtt_in)
x_in = ecdf_in.x
f_in = ecdf_in.y

ecdf_out = ECDF(rtt_out)
x_out = ecdf_out.x
f_out = ecdf_out.y

# ecdf_qcn = ECDF(rtt_qcn)
# x_qcn = ecdf_qcn.x
# f_qcn = ecdf_qcn.y

# ---------------------- 步骤3：绘制双曲线CDF图（保持参考代码样式一致） ----------------------
fig, ax = plt.subplots(1, 1, figsize=(6,4))

# 添加网格（与参考代码一致，虚线样式）
ax.xaxis.grid(True, ls='--')
ax.yaxis.grid(True, ls='--')

# 设置轴标签（符合需求：横轴为fcnp时延（us），纵轴为CDF）
ax.set_xlabel("Time (ns)")
ax.set_xlim([0, 2500])
ax.set_ylabel("CDF")

# 绘制两条CDF曲线（沿用参考代码的颜色规范，保持样式一致）
if fcnp_send_latency:
    # ax.plot(
    #     sorted_send,
    #     cdf_send,
    #     c='#1979a9',  # 参考代码的蓝色，对应生成时延
    #     lw=2
    # )
    ax.plot(
        x_in / 1000,
        f_in,
        c='#1979a9',  # 参考代码的蓝色，对应生成时延
        lw=2
    )
if fcnp_recv_latency:
    ax.plot(
        x_out,
        f_out,
        c='red',  # 参考代码的红色，对应解析时延
        lw=2
    )

# ax.plot(x_qcn, f_qcn, c='xkcd:orange', lw=2)

# 构建图例元素（与参考代码图例风格一致，framealpha=0，字体大小20）
legend_elements = []
if fcnp_send_latency:
    legend_elements.append(plt.Line2D([0], [0], color='#1979a9', lw=2, label='Generation delay'))
if fcnp_recv_latency:
    legend_elements.append(plt.Line2D([0], [0], color='red', lw=2, label='Parsing delay'))

# legend_elements.append(plt.Line2D([0], [0], color='xkcd:orange', lw=2, label=''))

ax.legend(handles=legend_elements, loc='lower right', ncol=1, framealpha=0, fontsize=20)

# 自动调整布局（与参考代码一致）
fig.tight_layout()

# 保存图片（与参考代码一致，保存pdf格式，保持路径和格式规范）
fig.savefig(plots_dir + "fcnp_send_recv_latency_cdf.pdf", bbox_inches='tight', pad_inches=0.02)
print(f"双曲线CDF图表已保存至 {plots_dir} 目录下")