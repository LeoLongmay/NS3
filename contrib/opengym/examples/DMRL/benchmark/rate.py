import numpy as np

from loguru import logger

from constant import PolicyName
from policy_map import get_policy_from_name
from utils import change_root, object_evaluate
from config import model_logs_dir, total_timesteps


def rate_benchmark_all() -> None:
    for p in list(PolicyName)[:5]:
        algo = p.value
        logger.info(f'Policy Name: {algo}')
        trainer, _ = get_policy_from_name(algo)
        model = trainer(model_logs_dir, total_timesteps, 0)
        logger.info(f'Policy Name: {algo} finished')
        objects = object_evaluate(model, model.get_env())
        logger.info(f'Policy Name: {algo} evaluated with success rate: {np.mean(objects[0])}')


if __name__ == '__main__':
    change_root()
    rate_benchmark_all()
