# 深度分析模型性能并提供改进建议
# 日期: 2026-03-03

import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import seaborn as sns
from sklearn.metrics import confusion_matrix, classification_report
import os

# 设置中文字体
plt.rcParams['font.sans-serif'] = ['SimHei', 'Microsoft YaHei']
plt.rcParams['axes.unicode_minus'] = False

def load_data(csv_path):
    """加载解析后的数据"""
    df = pd.read_csv(csv_path, encoding='utf-8-sig')
    return df

def analyze_by_scenario(df):
    """按场景分析模型性能"""
    print("\n" + "="*80)
    print("按场景分析模型性能")
    print("="*80)

    scenarios = df['position'].unique()

    results = []
    for scenario in scenarios:
        scenario_df = df[df['position'] == scenario]

        # 计算各项指标
        num_persons_acc = (scenario_df['true_num_persons'] == scenario_df['pred_num_persons']).mean()
        left_posture_acc = (scenario_df['true_left_posture'] == scenario_df['pred_left_posture']).mean()

        # 右侧睡姿准确率(仅双人情况)
        if (scenario_df['true_right_posture'] != 'FF').any():
            right_mask = scenario_df['true_right_posture'] != 'FF'
            right_posture_acc = (scenario_df[right_mask]['true_right_posture'] ==
                                scenario_df[right_mask]['pred_right_posture']).mean()
        else:
            right_posture_acc = np.nan

        results.append({
            '场景': scenario,
            '样本数': len(scenario_df),
            '人数检测准确率': f"{num_persons_acc*100:.2f}%",
            '左侧睡姿准确率': f"{left_posture_acc*100:.2f}%",
            '右侧睡姿准确率': f"{right_posture_acc*100:.2f}%" if not np.isnan(right_posture_acc) else 'N/A'
        })

    results_df = pd.DataFrame(results)
    print(results_df.to_string(index=False))

    return results_df

def analyze_confusion_patterns(df):
    """分析混淆模式"""
    print("\n" + "="*80)
    print("混淆模式分析")
    print("="*80)

    # 单人情况的睡姿混淆
    single_df = df[df['true_num_persons'] == 1]
    if len(single_df) > 0:
        print("\n单人情况 - 睡姿混淆矩阵:")
        print("真实\\预测    平躺(0)    侧卧(1)")

        cm = confusion_matrix(single_df['true_left_posture'],
                             single_df['pred_left_posture'],
                             labels=[0, 1])

        for i, row in enumerate(cm):
            label = "平躺(0)" if i == 0 else "侧卧(1)"
            print(f"{label:12s} {row[0]:6d}     {row[1]:6d}")

        # 计算误判率
        if cm[0, 1] + cm[0, 0] > 0:
            false_side_rate = cm[0, 1] / (cm[0, 1] + cm[0, 0])
            print(f"\n平躺被误判为侧卧的比例: {false_side_rate*100:.2f}%")

        if cm[1, 0] + cm[1, 1] > 0:
            false_supine_rate = cm[1, 0] / (cm[1, 0] + cm[1, 1])
            print(f"侧卧被误判为平躺的比例: {false_supine_rate*100:.2f}%")

    # 双人情况分析
    double_df = df[df['true_num_persons'] == 2]
    if len(double_df) > 0:
        print("\n\n双人情况 - 左侧人员睡姿混淆矩阵:")
        print("真实\\预测    平躺(0)    侧卧(1)")

        cm_left = confusion_matrix(double_df['true_left_posture'],
                                   double_df['pred_left_posture'],
                                   labels=[0, 1])

        for i, row in enumerate(cm_left):
            label = "平躺(0)" if i == 0 else "侧卧(1)"
            print(f"{label:12s} {row[0]:6d}     {row[1]:6d}")

        print("\n双人情况 - 右侧人员睡姿混淆矩阵:")
        print("真实\\预测    平躺(0)    侧卧(1)")

        cm_right = confusion_matrix(double_df['true_right_posture'],
                                    double_df['pred_right_posture'],
                                    labels=[0, 1])

        for i, row in enumerate(cm_right):
            label = "平躺(0)" if i == 0 else "侧卧(1)"
            print(f"{label:12s} {row[0]:6d}     {row[1]:6d}")

