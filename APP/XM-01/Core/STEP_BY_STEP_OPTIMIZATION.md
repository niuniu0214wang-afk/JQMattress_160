# 睡姿检测系统优化 - 分步指南
# Step-by-Step Optimization Guide
# 日期: 2026-03-03

## 问题诊断总结

**当前状态:**
- 人数检测准确率: 38.13%
- 单人被误判为双人: 98.48% (583/592样本)
- 根本原因: 双人检测阈值过于宽松 (0.25-0.75)

**关键代码位置:**
- 文件: `Core/Src/myEdge_ai_app.c`
- 行号: 587-588

```c
int is_balanced_pressure = (min_pressure_ratio > 0.25f) && (max_pressure_ratio < 0.75f);
int is_balanced_sensors = (min_sensor_ratio > 0.25f) && (max_sensor_ratio < 0.75f);
```

---

## 优化步骤

### 步骤1: 修改双人检测阈值 (最关键)

**目标:** 将阈值从 25%-75% 改为 40%-60%，使双人检测更严格

**操作:**
1. 打开文件: `Core/Src/myEdge_ai_app.c`
2. 找到第 587-588 行
3. 修改代码如下:

**原代码:**
```c
int is_balanced_pressure = (min_pressure_ratio > 0.25f) && (max_pressure_ratio < 0.75f);
int is_balanced_sensors = (min_sensor_ratio > 0.25f) && (max_sensor_ratio < 0.75f);
```

**修改为:**
```c
// 优化: 提高双人检测阈值，减少单人误判 (2026-03-03)
int is_balanced_pressure = (min_pressure_ratio > 0.40f) && (max_pressure_ratio < 0.60f);
int is_balanced_sensors = (min_sensor_ratio > 0.40f) && (max_sensor_ratio < 0.60f);
```

**预期效果:** 人数检测准确率从 38% → 70%+

---

### 步骤2: 增加最小区域大小检查

**目标:** 确保双人时每侧都有足够大的人体区域

**操作:**
1. 在同一文件中，找到第 622 行的判断条件
2. 在判断之前添加新的检查

**在第 619 行后添加:**
```c
// 条件5: 每侧区域大小检查 - 双人时每侧至少50个传感器 (2026-03-03)
int min_region_sensors = 50;
int upper_valid = (upper_sensors >= min_region_sensors);
int lower_valid = (lower_sensors >= min_region_sensors);
```

**修改第 622 行的判断条件:**

**原代码:**
```c
if (is_dual_by_components && is_dual_by_total && is_balanced_pressure && is_balanced_sensors && is_dual_by_magnitude)
```

**修改为:**
```c
if (is_dual_by_components && is_dual_by_total && is_balanced_pressure && is_balanced_sensors && is_dual_by_magnitude && upper_valid && lower_valid)
```

**预期效果:** 进一步减少误判，准确率提升到 75%+

---

### 步骤3: 增加压力差检查

**目标:** 双人时两侧压力不应相差太大

**操作:**
在第 619 行后继续添加:

```c
// 条件6: 压力差检查 - 双人时两侧压力差不超过总压力的40% (2026-03-03)
int pressure_diff = (upper_pressure > lower_pressure) ?
                    (upper_pressure - lower_pressure) :
                    (lower_pressure - upper_pressure);
int max_allowed_diff = (total_pressure * 40) / 100;  // 最大差异40%
int is_balanced_diff = (pressure_diff < max_allowed_diff);
```

**修改判断条件为:**
```c
if (is_dual_by_components && is_dual_by_total && is_balanced_pressure && is_balanced_sensors && is_dual_by_magnitude && upper_valid && lower_valid && is_balanced_diff)
```

**预期效果:** 准确率提升到 80%+

---

### 步骤4: 编译和测试

**操作:**
1. 保存修改后的文件
2. 在Keil MDK中编译项目
3. 烧录到STM32设备
4. 使用测试数据验证

**验证方法:**
- 重新收集测试数据
- 运行 `parse_sensor_data.py` 分析结果
- 检查人数检测准确率是否提升

---

## 完整修改后的代码

**文件: Core/Src/myEdge_ai_app.c (第 587-625 行)**

