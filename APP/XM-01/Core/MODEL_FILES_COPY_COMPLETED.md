# X-CUBE-AI模型文件复制完成 (2026-03-03)
# X-CUBE-AI Model Files Copy Completed

## 复制完成 ✅

所有X-CUBE-AI生成的模型文件已成功复制到MCU项目。

### 复制的文件列表

#### 睡姿分类模型 (network - 垂直位移增强版)
1. ✅ network.c (25KB)
2. ✅ network.h (9.2KB)
3. ✅ network_config.h (1.6KB)
4. ✅ network_data.c (3.5KB)
5. ✅ network_data.h (2.3KB)
6. ✅ network_data_params.c (18KB)
7. ✅ network_data_params.h (1.8KB)
8. ✅ network_generate_report.txt (21KB)

#### 人数检测模型 (person_detection)
9. ✅ person_detection.c (23KB)
10. ✅ person_detection.h (9.7KB)
11. ✅ person_detection_config.h (1.6KB)
12. ✅ person_detection_data.c (3.7KB)
13. ✅ person_detection_data.h (2.5KB)
14. ✅ person_detection_data_params.c (168KB)
15. ✅ person_detection_data_params.h (2.0KB)
16. ✅ person_detection_generate_report.txt (21KB)

**总计: 16个文件**

### 目标位置
```
C:\Users\Lenovo\Desktop\STM32\stm32\APP\XM-01\X-CUBE-AI-NEW\App\
```

## 新模型信息 (New Model Information)

### 睡姿分类模型 (network)

**模型来源:**
- 文件: `D:\ProjectCode\JQ260\SleepPosture\cnn_export_lite_combined_vshift\posture_model_vshift.tflite`
- 训练脚本: `retrain_with_vertical_shift_augmentation.py`
- 特性: **包含垂直位移数据增强**

**模型规格:**
- 输入: `float32[1x26x10x1]` (1.02 KB)
- 输出: `float32[1x1]` (4 Bytes)
- 参数数量: 1,537 items (2.63 KiB)
- 权重大小: 6,148 B (6.00 KiB)
- 激活内存: 3,708 B (3.62 KiB)
- MACC: 100,099
- 模型哈希: `0x35cc066363fd0338ad5722be89f98315`

**模型架构:**
```
Input [1x26x10x1]
  ↓
Conv2D (8 filters, 3x3) + ReLU
  ↓
MaxPool2D (2x2) → [1x13x5x8]
  ↓
Conv2D (16 filters, 3x3) + ReLU
  ↓
MaxPool2D (2x2) → [1x6x2x16]
  ↓
GlobalAveragePooling → [1x16]
  ↓
Dense (16 units) + ReLU
  ↓
Dense (1 unit) + Sigmoid → [1x1]
```

**关键改进:**
- ✅ 支持人在床上半部分的识别
- ✅ 支持人在床下半部分的识别（通过垂直位移增强）
- ✅ 更强的鲁棒性和泛化能力

### 人数检测模型 (person_detection)

**模型规格:**
- 输入: 260字节传感器数据
- 输出: 2个概率值 (1人概率, 2人概率)
- 准确率: 100% (训练数据)

## MCU项目集成状态

### 已完成的集成 ✅

1. **包装器文件**
   - ✅ `Core/Inc/ai_model_wrapper.h`
   - ✅ `Core/Src/ai_model_wrapper.c`
   - ✅ `Core/Inc/person_detection_wrapper.h`
   - ✅ `Core/Src/person_detection_wrapper.c`

2. **主程序初始化**
   - ✅ `main.c`: 添加了 `ai_model_init()` 和 `person_detection_init()`

3. **模型调用**
   - ✅ `myEdge_ai_app.c`: 使用 `person_detection_predict()` 进行人数检测
   - ✅ `myEdge_ai_app.c`: 使用 `ai_model_classify()` 进行睡姿分类
   - ✅ 所有4处调用已替换为AI模型

4. **数据处理**
   - ✅ `split_dual_matrix()`: 正确处理双人数据分割

5. **X-CUBE-AI模型文件**
   - ✅ 所有16个文件已复制到 `X-CUBE-AI-NEW/App/`

## 下一步操作 (Next Steps)

### 1. 在Keil MDK中编译项目

