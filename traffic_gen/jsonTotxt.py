import json
from typing import Union, Dict, List, Optional, Any
import os

"""
    读取并格式化 JSON 数据的类，支持从文件或字符串加载
"""
class jsonRsp:
    """
        初始化 jsonRsp 类
        参数:
            data: JSON 数据，可以是 JSON 字符串、字典或列表
            file_path: JSON 文件路径，如果提供则优先从文件读取
    """
    def __init__(self, data: Union[str, Dict, List, None] = None, file_path: Optional[str] = None):

        self.original_data = None
        self.parsed_data = None
        self.file_path = file_path.replace('\\', '/')
        self.file_name = self.file_path.split('/')[-1].replace(".json", "")
        self.file_type = self.file_path.split('/')[-1].split('.')[-1]

        if file_path:
            self.load_from_file(file_path)
        elif data is not None:
            self.load_from_data(data)
        else:
            raise ValueError("必须提供 data 或 file_path 参数")

    """
        从 JSON 文件加载数据
        参数:
            file_path: JSON 文件路径
    """
    def load_from_file(self, file_path: str) -> None:

        if not os.path.exists(file_path):
            raise FileNotFoundError(f"文件不存在: {file_path}")

        try:
            with open(file_path, 'r', encoding='utf-8') as f:
                content = f.read()
                self.original_data = content
                self.parsed_data = json.loads(content)
        except json.JSONDecodeError as e:
            raise ValueError(f"无效的 JSON 文件格式: {e}")
        except Exception as e:
            raise RuntimeError(f"读取文件时出错: {e}")

    """
        从数据加载 JSON
    
        参数:
            data: JSON 数据，可以是 JSON 字符串、字典或列表
    """
    def load_from_data(self, data: Union[str, Dict, List]) -> None:

        self.original_data = data
        self.parsed_data = self._parse_data(data)

    def _parse_data(self, data: Union[str, Dict, List]) -> Union[Dict, List]:
        """解析 JSON 数据"""
        if isinstance(data, (dict, list)):
            return data
        elif isinstance(data, str):
            try:
                return json.loads(data)
            except json.JSONDecodeError as e:
                raise ValueError(f"无效的 JSON 格式: {e}")
        else:
            raise TypeError(f"不支持的数据类型: {type(data).__name__}")

    """
        将解析后的数据转换为格式化的 JSON 字符串
    
        参数:
            indent: 缩进空格数，默认为 2
            sort_keys: 是否按键排序，默认为 False
            ensure_ascii: 是否确保所有非 ASCII 字符被转义，默认为 False
    
        返回:
            格式化的 JSON 字符串
    """
    def to_json(self, indent: int = 2, sort_keys: bool = False, ensure_ascii: bool = False) -> str:
        return json.dumps(
            self.parsed_data,
            indent=indent,
            sort_keys=sort_keys,
            ensure_ascii=ensure_ascii
        )

    """
        打印格式化的 JSON 数据
    
        参数:
            indent: 缩进空格数，默认为 2
            sort_keys: 是否按键排序，默认为 False
            ensure_ascii: 是否确保所有非 ASCII 字符被转义，默认为 False
    """
    def pretty_print(self, indent: int = 2, sort_keys: bool = False, ensure_ascii: bool = False) -> None:

        print(self.to_json(indent, sort_keys, ensure_ascii))

    """
        获取解析后的原始数据
    """
    def get_data(self) -> Union[Dict, List]:

        return self.parsed_data

    """
        通过路径获取 JSON 中的值
    
        参数:
            key_path: 键路径，使用点分隔，例如 "a.b.c"
            default: 键不存在时的默认值，默认为 None
    
        返回:
            指定路径的值，如果不存在则返回默认值
    """
    def get_value(self, key_path: str, default: Optional[Any] = None) -> Any:
        keys = key_path.split('.')
        current = self.parsed_data

        for key in keys:
            if isinstance(current, dict) and key in current:
                current = current[key]
            elif isinstance(current, list) and key.isdigit() and 0 <= int(key) < len(current):
                current = current[int(key)]
            else:
                return default

        return current

    """
        将json数据格式另存
        参数：
            output_path: 文件输出路径 (required)
            file_type: 文件输出类型, 默认为txt
    """
    def save_as(self, output_path: str, file_type: str = "txt"):
        output_path = output_path.replace('\\', '/')
        # 单位为秒，每隔0.5毫秒一个流
        start_time = 2.0005
        try:
            dataList = self.parsed_data['data']['data']
            with open(output_path + "/" + self.file_name + "." + file_type, 'w', encoding='utf-8') as f:
                for data in dataList:
                    nodePair = [node.split('-')[0].split('.')[-1][1:].lstrip('0') for node in data['linkDir'].split("->")]
                    # flowSize的单位为GB
                    flowSize = float(data['meanSpeed'].split(' ')[0]) * 4
                    # 转化为包的个数，包的大小定义在模拟脚本中的参数packet_payload_size里
                    packetCount = int(flowSize * 1000000000 / 1000)
                    flowInfo = [nodePair[0], nodePair[1], str(3), str(packetCount), "{0:.4f}".format(start_time)]
                    start_time += 0.0005
                    f.write(' '.join(flowInfo) + "\n")
                print(f"文件转写成功，共 {len(dataList)} 条流")
        except Exception as e:
            print(f"写入文件出错 {e}")
