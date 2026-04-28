# 解析传感器数据文件
# 从0225JQ_T12目录中的txt文件提取信息
# 分析模型预测与真实标签的对比
# 日期: 2026-03-03

import os
import pandas as pd
import re
import numpy as np
from sklearn.metrics import confusion_matrix, classification_report, accuracy_score

def parse_filename(filename):
    """
    解析文件名以提取位置信息和睡姿
    例如: 1_左侧.txt -> 左侧 (left side), left_posture=1, right_posture=FF
          1_平.txt -> 平 (supine), left_posture=0, right_posture=FF
          D_L平_R平.txt -> L平_R平 (left supine, right supine), left_posture=0, right_posture=0

    返回: (name, left_posture, right_posture)
    left_posture: 0=平躺, 1=左侧/右侧
    right_posture: 0=平躺, 1=左侧/右侧, FF=单人(无右侧人员)
    """
    name = filename.replace('.txt', '')

    # 判断是单人还是双人
    if name.startswith('D_'):
        # 双人情况
        # 提取L和R的姿势
        if 'L平' in name:
            left_posture = 0
        elif 'L左侧' in name or 'L右侧' in name:
            left_posture = 1
        else:
            left_posture = 0  # 默认

        if 'R平' in name:
            right_posture = 0
        elif 'R左侧' in name or 'R右侧' in name:
            right_posture = 1
        else:
            right_posture = 0  # 默认
    else:
        # 单人情况
        right_posture = 'FF'

        if '平' in name:
            left_posture = 0
        elif '左侧' in name or '右侧' in name:
            left_posture = 1
        else:
            left_posture = 0  # 默认

    return name, left_posture, right_posture

def parse_info_bytes(info_str):
    """
    解析info字节以提取模型预测
    info格式: 6个字节
    - 第1字节: 左侧人员睡姿预测 (02=侧卧, 01=平躺, FF=无人)
    - 第2-3字节: 其他信息
    - 第4字节: 右侧人员睡姿预测 (02=侧卧, 01=平躺, FF=无人)
    - 第5-6字节: 其他信息

    返回: (pred_left_posture, pred_right_posture, pred_num_persons)
    """
    bytes_list = info_str.split()

    if len(bytes_list) < 6:
        return None, None, None

    # 第1字节: 左侧睡姿预测
    left_byte = bytes_list[0]
    if left_byte == 'FF':
        pred_left = 'FF'  # 无人
        left_present = False
    elif left_byte == '01':
        pred_left = 0  # 平躺
        left_present = True
    elif left_byte == '02':
        pred_left = 1  # 侧卧
        left_present = True
    else:
        pred_left = int(left_byte, 16) if left_byte != 'FF' else 'FF'
        left_present = (left_byte != 'FF')

    # 第4字节: 右侧睡姿预测
    right_byte = bytes_list[3]
    if right_byte == 'FF':
        pred_right = 'FF'  # 无人
        right_present = False
    elif right_byte == '01':
        pred_right = 0  # 平躺
        right_present = True
    elif right_byte == '02':
        pred_right = 1  # 侧卧
        right_present = True
    else:
        pred_right = int(right_byte, 16) if right_byte != 'FF' else 'FF'
        right_present = (right_byte != 'FF')

    # 预测人数
    if left_present and right_present:
        pred_num_persons = 2
    elif left_present or right_present:
        pred_num_persons = 1
    else:
        pred_num_persons = 0

    return pred_left, pred_right, pred_num_persons

def extract_data_from_line(line):
    """
    从每一行中提取01 0A后的6字节信息和260字节传感器数据
    """
    # 提取hex数据部分 (在]之后)
    if ']' in line:
        hex_part = line.split(']')[1].strip()
    else:
        return None, None

    # 将hex字符串分割成字节列表
    bytes_list = hex_part.split()

    # 查找 "01 0A" 模式
    try:
        for i in range(len(bytes_list) - 1):
            if bytes_list[i] == '01' and bytes_list[i+1] == '0A':
                # 找到01 0A后，提取接下来的6字节作为info
                info_start = i + 2
                info_bytes = bytes_list[info_start:info_start + 6]

                # 提取剩余的260字节作为sensor_data
                sensor_start = info_start + 6
                sensor_bytes = bytes_list[sensor_start:sensor_start + 260]

                # 转换为字符串
                info = ' '.join(info_bytes)
                sensor_data = ' '.join(sensor_bytes)

                return info, sensor_data
    except:
        pass

    return None, None

