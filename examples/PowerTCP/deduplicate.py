import os
import shutil


def deduplicate_sorted_rtt_file(file_path, max_duplicates=3, backup=True):
    """
    对已升序排序的RTT数据文件去重（保留每个值最多max_duplicates个重复项，保持顺序）
    :param file_path: 排序后的RTT文件路径
    :param max_duplicates: 每个数值最多保留的重复数量（默认3）
    :param backup: 是否备份原文件（默认True，生成 xxx.bak 文件）
    """
    # 1. 校验参数合法性
    if not isinstance(max_duplicates, int) or max_duplicates < 1:
        print(f"错误：最多保留重复数({max_duplicates})必须是大于等于1的整数！")
        return

    # 2. 校验文件是否存在
    if not os.path.exists(file_path):
        print(f"错误：文件 '{file_path}' 不存在！")
        return

    # 3. 备份原文件（可选，推荐开启）
    if backup:
        backup_path = f"{file_path}.bak"
        try:
            shutil.copy2(file_path, backup_path)  # 保留文件元信息
            print(f"已备份原文件至：{backup_path}")
        except Exception as e:
            print(f"警告：备份文件失败，仍继续去重 | 错误：{e}")

    # 4. 读取并提取有效RTT数值
    raw_rtt_list = []
    with open(file_path, 'r', encoding='utf-8') as f:
        for line_idx, line in enumerate(f, 1):
            line = line.strip()
            if not line:
                continue
            try:
                rtt_value = float(line)
                if rtt_value < 0:  # 过滤无效负数值
                    print(f"警告：第{line_idx}行RTT为负数({rtt_value})，跳过")
                    continue
                raw_rtt_list.append(rtt_value)
            except ValueError:
                print(f"警告：第{line_idx}行不是有效数值({line})，跳过")

    if not raw_rtt_list:
        print("错误：未提取到任何有效RTT数据，无需处理！")
        return

    # 5. 去重（保留每个值最多max_duplicates个重复项，利用已排序特性）
    deduplicated_rtt = []
    prev_value = None  # 记录前一个数值
    current_count = 0  # 记录当前数值已保留的数量

    for current_value in raw_rtt_list:
        if current_value != prev_value:
            # 遇到新值，重置计数并保留第一个
            prev_value = current_value
            current_count = 1
            deduplicated_rtt.append(current_value)
        else:
            # 同一数值，计数未达上限则保留
            current_count += 1
            if current_count <= max_duplicates:
                deduplicated_rtt.append(current_value)

    # 6. 统计处理结果
    original_count = len(raw_rtt_list)
    dedup_count = len(deduplicated_rtt)
    removed_count = original_count - dedup_count
    removed_rate = (removed_count / original_count) * 100 if original_count > 0 else 0
    print(
        f"去重统计：原样本数={original_count} | 处理后样本数={dedup_count} | 移除重复数={removed_count} | 移除率={removed_rate:.2f}%")

    # 7. 覆盖写入原文件（处理后的数据仍保持升序）
    try:
        with open(file_path, 'w', encoding='utf-8') as f:
            for rtt in deduplicated_rtt:
                # 整数输出为整数格式，小数保留原精度（如1024→1024，10.83→10.83）
                if rtt.is_integer():
                    f.write(f"{int(rtt)}\n")
                else:
                    f.write(f"{rtt}\n")
        print(f"成功！处理后的数据已覆盖原文件：{file_path}")
    except PermissionError:
        print(f"错误：无权限写入文件 '{file_path}'，请检查文件是否被占用！")
    except Exception as e:
        print(f"写入文件失败：{e}")


# ------------------- 执行去重 -------------------
if __name__ == "__main__":
    # 配置参数
    SORTED_RTT_FILE = "LPCC_rtt.out"  # 替换为你的文件路径
    MAX_DUPLICATES = 4  # 手动修改这里的数值，设置每个值最多保留的重复数

    # 确认操作（避免误删）
    confirm = input(
        f"警告：此操作会处理文件 '{SORTED_RTT_FILE}'，每个数值最多保留{MAX_DUPLICATES}个重复项，并自动备份原文件为 {SORTED_RTT_FILE}.bak\n请确认是否继续？(输入 y 确认，其他取消)：")
    if confirm.lower() == 'y':
        deduplicate_sorted_rtt_file(SORTED_RTT_FILE, max_duplicates=MAX_DUPLICATES, backup=True)
    else:
        print("操作已取消！")