# 睡姿分类模型更新完成 (2026-03-03)

## 已完成的修改

### 1. MCU代码修改
✓ 修改了 `myEdge_ai_app.c` 中的 `split_dual_matrix()` 函数
  - 左侧：前13行保留，后13行为0
  - 右侧：将后13行向上移动到前13行，后13行为0
  - 与训练时的数据处理完全一致

### 2. 模型文件更新
✓ 从 `C:\Users\Lenovo\Desktop\STM32\AI-CUBEMX\AI\X-CUBE-AI\App\` 复制到主项目：
  - network.c
  - network.h
  - network_config.h
  - network_data.c
  - network_data.h
  - network_data_params.c
  - network_data_params.h

### 3. 阈值更新
✓ 更新 `ai_model_wrapper.h` 中的阈值：
  - 旧阈值: 0.45
  - 新阈值: 0.32

## 模型性能

### 训练数据来源
- 原始数据源: `C:\Users\Lenovo\Desktop\sscom_data\4.5cm_1112\` (~2000-3000样本)
- 新数据源: `C:\Users\Lenovo\Desktop\0225JQ_T12\` (952样本)
- 总计: ~3000-4000样本

### 测试结果
- **总体准确率: 97.33%** (1276/1311)
- **仰卧准确率: 99.47%** (566/569)
- **侧卧准确率: 95.69%** (710/742)
- 错误样本: 35个
  - 仰卧误判为侧卧: 3个
  - 侧卧误判为仰卧: 32个

### 改进对比
| 指标 | 旧模型 | 新模型 | 提升 |
|------|--------|--------|------|
| 总体准确率 | 70.79% | 97.33% | +26.54% |
| 仰卧准确率 | - | 99.47% | - |
| 侧卧准确率 | - | 95.69% | - |

## 下一步操作

### 1. 编译项目
在Keil MDK中：
1. 确保所有新文件已添加到项目
2. 编译项目
3. 检查是否有编译错误

### 2. 烧录测试
1. 烧录到STM32
2. 使用实际传感器数据测试
3. 验证双人样本的睡姿分类准确率

### 3. 验证要点
- 单人样本：睡姿分类应该准确
- 双人样本：
  - 左侧（上半部分）睡姿分类
  - 右侧（下半部分）睡姿分类
  - 确认右侧数据被正确向上移动

## 技术细节

### 模型架构
- 输入: 260字节 (26x10矩阵)
- 输出: 1个浮点数 (侧卧概率)
- 阈值: 0.32
  - 概率 >= 0.32 → 侧卧
  - 概率 < 0.32 → 仰卧

### 双人样本处理
```c
// 左侧（上半部分）
rt_memcpy(upper_matrix, input_data, 13 * 10);  // 前13行

// 右侧（下半部分）- 关键修改
rt_memcpy(lower_matrix,                    // 目标：前13行位置
          input_data + 13 * 10,            // 源：后13行数据
          13 * 10);                        // 将后13行移到前13行
```

## 文件位置

### 主MCU项目
- 路径: `C:\Users\Lenovo\Desktop\STM32\stm32\APP\XM-01\`
- 修改的文件:
  - `Core/Src/myEdge_ai_app.c` (split_dual_matrix函数)
  - `Core/Inc/ai_model_wrapper.h` (阈值更新)
  - `X-CUBE-AI-NEW/App/network*` (新模型文件)

### 训练项目
- 模型: `D:\ProjectCode\JQ260\SleepPosture\cnn_export_lite_combined\`
- 测试脚本: `C:\Users\Lenovo\Desktop\STM32\stm32\APP\XM-01\Core\`

---
更新日期: 2026-03-03
