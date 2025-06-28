import scipy.io as io
import gym
import numpy as np
import matplotlib.pyplot as plt
from ns3gym import ns3env
from stable_baselines3 import DDPG
import os 
from stable_baselines3.common.noise import NormalActionNoise
from stable_baselines3.common.callbacks import BaseCallback

class RewardTensorBoardCallback(BaseCallback):
    def __init__(self, log_dir: str):
        super(RewardTensorBoardCallback, self).__init__()
        self.log_dir = log_dir

    def _on_step(self) -> bool:
        if self.locals.get('rewards') is not None:
            reward = np.mean(self.locals['rewards'])
            self.logger.record('reward', reward)
        return True

# Environment initialization
port = 5040
simTime = 10 
startSim = True
stepTime = 0.05 
seed = 0
# simArgs = {"--simTime": simTime,
#            "--testArg": 123,
#            "--nodeNum": 5,
#            "--distance": 500}
debug = False
tensorboard_log_path = "./ddpg_tensorboard/"

env = ns3env.Ns3Env(port=port, stepTime=stepTime, startSim=startSim, simSeed=seed, debug=debug)

ob_space = env.observation_space
ac_space = env.action_space
print("Observation space: ", ob_space,  ob_space.dtype)
print("Action space: ", ac_space, ac_space.dtype)
n_actions = ac_space.shape[-1]
action_noise = NormalActionNoise(mean=np.zeros(n_actions), sigma=0.1 * np.ones(n_actions))

def train_ddpg():
    global env 
    env.reset()
    model = DDPG(
        "MlpPolicy", 
        env, 
        learning_rate=1e-5,  
        tensorboard_log=tensorboard_log_path,   
        action_noise=action_noise,        
        verbose=1
    )  
    reward_callback = RewardTensorBoardCallback(tensorboard_log_path)

    model.learn(
        # total_timesteps=int(10), 
        total_timesteps=int(2e5), 
        callback = reward_callback,
    )

    model.save("ddpg_rdma_test_2e5")

if __name__ == "__main__":
    train_ddpg()
    print("Training finished--------------")
    env.close()
