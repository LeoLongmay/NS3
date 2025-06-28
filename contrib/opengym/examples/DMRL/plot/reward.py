from __future__ import annotations

import os
import re
import numpy as np
import matplotlib.pyplot as plt

from utils import change_root
from config import grid_alpha
from config import model_logs_dir


def list_log_files() -> dict[str, str]:
    algorithm_pattern = r'^(.*?)_\d{8}_\d{6}$'
    logs = [log for log in os.listdir(model_logs_dir) if re.match(algorithm_pattern, log)]
    csvs = [os.path.join(model_logs_dir, log, 'monitor.csv') for log in logs]
    logs_dict = {re.match(algorithm_pattern, log).group(1): csv for log, csv in zip(logs, csvs)}
    return logs_dict


def smooth_rewards(rewards: list[float], window_size: int = 10) -> list[float] | np.ndarray:
    if len(rewards) < window_size:
        return rewards
    kernel = np.ones(window_size) / window_size
    smoothed = np.convolve(rewards, kernel, mode='valid')
    return smoothed


def extract_reward(logs_dict: dict[str, str]) -> dict[str, list[float] | np.ndarray]:
    reward_dict = {}
    reward_pattern = r'^(-?\d+\.\d+),'
    for algo, csv in logs_dict.items():
        with open(csv, 'r') as f:
            data = f.read()
            rewards = re.findall(reward_pattern, data, re.MULTILINE)
            rewards = [float(r) for r in rewards]
            reward_dict[algo] = smooth_rewards(rewards, 10)
    return reward_dict


def plot_reward() -> None:
    logs_dict = list_log_files()
    reward_dict = extract_reward(logs_dict)
    ours = {'DARS-PPO': reward_dict['DARS-PPO']}
    group1 = {algo: reward for algo, reward in reward_dict.items() if algo in ['PPO', 'D2SAC', 'TD3']}
    group2 = {algo: reward for algo, reward in reward_dict.items() if algo in ['SAC', 'DDPG', 'A2C']}
    groups = [group1, group2]

    for group_idx, group in enumerate(groups):
        fig, ax = plt.subplots()
        # ax.set_facecolor(face_color)
        line_colors = ['#FA7F6F', '#82B0D2', '#FFBE7A', '#8ECFC9']

        for algo_idx, (algorithm_name, rewards) in enumerate(list(ours.items()) + list(group.items())):
            ax.plot(rewards, label=algorithm_name, linewidth=2 if algo_idx != 0 else 3, color=line_colors[algo_idx])

        ax.set_xlabel('Episodes', fontsize=16, fontweight='bold')
        ax.set_ylabel('Test Rewards', fontsize=16, fontweight='bold')
        ax.tick_params(axis='both', which='major', labelsize=14)
        ax.set_ylim(top=800 if group_idx == 0 else 850)
        ax.set_ylim(bottom=0 if group_idx == 0 else -400)
        ax.set_xlim(left=0)

        ax.grid(True, linestyle='--', alpha=grid_alpha)
        if group_idx == 0:
            ax.legend(loc='upper center', fontsize=12, ncol=4,
                      bbox_to_anchor=(0.5, 1), handletextpad=0.15,
                      columnspacing=0.6, framealpha=1.0)
        else:
            ax.legend(loc='upper center', fontsize=12, ncol=4,
                      bbox_to_anchor=(0.5, 1), handletextpad=0.2,
                      columnspacing=0.4, framealpha=1.0)

        for spine in ax.spines.values():
            spine.set_visible(True)
            spine.set_linestyle('-')
            spine.set_linewidth(0.5)

        plt.tight_layout()
        svg_filename = os.path.join(model_logs_dir, f'rewards_{group_idx}.pdf')
        plt.savefig(svg_filename)

    plt.show()


if __name__ == '__main__':
    plt.rcParams['font.family'] = 'Times New Roman'
    plt.rcParams['pdf.fonttype'] = 42
    change_root()
    plot_reward()