def analyze_temporal_patterns(df):
    """分析时序模式"""
    print("\n" + "="*80)
    print("时序稳定性分析")
    print("="*80)

    for file in df['file'].unique():
        file_df = df[df['file'] == file].sort_values('line_number')

        # 计算预测的稳定性(连续预测的变化次数)
        left_changes = (file_df['pred_left_posture'].diff() != 0).sum()
        right_changes = (file_df['pred_right_posture'].diff() != 0).sum()

        print(f"\n文件: {file}")
        print(f"  样本数: {len(file_df)}")
        print(f"  左侧预测变化次数: {left_changes}")
        print(f"  右侧预测变化次数: {right_changes}")
        print(f"  预测稳定性: {(1 - left_changes/len(file_df))*100:.2f}%")

def generate_recommendations(df):
    """生成改进建议"""
    print("\n" + "="*80)
    print("模型改进建议")
    print("="*80)

    recommendations = []

    # 1. 整体准确率
    overall_acc = ((df['true_left_posture'] == df['pred_left_posture']) &
                   ((df['true_right_posture'] == df['pred_right_posture']) |
                    (df['true_right_posture'] == 'FF'))).mean()

    print(f"\n当前整体准确率: {overall_acc*100:.2f}%")

    if overall_acc < 0.95:
        recommendations.append("整体准确率低于95%，建议:")
        recommendations.append("  - 增加训练数据量")
        recommendations.append("  - 检查传感器数据质量")
        recommendations.append("  - 优化特征提取方法")

    # 2. 单人vs双人性能对比
    single_acc = (df[df['true_num_persons'] == 1]['true_left_posture'] ==
                  df[df['true_num_persons'] == 1]['pred_left_posture']).mean()
    double_acc = ((df[df['true_num_persons'] == 2]['true_left_posture'] ==
                   df[df['true_num_persons'] == 2]['pred_left_posture']) &
                  (df[df['true_num_persons'] == 2]['true_right_posture'] ==
                   df[df['true_num_persons'] == 2]['pred_right_posture'])).mean()

    print(f"\n单人场景准确率: {single_acc*100:.2f}%")
    print(f"双人场景准确率: {double_acc*100:.2f}%")

    if abs(single_acc - double_acc) > 0.1:
        recommendations.append("\n单人和双人场景性能差异较大，建议:")
        recommendations.append("  - 针对性能较差的场景增加训练样本")
        recommendations.append("  - 考虑使用场景自适应的模型结构")

    # 3. 混淆模式分析
    single_df = df[df['true_num_persons'] == 1]
    if len(single_df) > 0:
        cm = confusion_matrix(single_df['true_left_posture'],
                             single_df['pred_left_posture'],
                             labels=[0, 1])

        if cm[0, 1] > cm[1, 0]:
            recommendations.append("\n平躺更容易被误判为侧卧，建议:")
            recommendations.append("  - 增加平躺姿势的训练样本")
            recommendations.append("  - 优化平躺姿势的特征提取")
        elif cm[1, 0] > cm[0, 1]:
            recommendations.append("\n侧卧更容易被误判为平躺，建议:")
            recommendations.append("  - 增加侧卧姿势的训练样本")
            recommendations.append("  - 优化侧卧姿势的特征提取")

    # 4. 数据不平衡问题
    posture_dist = df['true_left_posture'].value_counts()
    if len(posture_dist) > 1:
        imbalance_ratio = posture_dist.max() / posture_dist.min()
        if imbalance_ratio > 2:
            recommendations.append(f"\n数据不平衡(比例: {imbalance_ratio:.2f}:1)，建议:")
            recommendations.append("  - 使用数据增强技术平衡样本")
            recommendations.append("  - 使用加权损失函数")
            recommendations.append("  - 考虑使用SMOTE等过采样方法")

    # 输出建议
    if recommendations:
        for rec in recommendations:
            print(rec)
    else:
        print("\n模型性能良好，继续保持!")

    print("\n" + "="*80)

def main():
    # 数据路径
    data_dir = r'C:\Users\Lenovo\Desktop\0225JQ_T12'
    csv_path = os.path.join(data_dir, 'parsed_sensor_data.csv')

    # 加载数据
    print("加载数据...")
    df = load_data(csv_path)
    print(f"总样本数: {len(df)}")

    # 各项分析
    scenario_results = analyze_by_scenario(df)
    analyze_confusion_patterns(df)
    analyze_temporal_patterns(df)
    generate_recommendations(df)

    # 保存场景分析结果
    output_path = os.path.join(data_dir, 'scenario_analysis.csv')
    scenario_results.to_csv(output_path, index=False, encoding='utf-8-sig')
    print(f"\n场景分析结果已保存到: {output_path}")

if __name__ == '__main__':
    main()
