#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Created on Mon Mar  1 06:46:30 2021

@author: vamsi
"""
import os
import requests
import pandas as pd
import numpy as np
import math
import matplotlib.pyplot as plt
import pylab
from matplotlib.lines import Line2D

NS3="./"
plots_dir="./plot_fairness/"
os.makedirs(plots_dir,exist_ok=True)
# plots_dir="/home/vamsi/Powertcp-NSDI/"
plt.rcParams.update({'font.size': 18})


# algs=list(["dcqcn", "powerInt", "hpcc", "powerDelay", "timely", "dctcp", "gemini"])
# algnames={}
# algnames["dcqcn"]="DCQCN"
# algnames["powerInt"]="PowerTCP"
# algnames["hpcc"]="HPCC"
# algnames["powerDelay"]=r'$\theta-PowerTCP$'
# algnames["timely"]="TIMELY"
# algnames["gemini"]="GEMINI"
# algnames["DCTCP"]="DCTCP"

# algs=list(["dcqcn"])

# algnames={}
# algnames["dcqcn"]="DCQCN"

# algs=list(["timely"])
# algnames={}
# algnames["timely"]="TIMELY"

algs=list(["dcqcn", "timely", "dctcp", "hpcc", "powertcp", "lpcc", "gemini", "bbr", "bicc", "themis"])
algnames={}
algnames["dcqcn"]="DCQCN"
algnames["timely"]="TIMELY"
algnames["dctcp"]="DCTCP"
algnames["hpcc"]="HPCC"
algnames["powertcp"]="PowerTCP"
algnames["lpcc"]="LPCC"
algnames["gemini"]="GEMINI"
algnames["bbr"]="BBR"
algnames["bicc"]="BICC"
algnames["themis"]="THEMIS"

# algs=list(["dcqcn", "gemini", "bicc", "lpcc", "hpcc"])
# algnames={}
# algnames["dcqcn"]="DCQCN"
# algnames["gemini"]="GEMINI"
# algnames["bicc"]="BICC"
# algnames["lpcc"]="LPCC"
# algnames["hpcc"]="HPCC"


######## FAIRNESS #############

# algs=list(["lpcc"])
# algs=list(["dcqcn", "powerInt", "hpcc", "powerDelay", "timely", "dctcp"])
results=NS3+"results_fairness/"

plt.rcParams.update({'font.size': 30})

figlegend = pylab.figure(figsize=(20.5,1.5))
lenged_elements=list()

colorsFair=list(["#d65151","#7ab547", "#478fb5","tab:brown","tab:gray"])
labels=list(['flow-1','flow-2','flow-3','flow-4','flow-5'])

for i in range(1,5):
    lenged_elements.append(Line2D([0],[0], color=colorsFair[i-1],lw=6, label=labels[i-1]))


for alg in algs:

    # Skip algorithms whose parsed result files are missing — lets us
    # plot one CC at a time without crashing on the others.
    missing = [k for k in (1, 2, 3, 4)
               if not os.path.exists(results + f"result-{alg}.{k}")]
    if missing:
        print(f"[skip] {alg}: missing result files {missing}")
        continue

    fig,ax = plt.subplots(1,1)
    ax.xaxis.grid(True,ls='--')
    ax.yaxis.grid(True,ls='--')
    
    ax.set_ylabel("Throughput (Gbps)")
    ax.set_xlabel("Time (s)")
    # fig.suptitle(alg)
    
    df1 = pd.read_csv(results+'result-'+alg+'.1',delimiter=' ',usecols=[5,7],names=["th","time"])
    df2 = pd.read_csv(results+'result-'+alg+'.2',delimiter=' ',usecols=[5,7],names=["th","time"])
    df3 = pd.read_csv(results+'result-'+alg+'.3',delimiter=' ',usecols=[5,7],names=["th","time"])
    df4 = pd.read_csv(results+'result-'+alg+'.4',delimiter=' ',usecols=[5,7],names=["th","time"])

    # Clip measured throughput at WAN line rate (100Gbps). Values above this
    # are NIC TX-counter artifacts (sender-side accounting includes packets
    # enqueued at the NIC before they physically egress; over short polling
    # windows this can briefly exceed line rate). Physical RX at receiver
    # never exceeds 100Gbps; we cap on display.
    LINE_RATE_BPS = 100e9
    for df in (df1, df2, df3, df4):
        df["th"] = df["th"].clip(upper=LINE_RATE_BPS)
    
    ax.set_xlim(0, 1.4)
    xticks = [0, 0.4, 0.8, 1.2]
    ax.set_xticks(xticks)
    ax.set_xticklabels(xticks)

    ax.set_ylim(0, 110)
    
    # ax.plot(df1["time"][::100],df1["th"][::100]/1e9)
    # ax.plot(df2["time"][::100],df2["th"][::100]/1e9)
    # ax.plot(df3["time"][::100],df3["th"][::100]/1e9)
    # ax.plot(df4["time"][::100],df4["th"][::100]/1e9)
    
    # ax.plot(df1["time"],df1["th"]/1e9,c=colorsFair[0])
    # ax.plot(df2["time"],df2["th"]/1e9,c=colorsFair[1])
    # ax.plot(df3["time"],df3["th"]/1e9,c=colorsFair[2])
    # ax.plot(df4["time"],df4["th"]/1e9,c=colorsFair[3])

    ax.plot(df1["time"].dropna().to_numpy(), (df1["th"]/1e9).dropna().to_numpy(), c=colorsFair[0], label=labels[0])
    ax.plot(df2["time"].dropna().to_numpy(), (df2["th"]/1e9).dropna().to_numpy(), c=colorsFair[1], label=labels[1])
    ax.plot(df3["time"].dropna().to_numpy(), (df3["th"]/1e9).dropna().to_numpy(), c=colorsFair[2], label=labels[2])
    ax.plot(df4["time"].dropna().to_numpy(), (df4["th"]/1e9).dropna().to_numpy(), c=colorsFair[3], label=labels[3])

    # ax.legend(
    #     loc='upper center',        # 图例锚点
    #     # 关键2：改用画布坐标（bbox_transform=fig.transFigure），避免依赖子图坐标
    #     bbox_to_anchor=(0.5, 0.95),# 画布坐标：x=0.5（水平居中），y=0.95（画布顶部下方）
    #     bbox_transform=fig.transFigure, # 明确坐标体系为画布（0-1范围）
    #     ncol=4,                    # 4列排列
    #     frameon=False,             # 无边框
    #     fontsize=22,               # 字体大小
    #     handlelength=1.5,          # 缩短图例线条长度，减少横向占比
    #     columnspacing=1,         # 减小列间距，避免横向挤压
    #     handletextpad=0.5,         # 线条与文字的间距，优化紧凑度
    #     borderaxespad=0            # 图例与子图的间距
    # )
    
    # # 关键3：先调边距，再用tight_layout，且指定rect参数限制子图范围
    # plt.subplots_adjust(top=1.1)  # 子图顶部占画布80%，上方20%留给图例
    # # rect参数：[左, 下, 宽, 高]，限制子图在画布的0-1范围，避免tight_layout覆盖边距
    # fig.tight_layout(rect=[0, 0, 1.1, 0.92])  

    fig.tight_layout()
    fig.savefig(plots_dir+alg+ '-fairness-new' + '.pdf', bbox_inches='tight', pad_inches=0)
    fig.savefig(plots_dir+alg+'-new.png', bbox_inches='tight', pad_inches=0, dpi=300)

    # ---------------- Jain's fairness index ----------------
    # Align by truncating to the shortest sample series across the 4 flows.
    th_arrays = [df1["th"].dropna().to_numpy(),
                 df2["th"].dropna().to_numpy(),
                 df3["th"].dropna().to_numpy(),
                 df4["th"].dropna().to_numpy()]
    t_arrays  = [df1["time"].dropna().to_numpy(),
                 df2["time"].dropna().to_numpy(),
                 df3["time"].dropna().to_numpy(),
                 df4["time"].dropna().to_numpy()]
    T = min(a.shape[0] for a in th_arrays)
    ths   = np.array([a[:T] for a in th_arrays])  # (4, T)
    times = t_arrays[0][:T]

    total = ths.sum(axis=0)
    sqsum = (ths**2).sum(axis=0)
    n_flows = ths.shape[0]
    with np.errstate(invalid="ignore", divide="ignore"):
        jain = np.where(sqsum > 0, (total**2) / (n_flows * sqsum), np.nan)

    # Steady-state mean (after all monitored flows have started: t >= 1.0 s)
    ss_mask  = times >= 1.0
    ss_mean  = float(np.nanmean(jain[ss_mask])) if ss_mask.any() else float("nan")
    ss_min   = float(np.nanmin(jain[ss_mask]))  if ss_mask.any() else float("nan")
    print(f"[jain] {alg}: steady-state (t>=1.0s) mean={ss_mean:.4f}  min={ss_min:.4f}")

    # Dump a CSV alongside the plots so it can be pasted into tables later.
    jain_csv = plots_dir + "jain-" + alg + ".csv"
    pd.DataFrame({"time": times, "jain": jain}).to_csv(jain_csv, index=False)

    # Jain time-series plot
    fig_j, ax_j = plt.subplots(1, 1)
    ax_j.plot(times, jain, color="black", linewidth=2)
    ax_j.set_xlabel("Time (s)")
    ax_j.set_ylabel("Jain's Fairness Index")
    ax_j.set_xlim(0, 1.4)
    ax_j.set_xticks([0, 0.4, 0.8, 1.2])
    ax_j.set_xticklabels([0, 0.4, 0.8, 1.2])
    ax_j.set_ylim(0, 1.05)
    ax_j.axhline(1.0, color="gray", linestyle="--", alpha=0.5)
    ax_j.set_title(f"{algnames[alg]}: steady-state J(t>=1s)={ss_mean:.3f}")
    ax_j.grid(True, ls="--")
    fig_j.tight_layout()
    fig_j.savefig(plots_dir + alg + "-jain.pdf", bbox_inches="tight", pad_inches=0)
    fig_j.savefig(plots_dir + alg + "-jain.png", bbox_inches="tight", pad_inches=0, dpi=300)
    plt.close(fig_j)

figlegend.tight_layout()
figlegend.legend(handles=lenged_elements,loc=9,ncol=5, framealpha=0,fontsize=48)
figlegend.savefig(plots_dir+'/fair-legend-new.pdf')
