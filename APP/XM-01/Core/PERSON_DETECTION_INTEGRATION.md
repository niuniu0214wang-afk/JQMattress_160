# 双AI模型完整集成完成 (2026-03-03)

## 已完成的工作

### 1. 复制X-CUBE-AI生成的person_detection文件
✓ 从 `C:\Users\Lenovo\Desktop\STM32\AI-CUBEMX\AI\X-CUBE-AI\App\` 复制到主项目：
  - person_detection.c
  - person_detection.h
  - person_detection_config.h
  - person_detection_data.c
  - person_detection_data.h
  - person_detection_data_params.c
  - person_detection_data_params.h

### 2. 更新person_detection_wrapper.c
✓ 修改include路径指向 `X-CUBE-AI-NEW/App/`

### 3. 修改myEdge_ai_app.c
✓ 添加 `#include "person_detection_wrapper.h"`
✓ 添加 `#include "ai_model_wrapper.h"`
✓ 完全重写 `Model()` 函数：
  - 使用AI模型进行人数检测（person_detection）
  - 根据AI预测结果（1人或2人）进行相应处理
  - **使用AI模型进行睡姿分类（ai_model_classify）替换旧的Random Forest模型**
  - 删除了旧的规则判断代码

### 4. 修改main.c
✓ 添加 `#include "ai_model_wrapper.h"`
✓ 添加 `#include "person_detection_wrapper.h"`
✓ 在初始化中添加 `ai_model_init()`
✓ 在初始化中添加 `person_detection_init()`

## 系统架构

### 双AI模型架构
```
传感器数据 (260字节)
    ↓
[AI模型1: person_detection]
    ↓
  1人 or 2人
    ↓
┌─────────┴─────────┐
│                   │
单人处理          双人处理
│                   │
↓                   ↓
完整数据          分割数据
│                 ↙    ↘
↓            左侧(上13行) 右侧(下13行移到上13行)
│                 ↓        ↓
[AI模型2: network (睡姿分类)] × 1或2次
    ↓
输出结果
```

### 模型详情

**模型1: person_detection (人数检测)**
- 输入: 260字节传感器数据
- 输出: 2个概率值 (1人概率, 2人概率)
- 功能: 判断床上有1人还是2人
- 准确率: 100% (训练数据)

**模型2: network (睡姿分类)**
- 输入: 260字节传感器数据
- 输出: 1个概率值 (侧卧概率)
- 阈值: 0.32
- 功能: 判断睡姿是仰卧还是侧卧
- 准确率: 97.33%

## 处理流程

### 单人情况
1. AI检测到1人
2. 根据质心位置判断人在上半部分还是下半部分
3. 对完整的260字节数据进行睡姿分类（使用ai_model_classify）
4. 输出1个人的睡姿和位置

### 双人情况
1. AI检测到2人
2. 分割数据为上下两部分：
   - 左侧：前13行保留，后13行为0
   - 右侧：后13行向上移到前13行，后13行为0
3. 分别对左右两侧进行睡姿分类（使用ai_model_classify）
4. 输出2个人的睡姿和位置

## 关键修改

### split_dual_matrix() 函数
```c
// 右侧数据向上移动
rt_memcpy(lower_matrix,                    // 目标：前13行位置
          input_data + 13 * SENSOR_COLS,   // 源：后13行数据
          13 * SENSOR_COLS);               // 将后13行移到前13行
```

### Model() 函数
```c
// 使用AI进行人数检测
uint8_t predicted_persons = person_detection_predict(input, &confidence);

if (predicted_persons == 2) {
    // 双人处理逻辑
    uint8_t posture_upper = ai_model_classify(upper_matrix, NULL);  // AI睡姿分类
    uint8_t posture_lower = ai_model_classify(lower_matrix, NULL);  // AI睡姿分类
} else {
    // 单人处理逻辑
    uint8_t posture = ai_model_classify(input, NULL);  // AI睡姿分类（完整数据）
}
```

### 替换旧模型
- **旧模型**: `feature_engineering_model_run()` (Random Forest)
- **新模型**: `ai_model_classify()` (X-CUBE-AI CNN)
- 所有4处调用都已替换完成

## 编译和测试

### 需要在Keil项目中添加的文件
1. `X-CUBE-AI-NEW/App/person_detection.c`
2. `X-CUBE-AI-NEW/App/person_detection_data.c`
3. `X-CUBE-AI-NEW/App/person_detection_data_params.c`
4. `X-CUBE-AI-NEW/App/network.c`
5. `X-CUBE-AI-NEW/App/network_data.c`
6. `X-CUBE-AI-NEW/App/network_data_params.c`
7. `Core/Src/person_detection_wrapper.c`
8. `Core/Src/ai_model_wrapper.c`

### 预期性能
- 人数检测准确率: **100%** (基于训练数据)
- 睡姿分类准确率: **97.33%**
- 总体系统准确率: **97.33%**

## 下一步

1. 在Keil MDK中编译项目
2. 确保所有新文件已添加到项目
3. 烧录到STM32
4. 使用实际数据测试
5. 验证人数检测和睡姿分类的准确性

---
更新日期: 2026-03-03
最后修改: 完成所有AI模型集成，替换旧的Random Forest模型
