import os


def find_root(current_dir: str, flag_file: str = 'main.py') -> str:
    """
    Find the root directory of the project by searching for a flag file.

    :param current_dir: The current directory to start searching.
    :param flag_file: The flag file to search for.
    :return: The root directory of the project.
    """
    if flag_file in os.listdir(current_dir):
        return current_dir
    else:
        parent_dir = os.path.dirname(current_dir)
        if parent_dir == current_dir:  # the root directory of system is reached
            raise FileNotFoundError(f"Can't find the root directory with the flag file {flag_file}!")
        return find_root(parent_dir, flag_file)


def change_root() -> str:
    """
    Change the current working directory to the root directory of the project.

    :return: The root directory of the project.
    """
    root = find_root(os.path.dirname(__file__))
    os.chdir(root)
    return root
