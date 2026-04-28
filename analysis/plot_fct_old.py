#!/usr/bin/python3

import subprocess
import os
import sys
import argparse
import matplotlib as mpl
import matplotlib.pyplot as plt
import matplotlib.lines as mlines  # 导入Line2D用于创建代理对象（备用）
import matplotlib.ticker as tick
import math
from cycler import cycler

# LB/CC mode matching
cc_modes = {
    1: "DCQCN",
    3: "HPCC",
    6: "PowerTCP",
    7: "Timely",
    8: "DCTCP",
    9: "LPCC",
}
lb_modes = {
    0: "fecmp",
    2: "drill",
    3: "conga",
    6: "letflow",
    9: "conweave",
}
topo2bdp = {
    "leaf_spine_128_100G_OS2": 104000,  # 2-tier
    "fat_k4_100G_OS2": 153000, # 3-tier -> core 400G
    "test_topoOS2": 50001000
}

C = [
    'xkcd:grass green',
    'xkcd:blue',
    'xkcd:purple',
    'xkcd:orange',
    'xkcd:teal',
    'xkcd:brick red',
    'xkcd:black',
    'xkcd:brown',
    'xkcd:grey',
]

LS = [
    'solid',
    'dashed',
    'dotted',
    'dashdot'
]

M = [
    'o',
    's',
    'x',
    'v',
    'D'
]

H = [
    '//',
    'o',
    '***',
    'x',
    'xxx',
]