```c
// 判断分布是否均衡
// 优化: 提高双人检测阈值，减少单人误判 (2026-03-03)
int is_balanced_pressure = (min_pressure_ratio > 0.40f) && (max_pressure_ratio < 0.60f);
int is_balanced_sensors = (min_sensor_ratio > 0.40f) && (max_sensor_ratio < 0.60f);

int person_count = 0;

// ======== 超细化决策逻辑 v2 - 2025-11-06 ========
// 核心原则: 双人检测必须基于多个强信号的组合，而非仅凭分布均衡
// 关键洞察: 单人可能有2-3个连通分量+相对均衡的分布(对角线躺卧)
//          但双人必须同时满足: 两侧都有人体信号 + 足够的总数据量

// ===== 双人检测 - ULTRA STRICT 条件 (ALL must be satisfied) =====
// 核心要求: 同时满足以下ALL条件才判为双人 (AND logic, not OR)
// 1. 两侧都有独立的连通分量 - 这是最重要的信号
// 2. 总连通分量 >= 3 (not just >= 2, because single can have 2-3)
// 3. 两侧压力/传感器都要充分均衡 (>40%各侧) - 优化 (2026-03-03)
// 4. 绝对值足够大: 总压力 > 7500, 总传感器 > 120
//    (这排除了只是"噪声均衡分布"的单人样本)
// 5. 每侧区域大小足够 (>= 50传感器) - 新增 (2026-03-03)
// 6. 两侧压力差不超过40% - 新增 (2026-03-03)

// ===== 双人检测 - MCU-Safe严格条件 - 2025-11-06 =====
// 使用整数比较避免浮点精度问题
// 条件1: 两侧都有独立的连通分量
int is_dual_by_components = (upper_components > 0) && (lower_components > 0);

// 条件2: 总连通分量 >= 3
int is_dual_by_total = (total_components >= 3);

// 条件3: 绝对值足够大 (使用整数比较，避免浮点误差)
// 双人特征: 总压力 >= 7500, 总传感器 >= 120
int is_dual_by_magnitude = (total_pressure >= 7500) && (total_sensors >= 120);

// 条件4: 分布均衡 (已计算的浮点值)
// is_balanced_pressure && is_balanced_sensors

// 条件5: 每侧区域大小检查 - 双人时每侧至少50个传感器 (2026-03-03)
int min_region_sensors = 50;
int upper_valid = (upper_sensors >= min_region_sensors);
int lower_valid = (lower_sensors >= min_region_sensors);

// 条件6: 压力差检查 - 双人时两侧压力差不超过总压力的40% (2026-03-03)
int pressure_diff = (upper_pressure > lower_pressure) ?
                    (upper_pressure - lower_pressure) :
                    (lower_pressure - upper_pressure);
int max_allowed_diff = (total_pressure * 40) / 100;  // 最大差异40%
int is_balanced_diff = (pressure_diff < max_allowed_diff);

// 综合判断 - 使用显式逻辑避免编译器优化问题
int is_true_dual = 0;
if (is_dual_by_components && is_dual_by_total &&
    is_balanced_pressure && is_balanced_sensors &&
    is_dual_by_magnitude &&
    upper_valid && lower_valid &&
    is_balanced_diff)
{
    is_true_dual = 1;
}
```

---

## 同步修改调试代码

如果你使用了调试功能，也需要修改 `MODEL_DEBUG.c`:

**文件: Core/Src/MODEL_DEBUG.c (第 53-54 行)**

**原代码:**
```c
int is_balanced_p = (min_p > 0.25f) && (max_p < 0.75f);
int is_balanced_s = (min_s > 0.25f) && (max_s < 0.75f);
```

**修改为:**
```c
int is_balanced_p = (min_p > 0.40f) && (max_p < 0.60f);
int is_balanced_s = (min_s > 0.40f) && (max_s < 0.60f);
```

---

## 测试验证清单

完成修改后，按以下步骤验证:

- [ ] 代码编译无错误
- [ ] 烧录到设备成功
- [ ] 收集单人测试数据 (至少50个样本)
- [ ] 收集双人测试数据 (至少30个样本)
- [ ] 运行 `parse_sensor_data.py` 分析
- [ ] 检查人数检测准确率 >= 70%
- [ ] 检查单人误判率 < 30%
- [ ] 检查双人检测准确率 >= 80%

---

## 预期结果对比

| 指标 | 修改前 | 修改后(预期) |
|------|--------|-------------|
| 人数检测准确率 | 38.13% | 75%+ |
| 单人误判为双人 | 98.48% | <25% |
| 双人检测准确率 | 100% | 95%+ |
| 单人检测准确率 | 1.52% | 85%+ |

---

## 如果效果不理想

如果修改后准确率仍低于70%，可以尝试:

1. **进一步提高阈值:** 改为 0.45-0.55
2. **增加最小传感器数:** 从50改为60或70
3. **减小压力差容忍度:** 从40%改为30%

---

文档创建日期: 2026-03-03
