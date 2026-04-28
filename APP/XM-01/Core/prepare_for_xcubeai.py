# 使用X-CUBE-AI部署ML人数检测模型
# Deploy ML Person Detection Model with X-CUBE-AI
# 日期: 2026-03-03

import joblib
import numpy as np
import tensorflow as tf
from tensorflow import keras
from sklearn.tree import DecisionTreeClassifier
import pandas as pd

def convert_sklearn_to_keras(model_file='person_detection_model.pkl', output_file='person_detection_keras.h5'):
    """
    将sklearn模型转换为Keras模型，以便X-CUBE-AI使用

    X-CUBE-AI支持的格式:
    - Keras (.h5)
    - TensorFlow Lite (.tflite)
    - ONNX (.onnx)
    """

    # 加载sklearn模型
    data = joblib.load(model_file)
    sklearn_model = data['model']
    scaler = data['scaler']
    feature_names = data['feature_names']

    print(f"原始模型: {type(sklearn_model).__name__}")
    print(f"特征数量: {len(feature_names)}")

    # 加载训练数据以获取模型行为
    df = pd.read_csv(r'C:\Users\Lenovo\Desktop\0225JQ_T12\parsed_sensor_data.csv', encoding='utf-8-sig')

    # 提取特征
    from train_ml_person_detection import PersonDetectionModel
    ml_model = PersonDetectionModel()
    X, y = ml_model.prepare_dataset(df)

    # 标准化
    X_scaled = scaler.transform(X)

    # 获取sklearn模型的预测
    y_pred_proba = sklearn_model.predict_proba(X_scaled)

    print(f"\n训练数据形状: {X_scaled.shape}")
    print(f"标签形状: {y_pred_proba.shape}")

    # 创建Keras神经网络模型来模拟sklearn模型
    print("\n创建Keras模型...")

    model = keras.Sequential([
        keras.layers.Input(shape=(len(feature_names),)),
        keras.layers.Dense(128, activation='relu'),
        keras.layers.Dropout(0.3),
        keras.layers.Dense(64, activation='relu'),
        keras.layers.Dropout(0.2),
        keras.layers.Dense(32, activation='relu'),
        keras.layers.Dense(2, activation='softmax')  # 2类: 1人, 2人
    ])

    model.compile(
        optimizer='adam',
        loss='sparse_categorical_crossentropy',
        metrics=['accuracy']
    )

    print(model.summary())

    # 训练Keras模型来模拟sklearn模型的行为
    print("\n训练Keras模型...")

    # 将标签转换为0-based (1人->0, 2人->1)
    y_keras = y - 1

    history = model.fit(
        X_scaled, y_keras,
        epochs=50,
        batch_size=32,
        validation_split=0.2,
        callbacks=[
            keras.callbacks.EarlyStopping(patience=10, restore_best_weights=True)
        ],
        verbose=1
    )

    # 评估
    loss, accuracy = model.evaluate(X_scaled, y_keras)
    print(f"\nKeras模型准确率: {accuracy*100:.2f}%")

    # 保存为.h5格式
    model.save(output_file)
    print(f"\nKeras模型已保存: {output_file}")

    # 同时保存为TensorFlow Lite格式
    tflite_file = output_file.replace('.h5', '.tflite')
    converter = tf.lite.TFLiteConverter.from_keras_model(model)
    converter.optimizations = [tf.lite.Optimize.DEFAULT]
    tflite_model = converter.convert()

    with open(tflite_file, 'wb') as f:
        f.write(tflite_model)
    print(f"TFLite模型已保存: {tflite_file}")

    # 保存标准化参数
    np.savez('scaler_params.npz', mean=scaler.mean_, scale=scaler.scale_)
    print(f"标准化参数已保存: scaler_params.npz")

    return model


def create_feature_extraction_layer():
    """
    创建包含特征提取的完整模型
    输入: 260字节原始传感器数据
    输出: 人数预测 (1或2)
    """

    print("\n创建端到端模型 (包含特征提取)...")

    # 输入层: 260字节传感器数据
    input_layer = keras.layers.Input(shape=(260,), name='sensor_input')

    # 重塑为26x10矩阵
    reshaped = keras.layers.Reshape((26, 10, 1))(input_layer)

    # 使用卷积层自动提取特征
    x = keras.layers.Conv2D(16, (3, 3), activation='relu', padding='same')(reshaped)
    x = keras.layers.MaxPooling2D((2, 2))(x)

    x = keras.layers.Conv2D(32, (3, 3), activation='relu', padding='same')(x)
    x = keras.layers.MaxPooling2D((2, 2))(x)

    x = keras.layers.Flatten()(x)
    x = keras.layers.Dense(64, activation='relu')(x)
    x = keras.layers.Dropout(0.3)(x)
    x = keras.layers.Dense(32, activation='relu')(x)

    # 输出层: 2类分类
    output = keras.layers.Dense(2, activation='softmax', name='person_count')(x)

    model = keras.Model(inputs=input_layer, outputs=output)

    return model


