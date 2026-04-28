# -*- coding: utf-8 -*-
"""
睡姿分类模型训练 - 增加垂直位移数据增强 (2026-03-03)
Sleep Posture Classification Training - With Vertical Shift Augmentation

为单人数据添加垂直位移增强，使模型能够处理人在床的上半部分或下半部分的情况
Add vertical shift augmentation for single person data to handle people in upper or lower half of bed

中文注释 (2026-03-03)
"""

import numpy as np
import pandas as pd
import os
from datetime import datetime

# TensorFlow导入 (2026-03-03)
import tensorflow as tf
from tensorflow.keras import layers, models, backend as K
from tensorflow.keras.callbacks import EarlyStopping, ModelCheckpoint, ReduceLROnPlateau

from sklearn.model_selection import train_test_split
from sklearn.metrics import (
    classification_report, confusion_matrix, accuracy_score, f1_score,
    roc_auc_score, precision_score, recall_score
)
from sklearn.utils.class_weight import compute_class_weight
import warnings
warnings.filterwarnings('ignore')

# =============================================================================
# 配置 Configuration (2026-03-03)
# =============================================================================

SENSOR_ROWS = 26
SENSOR_COLS = 10
SENSOR_TOTAL = SENSOR_ROWS * SENSOR_COLS  # 260
NUM_CLASSES = 2

OUTPUT_DIR = r'D:\ProjectCode\JQ260\SleepPosture\cnn_export_lite_combined_vshift'

# 数据文件路径 - 使用两个数据源 (2026-03-03)
DATA_SOURCE_1 = r'C:\Users\Lenovo\Desktop\sscom_data\4.5cm_1112\parsed_sensor_data.csv'
DATA_SOURCE_2 = r'C:\Users\Lenovo\Desktop\0225JQ_T12\parsed_sensor_data.csv'

# 训练参数 (2026-03-03)
EPOCHS = 200
BATCH_SIZE = 16
VALIDATION_SPLIT = 0.2
RANDOM_STATE = 42

# =============================================================================
# Focal Loss - 处理类别不平衡 (2026-03-03)
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
# 数据增强函数 (2026-03-03)
# =============================================================================

def vertical_shift_augmentation(sensor_data):
    """
    垂直位移数据增强 - 将上半部分数据移到下半部分 (2026-03-03)
    Vertical shift augmentation - Move upper half data to lower half

    输入: 260字节数据 (26x10矩阵)
    输出: 260字节数据，上13行移到下13行，上13行补零
    """
    matrix = sensor_data.reshape(SENSOR_ROWS, SENSOR_COLS)
    shifted_matrix = np.zeros((SENSOR_ROWS, SENSOR_COLS), dtype=np.float32)

    # 将前13行移到后13行 (2026-03-03)
    shifted_matrix[13:, :] = matrix[:13, :]

    return shifted_matrix.flatten()


