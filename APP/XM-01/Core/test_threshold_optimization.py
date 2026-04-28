# 模拟STM32人数检测算法 - 阈值测试工具
# Simulate STM32 Person Detection Algorithm - Threshold Testing Tool
# 日期: 2026-03-03

import pandas as pd
import numpy as np
from sklearn.metrics import confusion_matrix, classification_report, accuracy_score
import matplotlib.pyplot as plt
import seaborn as sns

# 设置中文字体
plt.rcParams['font.sans-serif'] = ['SimHei', 'Microsoft YaHei']
plt.rcParams['axes.unicode_minus'] = False

class PersonDetectionSimulator:
    """模拟STM32的人数检测算法"""

    def __init__(self, balance_min=0.25, balance_max=0.75,
                 min_region_sensors=0, max_pressure_diff_ratio=1.0,
                 min_total_pressure=7500, min_total_sensors=120,
                 min_components=3):
        """
        初始化检测器参数

        Args:
            balance_min: 最小均衡比例 (默认0.25)
            balance_max: 最大均衡比例 (默认0.75)
            min_region_sensors: 每侧最小传感器数 (默认0=不检查)
            max_pressure_diff_ratio: 最大压力差比例 (默认1.0=不检查)
            min_total_pressure: 最小总压力 (默认7500)
            min_total_sensors: 最小总传感器数 (默认120)
            min_components: 最小连通分量数 (默认3)
        """
        self.balance_min = balance_min
        self.balance_max = balance_max
        self.min_region_sensors = min_region_sensors
        self.max_pressure_diff_ratio = max_pressure_diff_ratio
        self.min_total_pressure = min_total_pressure
        self.min_total_sensors = min_total_sensors
        self.min_components = min_components

    def parse_sensor_data(self, sensor_data_str):
        """解析传感器数据字符串为26x10矩阵 (26行x10列)"""
        bytes_list = sensor_data_str.split()
        if len(bytes_list) < 260:
            return None

        values = [int(b, 16) for b in bytes_list[:260]]
        # 260字节按行优先存储: 26行 x 10列
        matrix = np.array(values).reshape(26, 10)
        return matrix

    def split_upper_lower(self, matrix):
        """
        将矩阵分为上下两部分
        - Upper (右侧/上侧): 前13行 (rows 0-12)
        - Lower (左侧/下侧): 后13行 (rows 13-25)
        """
        upper = matrix[:13, :]  # 前13行
        lower = matrix[13:, :]  # 后13行
        return upper, lower

    def calculate_region_stats(self, region):
        """计算区域统计信息"""
        threshold = 50  # 压力阈值
        active_sensors = np.sum(region > threshold)
        total_pressure = np.sum(region)

        # 简单的连通分量估算 (实际MCU使用flood fill)
        # 这里简化为: 活跃传感器数 / 20 作为连通分量数的估计
        components = max(1, active_sensors // 20) if active_sensors > 0 else 0

        return {
            'sensors': active_sensors,
            'pressure': total_pressure,
            'components': components
        }

    def detect_persons(self, sensor_data_str):
        """
        检测人数

        Returns:
            int: 预测的人数 (0, 1, 或 2)
        """
        matrix = self.parse_sensor_data(sensor_data_str)
        if matrix is None:
            return 0

        upper, lower = self.split_upper_lower(matrix)

        upper_stats = self.calculate_region_stats(upper)
        lower_stats = self.calculate_region_stats(lower)

        total_sensors = upper_stats['sensors'] + lower_stats['sensors']
        total_pressure = upper_stats['pressure'] + lower_stats['pressure']
        total_components = upper_stats['components'] + lower_stats['components']

        # 空床检测
        if total_components == 0 or total_pressure < 1000 or total_sensors < 20:
            return 0

        # 计算分布均衡度
        if total_pressure > 0:
            upper_pressure_ratio = upper_stats['pressure'] / total_pressure
            lower_pressure_ratio = lower_stats['pressure'] / total_pressure
            min_pressure_ratio = min(upper_pressure_ratio, lower_pressure_ratio)
            max_pressure_ratio = max(upper_pressure_ratio, lower_pressure_ratio)
        else:
            min_pressure_ratio = 0
            max_pressure_ratio = 0

        if total_sensors > 0:
            upper_sensor_ratio = upper_stats['sensors'] / total_sensors
            lower_sensor_ratio = lower_stats['sensors'] / total_sensors
            min_sensor_ratio = min(upper_sensor_ratio, lower_sensor_ratio)
            max_sensor_ratio = max(upper_sensor_ratio, lower_sensor_ratio)
        else:
            min_sensor_ratio = 0
            max_sensor_ratio = 0

        # 条件1: 两侧都有连通分量
        is_dual_by_components = (upper_stats['components'] > 0) and (lower_stats['components'] > 0)

        # 条件2: 总连通分量 >= min_components
        is_dual_by_total = (total_components >= self.min_components)

        # 条件3: 绝对值足够大
        is_dual_by_magnitude = (total_pressure >= self.min_total_pressure) and (total_sensors >= self.min_total_sensors)

        # 条件4: 分布均衡
        is_balanced_pressure = (min_pressure_ratio > self.balance_min) and (max_pressure_ratio < self.balance_max)
        is_balanced_sensors = (min_sensor_ratio > self.balance_min) and (max_sensor_ratio < self.balance_max)

        # 条件5: 每侧区域大小检查
        upper_valid = (upper_stats['sensors'] >= self.min_region_sensors)
        lower_valid = (lower_stats['sensors'] >= self.min_region_sensors)

        # 条件6: 压力差检查
        pressure_diff = abs(upper_stats['pressure'] - lower_stats['pressure'])
        max_allowed_diff = total_pressure * self.max_pressure_diff_ratio
        is_balanced_diff = (pressure_diff < max_allowed_diff)

        # 综合判断
        is_true_dual = (is_dual_by_components and
                       is_dual_by_total and
                       is_balanced_pressure and
                       is_balanced_sensors and
                       is_dual_by_magnitude and
                       upper_valid and
                       lower_valid and
                       is_balanced_diff)

        if is_true_dual:
            return 2
        else:
            return 1  # 默认单人


def test_threshold_configuration(df, config):
    """测试特定阈值配置"""
    detector = PersonDetectionSimulator(**config)

    predictions = []
    for idx, row in df.iterrows():
        pred = detector.detect_persons(row['sensor_data'])
        predictions.append(pred)

    df['pred_num_persons_sim'] = predictions

    # 计算准确率
    accuracy = accuracy_score(df['true_num_persons'], df['pred_num_persons_sim'])

    # 单人准确率
    single_df = df[df['true_num_persons'] == 1]
    single_acc = accuracy_score(single_df['true_num_persons'], single_df['pred_num_persons_sim']) if len(single_df) > 0 else 0

    # 双人准确率
    double_df = df[df['true_num_persons'] == 2]
    double_acc = accuracy_score(double_df['true_num_persons'], double_df['pred_num_persons_sim']) if len(double_df) > 0 else 0

    # 混淆矩阵
    cm = confusion_matrix(df['true_num_persons'], df['pred_num_persons_sim'], labels=[1, 2])

    return {
        'accuracy': accuracy,
        'single_acc': single_acc,
        'double_acc': double_acc,
        'confusion_matrix': cm,
        'predictions': predictions
    }


def grid_search_thresholds(df):
    """网格搜索最优阈值"""
    print("="*80)
    print("网格搜索最优阈值")
    print("="*80)

    # 定义搜索空间
    balance_min_values = [0.25, 0.30, 0.35, 0.40, 0.45]
    balance_max_values = [0.75, 0.70, 0.65, 0.60, 0.55]
    min_region_sensors_values = [0, 30, 40, 50, 60]
    max_pressure_diff_values = [1.0, 0.50, 0.40, 0.35, 0.30]

    best_config = None
    best_accuracy = 0
    results = []

    total_tests = len(balance_min_values) * len(min_region_sensors_values) * len(max_pressure_diff_values)
    test_count = 0

    for balance_min in balance_min_values:
        balance_max = 1.0 - balance_min  # 对称设置

        for min_region in min_region_sensors_values:
            for max_diff in max_pressure_diff_values:
                test_count += 1

                config = {
                    'balance_min': balance_min,
                    'balance_max': balance_max,
                    'min_region_sensors': min_region,
                    'max_pressure_diff_ratio': max_diff,
                    'min_total_pressure': 7500,
                    'min_total_sensors': 120,
                    'min_components': 3
                }

                result = test_threshold_configuration(df, config)

                results.append({
                    'balance_min': balance_min,
                    'balance_max': balance_max,
                    'min_region_sensors': min_region,
                    'max_pressure_diff_ratio': max_diff,
                    'accuracy': result['accuracy'],
                    'single_acc': result['single_acc'],
                    'double_acc': result['double_acc']
                })

                if result['accuracy'] > best_accuracy:
                    best_accuracy = result['accuracy']
                    best_config = config
                    best_result = result

                # 进度显示
                if test_count % 10 == 0:
                    print(f"进度: {test_count}/{total_tests} ({test_count/total_tests*100:.1f}%)")

    print(f"\n完成! 共测试 {len(results)} 种配置")

    # 转换为DataFrame
    results_df = pd.DataFrame(results)

    # 排序
    results_df = results_df.sort_values('accuracy', ascending=False)

    return results_df, best_config, best_result


def visualize_results(results_df, best_config):
    """可视化结果"""
    fig, axes = plt.subplots(2, 2, figsize=(15, 12))

    # 1. 准确率 vs balance_min
    ax1 = axes[0, 0]
    for min_region in results_df['min_region_sensors'].unique():
        subset = results_df[results_df['min_region_sensors'] == min_region]
        ax1.plot(subset['balance_min'], subset['accuracy'], marker='o', label=f'min_region={min_region}')
    ax1.set_xlabel('Balance Min Threshold')
    ax1.set_ylabel('Accuracy')
    ax1.set_title('准确率 vs 均衡阈值')
    ax1.legend()
    ax1.grid(True)

    # 2. 单人 vs 双人准确率
    ax2 = axes[0, 1]
    top_10 = results_df.head(10)
    x = np.arange(len(top_10))
    width = 0.35
    ax2.bar(x - width/2, top_10['single_acc'], width, label='单人准确率')
    ax2.bar(x + width/2, top_10['double_acc'], width, label='双人准确率')
    ax2.set_xlabel('配置编号')
    ax2.set_ylabel('准确率')
    ax2.set_title('Top 10配置: 单人 vs 双人准确率')
    ax2.legend()
    ax2.grid(True)

    # 3. 热力图: balance_min vs min_region_sensors
    ax3 = axes[1, 0]
    pivot = results_df.pivot_table(
        values='accuracy',
        index='balance_min',
        columns='min_region_sensors',
        aggfunc='mean'
    )
    sns.heatmap(pivot, annot=True, fmt='.3f', cmap='YlGnBu', ax=ax3)
    ax3.set_title('准确率热力图: Balance Min vs Min Region Sensors')

    # 4. 压力差阈值影响
    ax4 = axes[1, 1]
    for balance_min in [0.25, 0.35, 0.40, 0.45]:
        subset = results_df[results_df['balance_min'] == balance_min]
        grouped = subset.groupby('max_pressure_diff_ratio')['accuracy'].mean()
        ax4.plot(grouped.index, grouped.values, marker='o', label=f'balance_min={balance_min}')
    ax4.set_xlabel('Max Pressure Diff Ratio')
    ax4.set_ylabel('Accuracy')
    ax4.set_title('准确率 vs 压力差阈值')
    ax4.legend()
    ax4.grid(True)

    plt.tight_layout()
    plt.savefig('threshold_optimization_results.png', dpi=300, bbox_inches='tight')
    print("\n可视化结果已保存: threshold_optimization_results.png")


def main():
    # 加载数据
    print("加载数据...")
    df = pd.read_csv(r'C:\Users\Lenovo\Desktop\0225JQ_T12\parsed_sensor_data.csv', encoding='utf-8-sig')
    print(f"总样本数: {len(df)}")
    print(f"单人样本: {len(df[df['true_num_persons']==1])}")
    print(f"双人样本: {len(df[df['true_num_persons']==2])}")

    # 测试当前配置 (0.25-0.75)
    print("\n" + "="*80)
    print("测试当前配置 (balance: 0.25-0.75)")
    print("="*80)

    current_config = {
        'balance_min': 0.25,
        'balance_max': 0.75,
        'min_region_sensors': 0,
        'max_pressure_diff_ratio': 1.0,
        'min_total_pressure': 7500,
        'min_total_sensors': 120,
        'min_components': 3
    }

    current_result = test_threshold_configuration(df, current_config)
    print(f"\n当前配置准确率: {current_result['accuracy']*100:.2f}%")
    print(f"单人准确率: {current_result['single_acc']*100:.2f}%")
    print(f"双人准确率: {current_result['double_acc']*100:.2f}%")
    print("\n混淆矩阵:")
    print("真实\\预测    1人      2人")
    cm = current_result['confusion_matrix']
    print(f"1人:      {cm[0,0]:6d}   {cm[0,1]:6d}")
    print(f"2人:      {cm[1,0]:6d}   {cm[1,1]:6d}")

    # 测试推荐配置 (0.40-0.60)
    print("\n" + "="*80)
    print("测试推荐配置 (balance: 0.40-0.60, min_region: 50, max_diff: 0.40)")
    print("="*80)

    recommended_config = {
        'balance_min': 0.40,
        'balance_max': 0.60,
        'min_region_sensors': 50,
        'max_pressure_diff_ratio': 0.40,
        'min_total_pressure': 7500,
        'min_total_sensors': 120,
        'min_components': 3
    }

    recommended_result = test_threshold_configuration(df, recommended_config)
    print(f"\n推荐配置准确率: {recommended_result['accuracy']*100:.2f}%")
    print(f"单人准确率: {recommended_result['single_acc']*100:.2f}%")
    print(f"双人准确率: {recommended_result['double_acc']*100:.2f}%")
    print("\n混淆矩阵:")
    print("真实\\预测    1人      2人")
    cm = recommended_result['confusion_matrix']
    print(f"1人:      {cm[0,0]:6d}   {cm[0,1]:6d}")
    print(f"2人:      {cm[1,0]:6d}   {cm[1,1]:6d}")

    # 网格搜索最优配置
    print("\n" + "="*80)
    print("开始网格搜索最优配置...")
    print("="*80)

    results_df, best_config, best_result = grid_search_thresholds(df)

    # 显示Top 10配置
    print("\n" + "="*80)
    print("Top 10 最佳配置")
    print("="*80)
    print(results_df.head(10).to_string(index=False))

    # 显示最佳配置详情
    print("\n" + "="*80)
    print("最佳配置详情")
    print("="*80)
    print(f"Balance Min: {best_config['balance_min']}")
    print(f"Balance Max: {best_config['balance_max']}")
    print(f"Min Region Sensors: {best_config['min_region_sensors']}")
    print(f"Max Pressure Diff Ratio: {best_config['max_pressure_diff_ratio']}")
    print(f"\n准确率: {best_result['accuracy']*100:.2f}%")
    print(f"单人准确率: {best_result['single_acc']*100:.2f}%")
    print(f"双人准确率: {best_result['double_acc']*100:.2f}%")
    print("\n混淆矩阵:")
    print("真实\\预测    1人      2人")
    cm = best_result['confusion_matrix']
    print(f"1人:      {cm[0,0]:6d}   {cm[0,1]:6d}")
    print(f"2人:      {cm[1,0]:6d}   {cm[1,1]:6d}")

    # 保存结果
    results_df.to_csv('threshold_search_results.csv', index=False, encoding='utf-8-sig')
    print("\n搜索结果已保存: threshold_search_results.csv")

    # 可视化
    visualize_results(results_df, best_config)

    # 生成C代码建议
    print("\n" + "="*80)
    print("C代码修改建议")
    print("="*80)
    print(f"""
在 Core/Src/myEdge_ai_app.c 第587-588行修改为:

int is_balanced_pressure = (min_pressure_ratio > {best_config['balance_min']:.2f}f) && (max_pressure_ratio < {best_config['balance_max']:.2f}f);
int is_balanced_sensors = (min_sensor_ratio > {best_config['balance_min']:.2f}f) && (max_sensor_ratio < {best_config['balance_max']:.2f}f);

在第619行后添加:

// 条件5: 每侧区域大小检查 (2026-03-03)
int min_region_sensors = {best_config['min_region_sensors']};
int upper_valid = (upper_sensors >= min_region_sensors);
int lower_valid = (lower_sensors >= min_region_sensors);

// 条件6: 压力差检查 (2026-03-03)
int pressure_diff = (upper_pressure > lower_pressure) ?
                    (upper_pressure - lower_pressure) :
                    (lower_pressure - upper_pressure);
int max_allowed_diff = (total_pressure * {int(best_config['max_pressure_diff_ratio']*100)}) / 100;
int is_balanced_diff = (pressure_diff < max_allowed_diff);

修改第622行判断条件为:

if (is_dual_by_components && is_dual_by_total &&
    is_balanced_pressure && is_balanced_sensors &&
    is_dual_by_magnitude &&
    upper_valid && lower_valid &&
    is_balanced_diff)
""")

    print("\n优化完成!")


if __name__ == '__main__':
    main()
