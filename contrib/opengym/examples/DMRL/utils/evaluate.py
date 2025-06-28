from typing import Tuple
from stable_baselines3.common.vec_env import VecEnv
from stable_baselines3.common.base_class import BaseAlgorithm


def reward_evaluate(model: BaseAlgorithm, vec_env: VecEnv) -> float:
    obs = vec_env.reset()
    done = False
    reward = 0
    while not done:
        action, _ = model.predict(obs, deterministic=False)
        obs, rew, done, _ = vec_env.step(action)
        reward += rew
    return reward


def object_evaluate(model: BaseAlgorithm, vec_env: VecEnv) -> Tuple[float, float]:
    obs = vec_env.reset()
    done = False
    reward = 0
    infos = [{'success_rate': 0, 'communication_cost': 0}]
    while not done:
        action, _ = model.predict(obs, deterministic=False)
        obs, rew, done, infos = vec_env.step(action)
        reward += rew
    return infos[0]['success_rate'], infos[0]['communication_cost']
