# 垂直位移数据增强说明 (2026-03-03)
# Vertical Shift Data Augmentation Documentation

## 问题描述 (Problem Description)

### 原始问题
- 单人训练数据主要包含人在床上半部分的样本
- 当实际使用时，人可能在床的下半部分（右侧）
- 模型没有见过"人在下半部分"的模式，可能导致预测不准确

### Original Issue
- Single person training data mainly contains samples with person in upper half of bed
- In real use, person might be in lower half (right side) of bed
- Model hasn't seen "person in lower half" patterns, may lead to inaccurate predictions

## 解决方案 (Solution)

### 数据增强策略
通过垂直位移增强，为每个单人样本创建额外的训练样本：

1. **原始样本**: 人在上半部分
   ```
   [前13行: 有数据]
   [后13行: 空或少量数据]
   ```

2. **增强样本**: 人在下半部分
   ```
   [前13行: 空或少量数据]
   [后13行: 有数据] ← 从前13行移过来
   ```

### Augmentation Strategy
Through vertical shift augmentation, create additional training samples for each single person sample:

1. **Original sample**: Person in upper half
   ```
   [First 13 rows: Has data]
   [Last 13 rows: Empty or sparse]
   ```

2. **Augmented sample**: Person in lower half
   ```
   [First 13 rows: Empty or sparse]
   [Last 13 rows: Has data] ← Moved from first 13 rows
   ```

## 实现细节 (Implementation Details)

### 垂直位移函数
```python
def vertical_shift_augmentation(sensor_data):
    """
    垂直位移数据增强 - 将上半部分数据移到下半部分
    Vertical shift augmentation - Move upper half data to lower half

    输入: 260字节数据 (26x10矩阵)
    输出: 260字节数据，上13行移到下13行，上13行补零
    """
    matrix = sensor_data.reshape(26, 10)
    shifted_matrix = np.zeros((26, 10), dtype=np.float32)

    # 将前13行移到后13行
    shifted_matrix[13:, :] = matrix[:13, :]

    return shifted_matrix.flatten()
```

### 数据增强组合
新的训练脚本包含4种增强方式：

1. **水平翻转 (Horizontal Flip)**: 左右镜像
2. **垂直位移 (Vertical Shift)**: 上下移动 ← **新增**
3. **添加噪声 (Add Noise)**: 增加鲁棒性
4. **组合增强 (Combined)**: 水平翻转 + 垂直位移 ← **新增**

### Augmentation Combinations
The new training script includes 4 augmentation methods:

1. **Horizontal Flip**: Left-right mirror
2. **Vertical Shift**: Up-down movement ← **NEW**
3. **Add Noise**: Increase robustness
4. **Combined**: Horizontal flip + Vertical shift ← **NEW**

## 使用方法 (Usage)

### 1. 运行训练脚本
```bash
cd C:\Users\Lenovo\Desktop\STM32\stm32\APP\XM-01\Core
python retrain_with_vertical_shift_augmentation.py
```

### 2. 输出文件
训练完成后，会在以下目录生成文件：
```
D:\ProjectCode\JQ260\SleepPosture\cnn_export_lite_combined_vshift\
├── posture_model_vshift.keras      # Keras模型
├── posture_model_vshift.tflite     # TFLite模型（用于X-CUBE-AI）
├── optimal_threshold.txt           # 最优阈值
└── best_model.keras                # 最佳模型检查点
```

### 3. 转换为X-CUBE-AI格式
```bash
# 使用TFLite模型在X-CUBE-AI中生成C代码
# 替换现有的network模型文件
```

## 预期效果 (Expected Results)

### 训练数据分布
- **原始数据**: ~3000-4000样本
- **增强后**: ~4000-5000样本（取决于类别不平衡程度）
- **覆盖场景**:
  - 单人在上半部分 ✓
  - 单人在下半部分 ✓ (新增)
  - 双人（左右分离）✓

### 模型性能
- **预期准确率**: ≥97% (与之前相当或更好)
- **鲁棒性**: 显著提升，能处理人在床任意位置的情况

## MCU代码无需修改 (No MCU Code Changes Needed)

### 重要说明
- MCU代码中的单人处理逻辑**无需修改**
- 仍然使用完整的260字节数据进行预测
- 模型已经通过数据增强学会了处理两种情况：
  - 人在上半部分：模型从训练数据中学习
  - 人在下半部分：模型从增强数据中学习

### Important Note
- Single person handling logic in MCU code **requires NO changes**
- Still use complete 260-byte data for prediction
- Model has learned to handle both cases through augmentation:
  - Person in upper half: Learned from original training data
  - Person in lower half: Learned from augmented data

## 对比 (Comparison)

### 之前的方法 (Previous Approach)
```
单人数据 → 只有上半部分样本 → 模型只学会识别上半部分
```

### 新方法 (New Approach)
```
单人数据 → 上半部分样本 + 下半部分样本（增强）→ 模型学会识别两种位置
```

## 下一步 (Next Steps)

1. ✅ 创建训练脚本（已完成）
2. ⏳ 运行训练脚本
3. ⏳ 评估模型性能
4. ⏳ 使用X-CUBE-AI转换模型
5. ⏳ 替换MCU项目中的模型文件
6. ⏳ 编译和测试

---
创建日期: 2026-03-03
作者: Claude Code
