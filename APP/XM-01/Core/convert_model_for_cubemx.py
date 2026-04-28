# 转换新训练的模型为TFLite格式 (2026-03-03)
import tensorflow as tf
import numpy as np

# 加载模型 (2026-03-03)
model_path = r'D:\ProjectCode\JQ260\SleepPosture\cnn_export_lite_combined\jq260_cnn_lite_retrained.keras'
output_path = r'D:\ProjectCode\JQ260\SleepPosture\cnn_export_lite_combined\jq260_cnn_lite_retrained.tflite'

print("加载模型...")
model = tf.keras.models.load_model(model_path, compile=False)

print("\n模型结构:")
model.summary()

# 转换为TFLite格式 (2026-03-03)
print(f"\n转换为TFLite格式...")
converter = tf.lite.TFLiteConverter.from_keras_model(model)

# 优化选项 (2026-03-03)
converter.optimizations = [tf.lite.Optimize.DEFAULT]

# 转换 (2026-03-03)
tflite_model = converter.convert()

# 保存 (2026-03-03)
with open(output_path, 'wb') as f:
    f.write(tflite_model)

print(f"\n✓ 转换完成!")
print(f"TFLite模型文件: {output_path}")
print(f"文件大小: {len(tflite_model) / 1024:.2f} KB")

# 读取最优阈值 (2026-03-03)
threshold_path = r'D:\ProjectCode\JQ260\SleepPosture\cnn_export_lite_combined\optimal_threshold.npy'
threshold = np.load(threshold_path)[0]
print(f"\n最优阈值: {threshold:.4f}")
print("\n下一步:")
print("1. 在STM32CubeMX中导入这个.tflite文件")
print("2. 生成X-CUBE-AI代码")
print("3. 更新阈值为", threshold)