def process_file(filepath, filename):
    """
    处理单个txt文件
    """
    position, left_posture, right_posture = parse_filename(filename)
    data_rows = []

    # 确定真实人数
    if right_posture == 'FF':
        true_num_persons = 1
    else:
        true_num_persons = 2

    with open(filepath, 'r', encoding='utf-8') as f:
        for line_num, line in enumerate(f, 1):
            info, sensor_data = extract_data_from_line(line)
            if info and sensor_data:
                # 解析预测信息
                pred_left, pred_right, pred_num_persons = parse_info_bytes(info)

                data_rows.append({
                    'file': filename,
                    'position': position,
                    'true_left_posture': left_posture,
                    'true_right_posture': right_posture,
                    'true_num_persons': true_num_persons,
                    'pred_left_posture': pred_left,
                    'pred_right_posture': pred_right,
                    'pred_num_persons': pred_num_persons,
                    'line_number': line_num,
                    'info': info,
                    'sensor_data': sensor_data
                })

    return data_rows

def main():
    # 设置数据目录
    data_dir = r'C:\Users\Lenovo\Desktop\0225JQ_T12'

    # 获取所有txt文件
    txt_files = [f for f in os.listdir(data_dir) if f.endswith('.txt')]

    print(f"找到 {len(txt_files)} 个txt文件")

    # 处理所有文件
    all_data = []
    for txt_file in txt_files:
        filepath = os.path.join(data_dir, txt_file)
        print(f"处理文件: {txt_file}")
        file_data = process_file(filepath, txt_file)
        all_data.extend(file_data)
        print(f"  提取了 {len(file_data)} 条记录")

    # 创建DataFrame
    df = pd.DataFrame(all_data)

    # 保存为CSV
    output_file = os.path.join(data_dir, 'parsed_sensor_data.csv')
    df.to_csv(output_file, index=False, encoding='utf-8-sig')

    print(f"\n总共提取了 {len(df)} 条记录")
    print(f"数据已保存到: {output_file}")

    # 显示前几行
    print("\n数据预览:")
    print(df.head())

    # 显示统计信息
    print("\n各文件记录数:")
    print(df['file'].value_counts())

    print("\n真实睡姿分布:")
    print(f"左侧睡姿 (true_left_posture):")
    print(df['true_left_posture'].value_counts())
    print(f"\n右侧睡姿 (true_right_posture):")
    print(df['true_right_posture'].value_counts())

    # 模型性能分析
    print("\n" + "="*60)
    print("模型性能分析")
    print("="*60)

    # 1. 人数检测准确率
    print("\n1. 人数检测准确率:")
    num_persons_acc = accuracy_score(df['true_num_persons'], df['pred_num_persons'])
    print(f"   准确率: {num_persons_acc*100:.2f}%")
    print(f"   正确: {(df['true_num_persons'] == df['pred_num_persons']).sum()}/{len(df)}")

    # 2. 左侧睡姿分类准确率 (仅单人情况)
    single_person_df = df[df['true_num_persons'] == 1].copy()
    if len(single_person_df) > 0:
        # 过滤掉预测值为'FF'或None的样本，并确保类型一致
        valid_single = single_person_df[
            (single_person_df['pred_left_posture'] != 'FF') &
            (single_person_df['pred_left_posture'].notna())
        ].copy()

        # 确保数据类型一致
        valid_single['pred_left_posture'] = pd.to_numeric(valid_single['pred_left_posture'], errors='coerce')
        valid_single = valid_single[valid_single['pred_left_posture'].notna()]

        if len(valid_single) > 0:
            print("\n2. 左侧睡姿分类准确率 (单人情况):")
            left_posture_acc = accuracy_score(valid_single['true_left_posture'].astype(int),
                                              valid_single['pred_left_posture'].astype(int))
            print(f"   准确率: {left_posture_acc*100:.2f}%")
            print(f"   正确: {(valid_single['true_left_posture'].astype(int) == valid_single['pred_left_posture'].astype(int)).sum()}/{len(valid_single)}")

            print("\n   混淆矩阵 (0=平躺, 1=侧卧):")
            cm = confusion_matrix(valid_single['true_left_posture'].astype(int),
                                 valid_single['pred_left_posture'].astype(int))
            print(cm)
        else:
            print("\n2. 左侧睡姿分类准确率 (单人情况): 无有效预测数据")

    # 3. 双人情况分析
    double_person_df = df[df['true_num_persons'] == 2].copy()
    if len(double_person_df) > 0:
        print("\n3. 双人情况分析:")
        print(f"   总样本数: {len(double_person_df)}")

        # 过滤有效预测并转换类型
        valid_left = double_person_df[
            (double_person_df['pred_left_posture'] != 'FF') &
            (double_person_df['pred_left_posture'].notna())
        ].copy()
        valid_left['pred_left_posture'] = pd.to_numeric(valid_left['pred_left_posture'], errors='coerce')
        valid_left = valid_left[valid_left['pred_left_posture'].notna()]

        valid_right = double_person_df[
            (double_person_df['pred_right_posture'] != 'FF') &
            (double_person_df['pred_right_posture'].notna())
        ].copy()
        valid_right['pred_right_posture'] = pd.to_numeric(valid_right['pred_right_posture'], errors='coerce')
        valid_right = valid_right[valid_right['pred_right_posture'].notna()]

        # 左侧人员睡姿准确率
        if len(valid_left) > 0:
            left_acc = accuracy_score(valid_left['true_left_posture'].astype(int),
                                      valid_left['pred_left_posture'].astype(int))
            print(f"   左侧人员睡姿准确率: {left_acc*100:.2f}%")
        else:
            print(f"   左侧人员睡姿准确率: 无有效预测")

        # 右侧人员睡姿准确率
        if len(valid_right) > 0:
            right_acc = accuracy_score(valid_right['true_right_posture'].astype(int),
                                       valid_right['pred_right_posture'].astype(int))
            print(f"   右侧人员睡姿准确率: {right_acc*100:.2f}%")
        else:
            print(f"   右侧人员睡姿准确率: 无有效预测")

        # 两侧都正确的比例
        valid_both = double_person_df[
            (double_person_df['pred_left_posture'] != 'FF') &
            (double_person_df['pred_right_posture'] != 'FF') &
            (double_person_df['pred_left_posture'].notna()) &
            (double_person_df['pred_right_posture'].notna())
        ].copy()
        valid_both['pred_left_posture'] = pd.to_numeric(valid_both['pred_left_posture'], errors='coerce')
        valid_both['pred_right_posture'] = pd.to_numeric(valid_both['pred_right_posture'], errors='coerce')
        valid_both = valid_both[
            valid_both['pred_left_posture'].notna() &
            valid_both['pred_right_posture'].notna()
        ]

        if len(valid_both) > 0:
            both_correct = ((valid_both['true_left_posture'].astype(int) == valid_both['pred_left_posture'].astype(int)) &
                           (valid_both['true_right_posture'].astype(int) == valid_both['pred_right_posture'].astype(int))).sum()
            print(f"   两侧睡姿都正确: {both_correct}/{len(valid_both)} ({both_correct/len(valid_both)*100:.2f}%)")
        else:
            print(f"   两侧睡姿都正确: 无有效预测")

    # 4. 错误案例分析
    print("\n4. 错误案例分析:")

    # 计算错误时需要考虑'FF'的情况
    left_errors = (df['true_left_posture'] != df['pred_left_posture']) & (df['pred_left_posture'] != 'FF')
    right_errors = ((df['true_right_posture'] != df['pred_right_posture']) &
                   (df['true_right_posture'] != 'FF') &
                   (df['pred_right_posture'] != 'FF'))
    num_errors = (df['true_num_persons'] != df['pred_num_persons'])

    errors_df = df[left_errors | right_errors | num_errors]

    print(f"   错误样本数: {len(errors_df)}/{len(df)} ({len(errors_df)/len(df)*100:.2f}%)")

    if len(errors_df) > 0:
        print("\n   错误类型分布:")
        print(f"   - 人数检测错误: {num_errors.sum()}")
        print(f"   - 左侧睡姿错误: {left_errors.sum()}")
        print(f"   - 右侧睡姿错误: {right_errors.sum()}")

        # 保存错误案例
        error_file = os.path.join(os.path.dirname(output_file), 'error_cases.csv')
        errors_df.to_csv(error_file, index=False, encoding='utf-8-sig')
        print(f"\n   错误案例已保存到: {error_file}")

    print("\n" + "="*60)

if __name__ == '__main__':
    main()