def setup():
    """Called before every plot_ function"""

    def lcm(a, b):
        return abs(a*b) // math.gcd(a, b)

    def a(c1, c2):
        """Add cyclers with lcm."""
        l = lcm(len(c1), len(c2))
        c1 = c1 * (l//len(c1))
        c2 = c2 * (l//len(c2))
        return c1 + c2

    def add(*cyclers):
        s = None
        for c in cyclers:
            if s is None:
                s = c
            else:
                s = a(s, c)
        return s

    plt.rc('axes', prop_cycle=(add(cycler(color=C),
                                   cycler(linestyle=LS),
                                   cycler(marker=M))))
    plt.rc('lines', markersize=5)
    plt.rc('legend', handlelength=3, handleheight=1.5, labelspacing=0.25)
    # plt.rcParams["font.family"] = "sans"
    plt.rcParams["font.size"] = 12
    plt.rcParams['pdf.fonttype'] = 42
    plt.rcParams['ps.fonttype'] = 42


def getFilePath():
    dir_path = os.path.dirname(os.path.realpath(__file__))
    print("File directory: {}".format(dir_path))
    return dir_path

def get_pctl(a, p):
    i = int(len(a) * p)
    return a[i]

def size2str(steps):
    result = []
    for step in steps:
        if step < 10000:
            result.append("{:.1f}K".format(step / 1000))
        elif step < 1000000:
            result.append("{:.0f}K".format(step / 1000))
        else:
            result.append("{:.1f}M".format(step / 1000000))

    return result


def get_steps_from_raw(filename, time_start, time_end, step=5):
    # time_start = int(2.005 * 1000000000)
    # time_end = int(3.0 * 1000000000) 
    cmd_slowdown = "cat %s"%(filename)+" | awk '{ if ($6>"+"%d"%time_start+" && $6+$7<"+"%d"%(time_end)+") { slow=$7/$8; print slow<1?1:slow, $5} }' | sort -n -k 2"    
    output_slowdown = subprocess.check_output(cmd_slowdown, shell=True)
    aa = output_slowdown.decode("utf-8").split('\n')[:-2]
    nn = len(aa)

    # CDF of FCT
    res = [[i/100.] for i in range(0, 100, step)]
    for i in range(0,100,step):
        l = int(i * nn / 100)
        r = int((i+step) * nn / 100)
        fct_size = aa[l:r]
        fct_size = [[float(x.split(" ")[0]), int(x.split(" ")[1])] for x in fct_size]
        fct = sorted(map(lambda x: x[0], fct_size))
        
        res[int(i/step)].append(fct_size[-1][1]) # flow size
        
        res[int(i/step)].append(sum(fct) / len(fct)) # avg fct
        res[int(i/step)].append(get_pctl(fct, 0.5)) # mid fct
        res[int(i/step)].append(get_pctl(fct, 0.95)) # 95-pct fct
        res[int(i/step)].append(get_pctl(fct, 0.99)) # 99-pct fct
        res[int(i/step)].append(get_pctl(fct, 0.999)) # 99-pct fct
    
    # ## DEBUGING ###
    # print("{:5} {:10} {:5} {:5} {:5} {:5} {:5}  <<scale: {}>>".format("CDF", "Size", "Avg", "50%", "95%", "99%", "99.9%", "us-scale"))
    # for item in res:
    #     line = "%.3f %3d"%(item[0] + step/100.0, item[1])
    #     i = 1
    #     line += "\t{:.3f} {:.3f} {:.3f} {:.3f} {:.3f}".format(item[i+1], item[i+2], item[i+3], item[i+4], item[i+5])
    #     print(line)

    result = {"avg": [], "p99": [], "size": []}
    for item in res:
        result["avg"].append(item[2])
        result["p99"].append(item[5])
        result["size"].append(item[1])

    return result

def main():
    parser = argparse.ArgumentParser(description='Plotting FCT of results')
    parser.add_argument('-sT', dest='time_limit_begin', action='store', type=int, default=2005000000, help="only consider flows that finish after T, default=2005000000 ns")
    parser.add_argument('-fT', dest='time_limit_end', action='store', type=int, default=10000000000, help="only consider flows that finish before T, default=10000000000 ns")
    
    args = parser.parse_args()
    time_start = args.time_limit_begin
    time_end = args.time_limit_end
    STEP = 5 # 5% step

    file_dir = getFilePath()
    fig_dir = file_dir + "/figures"
    output_dir = file_dir + "/../mix/output"
    history_filename = file_dir + "/../mix/.history"

    # 创建figures目录（如果不存在）
    if not os.path.exists(fig_dir):
        os.makedirs(fig_dir)

    # 初始化全局图例元素（用字典存储，避免重复，key=cc_mode, value=Line2D对象）
    legend_dict = {}
    legend_collected = False  # 标记是否已收集图例元素
    # 固定的cc_mode顺序
    ccmode_order = ['DCQCN', 'HPCC', 'Timely', 'DCTCP', 'PowerTCP', 'LPCC']

    # read history file
    map_key_to_id = dict()

    # test_n = 10
    with open(history_filename, "r") as f:
        for line in f.readlines():
            for topo in topo2bdp.keys():
                if topo in line:
                    parsed_line = line.replace("\n", "").split(',')
                    config_id = parsed_line[1]
                    cc_mode = cc_modes[int(parsed_line[2])]
                    lb_mode = lb_modes[int(parsed_line[3])]
                    encoded_fc = (int(parsed_line[9]), int(parsed_line[10]))
                    if encoded_fc == (0, 1):
                        flow_control = "IRN"
                    elif encoded_fc == (1, 0):
                        flow_control = "Lossless"
                    else:
                        continue
                    topo = parsed_line[13]
                    netload = parsed_line[16]
                    key = (topo, netload, flow_control)
                    if key not in map_key_to_id:
                        map_key_to_id[key] = [[config_id, lb_mode, cc_mode]]
                    else:
                        map_key_to_id[key].append([config_id, lb_mode, cc_mode])
    print (map_key_to_id)

    for k, v in map_key_to_id.items():

        ################## AVG plotting ##################
        # fig = plt.figure(figsize=(4, 4))
        fig = plt.figure(figsize=(4, 3))
        ax = fig.add_subplot(111)
        fig.tight_layout()

        ax.set_xlabel("Flow Size (Bytes)", fontsize=12)
        ax.set_ylabel("Avg FCT Slowdown", fontsize=12)

        ax.spines['top'].set_visible(False)
        ax.spines['right'].set_visible(False)
        ax.yaxis.set_ticks_position('left')
        ax.xaxis.set_ticks_position('bottom')
        
        xvals = [i for i in range(STEP, 100 + STEP, STEP)]
        
        for tgt_ccmode in ccmode_order:
            for vv in v:
                config_id = vv[0]
                lb_mode = vv[1]
                cc_mode = vv[2]

                if cc_mode == tgt_ccmode:
                    fct_slowdown = output_dir + "/{id}/{id}_out_fct.txt".format(id=config_id)
                    result = get_steps_from_raw(fct_slowdown, int(time_start), int(time_end), STEP)

                    # 绘制曲线（确保返回的是Line2D对象）
                    line, = ax.plot(xvals,
                        result["avg"],
                        markersize=1.0,
                        linewidth=2.0,
                        label=cc_mode)
                    
                    # 只收集一次，且每个cc_mode只保留一个Line2D对象
                    if not legend_collected and cc_mode not in legend_dict:
                        legend_dict[cc_mode] = line
                     
        # 标记图例已收集（避免后续重复收集）
        if not legend_collected and len(legend_dict) > 0:
            legend_collected = True
        
        ax.tick_params(axis="x", rotation=40)
        ax.set_xticks(([0] + xvals)[::2])
        # ax.set_xticklabels(([0] + size2str(result["size"]))[::2], fontsize=12)
        ax.set_xticklabels(["0", "500K", "2M", "5M", "40M", "80M", "500M", "1G", "3G", "10G", "20G"], fontsize=12)
        ax.set_ylim(bottom=1)
        ax.set_yscale("log")

        fig.tight_layout()
        ax.grid(which='minor', alpha=0.2)
        ax.grid(which='major', alpha=0.5)
        fig_filename = fig_dir + "/{}.pdf".format("AVG_TOPO_{}_LOAD_{}_FC_{}".format(k[0], k[1], k[2]))
        print(fig_filename)
        plt.savefig(fig_filename, transparent=False, bbox_inches='tight')
        plt.close()
            



        ################## P99 plotting ##################
        # fig = plt.figure(figsize=(4, 4))
        fig = plt.figure(figsize=(4, 3))
        ax = fig.add_subplot(111)
        fig.tight_layout()

        ax.set_xlabel("Flow Size (Bytes)", fontsize=12)
        ax.set_ylabel("p99 FCT Slowdown", fontsize=12)

        ax.spines['top'].set_visible(False)
        ax.spines['right'].set_visible(False)
        ax.yaxis.set_ticks_position('left')
        ax.xaxis.set_ticks_position('bottom')
        
        xvals = [i for i in range(STEP, 100 + STEP, STEP)]

        for tgt_ccmode in ccmode_order:
            for vv in v:
                config_id = vv[0]
                lb_mode = vv[1]
                cc_mode = vv[2]

                if cc_mode == tgt_ccmode:
                    # plotting
                    fct_slowdown = output_dir + "/{id}/{id}_out_fct.txt".format(id=config_id)
                    result = get_steps_from_raw(fct_slowdown, int(time_start), int(time_end), STEP)
                    
                    # 绘制曲线（无需收集图例）
                    line, = ax.plot(xvals,
                        result["p99"],
                        markersize=1.0,
                        linewidth=2.0,
                        label=cc_mode)
                
        ax.tick_params(axis="x", rotation=40)
        ax.set_xticks(([0] + xvals)[::2])
        # ax.set_xticklabels(([0] + size2str(result["size"]))[::2], fontsize=12)
        ax.set_xticklabels(["0", "500K", "2M", "5M", "40M", "80M", "500M", "1G", "3G", "10G", "20G"], fontsize=12)
        ax.set_ylim(bottom=1)
        ax.set_yscale("log")

        fig.tight_layout()
        ax.grid(which='minor', alpha=0.2)
        ax.grid(which='major', alpha=0.5)
        fig_filename = fig_dir + "/{}.pdf".format("P99_TOPO_{}_LOAD_{}_FC_{}".format(k[0], k[1], k[2]))
        print(fig_filename)
        plt.savefig(fig_filename, transparent=False, bbox_inches='tight')
        plt.close()
    
    # ========== 所有图表绘制完成后，生成唯一的图例PDF ==========
    if len(legend_dict) > 0:
        # 按固定顺序整理handles和labels（确保顺序正确，且都是有效对象）
        legend_handles = []
        legend_labels = []
        for mode in ccmode_order:
            if mode in legend_dict:
                legend_handles.append(legend_dict[mode])
                legend_labels.append(mode)
        
        # 1. 调整画布尺寸为适配6列图例的最小尺寸（避免过大画布导致白边）
        legend_fig = plt.figure(figsize=(8, 0.8))  # 宽度适配6列，高度仅够容纳一行图例
        legend_ax = legend_fig.add_subplot(111)
        legend_ax.axis('off')  # 隐藏坐标轴
        
        # 2. 精准设置图例位置：锚定到画布左下角，铺满整个轴域，无额外间距
        legend = legend_ax.legend(
            handles=legend_handles,
            labels=legend_labels,
            bbox_to_anchor=(0, 0, 1, 1),  # (x0, y0, width, height) 铺满整个轴域
            loc="center",                  # 图例在bbox内居中
            borderaxespad=0,               # 轴域和图例无间距
            frameon=False,                 # 无边框
            fontsize=10,
            ncol=6,                        # 6列展示
            labelspacing=0.2,              # 标签垂直间距
            columnspacing=1.0,             # 列水平间距
            handletextpad=0.5              # 图例标记和文本间距
        )
        
        # 3. 保存时关键设置：bbox_inches捕获图例实际边界 + pad_inches=0 去除所有内边距
        # 获取图例的实际边界框
        legend_bbox = legend.get_window_extent().transformed(legend_fig.dpi_scale_trans.inverted())
        legend_filename = fig_dir + "/LEGEND_CC_MODES.pdf"
        # 保存时仅保留图例实际区域，pad_inches=0 彻底去除白边
        legend_fig.savefig(
            legend_filename, 
            transparent=False, 
            bbox_inches=legend_bbox,  # 仅保存图例的实际边界
            pad_inches=0.0,           # 去除保存时的额外内边距
            dpi=300                   # 可选：提高分辨率，不影响白边
        )
        print(f"\n统一的图例文件已保存至: {legend_filename}")
        plt.close(legend_fig)
    else:
        print("\n未收集到有效图例元素，跳过图例文件生成")

if __name__=="__main__":
    setup()
    main()