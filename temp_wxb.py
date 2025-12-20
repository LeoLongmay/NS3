def find_last_two_columns_max_with_line(file_path):
    """
    读取txt文件，提取倒数第二列、最后一列的数值及行号，返回两列的最大值和对应行号
    :param file_path: txt文件路径
    :return: 字典，包含倒数第二列（second_last）和最后一列（last）的最大值、对应行号；无有效数据则为None
    """
    # 存储格式：[(数值, 行号), ...]
    second_last_col = []  # 倒数第二列的有效数值和行号
    last_col = []         # 最后一列的有效数值和行号

    try:
        with open(file_path, 'r', encoding='utf-8') as f:
            for line_num, line in enumerate(f, start=1):
                stripped_line = line.strip()
                # 跳过空行
                if not stripped_line:
                    print(f"第{line_num}行：空行，已跳过")
                    continue

                # 按空格分割元素（多个连续空格视为一个分隔符）
                elements = stripped_line.split()
                # 检查元素数量是否至少为2
                if len(elements) < 2:
                    print(f"第{line_num}行：元素数量不足2个（仅{len(elements)}个），已跳过")
                    continue

                # 处理倒数第二列元素
                second_last_elem = elements[-2]
                try:
                    second_last_num = float(second_last_elem)
                    second_last_col.append((second_last_num, line_num))
                except ValueError:
                    print(f"第{line_num}行倒数第二列：'{second_last_elem}' 不是有效数字，已跳过")

                # 处理最后一列元素
                last_elem = elements[-1]
                try:
                    last_num = float(last_elem)
                    last_col.append((last_num, line_num))
                except ValueError:
                    print(f"第{line_num}行最后一列：'{last_elem}' 不是有效数字，已跳过")

        # 整理结果
        result = {
            "second_last": None,  # 格式：{"max_value": 值, "line_nums": [行号1, 行号2...]}
            "last": None
        }

        # 处理倒数第二列的最大值
        if second_last_col:
            # 找到最大值
            max_second_last = max([item[0] for item in second_last_col])
            # 找到所有对应行号
            line_nums_second_last = [item[1] for item in second_last_col if item[0] == max_second_last]
            result["second_last"] = {
                "max_value": max_second_last,
                "line_nums": line_nums_second_last
            }
        else:
            print("倒数第二列未找到有效数值")

        # 处理最后一列的最大值
        if last_col:
            max_last = max([item[0] for item in last_col])
            line_nums_last = [item[1] for item in last_col if item[0] == max_last]
            result["last"] = {
                "max_value": max_last,
                "line_nums": line_nums_last
            }
        else:
            print("最后一列未找到有效数值")

        return result

    except FileNotFoundError:
        print(f"错误：文件 '{file_path}' 不存在")
        return None
    except PermissionError:
        print(f"错误：没有权限读取文件 '{file_path}'")
        return None
    except Exception as e:
        print(f"读取文件时发生未知错误：{str(e)}")
        return None

# 主程序入口
if __name__ == "__main__":
    # 请替换为你的txt文件路径（相对路径/绝对路径）
    file_path = "/root/temp/ns-allinone-3.19/ns-3.19/mix/output/880334640/880334640_out_fct.txt"  # 示例："data.txt"、"D:/test/flow_data.txt"
    
    # 调用函数获取结果
    result = find_last_two_columns_max_with_line(file_path)
    
    # 打印结果
    if result is not None:
        # 打印倒数第二列的结果
        if result["second_last"]:
            max_val = result["second_last"]["max_value"]
            line_nums = result["second_last"]["line_nums"]
            # 处理多个行号的情况
            line_str = "、".join(map(str, line_nums))
            print(f"\n倒数第二列的最大值为：{max_val}")
            print(f"该值出现在第{line_str}行")
        else:
            print("\n倒数第二列无有效数值，无法计算最大值")

        # 打印最后一列的结果
        if result["last"]:
            max_val = result["last"]["max_value"]
            line_nums = result["last"]["line_nums"]
            line_str = "、".join(map(str, line_nums))
            print(f"\n最后一列的最大值为：{max_val}")
            print(f"该值出现在第{line_str}行")
        else:
            print("\n最后一列无有效数值，无法计算最大值")