# 优化人数检测模型
# 分析人数检测错误模式并提供训练数据建议
# 日期: 2026-03-03

import pandas as pd
import numpy as np
import os
from collections import Counter

def load_data(csv_path):
    """加载解析后的数据"""
    df = pd.read_csv(csv_path, encoding='utf-8-sig')
    return df

def analyze_person_detection_errors(df):
    """详细分析人数检测错误"""
    print("="*80)
    print("人数检测错误分析")
    print("="*80)

    # 统计真实人数分布
    print("\n真实人数分布:")
    true_dist = df['true_num_persons'].value_counts().sort_index()
    for num, count in true_dist.items():
        print(f"  {num}人: {count}样本 ({count/len(df)*100:.2f}%)")

    # 统计预测人数分布
    print("\n预测人数分布:")
    pred_dist = df['pred_num_persons'].value_counts().sort_index()
    for num, count in pred_dist.items():
        print(f"  {num}人: {count}样本 ({count/len(df)*100:.2f}%)")

    # 混淆矩阵
    print("\n人数检测混淆矩阵:")
    print("真实\\预测     0人      1人      2人")
    for true_num in sorted(df['true_num_persons'].unique()):
        row_data = []
        for pred_num in [0, 1, 2]:
            count = len(df[(df['true_num_persons'] == true_num) &
                          (df['pred_num_persons'] == pred_num)])
            row_data.append(count)
        print(f"{true_num}人:      {row_data[0]:6d}   {row_data[1]:6d}   {row_data[2]:6d}")

    # 错误类型分析
    print("\n错误类型详细分析:")

    # 单人被误判为双人
    single_to_double = df[(df['true_num_persons'] == 1) & (df['pred_num_persons'] == 2)]
    print(f"\n1. 单人被误判为双人: {len(single_to_double)}样本 ({len(single_to_double)/len(df[df['true_num_persons']==1])*100:.2f}%)")
    if len(single_to_double) > 0:
        print("   涉及场景:")
        for pos, count in single_to_double['position'].value_counts().items():
            print(f"     - {pos}: {count}次")

    # 双人被误判为单人
    double_to_single = df[(df['true_num_persons'] == 2) & (df['pred_num_persons'] == 1)]
    print(f"\n2. 双人被误判为单人: {len(double_to_single)}样本 ({len(double_to_single)/len(df[df['true_num_persons']==2])*100:.2f}%)")
    if len(double_to_single) > 0:
        print("   涉及场景:")
        for pos, count in double_to_single['position'].value_counts().items():
            print(f"     - {pos}: {count}次")

    # 被误判为0人
    to_zero = df[(df['true_num_persons'] > 0) & (df['pred_num_persons'] == 0)]
    print(f"\n3. 有人被误判为无人: {len(to_zero)}样本")
    if len(to_zero) > 0:
        print("   涉及场景:")
        for pos, count in to_zero['position'].value_counts().items():
            print(f"     - {pos}: {count}次")

    return single_to_double, double_to_single, to_zero

def analyze_info_bytes_pattern(df):
    """分析info字节的模式"""
    print("\n" + "="*80)
    print("Info字节模式分析")
    print("="*80)

    # 分析第1字节(左侧)和第4字节(右侧)的分布
    print("\n按真实人数统计预测字节分布:")

    for true_num in sorted(df['true_num_persons'].unique()):
        subset = df[df['true_num_persons'] == true_num]
        print(f"\n真实{true_num}人场景 (共{len(subset)}样本):")

        # 统计左侧预测
        left_pred_dist = subset['pred_left_posture'].value_counts()
        print(f"  左侧预测分布:")
        for val, count in left_pred_dist.items():
            label = "无人(FF)" if val == 'FF' else f"平躺(0)" if val == 0 else f"侧卧(1)" if val == 1 else f"其他({val})"
            print(f"    {label}: {count}次 ({count/len(subset)*100:.2f}%)")

        # 统计右侧预测
        right_pred_dist = subset['pred_right_posture'].value_counts()
        print(f"  右侧预测分布:")
        for val, count in right_pred_dist.items():
            label = "无人(FF)" if val == 'FF' else f"平躺(0)" if val == 0 else f"侧卧(1)" if val == 1 else f"其他({val})"
            print(f"    {label}: {count}次 ({count/len(subset)*100:.2f}%)")

