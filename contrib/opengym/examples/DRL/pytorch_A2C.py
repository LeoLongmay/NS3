import torch
import numpy as np
from torch import nn, optim
from torch.distributions import Dirichlet
from torch.nn import functional as F
from ns3gym import ns3env
import gym
import matplotlib.pyplot as plt

class NormalizationWrapper(gym.Wrapper):
    """观测值预处理Wrapper"""
    def __init__(self, env):
        super().__init__(env)
        self.observation_space = gym.spaces.Box(
            low=-np.inf, 
            high=np.inf, 
            shape=(126,),  # 确认观测维度
            dtype=np.float32
        )
        
    def step(self, action):
        # 确保动作满足约束
        action = np.clip(action, 1e-5, 1.0)  # 防止零值
        action /= action.sum()  # 归一化
        
        obs, reward, done, info = self.env.step(action)
        return obs, reward, done, info

class Actor(nn.Module):
    """策略网络（使用Dirichlet分布）"""
    def __init__(self, state_dim=126, action_dim=14):
        super().__init__()
        self.net = nn.Sequential(
            nn.Linear(state_dim, 256),
            nn.LayerNorm(256),
            nn.ReLU(),
            nn.Linear(256, 128),
            nn.LayerNorm(128),
            nn.ReLU(),
        )
        self.alpha = nn.Linear(128, action_dim)
        
    def forward(self, state):
        x = self.net(state)
        alpha = F.softplus(self.alpha(x)) + 1.0  # 确保浓度参数>0
        return alpha

class Critic(nn.Module):
    """价值网络"""
    def __init__(self, state_dim=126):
        super().__init__()
        self.net = nn.Sequential(
            nn.Linear(state_dim, 256),
            nn.LayerNorm(256),
            nn.ReLU(),
            nn.Linear(256, 128),
            nn.LayerNorm(128),
            nn.ReLU(),
            nn.Linear(128, 1)
        )
        
    def forward(self, state):
        return self.net(state)

class A2CAgent:
    def __init__(self):
        self.gamma = 0.99
        self.entropy_coef = 0.01
        
        # 环境初始化
        self.env = NormalizationWrapper(ns3env.Ns3Env(
            port=5050,
            stepTime=0.05,
            startSim=True,
            simSeed=0,
            debug=False
        ))
        self.initial_state = self.env.reset()
        
        # 确认维度
        state_dim = self.env.observation_space.shape[0]
        action_dim = self.env.action_space.shape[0]
        
        # 初始化网络
        self.actor = Actor(state_dim, action_dim)
        self.critic = Critic(state_dim)
        
        # 优化器
        self.optimizer = optim.Adam([
            {'params': self.actor.parameters(), 'lr': 1e-4},
            {'params': self.critic.parameters(), 'lr': 1e-3}
        ])
        
    def get_action(self, state):
        state = torch.FloatTensor(state)
        alpha = self.actor(state)
        dist = Dirichlet(alpha)
        action = dist.sample()
        log_prob = dist.log_prob(action)
        entropy = dist.entropy()
        return action.numpy(), log_prob, entropy
    
    def train_step(self, transitions):
        states, actions, rewards, next_states, dones, old_log_probs, entropies = zip(*transitions)
        
        # 转换为张量
        states = torch.FloatTensor(np.array(states))
        rewards = torch.FloatTensor(rewards)
        dones = torch.FloatTensor(dones)
        # old_log_probs = torch.cat(old_log_probs)
        old_log_probs = torch.stack(old_log_probs)
        entropies = torch.stack(entropies)
        
        # 计算TD目标
        with torch.no_grad():
            next_values = self.critic(states).squeeze()
            td_targets = rewards + self.gamma * (1 - dones) * next_values
        
        # 计算Critic损失
        values = self.critic(states).squeeze()
        critic_loss = F.mse_loss(values, td_targets)
        
        # 计算Actor损失
        advantages = td_targets - values.detach()
        actor_loss = -(old_log_probs * advantages).mean() - self.entropy_coef * entropies.mean()
        
        # 总损失
        total_loss = actor_loss + 0.5 * critic_loss
        
        # 反向传播
        self.optimizer.zero_grad()
        total_loss.backward()
        torch.nn.utils.clip_grad_norm_(self.actor.parameters(), 0.5)
        torch.nn.utils.clip_grad_norm_(self.critic.parameters(), 0.5)
        self.optimizer.step()
        
        return total_loss.item()

    def train(self, episodes=5, max_steps=500):
        rewards_history = []
        loss_history = []
        
        for ep in range(episodes):
            # state = self.env.reset()
            # state = self.initial_state
            self.env = NormalizationWrapper(ns3env.Ns3Env(
                port=6001,
                stepTime=0.05,
                startSim=True,
                simSeed=ep,  # 使用不同的 seed，防止重复
                debug=False
            ))
            state = self.env.reset()
            episode_reward = 0
            transitions = []
            
            for _ in range(max_steps):
                action, log_prob, entropy = self.get_action(state)
                next_state, reward, done, _ = self.env.step(action)
                
                transitions.append((
                    state, action, reward, next_state, done,
                    log_prob, entropy
                ))
                
                episode_reward += reward
                state = next_state
                
                if done:
                    break
            
            # 每episode更新一次
            loss = self.train_step(transitions)
            
            rewards_history.append(episode_reward)
            loss_history.append(loss)
            
            print(f"Ep {ep+1}/{episodes} | "
                  f"Reward: {episode_reward:.1f} | "
                  f"Loss: {loss:.3f} | "
                  f"Avg Reward: {np.mean(rewards_history[-10:]):.1f}")
            
        # 保存模型
        torch.save(self.actor.state_dict(), "a2c_actor.pth")
        torch.save(self.critic.state_dict(), "a2c_critic.pth")
        self.env.close()
        # 绘制训练曲线
        plt.figure(figsize=(12,5))
        plt.subplot(1,2,1)
        plt.plot(rewards_history)
        plt.title("Training Rewards")
        plt.subplot(1,2,2)
        plt.plot(loss_history)
        plt.title("Training Loss")
        plt.show()

