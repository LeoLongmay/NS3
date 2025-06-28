import scipy.io as io
import gym
# import gymnasium as gym
import tf_slim as slim
import numpy as np
import matplotlib.pyplot as plt
from ns3gym import ns3env
from stable_baselines3 import A2C,PPO
from stable_baselines3.common.vec_env import DummyVecEnv
import os 
from stable_baselines3.common.monitor import Monitor 
from stable_baselines3.common.env_util import make_vec_env
from stable_baselines3.common.results_plotter import load_results, ts2xy

def reshape_observation(obs):
    """Reshape the observation from (72,) to (8, 9)"""
    obs_array = np.array(obs)  # Convert to numpy array
    print("Original obs shape:", obs_array.shape)
    
    # Ensure it has 72 elements and reshape to (8, 9)
    # 最大的是14
    if obs_array.shape[0] == 126:
        return np.reshape(obs_array, (14, 9))
    else:
        raise ValueError(f"Expected 72 elements, but got {obs_array.shape[0]}")

class ReshapeObservationWrapper(gym.Wrapper):
    def __init__(self, env):
        super(ReshapeObservationWrapper, self).__init__(env)
    
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

class PlottingCallback(BaseCallback):
    """
    Callback 用于实时绘制训练性能。

    :param verbose: (int) 控制回调函数的日志输出，1 表示显示日志
    """
    def __init__(self, verbose=1):
        super().__init__(verbose)  
        self._plot = None  

    def _on_step(self) -> bool:
        # 获取监控数据，`ts2xy` 将数据转化为时间步和奖励的数组
        x, y = ts2xy(load_results(log_dir), 'timesteps')  
        
        # 如果图表尚未创建，进行初始化绘图
        if self._plot is None:
            plt.ion()  
            fig = plt.figure(figsize=(6,3)) 
            ax = fig.add_subplot(111) 
            line, = ax.plot(x, y)  
            self._plot = (line, ax, fig) 
            plt.show() 
    
        else:
            self._plot[0].set_data(x, y)  
            self._plot[-2].relim()  
            self._plot[-2].set_xlim([self.locals["total_timesteps"] * -0.02, 
                                     self.locals["total_timesteps"] * 1.02])  
            self._plot[-2].autoscale_view(True,True,True)  
            self._plot[-1].canvas.draw()  

log_dir = "/DRL_gym/A2c/plot"  
os.makedirs(log_dir, exist_ok=True)  

#  Environment initialization
port = 5555
simTime = 10 
startSim = True
stepTime = 0.05 
seed = 0
# simArgs = {"--simTime": simTime,
#            "--testArg": 123,
#            "--nodeNum": 5,
#            "--distance": 500}
debug = False
iterationNum = 2

env = ns3env.Ns3Env(port=port, stepTime=stepTime, startSim=startSim, simSeed=seed, debug=debug)
print("env",env)
print("Observation space & Action space: ", env.observation_space,env.action_space)

# obs = reshape_observation(obs)  # This ensures the observation is reshaped into (8, 9)
env = ReshapeObservationWrapper(env)
# env = make_vec_env(env, n_envs=1, monitor_dir=log_dir)

obs = env.reset()

print("env.reset: ", obs)
action = env.action_space.sample()
print("---action: ", action)
obs, reward, done, info = env.step(action)
print("---obs, reward, done, info: ", obs, reward, done, info)


# obs, reward, done, info = env.step(action)
# print("---obs, reward, done, info: ", obs, reward, done, info)

# obs = reshape_observation(obs)  # Ensure it's 2D
# print("---obs, reward, done, info: ", obs, reward, done, info)

# model = A2C("MlpPolicy",env,verbose=1,device='cpu')


# 实例化绘图回调函数
plotting_callback = PlottingCallback()

# 创建 PPO 模型，使用多层感知机策略（MlpPolicy）
model = A2C.load("test",env=env,verbose=1)
# model = A2C('MlpPolicy', env, verbose=0)

# 训练模型 10000 个时间步，并使用刚才创建的回调函数
model.learn(10000, callback=plotting_callback)