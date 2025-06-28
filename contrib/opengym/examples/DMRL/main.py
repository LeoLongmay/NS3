import random
import numpy as np

from loguru import logger
from ns3gym import ns3env

from config import *
from utils import init_logger
from policy_map import get_policy_from_name


def main():
    seed = random.random()
    init_logger(debug=True, sensitive_func_name=['main', 'load_best_model', 'save_best_model'])

    logger.info("Create the environment...")
    env = ns3env.Ns3Env(port=5010, simSeed=seed)
    np.random.seed(hash(seed) % 2 ** 32)
    state_dim = int(np.prod(env.observation_space.shape))
    action_dim = int(np.prod(env.action_space.shape))
    logger.info(f'Environment Name: NS3')
    logger.info(f'Algorithm Name: {algorithm}')
    logger.info(f'State Dimension: {state_dim}')
    logger.info(f'Action Dimension: {action_dim}')
    logger.info(f'Device: {device}')
    logger.info(f'Seed: {seed}')

    trainer = get_policy_from_name(str(algorithm))
    model = trainer(env, model_logs_dir, total_timesteps)

    # logger.info("Testing the model...")
    # vec_env = model.get_env()
    # obs = vec_env.reset()
    # done = False
    # reward = 0
    # infos = [{'total_resource_load': 0, 'average_resource_load': 0}]
    # while not done:
    #     action, _ = model.predict(obs, deterministic=False)
    #     obs, rew, done, infos = vec_env.step(action)
    #     vec_env.render("human")
    #     reward += rew

    # logger.info(f'Total Reward: {reward}')
    # logger.info(f'Total Resource Load: {infos[0]["total_resource_load"]}')
    # logger.info(f'Average Resource Load: {infos[0]["average_resource_load"]}')


if __name__ == '__main__':
    main()
