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

class ReshapeObservationWrapper(gym.Wrapper):
    def __init__(self, env):
        super(ReshapeObservationWrapper, self).__init__(env)
        # Update observation space
        original_space = env.observation_space
        self.observation_space = gym.spaces.Box(
            shape=(14, 9),
            low=original_space.low,
            high=original_space.high,
            dtype=original_space.dtype
        )
    
    def reshape_observation(self, obs):
        obs_array = np.array(obs)
        if obs_array.shape[0] == 126:
            return np.reshape(obs_array, (14, 9))
        else:
            raise ValueError(f"Expected 72 elements, but got {obs_array.shape[0]}")
    def reset(self, **kwargs):
        obs = self.env.reset(**kwargs)
        return self.reshape_observation(obs)

    def step(self, action):
        obs, reward, done, info = self.env.step(action)
        obs = self.reshape_observation(obs)
        return obs, reward, done, info
    
# class CustomPolicy(ActorCriticPolicy):
#     def __init__(self, *args, **kwargs):
#         super().__init__(*args, **kwargs,log_std_init=-0.5)
#         action_dim = self.action_space.shape[0]
#         self.action_net = nn.Sequential(
#             nn.Linear(256, 64),
#             nn.ReLU(),
#             nn.Linear(64, action_dim),
#             nn.Sigmoid()  # 确保输出在[0,1]
#         )

# Environment initialization
port = 5556
simTime = 10 
startSim = True
stepTime = 0.05 
seed = 0
# simArgs = {"--simTime": simTime,
#            "--testArg": 123,
#            "--nodeNum": 5,
#            "--distance": 500}
debug = False

env = ns3env.Ns3Env(port=port, stepTime=stepTime, startSim=startSim, simSeed=seed, debug=debug)

ob_space = env.observation_space
ac_space = env.action_space
print("Observation space: ", ob_space,  ob_space.dtype)
print("Action space: ", ac_space, ac_space.dtype)

def train_a2c():
    # num_process = 8
    global env 
    # env = ReshapeObservationWrapper(env)
    env.reset()
    print("Observation space: ", env.observation_space, env.observation_space.dtype)
    action = env.action_space.sample()
    print("---action: ", action)
    obs, reward, done, info = env.step(action)
    print("---obs, reward, done, info: ", obs, reward, done, info)
    model = A2C(
        "MlpPolicy", 
        env, 
        learning_rate=1e-4,  
        # tensorboard_log="./a2c_tensorboard/",   
        max_grad_norm=0.5,        
        ent_coef=0.1, 
        verbose=1
    )  

    model.learn(
        total_timesteps=int(10), 
        # callback = stepCallBack,
    )

    model.save("a2c_rdma")


def run_a2c():
    print("Running A2C--------------")
    model = A2C.load("a2c_rdma")
    global env 
    env = ReshapeObservationWrapper(env)
    print("Env reset11--------------")
    obs = env.reset()
    print("Env reset22--------------")
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
    run_a2c()
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
