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

results="./results_burst/"
plots_dir="./plot_burst/"
os.makedirs(plots_dir,exist_ok=True)
plt.rcParams.update({'font.size': 18})

algs=list(["dcqcn"])
algnames={}
algnames["dcqcn"]="DCQCN"

#%%

######### BURST ###############

plt.rcParams.update({'font.size': 20})

figlegend = pylab.figure(figsize=(11.5,1.5))
lenged_elements=list()

# red green blue brownm grey
colorsBurst=list(["#1979a9","red", "#478fb5","tab:brown","tab:gray"])
labels=list(['Throughput','Qlen'])

for i in range(1,3):
    lenged_elements.append(Line2D([0],[0], color=colorsBurst[i-1],lw=3, label=labels[i-1]))

for alg in algs:

    df = pd.read_csv(results+'result-'+alg+'.burst',delimiter=' ',usecols=[5,9,11,13],names=["th","qlen","time","power"])

    fig,ax = plt.subplots(1,1)
    # 调整子图边距，减少内部空白（可选，进一步优化）
    plt.subplots_adjust(left=0.12, right=0.9, top=0.9, bottom=0.12)
    ax.xaxis.grid(True,ls='--')
    ax.yaxis.grid(True,ls='--')
    ax1=ax.twinx()
    ax.set_yticks([10e9,25e9,40e9,55e9,70e9,85e9,100e9])
    ax.set_yticklabels(["10","25","40","55","70","85","100"])
    ax.set_ylabel("Throughput (Gbps)")

    start=0.17
    xtics=[i*0.0002+start for i in range(0,8)]
    print (xtics)
    ax.set_xticks(xtics)
    xticklabels=[str(i) for i in range(0,24,4)]
    print (xticklabels)
    ax.set_xticklabels(xticklabels)

    ax.set_xlabel("Time (ms)")
    ax.set_xlim(0.16995,0.17105)
    ax.plot(
        df["time"].dropna().to_numpy(),
        df["th"].dropna().to_numpy(),
        label="Throughput",
        c='#1979a9',
        lw=2
    )
    x1 = 0.17001
    x2 = 0.17026
    x3 = 0.17104

    ax.axvline(x=x1, linestyle='--', color='orange', alpha=0.8, linewidth=2)
    ax.axvline(x=x2, linestyle='--', color='orange', alpha=0.8, linewidth=2)
    ax.axvline(x=x3, linestyle='--', color='orange', alpha=0.8, linewidth=2)

    y_arrow = 40e9  # 箭头线的y坐标（可根据需要调整）
    y_text = y_arrow - 5e9  # 文字在箭头下方的偏移量

    # 4. 第一条箭头线（x1-x2之间）+ 文字标注 "Reaction delay"
    # annotate绘制双箭头水平线：arrowstyle='<->' 表示两端箭头
    ax.annotate(
        '',  # 注释文字先留空，后续单独加
        xy=(x2, y_arrow),  # 箭头终点
        xytext=(x1, y_arrow),  # 箭头起点
        arrowprops=dict(arrowstyle='<->', color='xkcd:orange', linewidth=2, mutation_scale=6)
    )
    # 添加文字 "Reaction delay"（居中对齐）
    ax.text(
        (x1 + x2) / 2,  # 文字x坐标（箭头中间）
        y_text,  # 文字y坐标（箭头下方）
        'Reaction\ndelay',
        ha='center',  # 水平居中
        va='center',  # 垂直居中
        fontsize=14,
        color='black',
        weight='bold'
    )

    ax.annotate(
        '',
        xy=(x3, y_arrow - 30e9),
        xytext=(x2, y_arrow - 30e9),
        arrowprops=dict(arrowstyle='<->', color='xkcd:orange', linewidth=2, mutation_scale=6)
    )
    # 添加文字（注意长文本可调整字体大小）
    ax.text(
        (x2 + x3) / 2,
        y_arrow - 26.5e9,
        'Long-term under-utilization',
        ha='center',
        va='center',
        fontsize=14,  # 长文本适当缩小字体
        color='black',
        weight='bold'
    )

    ax.legend(handles=lenged_elements,loc='upper right',ncol=1, framealpha=0,fontsize=18)

    ax1.set_ylim(0,5)
    ax1.set_ylabel("Queue length (MB)")
    ax1.plot(
        df["time"].dropna().to_numpy(),
        (df["qlen"].dropna().to_numpy()) / 1000000 * 2,
        c='r',
        label="Qlen",
        lw=2
    )
    fig.tight_layout()  # 自动调整子图布局，避免标签重叠
    # 保存图片时添加去除白边的关键参数
    fig.savefig(plots_dir+alg+'-burst.pdf', bbox_inches='tight', pad_inches=0)
    fig.savefig(plots_dir+alg+'.png', bbox_inches='tight', pad_inches=0, dpi=300)

    fig1,ax2 = plt.subplots(1,1)
    # 调整子图边距
    plt.subplots_adjust(left=0.12, right=0.9, top=0.9, bottom=0.12)
    ax2.xaxis.grid(True,ls='--')
    ax2.yaxis.grid(True,ls='--')
    ax3=ax2.twinx()
    ax2.set_yticks([10e9,25e9,40e9,55e9,70e9,850e9,100e9])
    ax2.set_yticklabels(["10","25","40","55","70","85","100"])
    ax2.set_ylabel("Throughput (Gbps)")

    start=0.15
    xtics=[i*0.001+start for i in range(0,6)]
    ax2.set_xticks(xtics)
    xticklabels=[str(i) for i in range(0,6)]
    ax2.set_xticklabels(xticklabels)
    ax2.set_xlabel("Time (ms)")
    ax2.set_xlim(0.1495,0.154)
    ax2.plot(
        df["time"].dropna().to_numpy(),
        df["th"].dropna().to_numpy(),
        label="Throughput",
        c='#1979a9',
        lw=2
    )
    ax3.set_ylabel("Normalized Power")
    ax3.set_ylim(0,2)
    ax3.plot(
        df["time"].dropna().to_numpy(),
        df["power"].dropna().to_numpy(),
        c='g',
        label="NormPower",
        lw=2
    )
    fig1.tight_layout()
    # 保存图片时添加去除白边的关键参数
    fig1.savefig(plots_dir+alg+'-power.pdf', bbox_inches='tight', pad_inches=0)
    fig1.savefig(plots_dir+alg+'-power.png', bbox_inches='tight', pad_inches=0, dpi=300)

# 处理图例图片的白边
figlegend.tight_layout()
figlegend.legend(handles=lenged_elements,loc=9,ncol=2, framealpha=0,fontsize=38)
figlegend.savefig(plots_dir+'burst-legend.pdf', bbox_inches='tight', pad_inches=0)