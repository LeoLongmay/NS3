from loguru import logger

import sys

default_format = ("<green>{time:YYYY-MM-DD HH:mm:ss.SSS}</green> | "
                  "<level>{level: <6}</level> | "
                  "<cyan>{name}</cyan>:<cyan>{line}</cyan> - <level>{message}</level>")


def init_logger(debug: bool, sensitive_func_name: list, formatter: str = default_format) -> None:
    """
    Initialize the logger with the specified settings.

    :param debug: Whether to enable debug mode.
    :param sensitive_func_name: The names of the sensitive functions.
    :param formatter: The format of the log message.
    """
    # Remove the default logger
    logger.remove()
    # Add new logger
    logger.add(
        sink=sys.stderr,
        format=formatter,
        level='DEBUG' if debug else 'INFO',
        filter=lambda record: record['function'] in sensitive_func_name,
    )