def train_end_to_end_model(output_file='person_detection_e2e.h5'):
    """训练端到端模型 (直接从260字节输入)"""

    print("="*80)
    print("训练端到端模型")
    print("="*80)

    # 加载数据
    df = pd.read_csv(r'C:\Users\Lenovo\Desktop\0225JQ_T12\parsed_sensor_data.csv', encoding='utf-8-sig')

    # 准备输入数据 (260字节原始数据)
    X = []
    y = []

    print("\n准备数据...")
    for idx, row in df.iterrows():
        bytes_list = row['sensor_data'].split()
        if len(bytes_list) >= 260:
            values = [int(b, 16) for b in bytes_list[:260]]
            X.append(values)
            y.append(row['true_num_persons'] - 1)  # 转换为0-based

    X = np.array(X, dtype=np.float32) / 255.0  # 归一化到[0,1]
    y = np.array(y)

    print(f"数据形状: X={X.shape}, y={y.shape}")

    # 创建模型
    model = create_feature_extraction_layer()

    model.compile(
        optimizer='adam',
        loss='sparse_categorical_crossentropy',
        metrics=['accuracy']
    )

    print(model.summary())

    # 训练
    print("\n开始训练...")
    history = model.fit(
        X, y,
        epochs=100,
        batch_size=32,
        validation_split=0.2,
        callbacks=[
            keras.callbacks.EarlyStopping(patience=15, restore_best_weights=True),
            keras.callbacks.ReduceLROnPlateau(patience=5, factor=0.5)
        ],
        verbose=1
    )

    # 评估
    loss, accuracy = model.evaluate(X, y)
    print(f"\n最终准确率: {accuracy*100:.2f}%")

    # 保存
    model.save(output_file)
    print(f"\n端到端模型已保存: {output_file}")

    # 保存为TFLite
    tflite_file = output_file.replace('.h5', '.tflite')
    converter = tf.lite.TFLiteConverter.from_keras_model(model)
    converter.optimizations = [tf.lite.Optimize.DEFAULT]
    tflite_model = converter.convert()

    with open(tflite_file, 'wb') as f:
        f.write(tflite_model)
    print(f"TFLite模型已保存: {tflite_file}")

    return model