if __name__ == "__main__":
    agent = A2CAgent()
    agent.train()



# import os
# import gym
# import time
# import torch
# import numpy as np
# from ns3gym import ns3env
# from torch import nn, optim
# import matplotlib.pyplot as plt
# from torch.distributions import Normal, Beta
# from torch.nn import functional as F


# def reshape_observation(obs):
#     """Reshape the observation from (72,) to (8, 9)"""
#     if obs is None:
#         raise ValueError("Observation is None. Please check the environment's step method.")
#     obs_array = np.array(obs)
#     obs_array = np.array(obs)  # Convert to numpy array
#     print("Original obs shape:", obs_array.shape)
    
#     # Ensure it has 72 elements and reshape to (8, 9)
#     # 最大的是14
#     if obs_array.shape[0] == 126:
#         return np.reshape(obs_array, (14, 9))
#     else:
#         raise ValueError(f"Expected 72 elements, but got {obs_array.shape[0]}")

# class ReshapeObservationWrapper(gym.Wrapper):
#     def __init__(self, env):
#         super(ReshapeObservationWrapper, self).__init__(env)
    
#     def reshape_observation(self, obs):
#         print(f"obs.shape: {obs}")
#         obs_array = np.array(obs)
#         print(f"obs_array.shape: {obs_array.shape}")
#         if obs_array.shape[0] == 126:
#             return np.reshape(obs_array, (14, 9))
#         else:
#             raise ValueError(f"Expected 72 elements, but got {obs_array.shape[0]}")
    
#     def reset(self, **kwargs):
#         obs = self.env.reset(**kwargs)
#         return self.reshape_observation(obs)

#     def step(self, action):
#         obs, reward, done, info = self.env.step(action)
#         print(f"Step returned obs: {obs}, reward: {reward}, done: {done}, info: {info}")
#         obs = self.reshape_observation(obs)
#         return obs, reward, done, info


