import os


def filter_and_sort_rtt_file(file_path):
    """
    过滤文件中的RTT行，提取数值并按从大到小排序，覆盖原文件
    :param file_path: 原数据文件的路径（绝对/相对路径）
    """
    # 1. 校验文件是否存在
    if not os.path.exists(file_path):
        print(f"错误：文件 '{file_path}' 不存在！")
        return

    # 2. 读取原文件并过滤提取RTT数值
    rtt_values = []
    with open(file_path, 'r', encoding='utf-8') as f:
        for line_idx, line in enumerate(f, 1):
            line = line.strip()
            # 跳过空行
            if not line:
                continue

            # 筛选以RTT:开头的行
            if line.startswith('RTT:'):
                try:
                    # 提取冒号后的数值（兼容 RTT:1024 或 RTT: 1024 格式）
                    rtt_str = line.split(':', 1)[1].strip()
                    rtt_num = float(rtt_str)  # 支持整数/小数RTT值
                    rtt_values.append(rtt_num)
                except (IndexError, ValueError) as e:
                    print(f"警告：第{line_idx}行RTT格式错误，跳过 | 行内容：{line} | 错误：{e}")

    # 3. 校验有效RTT数据（避免无数据时清空文件）
    if not rtt_values:
        print("错误：未提取到任何有效RTT数据，不会覆盖原文件！")
        return

    # 4. 对RTT数值降序排序（从大到小）
    rtt_values_sorted = sorted(rtt_values, reverse=False)

    # 5. 覆盖写入原文件（注意：此操作会清空原文件所有内容）
    try:
        with open(file_path, 'w', encoding='utf-8') as f:
            for rtt in rtt_values_sorted:
                # 若RTT是整数则输出整数格式，否则保留原小数（更美观）
                if rtt.is_integer():
                    f.write(f"{int(rtt)}\n")
                else:
                    f.write(f"{rtt}\n")
        print(f"成功！已过滤并排序RTT数据，覆盖原文件：{file_path}")
        print(f"提取并排序的RTT数量：{len(rtt_values_sorted)}")
        print(f"排序后RTT最大值：{rtt_values_sorted[0]} ms，最小值：{rtt_values_sorted[-1]} ms")
    except PermissionError:
        print(f"错误：无权限写入文件 '{file_path}'，请检查文件是否被占用或权限设置！")
    except Exception as e:
        print(f"写入文件失败：{e}")


# ------------------- 执行函数 -------------------
if __name__ == "__main__":
    # 替换为你的数据文件路径（例如："./data.txt" 或 "D:/rtt_log.txt"）
    TARGET_FILE = "LPCC_rtt.out"

    # 安全提示（覆盖文件有风险，建议先备份）
    confirm = input(f"警告：此操作会覆盖文件 '{TARGET_FILE}' 的所有内容！\n请确认是否继续？(输入 y 确认，其他取消)：")
    if confirm.lower() == 'y':
        filter_and_sort_rtt_file(TARGET_FILE)
    else:
        print("操作已取消！")