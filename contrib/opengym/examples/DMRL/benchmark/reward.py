from loguru import logger

from utils import change_root
from constant import PolicyName
from policy_map import get_policy_from_name
from config import model_logs_dir, total_timesteps


def reward_benchmark_all() -> None:
    for p in list(PolicyName)[:5]:
        algo = p.value
        logger.info(f'Policy Name: {algo}')
        trainer, _ = get_policy_from_name(algo)
        trainer(model_logs_dir, total_timesteps)
        logger.info(f'Policy Name: {algo} finished')


if __name__ == '__main__':
    change_root()
    reward_benchmark_all()
