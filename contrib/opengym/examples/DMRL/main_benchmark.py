import random
import numpy as np
from loguru import logger
from ns3gym import ns3env

from constant import PolicyName
from policy_map import get_policy_from_name
from config import model_logs_dir, total_timesteps


def reward_benchmark_all() -> None:
    port = 5000
    for p in list(PolicyName)[:5]:
        port = port + 10
        seed = random.random()
        np.random.seed(hash(seed) % 2 ** 32)
        env = ns3env.Ns3Env(port=port, simSeed=seed)

        algo = p.value
        logger.info(f'Policy Name: {algo}')
        trainer = get_policy_from_name(algo)
        trainer(env, model_logs_dir, total_timesteps)
        logger.info(f'Policy Name: {algo} finished')


if __name__ == '__main__':
    reward_benchmark_all()
