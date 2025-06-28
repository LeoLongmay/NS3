import scipy.io as io
import gym
import numpy as np
import matplotlib.pyplot as plt
from ns3gym import ns3env
from stable_baselines3 import DQN
from stable_baselines3.common.envs import DummyVecEnv

# Environment initialization
port = 5555
simTime = 10 # seconds
startSim = True
stepTime = 0.05 # seconds
seed = 0
# simArgs = {"--simTime": simTime,
#            "--testArg": 123,
#            "--nodeNum": 5,
#            "--distance": 500}
debug = False

env = ns3env.Ns3Env(port=port, stepTime=stepTime, startSim=startSim, simSeed=seed, debug=debug)
env = DummyVecEnv([lambda: env])
env.reset()

ob_space = env.observation_space
ac_space = env.action_space
print("Observation space: ", ob_space, ob_space.dtype)
print("Action space: ", ac_space, ac_space.dtype)
stepIdx = 0
currIt = 0

# Load the trained model
model = DQN.load("dqn_ns3")

try:
    while True:
        print("Start iteration: ", currIt)
        obs = env.reset()
        print("Step: ", stepIdx)
        print("---obs:", obs)

        while True:
            stepIdx += 1
            action, _states = model.predict(obs, deterministic=True)
            print("---action: ", action)

            print("Step: ", stepIdx)
            obs, reward, done, info = env.step(action)
            print("---obs, reward, done, info: ", obs, reward, done, info)

            if done:
                stepIdx = 0
                if currIt + 1 < iterationNum:
                    env.reset()
                break

        currIt += 1
        if currIt == iterationNum:
            break

except KeyboardInterrupt:
    print("Ctrl-C -> Exit")
finally:
    env.close()
    print("Done")