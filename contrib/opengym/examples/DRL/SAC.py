import scipy.io as io
import gym
import numpy as np
import matplotlib.pyplot as plt
from ns3gym import ns3env
from stable_baselines3 import SAC
from stable_baselines3.common.callbacks import EvalCallback
from stable_baselines3.common.monitor import Monitor
from stable_baselines3.common.callbacks import BaseCallback
import argparse
import os 

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
port = 5020
simTime = 10 
startSim = True
stepTime = 0.05 
seed = 0
# simArgs = {"--simTime": simTime,
#            "--testArg": 123,
#            "--nodeNum": 5,
#            "--distance": 500}
debug = False
tensorboard_log_path = "./sac_tensorboard/"

env = ns3env.Ns3Env(port=port, stepTime=stepTime, startSim=startSim, simSeed=seed, debug=debug)

ob_space = env.observation_space
ac_space = env.action_space
print("Observation space: ", ob_space,  ob_space.dtype)
print("Action space: ", ac_space, ac_space.dtype)

model = SAC(
        "MlpPolicy",
        env,
        verbose=1,
        ent_coef='auto',
        learning_rate=1e-4,
        buffer_size=1000000,
        tensorboard_log=tensorboard_log_path,
        # device='GPU',
        # policy_kwargs=policy_kwargs,
    )
reward_callback = RewardTensorBoardCallback(tensorboard_log_path)

def train_sac():
    global env 
    env.reset()
    model.load()
    model.learn(
        total_timesteps = 4e4, 
        # device='GPU',
        callback=reward_callback,
        # tb_log_name="sac_rdma"
    )

    model.save("sac_rdma_test_4e4")



for i in range(10):
    train_sac()



if __name__ == "__main__":
    train_sac()
    print("Training finished--------------")
    env.close()