import pandas as pd

df = pd.read_csv('posture_classification_results.csv')

print('=' * 80)
print('样本分布详细分析')
print('=' * 80)

# 检查是否有NaN值
print('\nNaN检查:')
print(f'true_posture有NaN: {df["true_posture"].isna().sum()}')
print(f'pred_posture有NaN: {df["pred_posture"].isna().sum()}')

# 检查唯一值
print('\n唯一值:')
print(f'true_posture: {sorted(df["true_posture"].dropna().unique())}')
print(f'pred_posture: {sorted(df["pred_posture"].dropna().unique())}')

# 按side分组统计
print('\n按side分组统计:')

# 单人样本
single = df[df['side'].isna() | (df['side'] == '')]
print(f'\n单人样本: {len(single)}')
print(f'  仰卧(0): {len(single[single["true_posture"] == 0])}')
print(f'  侧卧(1): {len(single[single["true_posture"] == 1])}')

# 双人左侧
left = df[df['side'] == 'left']
print(f'\n双人左侧: {len(left)}')
print(f'  仰卧(0): {len(left[left["true_posture"] == 0])}')
print(f'  侧卧(1): {len(left[left["true_posture"] == 1])}')

# 双人右侧
right = df[df['side'] == 'right']
print(f'\n双人右侧: {len(right)}')
print(f'  仰卧(0): {len(right[right["true_posture"] == 0])}')
print(f'  侧卧(1): {len(right[right["true_posture"] == 1])}')

# 总计
print(f'\n总计: {len(single) + len(left) + len(right)}')
print(f'  总仰卧(0): {len(df[df["true_posture"] == 0])}')
print(f'  总侧卧(1): {len(df[df["true_posture"] == 1])}')
print(f'  总和: {len(df[df["true_posture"] == 0]) + len(df[df["true_posture"] == 1])}')

print('\n问题诊断:')
if len(df[df["true_posture"] == 0]) + len(df[df["true_posture"] == 1]) < len(df):
    missing = len(df) - (len(df[df["true_posture"] == 0]) + len(df[df["true_posture"] == 1]))
    print(f'缺少 {missing} 个样本！可能true_posture有非0/1的值')
