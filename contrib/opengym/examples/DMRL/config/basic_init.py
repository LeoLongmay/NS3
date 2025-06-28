import torch
from constant import *

fix_seed = True
load_model = False
pause_render = False

model_logs_dir = "log/"
total_timesteps = 5_000_000
algorithm = PPO_POLICY

device = 'cuda' if torch.cuda.is_available() else 'cpu'
