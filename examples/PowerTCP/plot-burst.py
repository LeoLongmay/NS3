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
# plots_dir="/home/vamsi/Powertcp-NSDI/"
plt.rcParams.update({'font.size': 18})


# algs=list(["dcqcn", "powerInt", "hpcc", "powerDelay", "timely", "dctcp", "lpcc", "gemini"])
# algnames={}
# algnames["dcqcn"]="DCQCN"
# algnames["powerInt"]="PowerTCP"
# algnames["hpcc"]="HPCC"
# algnames["powerDelay"]=r'$\theta-PowerTCP$'
# algnames["timely"]="TIMELY"
# algnames["DCTCP"]="DCTCP"
# algnames["lpcc"]="LPCC"
# algnames["gemini"]="GEMINI"


# algs=list(["dcqcn", "powerInt", "hpcc", "powerDelay", "timely", "dctcp"])
# algnames={}
# algnames["dcqcn"]="DCQCN"
# algnames["powerInt"]="PowerTCP"
# algnames["hpcc"]="HPCC"
# algnames["powerDelay"]=r'$\theta-PowerTCP$'
# algnames["timely"]="TIMELY"
# algnames["DCTCP"]="DCTCP"


# algs=list(["dcqcn"])
# algnames={}
# algnames["dcqcn"]="DCQCN"

# algs=list(["timely"])
# algnames={}
# algnames["timely"]="TIMELY"

algs=list(["dcqcn", "gemini", "bifrost", "bbr"])
algnames={}
algnames["dcqcn"]="DCQCN"
algnames["bifrost"]="Bifrost"
algnames["bbr"]="BBR"
algnames["gemini"]="GEMINI"

#%%

######### BURST ###############

plt.rcParams.update({'font.size': 26})


figlegend = pylab.figure(figsize=(11.5,1.5))
lenged_elements=list()

# red green blue brownm grey
colorsBurst=list(["#1979a9","red", "#478fb5","tab:brown","tab:gray"])
labels=list(['Throughput','Qlen'])

for i in range(1,3):
    lenged_elements.append(Line2D([0],[0], color=colorsBurst[i-1],lw=6, label=labels[i-1]))

