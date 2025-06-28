from enum import Enum


class PolicyName(Enum):
    PPO = 'ppo'
    SAC = 'sac'
    TD3 = 'td3'
    A2C = 'a2c'
    DDPG = 'ddpg'
    DIFFUSION_PPO = 'diffusion_ppo'


# Constant Policy Name
PPO_POLICY = PolicyName.PPO.value
SAC_POLICY = PolicyName.SAC.value
TD3_POLICY = PolicyName.TD3.value
A2C_POLICY = PolicyName.A2C.value
DDPG_POLICY = PolicyName.DDPG.value
DIFFUSION_PPO_POLICY = PolicyName.DIFFUSION_PPO.value
