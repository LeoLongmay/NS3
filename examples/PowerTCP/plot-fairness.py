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

# algs=list(["dcqcn", "timely", "dctcp", "hpcc", "powertcp", "lpcc", "gemini", "bbr", "bicc"])
# algnames={}
# algnames["dcqcn"]="DCQCN"
# algnames["timely"]="TIMELY"
# algnames["dctcp"]="DCTCP"
# algnames["hpcc"]="HPCC"
# algnames["powertcp"]="PowerTCP"
# algnames["lpcc"]="LPCC"
# algnames["gemini"]="GEMINI"
# algnames["bbr"]="BBR"
# algnames["bicc"]="BICC"

algs=list(["gemini", "bbr", "bicc"])
algnames={}
algnames["gemini"]="GEMINI"
algnames["bbr"]="BBR"
algnames["bicc"]="BICC"


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
    
    # if (alg=="powerDelay"):
        # continue
    
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
    
    ax.set_xlim(0,0.7)
    original_ticks = [0, 0.2, 0.4, 0.6]
    ax.set_xticks(original_ticks)

    target_labels = [0, 0.2, 0.4, 0.6]
    ax.set_xticklabels(target_labels)

    ax.set_ylim(0,52)
    
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

figlegend.tight_layout()
figlegend.legend(handles=lenged_elements,loc=9,ncol=5, framealpha=0,fontsize=48)
figlegend.savefig(plots_dir+'/fair-legend-new.pdf')