def augment_data_with_vertical_shift(X, y):
    """
    数据增强：包括水平翻转和垂直位移 (2026-03-03)
    Data augmentation: including horizontal flip and vertical shift
    """
    print("\n" + "=" * 80)
    print("Data Augmentation with Vertical Shift")
    print("=" * 80)

    # 统计类别分布 (2026-03-03)
    unique, counts = np.unique(y, return_counts=True)
    print(f"\nOriginal class distribution:")
    for cls, cnt in zip(unique, counts):
        posture_name = "Supine (仰卧)" if cls == 0 else "Side (侧卧)"
        print(f"  Class {cls} ({posture_name}): {cnt} samples")

    # 找出少数类 (2026-03-03)
    minority_class = unique[np.argmin(counts)]
    majority_class = unique[np.argmax(counts)]
    minority_count = np.min(counts)
    majority_count = np.max(counts)

    print(f"\nMinority class: {minority_class}, count: {minority_count}")
    print(f"Majority class: {majority_class}, count: {majority_count}")

    # 计算需要增强的数量 (2026-03-03)
    augment_needed = majority_count - minority_count
    print(f"Need to augment minority class by {augment_needed} samples")

    # 获取少数类样本 (2026-03-03)
    minority_X = X[y == minority_class]

    augmented_X = [X]
    augmented_y = [y]

    augment_count = 0

    # 1. 水平翻转 (Horizontal flip) (2026-03-03)
    if augment_count < augment_needed:
        flipped = np.flip(minority_X.reshape(-1, SENSOR_ROWS, SENSOR_COLS), axis=2)
        flipped = flipped.reshape(-1, SENSOR_TOTAL)

        to_add = min(len(flipped), augment_needed - augment_count)
        augmented_X.append(flipped[:to_add])
        augmented_y.append(np.full(to_add, minority_class))
        augment_count += to_add
        print(f"  Added {to_add} horizontal flipped samples")

    # 2. 垂直位移 (Vertical shift) - 新增 (2026-03-03)
    if augment_count < augment_needed:
        vshifted = np.array([vertical_shift_augmentation(sample) for sample in minority_X])

        to_add = min(len(vshifted), augment_needed - augment_count)
        augmented_X.append(vshifted[:to_add])
        augmented_y.append(np.full(to_add, minority_class))
        augment_count += to_add
        print(f"  Added {to_add} vertical shifted samples")

    # 3. 添加噪声 (Add noise) (2026-03-03)
    if augment_count < augment_needed:
        noisy = minority_X + np.random.normal(0, 2, minority_X.shape)
        noisy = np.clip(noisy, 0, 255)

        to_add = min(len(noisy), augment_needed - augment_count)
        augmented_X.append(noisy[:to_add])
        augmented_y.append(np.full(to_add, minority_class))
        augment_count += to_add
        print(f"  Added {to_add} noisy samples")

    # 4. 水平翻转 + 垂直位移组合 (Horizontal flip + Vertical shift) - 新增 (2026-03-03)
    if augment_count < augment_needed:
        flipped_vshifted = np.array([
            vertical_shift_augmentation(
                np.flip(sample.reshape(SENSOR_ROWS, SENSOR_COLS), axis=1).flatten()
            ) for sample in minority_X
        ])

        to_add = min(len(flipped_vshifted), augment_needed - augment_count)
        augmented_X.append(flipped_vshifted[:to_add])
        augmented_y.append(np.full(to_add, minority_class))
        augment_count += to_add
        print(f"  Added {to_add} flipped + shifted samples")

    # 合并所有数据 (2026-03-03)
    X_aug = np.vstack(augmented_X)
    y_aug = np.concatenate(augmented_y)

    # 打印增强后的分布 (2026-03-03)
    unique_aug, counts_aug = np.unique(y_aug, return_counts=True)
    print(f"\nAugmented class distribution:")
    for cls, cnt in zip(unique_aug, counts_aug):
        posture_name = "Supine (仰卧)" if cls == 0 else "Side (侧卧)"
        print(f"  Class {cls} ({posture_name}): {cnt} samples")

    return X_aug, y_aug


# =============================================================================
# 数据加载 (2026-03-03)
# =============================================================================