for alg in algs:

    df = pd.read_csv(results+'result-'+alg+'.burst',delimiter=' ',usecols=[5,9,11,13],names=["th","qlen","time","power"])

    fig,ax = plt.subplots(1,1)
    # fig.suptitle(alg)
    ax.xaxis.grid(True,ls='--')
    ax.yaxis.grid(True,ls='--')
    ax1=ax.twinx()
    # ax.set_yticks([10e9,25e9,40e9,55e9,70e9,85e9,100e9])
    # ax.set_yticklabels(["10","25","40","55","70","85","100"])
    ax.set_yticks([0, 50e9,100e9,150e9,200e9])
    ax.set_yticklabels(["0","50","100","150","200"])
    ax.set_ylabel("Throughput (Gbps)")

    start=0.14
    xtics=[i*0.01+start for i in range(0,11)]
    ax.set_xticks(xtics)
    xticklabels=[str(i * 10) for i in range(0,11)]
    ax.set_xticklabels(xticklabels)

    ax.set_xlabel("Time (ms)")
    ax.set_xlim(0.1395,0.2)
    # ax.plot(df["time"],df["th"],label="Throughput",c='#1979a9',lw=2)
    ax.plot(
        df["time"].dropna().to_numpy(),
        df["th"].dropna().to_numpy(),
        label="Throughput",
        c='#1979a9',
        lw=2
    )
    ax1.set_ylim(0,5)
    ax1.set_ylabel("Queue length (MB)")
    # ax1.plot(df["time"],df["qlen"]/(1000),c='r',label="Qlen",lw=2)
    ax1.plot(
        df["time"].dropna().to_numpy(),
        (df["qlen"].dropna().to_numpy()) / 1000000,
        c='r',
        label="Qlen",
        lw=2
    )


    # annotate_x = 0.150023839
    # annotate_x1 = 0.150065006
    # annotate_x2 = 0.150137024

    # annotate_x = 0.150050208
    # annotate_x1 = 0.160000837
    # annotate_x2 = 0.170000859    

    # ax.axvline(x=annotate_x, linestyle='--', color='orange', alpha=0.8, linewidth=2)
    # ax.axvline(x=annotate_x1, linestyle='--', color='orange', alpha=0.8, linewidth=2)
    # ax.axvline(x=annotate_x2, linestyle='--', color='orange', alpha=0.8, linewidth=2)

    # closest_idx = np.argmin(np.abs(df["time"].dropna().to_numpy() - annotate_x))
    # closest_idx1 = np.argmin(np.abs(df["time"].dropna().to_numpy() - annotate_x1))
    # closest_idx2 = np.argmin(np.abs(df["time"].dropna().to_numpy() - annotate_x2))


    # annotate_y = df["th"].dropna().to_numpy()[closest_idx]
    # annotate_y1 = df["th"].dropna().to_numpy()[closest_idx1]
    # annotate_y2 = df["th"].dropna().to_numpy()[closest_idx2]
    
    # ax.annotate(
    #     'ECN\nmarked',
    #     xy=(annotate_x, annotate_y),
    #     xytext=(annotate_x + 0.006, annotate_y - 50e9),
    #     arrowprops=dict(
    #         arrowstyle='->',
    #         color='xkcd:orange',
    #         lw=2,
    #         alpha=0.8,
    #         mutation_scale=10,
    #     ),
    #     fontsize=18,
    #     color='black',
    #     weight='bold',
    #     ha='center',
    # )

    # ax.annotate(
    #     'return CNP',
    #     xy=(annotate_x1, annotate_y1),
    #     xytext=(annotate_x1 - 0.007, annotate_y1 - 25e9),
    #     arrowprops=dict(
    #         arrowstyle='->',
    #         color='xkcd:orange',
    #         lw=2,
    #         alpha=0.8,
    #         mutation_scale=10,
    #     ),
    #     fontsize=18,
    #     color='black',
    #     weight='bold',
    #     ha='center',
    # )

    # ax.annotate(
    #     'deceleration',
    #     xy=(annotate_x2, annotate_y2),
    #     xytext=(annotate_x2 - 0.005, annotate_y2 - 50e9),
    #     arrowprops=dict(
    #         arrowstyle='->',
    #         color='xkcd:orange',
    #         lw=2,
    #         alpha=0.8,
    #         mutation_scale=10,
    #     ),
    #     fontsize=18,
    #     color='black',
    #     weight='bold',
    #     ha='center',
    # )
    # ax.legend(loc=1)
    # ax1.legend(loc=3)
    # fig.legend(loc=2,ncol=2,framealpha=0,borderpad=-0.1)
    fig.tight_layout()
    fig.savefig(plots_dir+alg+'-burst.pdf', bbox_inches='tight', pad_inches=0)
    fig.savefig(plots_dir+alg+'.png', bbox_inches='tight', pad_inches=0, dpi=300)

    fig1,ax2 = plt.subplots(1,1)
    # fig.suptitle(alg)
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
    ax2.set_xlim(0.1495,0.169)
    # ax2.plot(df["time"],df["th"],label="Throughput",c='#1979a9',lw=2)
    ax2.plot(
        df["time"].dropna().to_numpy(),
        df["th"].dropna().to_numpy(),
        label="Throughput",
        c='#1979a9',
        lw=2
    )
    ax3.set_ylabel("Normalized Power")
    ax3.set_ylim(0,2)
    # ax3.plot(df["time"],df["power"],c='g',label="NormPower",lw=2)
    ax3.plot(
        df["time"].dropna().to_numpy(),
        df["power"].dropna().to_numpy(),
        c='g',
        label="NormPower",
        lw=2
    )
    fig1.tight_layout()
    fig1.savefig(plots_dir+alg+'-power.pdf')
    fig1.savefig(plots_dir+alg+'-power.png')


figlegend.tight_layout()
figlegend.legend(handles=lenged_elements,loc=9,ncol=2, framealpha=0,fontsize=38)
# figlegend.savefig(plots_dir+'burst-legend.pdf', bbox_inches='tight', pad_inches=0)