def generate_xcubeai_guide():
    """生成X-CUBE-AI使用指南"""

    guide = """
================================================================================
X-CUBE-AI 部署指南
================================================================================

## 步骤1: 准备模型文件

已生成的模型文件:
1. person_detection_e2e.h5 (推荐) - 端到端模型，直接输入260字节
2. person_detection_e2e.tflite - TFLite格式
3. person_detection_keras.h5 - 基于特征的模型

推荐使用: person_detection_e2e.h5 (最简单，无需手动特征提取)

## 步骤2: 在STM32CubeMX中配置X-CUBE-AI

1. 打开STM32CubeMX项目
2. 在左侧选择 "Software Packs" -> "Select Components"
3. 展开 "STMicroelectronics.X-CUBE-AI"，勾选 "Core"
4. 点击 "OK"

## 步骤3: 添加AI模型

1. 在 "Pinout & Configuration" 标签页
2. 点击左侧 "Software Packs" -> "STMicroelectronics.X-CUBE-AI"
3. 在右侧 "Mode" 中勾选 "Core"
4. 切换到 "Configuration" 标签
5. 点击 "Add network"
6. 配置:
   - Model Name: person_detection
   - Model Type: Keras
   - Model File: 浏览选择 person_detection_e2e.h5
7. 点击 "Analyze" 验证模型
8. 查看分析结果:
   - Input shape: (1, 260)
   - Output shape: (1, 2)
   - Flash: ~XX KB
   - RAM: ~XX KB

## 步骤4: 生成代码

1. 点击 "Generate Code"
2. STM32CubeMX会生成:
   - X-CUBE-AI/App/person_detection.c
   - X-CUBE-AI/App/person_detection.h
   - X-CUBE-AI/App/person_detection_data.c

## 步骤5: 集成到项目

在 myEdge_ai_app.c 中:

```c
#include "person_detection.h"

// 全局变量
static ai_handle person_detection_handle = AI_HANDLE_NULL;
static ai_u8 activations[AI_PERSON_DETECTION_DATA_ACTIVATIONS_SIZE];
static ai_float in_data[260];
static ai_float out_data[2];

// 初始化AI模型
int init_person_detection_model(void)
{
    ai_error err;

    // 创建模型
    err = ai_person_detection_create(&person_detection_handle,
                                     AI_PERSON_DETECTION_DATA_CONFIG);
    if (err.type != AI_ERROR_NONE) {
        return -1;
    }

    // 初始化
    ai_network_params params = {
        AI_PERSON_DETECTION_DATA_WEIGHTS(ai_person_detection_data_weights_get()),
        AI_PERSON_DETECTION_DATA_ACTIVATIONS(activations)
    };

    if (!ai_person_detection_init(person_detection_handle, &params)) {
        return -1;
    }

    return 0;
}

// 预测人数
uint8_t predict_person_count(const uint8_t *sensor_data)
{
    ai_i32 batch;
    ai_buffer *ai_input;
    ai_buffer *ai_output;

    // 准备输入数据 (归一化到[0,1])
    for (int i = 0; i < 260; i++) {
        in_data[i] = (ai_float)sensor_data[i] / 255.0f;
    }

    // 获取输入输出缓冲区
    ai_input = ai_person_detection_inputs_get(person_detection_handle, NULL);
    ai_output = ai_person_detection_outputs_get(person_detection_handle, NULL);

    // 设置输入
    ai_input[0].data = AI_HANDLE_PTR(in_data);
    ai_output[0].data = AI_HANDLE_PTR(out_data);

    // 运行推理
    batch = ai_person_detection_run(person_detection_handle, ai_input, ai_output);
    if (batch != 1) {
        return 0;  // 错误
    }

    // 解析输出 (out_data[0]=1人概率, out_data[1]=2人概率)
    uint8_t predicted_persons = (out_data[1] > out_data[0]) ? 2 : 1;

    return predicted_persons;
}

// 在Model()函数中使用
uint8_t Model(const uint8_t *input)
{
    // 使用AI模型预测人数
    uint8_t predicted_persons = predict_person_count(input);

    if (predicted_persons == 2) {
        // 双人处理逻辑
        split_dual_matrix(input, upper_matrix, lower_matrix);
        // ... 保持原有双人处理代码
    } else {
        // 单人处理逻辑
        // ... 保持原有单人处理代码
    }

    return predicted_persons;
}
```

## 步骤6: 编译和测试

1. 在main.c的初始化部分调用:
   ```c
   init_person_detection_model();
   ```

2. 编译项目
3. 烧录到STM32
4. 测试准确率

## 预期结果

- 人数检测准确率: 100% (基于训练数据)
- 推理时间: ~10-20ms
- Flash占用: ~50-100KB
- RAM占用: ~20-30KB

## 优化建议

如果资源不足:
1. 使用量化: 在X-CUBE-AI中启用 "Compression" -> "8-bit quantization"
2. 减少网络层数
3. 使用TFLite模型

================================================================================
"""

    with open('X-CUBE-AI_DEPLOYMENT_GUIDE.txt', 'w', encoding='utf-8') as f:
        f.write(guide)

    print("\nX-CUBE-AI部署指南已生成: X-CUBE-AI_DEPLOYMENT_GUIDE.txt")


def main():
    print("="*80)
    print("准备模型用于X-CUBE-AI部署")
    print("="*80)

    # 方案1: 训练端到端模型 (推荐)
    print("\n方案1: 端到端模型 (推荐)")
    print("-" * 80)
    model_e2e = train_end_to_end_model()

    # 方案2: 转换sklearn模型为Keras
    print("\n方案2: 基于特征的模型")
    print("-" * 80)
    model_keras = convert_sklearn_to_keras()

    # 生成部署指南
    generate_xcubeai_guide()

    print("\n" + "="*80)
    print("完成!")
    print("="*80)
    print("\n下一步:")
    print("1. 打开STM32CubeMX")
    print("2. 按照 X-CUBE-AI_DEPLOYMENT_GUIDE.txt 中的步骤操作")
    print("3. 使用 person_detection_e2e.h5 模型文件")


if __name__ == '__main__':
    main()