def load_combined_data():
    """
    加载并合并两个数据源 (2026-03-03)
    Load and combine two data sources
    """
    print("\n" + "=" * 80)
    print("Loading Training Data from Multiple Sources")
    print("=" * 80)

    all_sensor_data = []
    all_labels = []

    # 加载数据源1 (2026-03-03)
    if os.path.exists(DATA_SOURCE_1):
        print(f"\nLoading from: {DATA_SOURCE_1}")
        df1 = pd.read_csv(DATA_SOURCE_1, encoding='utf-8-sig')

        for idx, row in df1.iterrows():
            try:
                # 解析传感器数据 (2026-03-03)
                sensor_str = str(row['sensor_data']).strip()
                sensors = [int(h, 16) for h in sensor_str.split()]

                if len(sensors) != 260:
                    continue

                matrix = np.array(sensors, dtype=np.float32).reshape(SENSOR_ROWS, SENSOR_COLS)

                # 处理单人样本 (2026-03-03)
                if row['true_num_persons'] == 1:
                    # 单人：使用完整数据 (2026-03-03)
                    all_sensor_data.append(matrix.flatten())
                    posture = int(row['true_left_posture'])
                    all_labels.append(posture)

                # 处理双人样本 (2026-03-03)
                elif row['true_num_persons'] == 2:
                    # 上半部分 (左侧) - 前13行保留，后13行为0 (2026-03-03)
                    upper_matrix = np.zeros((SENSOR_ROWS, SENSOR_COLS), dtype=np.float32)
                    upper_matrix[:13, :] = matrix[:13, :]
                    all_sensor_data.append(upper_matrix.flatten())
                    all_labels.append(int(row['true_left_posture']))

                    # 下半部分 (右侧) - 将后13行移动到前13行，后13行为0 (2026-03-03)
                    lower_matrix = np.zeros((SENSOR_ROWS, SENSOR_COLS), dtype=np.float32)
                    lower_matrix[:13, :] = matrix[13:, :]  # 将后13行移到前13行位置
                    all_sensor_data.append(lower_matrix.flatten())
                    all_labels.append(int(row['true_right_posture']))

            except Exception as e:
                continue

        print(f"  Loaded {len(all_sensor_data)} samples from source 1")

    # 加载数据源2 (2026-03-03)
    if os.path.exists(DATA_SOURCE_2):
        print(f"\nLoading from: {DATA_SOURCE_2}")
        df2 = pd.read_csv(DATA_SOURCE_2, encoding='utf-8-sig')

        count_before = len(all_sensor_data)

        for idx, row in df2.iterrows():
            try:
                # 解析传感器数据 (2026-03-03)
                sensor_str = str(row['sensor_data']).strip()
                sensors = [int(h, 16) for h in sensor_str.split()]

                if len(sensors) != 260:
                    continue

                matrix = np.array(sensors, dtype=np.float32).reshape(SENSOR_ROWS, SENSOR_COLS)

                # 处理单人样本 (2026-03-03)
                if row['true_num_persons'] == 1:
                    # 单人：使用完整数据 (2026-03-03)
                    all_sensor_data.append(matrix.flatten())
                    posture = int(row['true_left_posture'])
                    all_labels.append(posture)

                # 处理双人样本 (2026-03-03)
                elif row['true_num_persons'] == 2:
                    # 上半部分 (左侧) - 前13行保留，后13行为0 (2026-03-03)
                    upper_matrix = np.zeros((SENSOR_ROWS, SENSOR_COLS), dtype=np.float32)
                    upper_matrix[:13, :] = matrix[:13, :]
                    all_sensor_data.append(upper_matrix.flatten())
                    all_labels.append(int(row['true_left_posture']))

                    # 下半部分 (右侧) - 将后13行移动到前13行，后13行为0 (2026-03-03)
                    lower_matrix = np.zeros((SENSOR_ROWS, SENSOR_COLS), dtype=np.float32)
                    lower_matrix[:13, :] = matrix[13:, :]  # 将后13行移到前13行位置
                    all_sensor_data.append(lower_matrix.flatten())
                    all_labels.append(int(row['true_right_posture']))

            except Exception as e:
                continue

        print(f"  Loaded {len(all_sensor_data) - count_before} samples from source 2")

    X = np.array(all_sensor_data, dtype=np.float32)
    y = np.array(all_labels, dtype=np.int32)

    print(f"\nTotal samples loaded: {len(X)}")
    print(f"  Supine (仰卧, label=0): {np.sum(y == 0)}")
    print(f"  Side (侧卧, label=1): {np.sum(y == 1)}")

    return X, y


# =============================================================================
# 模型构建 (2026-03-03)
# =============================================================================

def build_lite_cnn():
    """构建轻量级CNN模型 (2026-03-03)"""
    model = models.Sequential([
        # 输入层 (2026-03-03)
        layers.Input(shape=(SENSOR_ROWS, SENSOR_COLS, 1)),

        # 第1个卷积块 (2026-03-03)
        layers.Conv2D(8, (3, 3), activation='relu', padding='same'),
        layers.MaxPooling2D((2, 2)),
        layers.Dropout(0.2),

        # 第2个卷积块 (2026-03-03)
        layers.Conv2D(16, (3, 3), activation='relu', padding='same'),
        layers.MaxPooling2D((2, 2)),
        layers.Dropout(0.2),

        # 全局平均池化 (2026-03-03)
        layers.GlobalAveragePooling2D(),

        # 全连接层 (2026-03-03)
        layers.Dense(16, activation='relu'),
        layers.Dropout(0.3),

        # 输出层 (2026-03-03)
        layers.Dense(1, activation='sigmoid')
    ])

    return model


# =============================================================================
# 主训练流程 (2026-03-03)
# =============================================================================

