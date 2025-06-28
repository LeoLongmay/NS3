import scipy.io as io
import gym
import numpy as np
import matplotlib.pyplot as plt
from ns3gym import ns3env
from stable_baselines3 import TD3
import os 
from stable_baselines3.common.monitor import Monitor 
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
port = 5030
simTime = 10 
startSim = True
stepTime = 0.05 
seed = 0
# simArgs = {"--simTime": simTime,
#            "--testArg": 123,
#            "--nodeNum": 5,
#            "--distance": 500}
debug = False
tensorboard_log_path = "./td3_tensorboard/"

env = ns3env.Ns3Env(port=port, stepTime=stepTime, startSim=startSim, simSeed=seed, debug=debug)
env.reset()

ob_space = env.observation_space
ac_space = env.action_space
print("Observation space: ", ob_space,  ob_space.dtype)
print("Action space: ", ac_space, ac_space.dtype)

def train_td3():
    global env 
    env.reset()
    model = TD3(
        "MlpPolicy",
        env,
        verbose=1,
        learning_rate=1e-4,
        buffer_size=int(1e6),
        policy_delay=2,       
        target_policy_noise=0.2, 
        tensorboard_log=tensorboard_log_path,
        # device='cpu'
    )
    reward_callback = RewardTensorBoardCallback(tensorboard_log_path)

    model.learn(
        # total_timesteps=int(10), 
        total_timesteps=int(5e4),
        callback = reward_callback,
    )

    model.save("td3_rdma_test_5e4")


if __name__ == "__main__":
    train_td3()
    print("Training finished--------------")
    env.close()

