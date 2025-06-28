from constant import *
from benchmark.policy.a2c import a2c_trainer
from benchmark.policy.ppo import ppo_trainer
from benchmark.policy.sac import sac_trainer
from benchmark.policy.td3 import td3_trainer
from benchmark.policy.ddpg import ddpg_trainer
from policy.diffusion_ppo import diffusion_ppo_trainer as diffusion_ppo_trainer_default


def get_policy_from_name(name: str) -> callable:
    if name == A2C_POLICY:
        return a2c_trainer
    elif name == PPO_POLICY:
        return ppo_trainer
    elif name == SAC_POLICY:
        return sac_trainer
    elif name == TD3_POLICY:
        return td3_trainer
    elif name == DDPG_POLICY:
        return ddpg_trainer
    elif name == DIFFUSION_PPO_POLICY:
        return diffusion_ppo_trainer_default
    else:
        raise ValueError("Unknown policy {}".format(name))
