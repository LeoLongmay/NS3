import matplotlib.pyplot as plt
import os
import numpy as np
from scipy.interpolate import interp1d
from numpy import linspace

def read_rtt_data(file_path):
    rtt_values = []
    if not os.path.exists(file_path):
        print(f"error: file {file_path} doesn't exist!")
        return rtt_values

    with open(file_path, 'r', encoding='utf-8') as f:
        for line_num, line in enumerate(f, 1):
            line = line.strip()
            if line.startswith("rtt:"):
                try:
                    rtt_str = line.split("rtt:")[1].strip()
                    rtt_value = float(rtt_str)
                    rtt_values.append(rtt_value)
                except (IndexError, ValueError) as e:
                    print(f"Warning: line {line_num}'s data format error. Error msg: {e}")
                    continue
    
    if not rtt_values:
        print("Warning: No valid RTT data found!")
    return rtt_values

def plot_rtt_curve(rtt_values, save_path="/home/leo/PowerTCP-RAW/ns-3.39/examples/PowerTCP/plot_burst/rtt_timely.pdf"):

    plt.rcParams.update({'font.size': 26})

    num_points = len(rtt_values)
    print(num_points)
    plot_points = 300
    # time_axis = [200 * i / (num_points - 1) if num_points > 1 else 0 for i in range(num_points)]
    time_axis = [i for i in range(num_points)]
    print(max(rtt_values))
    print(min(rtt_values))
    
    fig, ax = plt.subplots(1,1)

    x_original = np.array(time_axis)
    y_original = np.array(rtt_values)
    x_smooth = linspace(x_original.min(), x_original.max(), 1000)
    interp_func = interp1d(x_original, y_original, kind='cubic', fill_value="extrapolate")
    y_smooth = interp_func(x_smooth)
    # -----------------------------------------------------------------------------------

    ax.plot(x_smooth, y_smooth, color='#1f77b4', linewidth=2, label='RTT')
    
    # ax.plot(time_axis, rtt_values, color='#1f77b4', linewidth=2, marker='o', markersize=4, label='RTT')
    
    ax.set_xlim(0, num_points)
    ax.set_ylim(min(rtt_values) - 50000, max(rtt_values) + 50000)
    ax.set_xticks([0, 35000, 70000, 105000])
    ax.set_xticklabels(["0", "10", "20", "30"])

    ax.set_yticks([20000000, 20200000, 20400000, 20600000, 20800000])
    ax.set_yticklabels(["20", "20.2", "20.4", "20.6", "20.8"])

    y1 = 20000000
    # y2 = max(rtt_values)
    y2 = 20697538
    
    ax.axhline(y=y1, linestyle='--', color='orange', alpha=0.8, linewidth=2)
    ax.axhline(y=y2, linestyle='--', color='orange', alpha=0.8, linewidth=2)

    xline = 118000
    
    ax.annotate(
        '',
        xy=(xline, y2),
        xytext=(xline, y1),
        arrowprops=dict(arrowstyle='<|-|>', color='xkcd:orange', linewidth=2, mutation_scale=10)
    )
    ax.text(
        xline - 20000,
        (y1 + y2) / 2,
        'RTT\nVariation',
        ha='center',
        va='center',
        fontsize=18,
        color='black',
        weight='bold'
    )

    # y_ticks = np.arange(19999900, 2000800, 10)
    # ax.set_yticks(y_ticks)
    # ax.set_yticklabels([f"{tick:.8f}" for tick in y_ticks], fontsize=10)
    
    ax.set_xlabel('Time (ms)')
    ax.set_ylabel('RTT (ms)')
    # ax.set_title('RTT Variation Over Time', fontsize=14, fontweight='bold')
    
    # ax.tick_params(axis='both', which='major', labelsize=10)
    
    ax.grid(True, linestyle='--')
    
    # ax.legend(fontsize=10)
    
    plt.tight_layout()
    plt.savefig(save_path, dpi=300, bbox_inches='tight')
    print(f"图表已保存至：{save_path}")
    
    # 显示图表
    # plt.show()

if __name__ == "__main__":
    DATA_FILE = "/home/leo/PowerTCP-RAW/ns-3.39/examples/PowerTCP/dump_burst/evaluation-timely.out"
    rtt_data = read_rtt_data(DATA_FILE)
    if rtt_data:
        plot_rtt_curve(rtt_data)