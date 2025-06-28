from __future__ import annotations

from stable_baselines3.common.results_plotter import load_results, ts2xy, X_EPISODES, X_TIMESTEPS
import matplotlib.pyplot as plt


def plot_results(logs_dir: str, algorithm: str) -> None:
    """
    Plot the training results of the model.

    :param logs_dir: The directory of the model logs.
    :param algorithm: The algorithm of the model.
    """
    episodes, rewards = ts2xy(load_results(logs_dir), X_EPISODES)
    timesteps, rewards = ts2xy(load_results(logs_dir), X_TIMESTEPS)
    plt.figure("Episode", (8, 2))
    plt.plot(episodes, rewards, c='b')
    plt.title(f"Training Results per Episode of {algorithm}")
    plt.xlabel("Episodes")
    plt.ylabel("Rewards")

    plt.figure("Timestep", (8, 2))
    plt.plot(timesteps, rewards, c='b')
    plt.title(f"Training Results per Timestep of {algorithm}")
    plt.xlabel("Timesteps")
    plt.ylabel("Rewards")
    plt.show()
