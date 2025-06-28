import gymnasium
from loguru import logger

from stable_baselines3 import PPO
from utils import plot_results, RewardTensorBoardCallback


def ppo_trainer(env: gymnasium.Env, model_logs_dir: str, total_timesteps: int = 500_000, verbose: int = 1) -> PPO:
    logger.info("Creating PPO model...")
    model = PPO("MlpPolicy", env, verbose=verbose, device='cpu', tensorboard_log=model_logs_dir)
    reward_callback = RewardTensorBoardCallback(model_logs_dir)

    logger.info("Training PPO model...")
    model.learn(total_timesteps=total_timesteps, callback = reward_callback)

    # logger.info("Plotting training results...")
    # plot_results(result_dir, 'PPO')
    return model