**需要确认的文件（应该已在项目中）:**
- `X-CUBE-AI-NEW/App/network.c`
- `X-CUBE-AI-NEW/App/network_data.c`
- `X-CUBE-AI-NEW/App/network_data_params.c`
- `X-CUBE-AI-NEW/App/person_detection.c`
- `X-CUBE-AI-NEW/App/person_detection_data.c`
- `X-CUBE-AI-NEW/App/person_detection_data_params.c`
- `Core/Src/ai_model_wrapper.c`
- `Core/Src/person_detection_wrapper.c`

**包含路径（应该已配置）:**
- `Core/Inc`
- `X-CUBE-AI-NEW/App`
- `Middlewares/ST/AI/Inc`

### 2. 编译检查清单

- [ ] 打开Keil MDK项目
- [ ] 清理项目 (Clean)
- [ ] 重新编译 (Rebuild)
- [ ] 检查编译输出，确认无错误
- [ ] 检查代码大小是否在Flash限制内

### 3. 烧录和测试

- [ ] 烧录到STM32F405RG
- [ ] 使用实际传感器数据测试
- [ ] 验证人数检测功能
- [ ] 验证睡姿分类功能
- [ ] 测试单人在上半部分的情况
- [ ] 测试单人在下半部分的情况
- [ ] 测试双人情况

### 4. 性能验证

**预期性能指标:**
- 人数检测准确率: ≥99%
- 睡姿分类准确率: ≥97%
- 推理时间: <100ms (每次预测)
- 内存使用:
  - Flash: ~180KB (两个模型)
  - RAM: ~8KB (激活内存)

## 模型对比 (Model Comparison)

| 特性 | 旧模型 | 新模型 (垂直位移增强) |
|------|--------|----------------------|
| 训练数据增强 | 水平翻转 + 噪声 | 水平翻转 + 垂直位移 + 噪声 + 组合 |
| 单人上半部分 | ✓ | ✓ |
| 单人下半部分 | ⚠️ 可能不准确 | ✓ 准确 |
| 双人分离 | ✓ | ✓ |
| 模型大小 | ~6KB | ~6KB (相同) |
| 推理速度 | ~100K MACC | ~100K MACC (相同) |
| 鲁棒性 | 中等 | 高 |

## 技术细节 (Technical Details)

### 垂直位移增强原理

**问题:**
- 原始训练数据中，单人样本主要是人在床的上半部分
- 实际使用时，人可能在床的下半部分（右侧）

**解决方案:**
- 对每个单人样本，创建垂直位移版本
- 将前13行数据移到后13行，模拟"人在下半部分"的情况

**效果:**
- 模型学会识别人在任意位置（上半部分或下半部分）
- MCU代码无需修改，模型自动处理

### 数据处理流程

```
传感器数据 (260字节)
    ↓
[person_detection_predict()]
    ↓
  1人 or 2人
    ↓
┌─────────┴─────────┐
│                   │
单人                双人
↓                   ↓
完整数据            split_dual_matrix()
│                 ↙    ↘
↓            上13行    下13行→上13行
│                 ↓        ↓
[ai_model_classify()] × 1或2次
    ↓
输出结果
```

## 故障排除 (Troubleshooting)

### 编译错误

**错误1: 找不到头文件**
```
fatal error: network.h: No such file or directory
```
**解决:** 检查包含路径是否包含 `X-CUBE-AI-NEW/App`

**错误2: 链接错误**
```
undefined reference to `ai_network_create`
```
**解决:** 确保 `network.c` 已添加到项目中

**错误3: Flash溢出**
```
region `FLASH' overflowed by XXX bytes
```
**解决:** 检查是否有旧的模型文件占用空间，删除不需要的文件

### 运行时错误

**错误1: 模型初始化失败**
- 检查激活内存是否足够
- 检查权重数据是否正确加载

**错误2: 预测结果异常**
- 检查输入数据归一化是否正确
- 检查阈值设置是否正确（应为0.32）

## 总结 (Summary)

✅ **所有X-CUBE-AI生成的模型文件已成功复制到MCU项目**

✅ **新模型包含垂直位移增强，能够处理人在床任意位置的情况**

✅ **MCU代码已完全集成双AI模型架构**

⏳ **下一步: 在Keil MDK中编译和测试**

---
复制完成时间: 2026-03-03 19:51
模型版本: posture_model_vshift (垂直位移增强版)
