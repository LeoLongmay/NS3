import gymnasium
from loguru import logger

from stable_baselines3 import A2C
from utils import plot_results, RewardTensorBoardCallback


def a2c_trainer(env: gymnasium.Env, model_logs_dir: str, total_timesteps: int = 500_000, verbose: int = 1) -> A2C:
    logger.info("Creating A2C model...")
    model = A2C("MlpPolicy", env, verbose=verbose, device='cpu', tensorboard_log=model_logs_dir)
    reward_callback = RewardTensorBoardCallback(model_logs_dir)

    logger.info("Training A2C model...")
    model.learn(total_timesteps=total_timesteps, callback = reward_callback)

    # logger.info("Plotting training results...")
    # plot_results(result_dir, 'A2C')
    return model