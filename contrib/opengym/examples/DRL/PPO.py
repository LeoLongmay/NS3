import scipy.io as io
import gym
import numpy as np
import matplotlib.pyplot as plt
from ns3gym import ns3env
from stable_baselines3 import PPO
from stable_baselines3.common.vec_env import DummyVecEnv
import os 
from stable_baselines3.common.monitor import Monitor 
from stable_baselines3.common.env_util import make_vec_env
from stable_baselines3.common.results_plotter import load_results, ts2xy
from stable_baselines3.common.callbacks import BaseCallback
from stable_baselines3.common.callbacks import EvalCallback

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
port = 5010
simTime = 10 
startSim = True
stepTime = 0.05 
seed = 0
# simArgs = {"--simTime": simTime,
#            "--testArg": 123,
#            "--nodeNum": 5,
#            "--distance": 500}
debug = False
tensorboard_log_path = "./ppo_tensorboard/"

env = ns3env.Ns3Env(port=port, stepTime=stepTime, startSim=startSim, simSeed=seed, debug=debug)

ob_space = env.observation_space
ac_space = env.action_space
print("Observation space: ", ob_space,  ob_space.dtype)
print("Action space: ", ac_space, ac_space.dtype)

def train_PPO():
    global env 
    env.reset()
    model = PPO(
        "MlpPolicy",
        env,
        verbose=1,
        ent_coef=0.01,        
        learning_rate=3e-4,   
        clip_range=0.2,      
        n_steps=2048, 
        tensorboard_log=tensorboard_log_path,        
        device='cpu'
    ) 
    reward_callback = RewardTensorBoardCallback(tensorboard_log_path)

    model.learn(
        # total_timesteps=int(10), 
        total_timesteps=3e5, 
        callback = reward_callback,
    )
    model.save("ppo_rdma_3e5")

if __name__ == "__main__":
    train_PPO()
    print("Training finished--------------")
# eval_callback = EvalCallback(env, best_model_save_path="./logs/")
# model.learn(total_timesteps=1e6, callback=eval_callback)
# model.save("ppo_rdma_test")
