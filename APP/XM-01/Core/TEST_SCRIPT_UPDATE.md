# 测试脚本更新说明 (2026-03-03)
# Test Script Update Documentation

## 更新内容 (Updates)

### test_posture_classification.py

已更新测试脚本以使用新的垂直位移增强模型。

**主要修改:**

1. **模型路径更新**
   ```python
   # 旧路径
   MODEL_DIR = r'D:\ProjectCode\JQ260\SleepPosture\cnn_export_lite_combined'
   MODEL_PATH = os.path.join(MODEL_DIR, 'jq260_cnn_lite_retrained.keras')
   THRESHOLD_PATH = os.path.join(MODEL_DIR, 'optimal_threshold.npy')

   # 新路径 (2026-03-03)
   MODEL_DIR = r'D:\ProjectCode\JQ260\SleepPosture\cnn_export_lite_combined_vshift'
   MODEL_PATH = os.path.join(MODEL_DIR, 'posture_model_vshift.keras')
   THRESHOLD_PATH = os.path.join(MODEL_DIR, 'optimal_threshold.txt')
   ```

2. **阈值加载方式更新**
   ```python
   # 旧方式: 从.npy文件加载
   threshold = np.load(THRESHOLD_PATH)[0]

   # 新方式: 从.txt文件加载 (2026-03-03)
   with open(THRESHOLD_PATH, 'r') as f:
       threshold = float(f.read().strip())
   ```

3. **文档字符串更新**
   - 添加了关于垂直位移增强的说明
   - 说明新模型能够处理人在床上半部分或下半部分的情况

## 使用流程 (Usage Workflow)

### 完整训练和测试流程

```
1. 训练新模型（包含垂直位移增强）
   ↓
   python retrain_with_vertical_shift_augmentation.py
   ↓
   生成文件:
   - posture_model_vshift.keras
   - posture_model_vshift.tflite
   - optimal_threshold.txt

2. 测试模型性能
   ↓
   python test_posture_classification.py
   ↓
   输出:
   - 总体准确率
   - 仰卧准确率
   - 侧卧准确率
   - 混淆矩阵
   - 详细结果CSV

3. 转换为X-CUBE-AI格式
   ↓
   使用X-CUBE-AI工具导入posture_model_vshift.tflite
   ↓
   生成C代码文件

4. 集成到MCU项目
   ↓
   替换X-CUBE-AI-NEW/App/network*文件
   ↓
   编译和测试
```

## 预期改进 (Expected Improvements)

### 与之前模型的对比

| 特性 | 旧模型 | 新模型 (垂直位移增强) |
|------|--------|----------------------|
| 训练数据 | 单一位置 | 上半部分 + 下半部分 |
| 单人上半部分 | ✓ 准确 | ✓ 准确 |
| 单人下半部分 | ⚠️ 可能不准确 | ✓ 准确 |
| 双人分离 | ✓ 准确 | ✓ 准确 |
| 数据增强方式 | 2种 | 4种 |
| 鲁棒性 | 中等 | 高 |

### 数据增强对比

**旧模型增强方式:**
1. 水平翻转
2. 添加噪声

**新模型增强方式:**
1. 水平翻转
2. 垂直位移 ← **新增**
3. 添加噪声
4. 组合增强（翻转+位移）← **新增**

## 测试数据说明 (Test Data Description)

### 测试数据来源
- 路径: `C:\Users\Lenovo\Desktop\0225JQ_T12\parsed_sensor_data.csv`
- 包含: 单人和双人样本
- 格式: 每行包含260字节传感器数据和标签

### 测试覆盖场景
1. **单人样本**
   - 人在上半部分（原始数据）
   - 人在下半部分（通过增强学习）
   - 仰卧姿势
   - 侧卧姿势

2. **双人样本**
   - 左侧（上半部分）
   - 右侧（下半部分）
   - 各种姿势组合

## 运行测试 (Run Tests)

### 命令
```bash
cd C:\Users\Lenovo\Desktop\STM32\stm32\APP\XM-01\Core
python test_posture_classification.py
```

### 预期输出
```
================================================================================
加载CNN睡姿分类模型
================================================================================
模型路径: D:\ProjectCode\JQ260\SleepPosture\cnn_export_lite_combined_vshift\posture_model_vshift.keras
[成功] 模型加载完成
分类阈值: 0.32

================================================================================
加载测试数据
================================================================================
数据路径: C:\Users\Lenovo\Desktop\0225JQ_T12\parsed_sensor_data.csv
总样本数: XXXX
  单人样本: XXX
  双人样本: XXX

================================================================================
测试结果
================================================================================
总体准确率: XX.XX%
仰卧准确率: XX.XX%
侧卧准确率: XX.XX%

混淆矩阵:
[[TP  FP]
 [FN  TN]]
```

## 故障排除 (Troubleshooting)

### 问题1: 模型文件不存在
**错误信息:**
```
[错误] 模型文件不存在: D:\ProjectCode\JQ260\SleepPosture\cnn_export_lite_combined_vshift\posture_model_vshift.keras
```

**解决方案:**
1. 先运行训练脚本: `python retrain_with_vertical_shift_augmentation.py`
2. 确认模型文件已生成

### 问题2: 阈值文件不存在
**错误信息:**
```
分类阈值: 0.32 (默认值)
```

**解决方案:**
- 这不是错误，会使用默认阈值0.32
- 如需使用训练时的最优阈值，确保训练脚本已完整运行

### 问题3: TensorFlow版本不兼容
**错误信息:**
```
ValueError: Unknown loss function: focal_loss_fn
```

**解决方案:**
- 确保在加载模型时传入custom_objects参数
- 脚本已包含此修复

## 下一步 (Next Steps)

1. ✅ 更新测试脚本（已完成）
2. ⏳ 运行训练脚本生成新模型
3. ⏳ 运行测试脚本验证性能
4. ⏳ 如果性能满意，使用X-CUBE-AI转换
5. ⏳ 集成到MCU项目

---
更新日期: 2026-03-03
更新内容: 支持垂直位移增强模型测试