def generate_training_recommendations(df, single_to_double, double_to_single):
    """生成训练数据优化建议"""
    print("\n" + "="*80)
    print("模型优化建议")
    print("="*80)

    # 计算当前准确率
    person_acc = (df['true_num_persons'] == df['pred_num_persons']).mean()
    print(f"\n当前人数检测准确率: {person_acc*100:.2f}%")
    print(f"目标准确率: 95%+")
    print(f"需要提升: {(0.95 - person_acc)*100:.2f}个百分点")

    print("\n关键问题:")

    # 问题1: 单人误判为双人
    if len(single_to_double) > len(df) * 0.1:
        print(f"\n1. 【严重】单人场景被大量误判为双人 ({len(single_to_double)}样本)")
        print("   可能原因:")
        print("   - 单人数据训练不足")
        print("   - 传感器噪声被误识别为第二个人")
        print("   - 阈值设置不当")
        print("\n   建议措施:")
        print("   ✓ 增加单人场景训练数据，特别是以下场景:")
        for pos in single_to_double['position'].value_counts().head(3).index:
            print(f"     - {pos}")
        print("   ✓ 调整人数检测阈值，提高检测灵敏度")
        print("   ✓ 添加噪声过滤机制")
        print("   ✓ 使用数据增强技术模拟单人场景的各种变化")

    # 问题2: 双人误判为单人
    if len(double_to_single) > len(df) * 0.1:
        print(f"\n2. 【严重】双人场景被大量误判为单人 ({len(double_to_single)}样本)")
        print("   可能原因:")
        print("   - 双人数据训练不足")
        print("   - 某一侧人员信号较弱")
        print("   - 特征提取不充分")
        print("\n   建议措施:")
        print("   ✓ 增加双人场景训练数据，特别是以下场景:")
        for pos in double_to_single['position'].value_counts().head(3).index:
            print(f"     - {pos}")
        print("   ✓ 优化特征提取，增强对弱信号的检测能力")
        print("   ✓ 考虑使用注意力机制关注两侧区域")

    # 数据平衡建议
    print("\n3. 数据集平衡建议:")
    single_count = len(df[df['true_num_persons'] == 1])
    double_count = len(df[df['true_num_persons'] == 2])
    ratio = max(single_count, double_count) / min(single_count, double_count)

    print(f"   当前数据分布:")
    print(f"   - 单人: {single_count}样本 ({single_count/len(df)*100:.2f}%)")
    print(f"   - 双人: {double_count}样本 ({double_count/len(df)*100:.2f}%)")
    print(f"   - 不平衡比例: {ratio:.2f}:1")

    if ratio > 1.5:
        print(f"\n   ⚠ 数据不平衡较严重")
        if single_count > double_count:
            print(f"   建议: 增加{int((single_count - double_count) * 0.8)}个双人样本")
        else:
            print(f"   建议: 增加{int((double_count - single_count) * 0.8)}个单人样本")

    # 具体训练数据收集建议
    print("\n4. 训练数据收集计划:")
    print("\n   优先级1 - 高错误率场景 (需要大量补充):")

    # 找出错误率最高的场景
    error_by_scene = []
    for pos in df['position'].unique():
        scene_df = df[df['position'] == pos]
        error_rate = (scene_df['true_num_persons'] != scene_df['pred_num_persons']).mean()
        error_by_scene.append((pos, error_rate, len(scene_df)))

    error_by_scene.sort(key=lambda x: x[1], reverse=True)

    for pos, error_rate, count in error_by_scene[:5]:
        if error_rate > 0.5:
            print(f"   - {pos}: 错误率{error_rate*100:.1f}%, 建议增加{int(count * 0.5)}个样本")

    print("\n   优先级2 - 样本量不足场景:")
    sample_by_scene = df['position'].value_counts()
    min_samples = sample_by_scene.min()
    for pos, count in sample_by_scene.items():
        if count < min_samples * 1.2:
            print(f"   - {pos}: 当前{count}样本, 建议增加到{int(min_samples * 1.5)}样本")

def export_error_samples_for_retraining(df, output_dir):
    """导出错误样本用于重新训练"""
    print("\n" + "="*80)
    print("导出错误样本")
    print("="*80)

    # 人数检测错误的样本
    person_errors = df[df['true_num_persons'] != df['pred_num_persons']].copy()

    if len(person_errors) > 0:
        # 按场景分组导出
        for pos in person_errors['position'].unique():
            scene_errors = person_errors[person_errors['position'] == pos]
            filename = f"error_samples_{pos.replace('/', '_')}.csv"
            filepath = os.path.join(output_dir, filename)
            scene_errors.to_csv(filepath, index=False, encoding='utf-8-sig')
            print(f"  导出 {pos}: {len(scene_errors)}个错误样本 -> {filename}")

        # 导出汇总
        summary_file = os.path.join(output_dir, 'person_detection_errors_summary.csv')
        person_errors.to_csv(summary_file, index=False, encoding='utf-8-sig')
        print(f"\n  错误样本汇总已保存: person_detection_errors_summary.csv")

def main():
    # 数据路径
    data_dir = r'C:\Users\Lenovo\Desktop\0225JQ_T12'
    csv_path = os.path.join(data_dir, 'parsed_sensor_data.csv')

    # 加载数据
    print("加载数据...")
    df = load_data(csv_path)
    print(f"总样本数: {len(df)}\n")

    # 分析人数检测错误
    single_to_double, double_to_single, to_zero = analyze_person_detection_errors(df)

    # 分析info字节模式
    analyze_info_bytes_pattern(df)

    # 生成优化建议
    generate_training_recommendations(df, single_to_double, double_to_single)

    # 导出错误样本
    export_error_samples_for_retraining(df, data_dir)

    print("\n" + "="*80)
    print("分析完成!")
    print("="*80)

if __name__ == '__main__':
    main()