# # 定义智能体（连续动作空间）
# class Actor(nn.Module):
#     """策略网络（使用Beta分布处理连续动作）"""
#     def __init__(self, action_dim, state_dim):
#         super(Actor, self).__init__()
#         self.fc1 = nn.Linear(state_dim, 256)
#         self.fc2 = nn.Linear(256, 128)
#         self.alpha_head = nn.Linear(128, action_dim)  # Beta分布参数alpha
#         self.beta_head = nn.Linear(128, action_dim)  # Beta分布参数beta

#     def forward(self, x):
#         x = F.relu(self.fc1(x))
#         x = F.relu(self.fc2(x))
#         alpha = F.softplus(self.alpha_head(x)) + 1e-5  # 保证正数
#         beta = F.softplus(self.beta_head(x)) + 1e-5
#         return alpha, beta

# class Critic(nn.Module):
#     """价值网络"""
#     def __init__(self, state_dim):
#         super(Critic, self).__init__()
#         self.fc1 = nn.Linear(state_dim, 256)
#         self.fc2 = nn.Linear(256, 128)
#         self.fc3 = nn.Linear(128, 1)

#     def forward(self, x):
#         x = F.relu(self.fc1(x))
#         x = F.relu(self.fc2(x))
#         x = self.fc3(x)
#         return x

# class ActorCritic:
#     def __init__(self, env):
#         self.gamma = 0.99
#         self.lr_a = 0.001
#         self.lr_c = 0.001

#         self.env = env
#         self.action_dim = env.action_space.shape[0]  # 假设环境是Box动作空间
#         self.state_dim = env.observation_space.shape[0]

#         self.actor = Actor(self.action_dim, self.state_dim)
#         self.critic = Critic(self.state_dim)

#         self.actor_optimizer = optim.Adam(self.actor.parameters(), lr=self.lr_a)
#         self.critic_optimizer = optim.Adam(self.critic.parameters(), lr=self.lr_c)

#     def get_action(self, state):
#         state = torch.FloatTensor(state)
#         alpha, beta = self.actor(state)
#         dist = Beta(alpha, beta)
#         action = dist.sample()
#         log_prob = dist.log_prob(action).sum(dim=-1)
#         return action.detach().numpy(), log_prob

#     def learn(self, state, action, reward, next_state, done, log_prob):
#         state = torch.FloatTensor(state)
#         next_state = torch.FloatTensor(next_state)
#         reward = torch.FloatTensor([reward])
#         done = torch.FloatTensor([done])
        
#         # 计算TD目标和delta
#         td_target = reward + self.gamma * self.critic(next_state) * (1 - done)
#         critic_value = self.critic(state)
#         delta = td_target.detach() - critic_value

#         # 计算Actor损失
#         actor_loss = -log_prob * delta.detach()

#         # 计算Critic损失
#         critic_loss = F.mse_loss(td_target.detach(), critic_value)

#         # 优化步骤
#         self.actor_optimizer.zero_grad()
#         self.critic_optimizer.zero_grad()
#         actor_loss.backward()
#         critic_loss.backward()
#         self.actor_optimizer.step()
#         self.critic_optimizer.step()

# # 环境初始化
# port = 6001
# simTime = 10 
# startSim = True
# stepTime = 0.05 
# seed = 0
# debug = False

# env = ns3env.Ns3Env(port=port, stepTime=stepTime, startSim=startSim, simSeed=seed, debug=debug)
# # env = ReshapeObservationWrapper(env)
# env.reset()

# # 确保环境动作空间是Box类型
# print("Observation space:", env.observation_space)
# print("Action space:", env.action_space)

# model = ActorCritic(env)

# # 训练循环
# rewards = []
# for episode in range(10):
#     total_reward = 0
#     state = env.reset()
#     done = False
#     while not done:
#         action, log_prob = model.get_action(state)
#         next_state, reward, done, _ = env.step(action)
#         model.learn(state, action, reward, next_state, done, log_prob)
#         state = next_state
#         total_reward += reward
#     rewards.append(total_reward)
#     print(f"Episode: {episode}, Reward: {total_reward}")

# # 绘制奖励曲线
# plt.plot(rewards)
# plt.xlabel('Episode')
# plt.ylabel('Total Reward')
# plt.show()

