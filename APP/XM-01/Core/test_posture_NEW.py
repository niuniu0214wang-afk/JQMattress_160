# -*- coding: utf-8 -*-
"""
测试睡姿分类模型准确率
Test Sleep Posture Classification Model Accuracy

使用parsed_sensor_data.csv中的数据测试D:\ProjectCode\JQ260\SleepPosture\test_cnn_model.py中的CNN模型
Use data from parsed_sensor_data.csv to test CNN model from D:\ProjectCode\JQ260\SleepPosture\test_cnn_model.py

中文注释 (2026-03-03)
"""

import numpy as np
import pandas as pd
import os
import sys

# 添加模型路径到系统路径 (2026-03-03)
sys.path.append(r'D:\ProjectCode\JQ260\SleepPosture')

# 导入测试模型函数 (2026-03-03)
import tensorflow as tf
from tensorflow.keras import backend as K

# =============================================================================
# 配置 Configuration (2026-03-03)
# =============================================================================

SENSOR_ROWS = 26
SENSOR_COLS = 10
SENSOR_TOTAL = SENSOR_ROWS * SENSOR_COLS  # 260

# 模型路径 (2026-03-03)
MODEL_DIR = r'D:\ProjectCode\JQ260\SleepPosture\cnn_export_lite_combined'
MODEL_PATH = os.path.join(MODEL_DIR, 'jq260_cnn_lite_retrained.keras')
THRESHOLD_PATH = os.path.join(MODEL_DIR, 'optimal_threshold.npy')

# 数据路径 (2026-03-03)
DATA_PATH = r'C:\Users\Lenovo\Desktop\0225JQ_T12\parsed_sensor_data.csv'

# 默认阈值 (2026-03-03)
DEFAULT_THRESHOLD = 0.32


# =============================================================================
# Focal Loss - 需要重新定义以加载模型 (2026-03-03)
# =============================================================================

def focal_loss(gamma=2.0, alpha=0.25):
    """Focal Loss (2026-03-03)"""
    def focal_loss_fn(y_true, y_pred):
        y_true = tf.cast(y_true, tf.float32)
        y_pred = tf.clip_by_value(y_pred, K.epsilon(), 1 - K.epsilon())
        p_t = tf.where(tf.equal(y_true, 1), y_pred, 1 - y_pred)
        alpha_t = tf.where(tf.equal(y_true, 1), alpha, 1 - alpha)
        focal_weight = alpha_t * tf.pow(1 - p_t, gamma)
        ce = -y_true * tf.math.log(y_pred) - (1 - y_true) * tf.math.log(1 - y_pred)
        return tf.reduce_mean(focal_weight * ce)
    return focal_loss_fn


# =============================================================================
# 工具函数 (2026-03-03)
# =============================================================================

def preprocess_for_cnn(X):
    """
    预处理数据用于CNN输入
    Preprocess data for CNN input
    (2026-03-03)
    """
    X_normalized = X / 255.0
    X_reshaped = X_normalized.reshape(-1, SENSOR_ROWS, SENSOR_COLS, 1)
    return X_reshaped


def load_model():
    """
    加载训练好的模型
    Load trained model
    (2026-03-03)
    """
    print("=" * 80)
    print("加载CNN睡姿分类模型")
    print("=" * 80)

    if not os.path.exists(MODEL_PATH):
        print(f"[错误] 模型文件不存在: {MODEL_PATH}")
        return None, DEFAULT_THRESHOLD

    # 加载模型 (2026-03-03)
    print(f"模型路径: {MODEL_PATH}")
    model = tf.keras.models.load_model(
        MODEL_PATH,
        custom_objects={'focal_loss_fn': focal_loss()}
    )
    print("[成功] 模型加载完成")

    # 加载阈值 (2026-03-03)
    threshold = DEFAULT_THRESHOLD
    if os.path.exists(THRESHOLD_PATH):
        threshold = np.load(THRESHOLD_PATH)[0]
        print(f"分类阈值: {threshold:.4f}")
    else:
        print(f"分类阈值: {threshold:.4f} (默认值)")

    return model, threshold


