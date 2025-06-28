import gymnasium
from loguru import logger

from stable_baselines3 import SAC
from utils import plot_results, RewardTensorBoardCallback


def sac_trainer(env: gymnasium.Env, model_logs_dir: str, total_timesteps: int = 500_000, verbose: int = 1) -> SAC:
    logger.info("Creating SAC model...")
    model = SAC("MlpPolicy", env, verbose=verbose, device='cpu', tensorboard_log=model_logs_dir)
    reward_callback = RewardTensorBoardCallback(model_logs_dir)

    logger.info("Training SAC model...")
    model.learn(total_timesteps=total_timesteps, callback = reward_callback)

    # logger.info("Plotting training results...")
    # plot_results(result_dir, 'SAC')
    return model