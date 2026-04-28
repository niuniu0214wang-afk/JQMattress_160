# 分析睡姿分类结果 - 修正版 (2026-03-03)
import pandas as pd

# 读取CSV，正确处理布尔列 (2026-03-03)
df = pd.read_csv('posture_classification_results.csv')

# 将correct列从字符串转换为布尔值 (2026-03-03)
if df['correct'].dtype == 'object':
    df['correct'] = df['correct'].map({'True': True, 'False': False})

print("=" * 80)
print("详细分析")
print("=" * 80)

print(f"\n总样本数: {len(df)}")
print(f"总正确数: {df['correct'].sum()}")
print(f"总错误数: {(~df['correct']).sum()}")
print(f"总体准确率: {df['correct'].sum() / len(df) * 100:.2f}%")

print("\n按真实睡姿统计:")
print(f"  真实仰卧(0): {len(df[df['true_posture'] == 0])}")
print(f"  真实侧卧(1): {len(df[df['true_posture'] == 1])}")

print("\n按真实睡姿的准确率:")
for posture in [0, 1]:
    posture_name = "仰卧(Supine)" if posture == 0 else "侧卧(Side)"
    posture_df = df[df['true_posture'] == posture]
    correct = posture_df['correct'].sum()
    total = len(posture_df)
    print(f"  {posture_name}: {correct}/{total} = {correct/total*100:.2f}%")

print("\n混淆矩阵详细:")
TP = len(df[(df['true_posture'] == 0) & (df['pred_posture'] == 0)])  # 真实仰卧预测为仰卧
FP = len(df[(df['true_posture'] == 0) & (df['pred_posture'] == 1)])  # 真实仰卧预测为侧卧
FN = len(df[(df['true_posture'] == 1) & (df['pred_posture'] == 0)])  # 真实侧卧预测为仰卧
TN = len(df[(df['true_posture'] == 1) & (df['pred_posture'] == 1)])  # 真实侧卧预测为侧卧

print("              预测仰卧  预测侧卧")
print(f"  真实仰卧:    {TP:4d}      {FP:4d}")
print(f"  真实侧卧:    {FN:4d}      {TN:4d}")

print("\n验证:")
total_classified = TP + FP + FN + TN
print(f"混淆矩阵总数: {total_classified}")
print(f"应该等于总样本数: {len(df)}")
print(f"匹配: {'✓' if total_classified == len(df) else '✗'}")

print("\n正确数验证:")
correct_from_matrix = TP + TN
print(f"从混淆矩阵计算的正确数: {correct_from_matrix}")
print(f"从correct列统计的正确数: {df['correct'].sum()}")
print(f"匹配: {'✓' if correct_from_matrix == df['correct'].sum() else '✗'}")

# 检查数据类型问题 (2026-03-03)
print("\n" + "=" * 80)
print("数据类型检查")
print("=" * 80)
print(f"true_posture类型: {df['true_posture'].dtype}")
print(f"pred_posture类型: {df['pred_posture'].dtype}")
print(f"correct类型: {df['correct'].dtype}")

print(f"\ntrue_posture唯一值: {sorted(df['true_posture'].unique())}")
print(f"pred_posture唯一值: {sorted(df['pred_posture'].unique())}")
print(f"correct唯一值: {sorted(df['correct'].unique())}")
