import gym
import numpy as np
import matplotlib.pyplot as plt
from ns3gym import ns3env
from stable_baselines3 import A2C 
import os 
# from stable_baselines3.common.callbacks import stepCallBack
from stable_baselines3.common.policies import ActorCriticPolicy
from torch import nn
from stable_baselines3.common.evaluation import evaluate_policy
from stable_baselines3.common.vec_env import SubprocVecEnv, DummyVecEnv
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
port = 5000
simTime = 10 
startSim = True
stepTime = 0.05 
seed = 0
# simArgs = {"--simTime": simTime,
#            "--testArg": 123,
#            "--nodeNum": 5,
#            "--distance": 500}
debug = False
tensorboard_log_path = "./a2c_tensorboard/"

env = ns3env.Ns3Env(port=port, stepTime=stepTime, startSim=startSim, simSeed=seed, debug=debug)

ob_space = env.observation_space
ac_space = env.action_space
print("Observation space: ", ob_space,  ob_space.dtype)
print("Action space: ", ac_space, ac_space.dtype)

def train_a2c():
    global env 
    env.reset()
    model = A2C(
        "MlpPolicy", 
        env, 
        learning_rate=1e-5,  
        tensorboard_log=tensorboard_log_path,   
        max_grad_norm=0.5,        
        ent_coef=0.5, 
        verbose=1
    )  
    reward_callback = RewardTensorBoardCallback(tensorboard_log_path)

    model.learn(
        # total_timesteps=int(10), 
        total_timesteps=int(1e5), 
        callback = reward_callback,
    )

    model.save("a2c_rdma_1e5")


def run_a2c():
    print("Running A2C--------------")
    model = A2C.load("a2c_rdma_1e5")
    global env 
    obs = env.reset()
    for i in range(10):
        print(f"obs: {obs}")
        action, _states = model.predict(obs)
        obs, rewards, dones, info = env.step(action)
        print(f"obs: {obs}")
        print(f"action: {action}, rewards: {rewards}, dones: {dones}")
        if dones:
            obs = env.reset()
    


if __name__ == "__main__":
    train_a2c()
    print("Training finished--------------")
    # run_a2c()
    env.close()


# model = A2C(CustomPolicy, env, verbose=1)
# model = A2C(
#     "MlpPolicy", 
#     env, 
#     learning_rate=1e-4,     
#     max_grad_norm=0.5,        
#     ent_coef=0.1, 
#     verbose=1
# )  

# # 创建日志目录
# model.learn(
#     total_timesteps=int(5000), 
#     callback=eval_callback,
# )
# model.save("a2c_rdma_test1")
# evaluate_policy(model, env, n_eval_episodes=10)
# env.close()