def predict_posture(model, data, threshold):
    """
    预测睡姿
    Predict sleep posture

    返回: (prediction, probability)
      prediction: 0=仰卧(Supine), 1=侧卧(Side)
      probability: 侧卧概率

    (2026-03-03)
    """
    # 预处理 (2026-03-03)
    X = preprocess_for_cnn(data.reshape(1, -1))

    # 预测 (2026-03-03)
    prob = model.predict(X, verbose=0)[0][0]

    # 应用阈值 (2026-03-03)
    if prob >= threshold:
        prediction = 1  # 侧卧 Side
    else:
        prediction = 0  # 仰卧 Supine

    return prediction, prob


def parse_sensor_data(sensor_data_str):
    """
    解析sensor_data字符串为numpy数组
    Parse sensor_data string to numpy array
    (2026-03-03)
    """
    bytes_list = sensor_data_str.split()
    if len(bytes_list) < SENSOR_TOTAL:
        return None

    values = [int(b, 16) for b in bytes_list[:SENSOR_TOTAL]]
    return np.array(values, dtype=np.float32)


# =============================================================================
# 主测试函数 (2026-03-03)
# =============================================================================

def test_posture_classification():
    """
    测试睡姿分类模型准确率
    Test posture classification model accuracy
    (2026-03-03)
    """
    print("\n" + "=" * 80)
    print("睡姿分类模型测试")
    print("=" * 80)

    # 加载模型 (2026-03-03)
    model, threshold = load_model()
    if model is None:
        return

    # 加载数据 (2026-03-03)
    print(f"\n加载数据: {DATA_PATH}")
    df = pd.read_csv(DATA_PATH, encoding='utf-8-sig')
    print(f"总样本数: {len(df)}")

    # 统计信息 (2026-03-03)
    print("\n数据分布:")
    print(f"  单人样本: {len(df[df['true_num_persons'] == 1])}")
    print(f"  双人样本: {len(df[df['true_num_persons'] == 2])}")

    # 准备测试 (2026-03-03)
    results = []

    print("\n" + "=" * 80)
    print("开始测试...")
    print("=" * 80)

    # 遍历所有样本 (2026-03-03)
    for idx, row in df.iterrows():
        # 解析传感器数据 (2026-03-03)
        sensor_data = parse_sensor_data(row['sensor_data'])
        if sensor_data is None:
            continue

        # 获取真实标签 (2026-03-03)
        true_num_persons = row['true_num_persons']

        # 对于单人样本，测试睡姿分类 (2026-03-03)
        if true_num_persons == 1:
            true_posture = row['true_left_posture']  # 0=仰卧, 1=侧卧

            # 预测睡姿 (2026-03-03)
            pred_posture, prob = predict_posture(model, sensor_data, threshold)

            # 记录结果 (2026-03-03)
            results.append({
                'filename': row['file'],
                'true_posture': true_posture,
                'pred_posture': pred_posture,
                'probability': prob,
                'correct': (pred_posture == true_posture)
            })

        # 对于双人样本，分别测试左右两侧 (2026-03-03)
        elif true_num_persons == 2:
            # 分割上下两部分 (2026-03-03)
            # 左侧：前13行保留，后13行为0
            # 右侧：将后13行移动到前13行，后13行为0（保持数据在上半部分）
            matrix = sensor_data.reshape(SENSOR_ROWS, SENSOR_COLS)

            # 上半部分 (左侧) - 前13行保留，后13行为0 (2026-03-03)
            upper_matrix = np.zeros((SENSOR_ROWS, SENSOR_COLS), dtype=np.float32)
            upper_matrix[:13, :] = matrix[:13, :]  # 复制前13行
            upper_matrix_flat = upper_matrix.flatten()

            # 下半部分 (右侧) - 将后13行移动到前13行，后13行为0 (2026-03-03)
            lower_matrix = np.zeros((SENSOR_ROWS, SENSOR_COLS), dtype=np.float32)
            lower_matrix[:13, :] = matrix[13:, :]  # 将后13行移到前13行位置
            lower_matrix_flat = lower_matrix.flatten()

            # 测试左侧 (2026-03-03)
            true_left_posture = row['true_left_posture']
            pred_left_posture, prob_left = predict_posture(model, upper_matrix_flat, threshold)

            results.append({
                'filename': row['file'],
                'side': 'left',
                'true_posture': true_left_posture,
                'pred_posture': pred_left_posture,
                'probability': prob_left,
                'correct': (pred_left_posture == true_left_posture)
            })

            # 测试右侧 (2026-03-03)
            true_right_posture = row['true_right_posture']
            pred_right_posture, prob_right = predict_posture(model, lower_matrix_flat, threshold)

            results.append({
                'filename': row['file'],
                'side': 'right',
                'true_posture': true_right_posture,
                'pred_posture': pred_right_posture,
                'probability': prob_right,
                'correct': (pred_right_posture == true_right_posture)
            })

        # 进度显示 (2026-03-03)
        if (idx + 1) % 100 == 0:
            print(f"  已处理: {idx + 1}/{len(df)}")

    # 转换为DataFrame (2026-03-03)
    results_df = pd.DataFrame(results)

    # 计算准确率 (2026-03-03)
    print("\n" + "=" * 80)
    print("测试结果")
    print("=" * 80)

    # 直接从混淆矩阵计算准确率，避免布尔值类型问题 (2026-03-03)
    TP = len(results_df[(results_df['true_posture'] == 0) & (results_df['pred_posture'] == 0)])
    FP = len(results_df[(results_df['true_posture'] == 0) & (results_df['pred_posture'] == 1)])
    FN = len(results_df[(results_df['true_posture'] == 1) & (results_df['pred_posture'] == 0)])
    TN = len(results_df[(results_df['true_posture'] == 1) & (results_df['pred_posture'] == 1)])

    total_samples = len(results_df)
    correct_samples = TP + TN
    accuracy = correct_samples / total_samples * 100

    print(f"\n总体准确率: {accuracy:.2f}% ({correct_samples}/{total_samples})")

    # 按睡姿分类统计 (2026-03-03)
    print("\n按睡姿分类:")

    # 仰卧准确率 (2026-03-03)
    supine_total = TP + FP
    supine_correct = TP
    supine_acc = supine_correct / supine_total * 100 if supine_total > 0 else 0
    print(f"  仰卧(Supine): {supine_acc:.2f}% ({supine_correct}/{supine_total})")

    # 侧卧准确率 (2026-03-03)
    side_total = FN + TN
    side_correct = TN
    side_acc = side_correct / side_total * 100 if side_total > 0 else 0
    print(f"  侧卧(Side): {side_acc:.2f}% ({side_correct}/{side_total})")

    # 混淆矩阵 (2026-03-03)
    print("\n混淆矩阵:")
    print("              预测仰卧  预测侧卧")
    print(f"  真实仰卧:    {TP:4d}      {FP:4d}")
    print(f"  真实侧卧:    {FN:4d}      {TN:4d}")

    # 保存详细结果 (2026-03-03)
    output_file = 'posture_classification_results.csv'
    results_df.to_csv(output_file, index=False, encoding='utf-8-sig')
    print(f"\n详细结果已保存: {output_file}")

    # 错误样本分析 (2026-03-03)
    error_samples = results_df[~results_df['correct']]
    if len(error_samples) > 0:
        print(f"\n错误样本数: {len(error_samples)}")
        print("\n前10个错误样本:")
        print(error_samples.head(10)[['filename', 'true_posture', 'pred_posture', 'probability']])

    return results_df


# =============================================================================
# 主入口 (2026-03-03)
# =============================================================================

if __name__ == "__main__":
    results = test_posture_classification()

    print("\n" + "=" * 80)
    print("测试完成")
    print("=" * 80)