def main():
    """主训练流程 (2026-03-03)"""
    print("\n" + "=" * 80)
    print("JQ260 Sleep Posture Classification Training")
    print("With Vertical Shift Augmentation")
    print("=" * 80)
    print(f"Start time: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")

    # 创建输出目录 (2026-03-03)
    os.makedirs(OUTPUT_DIR, exist_ok=True)

    # 1. 加载数据 (2026-03-03)
    X, y = load_combined_data()

    # 2. 数据增强（包括垂直位移）(2026-03-03)
    X, y = augment_data_with_vertical_shift(X, y)

    # 3. 归一化 (2026-03-03)
    X = X / 255.0

    # 4. Reshape为CNN输入格式 (2026-03-03)
    X = X.reshape(-1, SENSOR_ROWS, SENSOR_COLS, 1)

    # 5. 划分训练集和验证集 (2026-03-03)
    X_train, X_val, y_train, y_val = train_test_split(
        X, y, test_size=VALIDATION_SPLIT, random_state=RANDOM_STATE, stratify=y
    )

    print(f"\nTraining set: {len(X_train)} samples")
    print(f"Validation set: {len(X_val)} samples")

    # 6. 计算类别权重 (2026-03-03)
    class_weights_array = compute_class_weight(
        'balanced', classes=np.unique(y_train), y=y_train
    )
    class_weights = {i: class_weights_array[i] for i in range(len(class_weights_array))}
    print(f"\nClass weights: {class_weights}")

    # 7. 构建模型 (2026-03-03)
    print("\nBuilding model...")
    model = build_lite_cnn()
    model.summary()

    # 8. 编译模型 (2026-03-03)
    model.compile(
        optimizer=tf.keras.optimizers.Adam(learning_rate=0.001),
        loss=focal_loss(gamma=2.0, alpha=0.25),
        metrics=['accuracy', tf.keras.metrics.AUC(name='auc')]
    )

    # 9. 回调函数 (2026-03-03)
    callbacks = [
        EarlyStopping(
            monitor='val_loss',
            patience=20,
            restore_best_weights=True,
            verbose=1
        ),
        ModelCheckpoint(
            os.path.join(OUTPUT_DIR, 'best_model.keras'),
            monitor='val_accuracy',
            save_best_only=True,
            verbose=1
        ),
        ReduceLROnPlateau(
            monitor='val_loss',
            factor=0.5,
            patience=10,
            min_lr=1e-6,
            verbose=1
        )
    ]

    # 10. 训练模型 (2026-03-03)
    print("\nTraining model...")
    history = model.fit(
        X_train, y_train,
        validation_data=(X_val, y_val),
        epochs=EPOCHS,
        batch_size=BATCH_SIZE,
        class_weight=class_weights,
        callbacks=callbacks,
        verbose=1
    )

    # 11. 评估模型 (2026-03-03)
    print("\n" + "=" * 80)
    print("Model Evaluation")
    print("=" * 80)

    y_pred_prob = model.predict(X_val)
    y_pred = (y_pred_prob > 0.5).astype(int).flatten()

    print("\nClassification Report:")
    print(classification_report(y_val, y_pred, target_names=['Supine (仰卧)', 'Side (侧卧)']))

    print("\nConfusion Matrix:")
    cm = confusion_matrix(y_val, y_pred)
    print(cm)

    # 12. 寻找最优阈值 (2026-03-03)
    print("\n" + "=" * 80)
    print("Finding Optimal Threshold")
    print("=" * 80)

    best_threshold = 0.5
    best_accuracy = 0.0

    for threshold in np.arange(0.1, 0.9, 0.01):
        y_pred_thresh = (y_pred_prob > threshold).astype(int).flatten()
        accuracy = accuracy_score(y_val, y_pred_thresh)
        if accuracy > best_accuracy:
            best_accuracy = accuracy
            best_threshold = threshold

    print(f"\nOptimal threshold: {best_threshold:.2f}")
    print(f"Accuracy at optimal threshold: {best_accuracy:.4f}")

    # 13. 保存模型 (2026-03-03)
    print("\n" + "=" * 80)
    print("Saving Model")
    print("=" * 80)

    # 保存为.keras格式 (2026-03-03)
    keras_path = os.path.join(OUTPUT_DIR, 'posture_model_vshift.keras')
    model.save(keras_path)
    print(f"Saved Keras model: {keras_path}")

    # 保存为TFLite格式 (2026-03-03)
    converter = tf.lite.TFLiteConverter.from_keras_model(model)
    converter.optimizations = [tf.lite.Optimize.DEFAULT]
    tflite_model = converter.convert()

    tflite_path = os.path.join(OUTPUT_DIR, 'posture_model_vshift.tflite')
    with open(tflite_path, 'wb') as f:
        f.write(tflite_model)
    print(f"Saved TFLite model: {tflite_path}")

    # 保存阈值 (2026-03-03)
    threshold_path = os.path.join(OUTPUT_DIR, 'optimal_threshold.txt')
    with open(threshold_path, 'w') as f:
        f.write(f"{best_threshold:.2f}\n")
    print(f"Saved optimal threshold: {threshold_path}")

    print(f"\nEnd time: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    print("=" * 80)
    print("Training Complete!")
    print("=" * 80)


if __name__ == '__main__':
    main()