# import os
# import gym
# import time
# import torch
# import numpy as np
# from ns3gym import ns3env
# from torch import nn, optim
# import matplotlib.pyplot as plt
# from torch.nn import functional as F
# from torch.distributions import Categorical


# # 定义智能体
# class Actor(nn.Module):
#     """策略网络"""
#     def __init__(self, action_dim, state_dim):
#         super(Actor, self).__init__()
#         self.fc1 = nn.Linear(state_dim, 256)
#         self.fc2 = nn.Linear(256, 128)
#         self.fc3 = nn.Linear(128, action_dim)

#     def forward(self, x):
#         x = F.relu(self.fc1(x))
#         x = F.relu(self.fc2(x))
#         x = F.softmax(self.fc3(x), dim=-1)
#         return x
    
# class Critic(nn.Module):
#     """价值网络"""
#     def __init__(self, state_dim):
#         super(Critic, self).__init__()
#         self.fc1 = nn.Linear(state_dim, 256)
#         self.fc2 = nn.Linear(256, 128)
#         self.fc3 = nn.Linear(128, 1)

#     def forward(self, x):
#         x = F.relu(self.fc1(x))
#         x = F.relu(self.fc2(x))
#         x = self.fc3(x)
#         return x

# class ActorCritic:
#     def __init__(self, env):
#         self.gamma = 0.99
#         self.lr_a = 0.001
#         self.lr_c = 0.001

#         self.env = env
#         print("action_space: ", self.env.action_space)
#         print("observation_space: ", self.env.observation_space)
#         self.action_dim = self.env.action_space.shape[0]
#         self.state_dim = self.env.observation_space.shape[0]

#         self.actor = Actor(self.action_dim, self.state_dim)
#         self.critic = Critic(self.state_dim)

#         self.action_optimizer = optim.Adam(self.actor.parameters(), lr=self.lr_a)
#         self.critic_optimizer = optim.Adam(self.critic.parameters(), lr=self.lr_c)

#         self.loos = nn.MSELoss()

#         def get_action(self, state):
#             state = torch.tensor(state, dtype=torch.float32)
#             probs = self.actor(state)
#             m = Categorical(probs)
#             action = m.sample()
#             return action.item()

#         def learn(self, state, action, reward, next_state, done):
#             state = torch.tensor(state, dtype=torch.float32)
#             action = torch.tensor(action, dtype=torch.int64)
#             reward = torch.tensor(reward, dtype=torch.float32)
#             next_state = torch.tensor(next_state, dtype=torch.float32)
#             done = torch.tensor(done, dtype=torch.float32)

#             td_target = reward + self.gamma * self.critic(next_state) * (1. - done)
#             delta = td_target - self.critic(state)

#             self.actor.zero_grad()
#             self.critic.zero_grad()

#             probs = self.actor(state)
#             m = Categorical(probs)
#             log_prob = m.log_prob(action)

#             actor_loss = -log_prob * delta
#             critic_loss = self.loss(td_target, self.critic(state))

#             actor_loss.backward()
#             critic_loss.backward()

#             self.action_optimizer.step()
#             self.critic_optimizer.step()

        

# # 环境初始化 仿真参数设置
# port = 6001
# simTime = 10 
# startSim = True
# stepTime = 0.05 
# seed = 0
# # simArgs = {"--simTime": simTime,
# #            "--testArg": 123,
# #            "--nodeNum": 5,
# #            "--distance": 500}         
# debug = False

# env = ns3env.Ns3Env(port=port, stepTime=stepTime, startSim=startSim, simSeed=seed, debug=debug)
# env.reset()

# ob_space = env.observation_space
# ac_space = env.action_space
# print("Observation space: ", ob_space,  ob_space.dtype)
# print("Action space: ", ac_space, ac_space.dtype)

# model = ActorCritic(env)

# for episode in range(2):
#     total_reward = 0
#     state = env.reset()
#     done = False
#     while not done:
#         action = model.get_action(state)
#         next_state, reward, done, _ = env.step(action)
#         model.learn(state, action, reward, next_state, done)
#         state = next_state
#         total_reward += reward
#     print(f"Episode: {episode}, Reward: {total_reward}")
#     plt.plot(total_reward)
