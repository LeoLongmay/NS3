import os
import time
import gymnasium

from loguru import logger
from stable_baselines3 import PPO
from utils import plot_results, RewardTensorBoardCallback
from policy.diffusion_ac import DiffusionActorCriticPolicy
from config import beta_schedule, hidden_sizes, n_timesteps


def diffusion_ppo_trainer(env: gymnasium.Env, model_logs_dir: str, total_timesteps: int = 500_000, verbose: int = 1) -> PPO:
    identifier = time.strftime("%Y%m%d_%H%M%S", time.localtime())
    result_dir = os.path.join(model_logs_dir, f'diffusion_ppo_{identifier}')
    os.makedirs(result_dir, exist_ok=True)

    diffusion_params = {
        "beta_schedule": beta_schedule,
        "last_layer_dim_pi": 64,
        "last_layer_dim_vf": 64,
        "hidden_sizes": hidden_sizes,
        "n_timesteps": n_timesteps
    }

    logger.info("Creating DiffusionPPO model...")
    model = PPO(DiffusionActorCriticPolicy, env, tensorboard_log=result_dir,
                verbose=verbose, device='cpu', policy_kwargs=diffusion_params)
    reward_callback = RewardTensorBoardCallback(model_logs_dir)
    logger.info("Training DiffusionPPO model...")
    model.learn(total_timesteps=total_timesteps, callback = reward_callback)

    # logger.info("Plotting training results...")
    # plot_results(result_dir, 'DiffusionPPO')
    return model